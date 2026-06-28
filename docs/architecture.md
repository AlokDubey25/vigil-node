# Vigil Node — Architecture

## what it does

real-time pre-trade fraud detection engine. every order that enters
the marketplace gets validated, scored, and either allowed
into the book or blocked before execution.

---

## layer overview

``` architecture
                    ┌─────────────────────────────┐
  order in ──────→  │  C++ Engine (main.cpp)       │
                    │  OrderBook + DatabaseHandler  │
                    └───────────┬─────────────────-┘
                                │ feature vector
                                ↓
                    ┌─────────────────────────────┐
                    │  Python ML         │
                    │  XGBoost + RF ensemble       │
                    │  scorer.py via stdin/stdout  │
                    └───────────┬─────────────────-┘
                                │ fraud score 0.0–1.0
                                ↓
                    ┌─────────────────────────────┐
                    │  SQLite (vigil.db)           │
                    │  orders, trades, risk_log    │
                    └─────────────────────────────┘
```

---

## components built

### OrderBook  (cpp/src/orderbook.cpp)

- BUY book: std::map>
- SELL book: std::map
- keys in paise (price × 100) — exact integer comparison
- addOrder() validates, returns bool
- matchOrders() returns Trade struct with filled/partial flags
- all Catch2 tested in tests/cpp/test_orderbook.cpp

### DatabaseHandler  (cpp/src/database.cpp)

- opens vigil.db, creates 3 tables if not exist
- WAL journal mode for concurrent read+write
- saveOrder(), saveTrade(), saveRiskEvent(), updateOrderStatus()
- loadBlacklist() — loads PERMANENT_BLOCK users on startup
- all Catch2 tested in tests/cpp/test_database.cpp using :memory: DB

---

## data flow — current state

``` architecture
order arrives
    → saveOrder(status=PENDING)
    → addOrder() into std::map
    → matchOrders()
         ↓ Trade returned
    → saveTrade()
    → updateOrderStatus(FILLED) for consumed orders
```

---

## key design decisions

- paise keys: prices stored as long long (× 100) to avoid float comparison bugs
- RAII: DatabaseHandler opens on construct, closes on destruct automatically
- WAL mode: allows dashboard to read while engine writes concurrently
- :memory: tests: test DB never touches disk — fast and clean
- separation: OrderBook knows nothing about the DB, DB knows nothing about OrderBook
- stub pattern: methods declared, stubbed, implemented

---

## file layout

``` architecture
cpp/include/   ← headers (contracts)
cpp/src/       ← implementations
cpp/vendor/    ← sqlite3 amalgamation
tests/cpp/     ← Catch2 unit tests
python/utils/  ← config + logging helpers
docs/          ← this file + schema.md + bridge_contract.md
```

## components built (Phases 3–8)

### FeatureExtractor  (cpp/src/feature_extractor.cpp)

- rolling 100-order window per user via unordered_map<string deque<Order>> 
- 6 behavioral features: velocity, priceDeviation, cancelRate,
  sizeRatio, timeBetween, repeatPriceRate
- toJSON() for the Python bridge, isSuspicious() rule-based pre-check
- all features capped to prevent ML outlier distortion

### Python ML ensemble  (python/models/, python/bridge/scorer.py)

- XGBoost + Random Forest, both handling class imbalance independently
- ensemble_score() averages both models' fraud probability
- scorer.py: stdin/stdout loop, validates input, never crashes the pipe

### Bridge  (cpp/src/bridge.cpp)

- fork + pipe + dup2 + execl — bidirectional C++↔Python communication
- handshake protocol (Python sends {"ready":true} before scoring starts)
- fallback score (0.5) if Python is unavailable — engine never crashes

### Config  (cpp/src/config.cpp)

- pimpl pattern — nlohmann/json.hpp isolated to one .cpp file
- every threshold, weight, and limit is in config/settings.json
- no recompilation needed to retune the system

### Graph  (cpp/src/graph.cpp)

- directed adjacency list — buyer → seller edges per trade
- DFS cycle detection (depth ≤ 3) catches wash trading
- BFS connected components find entire fraud rings, not just pairs
- getNetworkScore() feeds into the final combined verdict

### Dashboard  (python/dashboard/)

- db_reader.py: read-only SQLite access, safe alongside the writing engine (WAL mode)
- dashboard.py: Rich-based 4-panel live terminal UI
- header bar shows engine online/offline status + current threshold
- event-driven refresh — redraws only when data changes, not on a blind timer

---

## final data flow (Phases 1–8 combined)

``` architecture
order arrives
  → saveOrder(PENDING)                      
  → blacklist check (O(1) unordered_set)    
  → extract 6 features                       
  → ML score (XGBoost + RF ensemble)         
  → graph network score (cycle + degree)     
  → combined = ml*0.7 + graph*0.3            
  → verdict vs config threshold              
       REJECTED → risk_log + escalation
       ALLOWED  → order book → match engine  
                → trade saved → graph edge added
  → dashboard reads live from SQLite         
```

---

## status — all phases complete

| phase | component           | status |
|-------|---------------------|--------|
| 1     | Order book          | ✓ done |
| 2     | SQLite persistence  | ✓ done |
| 3     | Feature extractor   | ✓ done |
| 4     | ML ensemble         | ✓ done |
| 5     | C++↔Python bridge   | ✓ done |
| 6     | Blacklist + verdict | ✓ done |
| 7     | Graph logic         | ✓ done |
| 8     | CLI dashboard       | ✓ done |
