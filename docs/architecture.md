# Vigil Node — Architecture

## Overview

Vigil Node is a hybrid C++/Python fraud detection engine. The two languages are not interchangeable — they exist for distinct reasons and communicate through a well-defined IPC protocol.

**C++ owns:** order book state, feature extraction, graph analysis, IPC bridge, config, audit writes  
**Python owns:** ML inference (XGBoost + Random Forest), SHAP explainability, live dashboard reads

The SQLite database is the only shared state between them, accessed safely via WAL mode — writers never block readers.

---

## Full data flow

``` How model actually works...

Order arrives (interactive menu or run command)
        │
        ▼
saveOrder(PENDING)                      writes to orders table
        │
        ▼
Blacklist check  ─── O(1) unordered_set lookup ─── BLOCKED? ──► REJECT + risk_log
        │ clear
        ▼
Balance check (BUY only)  ──────────────────────── too low? ──► REJECT_FUNDS + risk_log
        │ sufficient
        ▼
FeatureExtractor::extract()
  rolling 100-order window per user
  6 features: velocity, priceDeviation, cancelRate,
              sizeRatio, timeBetween, repeatPriceRate
        │
        ├──────────────────────────────────────────────────────────────────────────►
        │                                                Python process (scorer.py)
        │  fv.toJSON() ──► pipe (write fd) ──────────►  validate_features()
        │                                                ensemble_score()
        │                                                if score > 0.3: explain_text()
        │  {"score":X,"explanation":"..."} ◄── pipe ──  json.dumps() + flush
        │◄─────────────────────────────────────────────────────────────────────────
        │  select() timeout: 100ms
        │  fallback score: 0.5 if timeout or pipe error
        │
        ├── Graph::getNetworkScore(userID)
        │     degree bonus: +0.1 if degree≥3, +0.2 if degree≥5
        │     cycle bonus:  +cycle_base_score (default 0.50) if in DFS cycle
        │     capped at 1.0
        │
        ▼
combined = mlScore × ml_weight + graphScore × graph_weight
         = mlScore × 0.70     + graphScore × 0.30
        │
        ├── combined > fraud_threshold (0.80)?
        │         YES ──► escalateAction() ──► WARN / TEMP_BLOCK / PERMANENT_BLOCK
        │                 updateOrderStatus(REJECTED)
        │                 saveRiskEvent(score, SHAP reason, action)
        │                 if PERMANENT_BLOCK: blocked.insert(userID)
        │
        │         NO  ──► OrderBook::addOrder()
        │                 updateOrderStatus(PENDING→stays)
        │
        ▼
runMatchLoop()
  while best_bid >= best_ask:
    Trade t = matchOrders()
    saveTrade(t)
    updateOrderStatus(FILLED)
    settleTrade(buyer, seller, price×qty)   ── BEGIN/COMMIT/ROLLBACK transaction
    tradeGraph.addEdge(buyer, seller)
    if isInCycle(buyer) or isInCycle(seller):
      getConnectedComponent() ──► saveRiskEvent() for all ring members
        │
        ▼
Dashboard (separate process, separate terminal)
  polls SQLite every 0.5s via WAL-safe reads
  redraws only when (max_orderID, max_logID, max_txnID) tuple changes
  5 panels: Engine Stats, Recent Orders, Risk Log, Trading Network, Money & Orders
```

---

## Components

### L2 Order Book (cpp/src/orderbook.cpp)

Two `map` containers with opposite sort orders:

``` Orderbook
buyBook_  : map<double, Order, greater<double>>   highest bid first
sellBook_ : map<double, Order>                          lowest ask first
```

Keys are prices in rupees (floating point). One order per exact price level — last writer wins on collision. Matching runs when `bestBid >= bestAsk`. Partial fills supported — the unfilled remainder stays in the book.

### Feature Extractor (cpp/src/feature_extractor.cpp)

Per-user rolling window of the last 100 orders stored in `unordered_map<string, deque<Order>>`. Six features computed at extract time:

| Feature         | Description                            | Cap |
|-----------------|----------------------------------------|-----|
| velocity        | orders per minute in window            | 100 |
| priceDeviation  | distance from mid-price as fraction    | 1.0 |
| cancelRate      | cancels / window size                  | 1.0 |
| sizeRatio       | this order qty / mean window qty       | 20  |
| timeBetween     | seconds since last order (inverted)    | 1.0 |
| repeatPriceRate | fraction of orders at this exact price | 1.0 |

`isSuspicious()` flags an order if 2+ features exceed their thresholds. `friendlyVerdict()` translates the same thresholds into a human sentence.

### C++↔Python Bridge (cpp/src/bridge.cpp)

``` Bridge
C++ parent process                Python child process (scorer.py)
─────────────────                 ──────────────────────────────
fork()
  child: dup2 stdin/stdout
         execl scorer.py
parent: close unused pipe ends
        rfd_ = raw read fd        loads XGBoost + RF models
        wfd_ = write fd           prints {"ready": true}
waitForReady(5000ms timeout) ◄────────────────────────────
                                  
score(featureJSON):
  writeLine(JSON array)    ──────► validate_features()
                                   ensemble_score()
  readLineWithTimeout(100ms)        if score>0.3: explain_text()
       ◄─────────────────────────  print({"score":X,"explanation":"..."})

select() on rfd_ before every read — never blocks 
pending_ string buffers partial reads across multiple read() calls
timeout → FALLBACK_SCORE(0.5), ready_ stays true
empty response → FALLBACK_SCORE, ready_ = false
```

