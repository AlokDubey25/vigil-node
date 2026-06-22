# Inlogs

## orders

Every order that enters the engine gets one row here.
Written by: database.cpp saveOrder()
Updated by: database.cpp updateOrderStatus()

| column     | type    | notes                               |
|------------|---------|-------------------------------------|
| orderID    | INTEGER | primary key, set by C++ engine      |
| userID     | TEXT    | who placed the order                |
| price      | REAL    | price in rupees (not paise)         |
| quantity   | INTEGER | units remaining (not original qty)  |
| side       | TEXT    | "BUY" or "SELL"                     |
| timestamp  | INTEGER | unix epoch seconds                  |
| status     | TEXT    | PENDING → FILLED or REJECTED        |
| blockScore | REAL    | ML fraud score 0.0–1.0 (Phase 4)    |
| fraudFlag  | INTEGER | 0 = safe, 1 = flagged (Phase 6)     |

---

## trades

One row per executed trade.
Written by: database.cpp saveTrade()

| column      | type    | notes                               |
|-------------|---------|-------------------------------------|
| tradeID     | INTEGER | auto-increment, DB assigns          |
| buyOrderID  | INTEGER | FK → orders.orderID                 |
| sellOrderID | INTEGER | FK → orders.orderID                 |
| price       | REAL    | price the trade executed at         |
| quantity    | INTEGER | units traded                        |
| timestamp   | INTEGER | when the match happened             |

---

## risk_log

Every time the fraud detector flags a user, a row goes here.
Also how the blacklist persists across restarts.
Written by: database.cpp saveRiskEvent()
Read by: database.cpp loadBlacklist()

| column     | type    | notes                               |
|------------|---------|-------------------------------------|
| logID      | INTEGER | auto-increment                      |
| userID     | TEXT    | the flagged user                    |
| orderID    | INTEGER | FK → orders.orderID                 |
| fraudScore | REAL    | ML score that triggered this        |
| reason     | TEXT    | human-readable: "high velocity"     |
| action     | TEXT    | WARN / TEMP_BLOCK / PERMANENT_BLOCK |
| timestamp  | INTEGER | when the event happened             |

---

## action values

| value           | meaning                                |
|-----------------|----------------------------------------|
| WARN            | flagged once, still allowed to trade   |
| TEMP_BLOCK      | blocked for this session only          |
| PERMANENT_BLOCK | blocked forever, loaded on every start |
