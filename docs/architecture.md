# Vigil Node — Architecture

## what it does

real-time pre-trade fraud detection engine. every order that enters
the marketplace gets validated, scored, and either allowed
into the book or blocked before execution.

---

## layer overview

```
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

```
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

```
cpp/include/   ← headers (contracts)
cpp/src/       ← implementations
cpp/vendor/    ← sqlite3 amalgamation
tests/cpp/     ← Catch2 unit tests
python/utils/  ← config + logging helpers
docs/          ← this file + schema.md + bridge_contract.md
```