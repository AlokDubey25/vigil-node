<div align="center">

  <!-- Main Animated Premium Banner -->
  <img src="https://capsule-render.vercel.app/render?type=waving&color=gradient&customColorList=23&height=220&section=header&text=🛡️%20VIGIL%20NODE&fontSize=56&fontColor=ffffff&animation=twinkling" alt="Vigil Node Banner" />

  <samp>
    <h2>⚡ Ultra-Low Latency Pre-Trade Fraud Detection Engine ⚡</h2>
  </samp>

  <!-- Bright, Professional Technology Badges -->
  [![C++_17](https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
  [![Python_3.10+](https://img.shields.io/badge/Python-3.10+-3776AB?style=for-the-badge&logo=python&logoColor=white)](https://www.python.org/)
  [![Linux_WSL2](https://img.shields.io/badge/Linux-WSL2-FCC624?style=for-the-badge&logo=linux&logoColor=black)](https://ubuntu.com/)
  [![SQLite3](https://img.shields.io/badge/SQLite-3-003B57?style=for-the-badge&logo=sqlite&logoColor=white)](https://www.sqlite.org/)
  [![Build_Status](https://img.shields.io/badge/CI-passing-4c1?style=for-the-badge&logo=github-actions&logoColor=white)](https://github.com)

  <br>

  <!-- Live Engine Operational Matrix -->
  <code>🟢 Engine: ONLINE</code> │ <code>⏱️ Latency: < 5µs</code> │ <code>📊 5-Panel TUI Console: ENABLED</code> │ <code>🗄️ Storage: SQLite WAL</code>

  <br>

  ---


## 🛡️ Core Mechanism: What It Does

Every order placed through Vigil Node passes through three completely independent checks before it can touch the order book:

---

1. **🟩 Balance check** — instant rejection if funds are insufficient, no ML cost paid
2. **🟨 Behavioral ML** — six features extracted per order (velocity, cancel rate, price deviation, size ratio, time between orders, repeat price rate) sent to an XGBoost + Random Forest ensemble over a live IPC bridge
3. **🟥 Graph network analysis** — directed trade graph tracks buyer→seller edges; DFS cycle detection catches wash trading rings the ML never sees

> 📝 **Enforcement Verdict:** If any pipeline check fails, the instruction is rejected with a plain-English explanation, a SHAP-based technical reason, and a permanent forensic audit log entry. If all three pass, the order safely logs into the L2 order book matrix and the matching engine executes.

---

## 🏗️ System Architecture

``` Architecture

User (interactive menu or run command)
        │
        ▼
  ┌─────────────────────────────────────────────┐
  │           C++ Engine (main.cpp)             │
  │                                             │
  │  1. Balance check  ──► REJECT_FUNDS         │
  │  2. Blacklist O(1) ──► REJECT               │
  │  3. Feature extractor (6 signals)           │
  │       │                                     │
  │       ├──► ML Bridge (fork+pipe+select) ───►│──► Python scorer
  │       │         score + SHAP reason ◄───────│◄── XGBoost + RF ensemble
  │       │                                     │
  │       └──► Graph engine (C++)               │
  │              DFS cycle / BFS ring           │
  │                                             │
  │  Combined = ML×0.70 + Graph×0.30            │
  │  vs fraud_threshold (default 0.80)          │
  │                                             │
  │  ALLOWED ──► L2 Order Book ──► Match engine │
  │  BLOCKED ──► risk_log + escalation          │
  └─────────────────────────────────────────────┘
        │                         │
        ▼                         ▼
  SQLite (WAL mode)        Rich terminal dashboard
  orders, trades,          reads live, 5 panels,
  risk_log, accounts,      event-driven refresh
  transactions
```

### Why two languages

C++ handles the order book, feature extraction, graph analysis, and IPC bridge — everything that needs deterministic latency with no GC pauses. Python handles the ML ensemble — the ecosystem is unmatched and inference runs in a separate process, so a Python crash or timeout never takes down the engine. The bridge has a 100ms timeout and a fallback score.

### Stack

| Component          | Technology                                 |
|--------------------|--------------------------------------------|
| Order book         | C++, `map` price-time priority             |
| Persistence        | SQLite 3 (WAL mode, concurrent reads)      |
| Feature extraction | C++, rolling 100-order window per user     |
| ML ensemble        | Python, XGBoost + Random Forest            |
| IPC bridge         | fork + pipe + exec + `select()` timeout    |
| Graph analysis     | C++,DFS cycle detection+BFS ring discovery |
| Explainability     | SHAP TreeExplainer, plain-English verdict  |
| Dashboard          | Python, Rich, event-driven 5-panel TUI     |
| CI                 | GitHub Actions, parallel C++ + Python jobs |

---

## Quick start

### Prerequisites

- GCC 11+ or Clang 13+ with C++17
- CMake 3.16+
- Python 3.10+
- SQLite3 (vendored amalgamation included)

### Build

```bash
git clone https://github.com/AlokDubey25/vigil-node.git
cd vigil-node

# C++ engine
mkdir build && cd build && cmake .. && make && cd ..

# Python environment
python3 -m venv .venv && source .venv/bin/activate
pip install -r python/requirements.txt

# Config
cp config/settings.example.json config/settings.json

# Train ML models
python data/training_data/simulate_data.py
python python/models/train_xgb.py
python python/models/train_rf.py
```

### Run

```bash
# always reset before a fresh demo
./build/vigil reset

# terminal 1 — the engine
./build/vigil interactive

# terminal 2 — live dashboard (while terminal 1 is open)
python python/dashboard/dashboard.py
```

---

## Commands

```bash
./build/vigil run                    # automated demo — normal orders + wash trade detection
./build/vigil interactive            # guided menu — buy, sell, deposit, withdraw, history
./build/vigil explain <orderID>      # full decision trail for one order
./build/vigil history <userID>       # last 10 transactions for a user
./build/vigil benchmark [--n N]      # 4-stage pipeline throughput measurement
./build/vigil reset                  # wipe vigil.db and start fresh
./build/vigil --help
```

### Interactive menu options

``` Options
1. Buy          place a buy order
2. Sell         place a sell order
3. Deposit      add funds to your account
4. Withdraw     remove funds
5. Balance      check current balance
6. History      view last 10 transactions
7. Order book   print current book state
8. Cancel       cancel a pending order by ID
9. Switch user  change active user
0. Quit
```

---

## Fraud detection in practice

### Behavioral ML — spoofing / layering

A user placing many orders rapidly at the same price level will see velocity and repeatPriceRate climb. Adding cancellations drives cancelRate up. The ML ensemble combines all six features and outputs a fraud probability. Above the configured threshold (default 0.80), the order is blocked and the user's flag count increments toward a temporary or permanent ban.

``` Output of fraudster (Spoofing)
[FV] Fraudster vel=7.000 crate=0.600 rpprice=0.800
       [ML]    score=0.8450
       [FINAL] score=0.5915
"Order blocked — trading unusually fast, cancelling unusually often,repeatedly hitting the exact same price level."
[FLAGGED] Fraudster action=WARN
```

### Graph detection — wash trading

Two users trading back and forth form a directed cycle in the trade graph. DFS detects the cycle the moment the second edge completes. Both users are flagged even if every individual order scored clean on the ML — the graph catches what behavioral analysis cannot see.

``` Wash Trading
[GRAPH] edge: WashA -> WashB
[GRAPH] edge: WashB -> WashA
[RING] wash trade ring detected - 2 member
  [RING] flagging WashA
  [RING] flagging WashB
```

### Escalating enforcement

| Flags | Action          | Effect                                                          |
|-------|-----------------|-----------------------------------------------------------------|
| 1–2   | WARN            | order rejected, next order scored fresh                         |
| 3–4   | TEMP_BLOCK      | This order rejected                                             |
| 5+    | PERMANENT_BLOCK | User added to blacklist, all future orders instantly rejected   |

---

## Performance

Measured with `./build/vigil benchmark --n 5000` on a typical development machine:

| Stage                | Throughput          | Notes                        |
|----------------------|---------------------|------------------------------|
| Order book only      | ~600,000 orders/sec | Pure in-memory, map          |
| + SQLite persistence | ~12,000 orders/sec  | Disk writes dominate         |
| + Feature extraction | ~9,000 orders/sec   | Minimal overhead             |
| + ML bridge          | ~200 orders/sec     | One IPC round-trip per order |

The ML bridge is the bottleneck by design — replacing stdin/stdout with gRPC would cut Stage 4 latency by roughly 10x.

---

## Tests

```bash
cd build && ctest --output-on-failure    # 6 C++ binaries
pytest tests/python/ -v                  # 39 Python tests
```

| Suite              | Tests | Covers                                             |
|--------------------|-------|----------------------------------------------------|
| test_orderbook     | 6     | BUY/SELL maps, matching, partial fills, cancel     |
| test_database      | 19    | All 12 DB methods, transaction log, rollback       |
| test_features      | 18    | 6 features, isSuspicious, friendlyVerdict          |
| test_config        | 2     | JSON loading, fallback defaults                    |
| test_graph         | 16    | Cycle detection, BFS, network score, summary       |
| test_bridge        | 7     | Timeout, fallback, JSON parsing, explanation field |
| test_validators.py | 12    | Feature range validation                           |
| test_model.py      | 10    | XGBoost + RF training and inference                |
| test_scorer.py     | 6     | stdin/stdout bridge protocol                       |
| test_db_reader.py  | 6     | Dashboard read functions                           |
| test_explain.py    | 5     | SHAP output structure and text format              |

---

## Configuration

All thresholds are runtime-configurable in `config/settings.json` — no recompilation needed.

```json
{
  "fraud_threshold":          0.80,
  "blacklist_temp_threshold": 3,
  "blacklist_perm_threshold": 5,
  "ml_weight":                0.70,
  "graph_weight":             0.30,
  "cycle_base_score":         0.50,
  "bridge_timeout_ms":        100
}
```

---

## Database schema

Five tables, all written by the C++ engine and read by the Python dashboard simultaneously via SQLite WAL mode.

| Table        | Written by             | Purpose                                         |
|--------------|------------------------|-------------------------------------------------|
| orders       | saveOrder()            | Every order placed, status updated as it moves  |
| trades       | saveTrade()            | Every matched execution                         |
| risk_log     | saveRiskEvent()        | Every fraud flag, score, reason, and action     |
| accounts     | deposit() / withdraw() | Current cash balance per user                   |
| transactions | logTransaction()       | Full money movement audit trail                 |

---

## Project structure

``` Structure
vigil-node/
├── cpp/
│   ├── include/          order.h, orderbook.h, database.h, feature_extractor.h,
│   │                     bridge.h, config.h, graph.h, colors.h, trade.h
│   ├── src/              main.cpp, orderbook.cpp, database.cpp, feature_extractor.cpp,
│   │                     bridge.cpp, config.cpp, graph.cpp
│   └── vendor/           sqlite3.h, sqlite3.c, nlohmann/json.hpp
├── python/
│   ├── models/           train_xgb.py, train_rf.py, ensemble.py, explain.py
│   ├── bridge/           scorer.py
│   ├── utils/            config.py, logger.py, validators.py
│   └── dashboard/        dashboard.py, db_reader.py
├── tests/
│   ├── cpp/              test_orderbook, test_database, test_features,
│   │                     test_config, test_graph, test_bridge
│   └── python/           test_validators, test_model, test_scorer,
│                         test_db_reader, test_explain
├── data/training_data/   simulate_data.py
├── config/               settings.example.json
├── docs/                 architecture.md, schema.md, bridge_contract.md
├── .github/workflows/    ci.yml
└── CMakeLists.txt
```

---

## Known design boundaries

- Buying power is checked per-order, not reserved — concurrent orders could collectively overspend
- SELL orders are not checked against share ownership — no position tracking
- Single-threaded engine — one active session at a time
- TEMP_BLOCK and WARN currently behave identically (both reject the triggering order only)
- UserID is free-text trust — no authentication layer

These are documented deliberate choices for the current scope, not bugs.

---

## Built by

Alok — Integrated MSc Mathematics, SVNIT Surat  
Built over months as an independent systems project.