### Graph Engine (cpp/src/graph.cpp)

Directed adjacency list: `unordered_map<string, unordered_set<string>>`.

**DFS cycle detection** — depth limited to 3 to avoid full traversal cost on large graphs. Starts from a node, follows outgoing edges, checks if any path returns to the start within 3 hops.

**BFS connected component** — undirected traversal (forward + reverse edges) finds all users reachable from a given node. Used after a cycle is detected to flag the entire ring, not just the two endpoints.

**Network score formula:**

``` Network works -
score = 0.0
if inCycle:  score += cycle_base_score   (config, default 0.50)
if degree≥3: score += 0.1
if degree≥5: score += 0.2
score = min(score, 1.0)
```

### Python ML Ensemble (python/models/)

Training data: 4,000 normal + 400 fraud simulated orders (10:1 imbalance). Both models handle imbalance independently — XGBoost via `scale_pos_weight`, Random Forest via `class_weight="balanced"`. Final score is the mean of both models' fraud probability outputs.

SHAP uses `TreeExplainer` on the XGBoost model only — exact and fast for tree models. Runs only when `score > 0.3` to avoid paying the compute cost on clearly clean orders.

### Dashboard (python/dashboard/)

Reads SQLite in a separate OS process. WAL mode guarantees reads never block the C++ writer. Change detection uses a 3-tuple `(max_orderID, max_logID, max_txnID)` — redraws only when any value changes, not on a fixed timer. Keyboard listener runs in a daemon thread using `termios` raw mode.

---

## Database schema

```sql
orders (
    orderID   INTEGER PRIMARY KEY,
    userID    TEXT, price REAL, quantity INTEGER, side TEXT,
    timestamp INTEGER, status TEXT DEFAULT 'PENDING',
    blockScore REAL DEFAULT 0.0, fraudFlag INTEGER DEFAULT 0
)

trades (
    tradeID     INTEGER PRIMARY KEY AUTOINCREMENT,
    buyOrderID  INTEGER REFERENCES orders(orderID),
    sellOrderID INTEGER REFERENCES orders(orderID),
    price REAL, quantity INTEGER, timestamp INTEGER
)

risk_log (
    logID      INTEGER PRIMARY KEY AUTOINCREMENT,
    userID     TEXT, orderID INTEGER REFERENCES orders(orderID),
    fraudScore REAL, reason TEXT, action TEXT, timestamp INTEGER
)

accounts (
    userID  TEXT PRIMARY KEY,
    balance REAL NOT NULL DEFAULT 0.0
)

transactions (
    txnID        INTEGER PRIMARY KEY AUTOINCREMENT,
    userID       TEXT, type TEXT,
    amount       REAL, balanceAfter REAL,
    timestamp    INTEGER, note TEXT
)
```

**action values in risk_log:**

| Value           | Meaning                                                                               |
|-----------------|---------------------------------------------------------------------------------------|
| WARN            | Flagged 1–2 times — order rejected, no lasting change                                 |
| TEMP_BLOCK      | Flagged 3–4 times — order rejected                                                    |
| PERMANENT_BLOCK | Flagged 5+ times — added to in-memory blacklist, all future orders instantly rejected |
| REJECT          | Already on blacklist — instant reject, no scoring                                     |
| REJECT_FUNDS    | Insufficient balance for BUY — not a fraud signal                                     |

---

## IPC bridge protocol

``` IPC brige -
startup:
  Python → C++:  {"ready": true}
  timeout: 5000ms (model loading is slow)

per order:
  C++ → Python:  [vel, pdev, crate, sratio, tbetween, rpprice]
                 one JSON array, one line, newline-terminated
  Python → C++:  {"score": 0.0312}
              or {"score": 0.8450, "explanation": "velocity increased the score (52%)"}
              or {"error": "velocity outside range [0, 100]"}
  timeout: 100ms (bridge_timeout_ms in settings.json)

failure handling:
  timeout          → FALLBACK_SCORE (0.5), ready_ stays true
  empty response   → FALLBACK_SCORE, ready_ = false
  {"error": ...}   → FALLBACK_SCORE, logged as warning
  pipe closed      → FALLBACK_SCORE, ready_ = false
```

---

## Config reference

`config/settings.json` — all values read at startup, no recompilation needed:

```json
{
  "fraud_threshold":          0.80,   // combined score above which order is blocked
  "blacklist_temp_threshold": 3,      // flags before TEMP_BLOCK
  "blacklist_perm_threshold": 5,      // flags before PERMANENT_BLOCK
  "ml_weight":                0.70,   // ML contribution to combined score
  "graph_weight":             0.30,   // graph contribution to combined score
  "cycle_base_score":         0.50,   // graph score added when user is in a cycle
  "bridge_timeout_ms":        100     // how long to wait for Python to respond
}
```

---

## Known design boundaries

| Boundary                             | Reason                                                                                   |
|--------------------------------------|------------------------------------------------------------------------------------------|
| Buying power not reserved            | Concurrent orders can collectively overspend; full fix needs a reservation system        |
| SELL not checked for share ownership | No position tracking table; out of scope                                                 |
| Single-threaded engine               | Shared in-memory state (book, graph, blocked set) was never made thread-safe             |
| TEMP_BLOCK = WARN in behavior        | Both reject the triggering order only; real session-scoped ban needs time-based tracking |
| One order per price level            | map key is price; collision = overwrite                                                  |
| UserID is free text                  | No authentication; trust model is "whoever says they're X is X"                          |
