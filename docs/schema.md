# SQLite Schema Contract

Vigil Node uses one SQLite database, `vigil.db`, as the durable audit log and as the read model for the dashboard. The C++ engine is the only writer. Python dashboard code opens the same database for reads while SQLite WAL mode keeps reads and writes from blocking each other.

Database setup is owned by `DatabaseHandler` in `cpp/src/database.cpp`.

```sql
PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
PRAGMA foreign_keys = ON;
```

## Ownership

| Table          | Written by                    | Read by                                                | Purpose                                       |
|----------------|-------------------------------|--------------------------------------------------------|-----------------------------------------------|
| `orders`       | C++ engine                    | C++ engine, Python dashboard, `explain` command        | Every order submitted to the engine           |
| `trades`       | C++ matching engine           | C++ graph loader, Python dashboard                     | Every matched execution                       |
| `risk_log`     | C++ fraud pipeline            | C++ blacklist loader,Python dashboard,`explain` command| Fraud,funds,blacklist,ring-detect audit trail |
| `accounts`     | C++ account methods           | C++ balance checks, Python dashboard                   | Current cash balance per user                 |
| `transactions` | C++ account/settlement method | C++ history command, Python dashboard                  | Money movement ledger                         |

## `orders`

One row is created before pre-trade checks run. The order may later be rejected, remain pending in the book, or be marked filled after matching.

```sql
CREATE TABLE IF NOT EXISTS orders (
    orderID     INTEGER PRIMARY KEY,
    userID      TEXT    NOT NULL,
    price       REAL    NOT NULL,
    quantity    INTEGER NOT NULL,
    side        TEXT    NOT NULL,
    timestamp   INTEGER NOT NULL,
    status      TEXT    NOT NULL DEFAULT 'PENDING',
    blockScore  REAL             DEFAULT 0.0,
    fraudFlag   INTEGER          DEFAULT 0
);
```

| Column       | Type    | Notes                                                              |
|--------------|---------|--------------------------------------------------------------------|
| `orderID`    | INTEGER | Engine-assigned primary key. Must be unique.                       |
| `userID`     | TEXT    | Free-text user identifier supplied by the session.                 |
| `price`      | REAL    | Limit price in rupees.                                             |
| `quantity`   | INTEGER | Current order quantity recorded by the engine.                     |
| `side`       | TEXT    | `BUY` or `SELL`.                                                   |
| `timestamp`  | INTEGER | Unix epoch seconds.                                                |
| `status`     | TEXT    | `PENDING`, `FILLED`, `REJECTED`, or `REJECT_FUNDS`.                |
| `blockScore` | REAL    | Stored fraud score field from the order object. Defaults to `0.0`. |
| `fraudFlag`  | INTEGER | Stored fraud flag field from the order object. Defaults to `0`.    |

Status transitions are intentionally simple:

| Transition                  | Meaning                                   |
|-----------------------------|-------------------------------------------|
| New row -> `PENDING`        | Order has entered the pre-trade pipeline. |
| `PENDING` -> `REJECTED`     | Fraud, blacklist, or policy rejection.    |
| `PENDING` -> `REJECT_FUNDS` | BUY order failed the balance check.       |
| `PENDING` -> `FILLED`       | Matching engine fully executed the order. |

## `trades`

One row is written for each execution produced by the matching engine. Trade rows reference the buy and sell orders that matched.

```sql
CREATE TABLE IF NOT EXISTS trades (
    tradeID     INTEGER PRIMARY KEY AUTOINCREMENT,
    buyOrderID  INTEGER NOT NULL,
    sellOrderID INTEGER NOT NULL,
    price       REAL    NOT NULL,
    quantity    INTEGER NOT NULL,
    timestamp   INTEGER NOT NULL,
    FOREIGN KEY (buyOrderID)  REFERENCES orders(orderID),
    FOREIGN KEY (sellOrderID) REFERENCES orders(orderID)
);
```

| Column        | Type    | Notes                                  |
|---------------|---------|----------------------------------------|
| `tradeID`     | INTEGER | Database-assigned primary key.         |
| `buyOrderID`  | INTEGER | Foreign key to the matched buy order.  |
| `sellOrderID` | INTEGER | Foreign key to the matched sell order. |
| `price`       | REAL    | Execution price in rupees.             |
| `quantity`    | INTEGER | Executed quantity.                     |
| `timestamp`   | INTEGER | Unix epoch seconds.                    |

After a trade is saved, the engine settles cash, adds a buyer -> seller edge to the graph, and may write ring-detection events to `risk_log`.

## `risk_log`

Append-only fraud and rejection ledger. This table is also the persistence source for the permanent blacklist: on startup, C++ loads distinct users where `action = 'PERMANENT_BLOCK'`.

```sql
CREATE TABLE IF NOT EXISTS risk_log (
    logID      INTEGER PRIMARY KEY AUTOINCREMENT,
    userID     TEXT    NOT NULL,
    orderID    INTEGER NOT NULL,
    fraudScore REAL    NOT NULL,
    reason     TEXT    NOT NULL,
    action     TEXT    NOT NULL,
    timestamp  INTEGER NOT NULL,
    FOREIGN KEY (orderID) REFERENCES orders(orderID)
);
```

| Column       | Type| Notes                                                                                                                              |
|--------------|-----|------------------------------------------------------------------------------------------------------------------------------------|
| `logID`      | INT | Database-assigned primary key.                                                                                                     |
| `userID`     | TEXT| User associated with the decision.                                                                                                 |
| `orderID`    | INT | Related order. Must exist in `orders`.                                                                                             |
| `fraudScore` | REAL| Score for the event.Combined ML + graph score(fraud blocks),`0.0`-funds rejection,`1.0`-blacklist rejection, and graph value[ring].|
| `reason`     | TEXT| Human-readable reason such as `combined ML+graph score`, `insufficient funds`, or `circular trading ring detected`.                |
| `action`     | TEXT| Enforcement outcome. See action values below.                                                                                      |
| `timestamp`  | INT | Unix epoch seconds, assigned when the event is saved.                                                                              |

### Action values

| Action            | Meaning                                                                                      |
|-------------------|----------------------------------------------------------------------------------------------|
| `WARN`            | User has 1-2 flags. The triggering order is rejected.                                        |
| `TEMP_BLOCK`      | User has 3-4 flags. The triggering order is rejected. Current behavior matches `WARN`.       |
| `PERMANENT_BLOCK` | User has 5+ flags. User is added to the in-memory blacklist and persists through `risk_log`. |
| `REJECT`          | User was already permanently blacklisted. Order is rejected before ML or graph scoring.      |
| `REJECT_FUNDS`    | BUY order failed the balance check. This is an audit event, not a fraud signal.              |

## `accounts`

Current balance per user. Accounts are created lazily on first deposit or settlement credit.

```sql
CREATE TABLE IF NOT EXISTS accounts (
    userID  TEXT PRIMARY KEY,
    balance REAL NOT NULL DEFAULT 0.0
);
```

| Column    | Type | Notes                                                    |
|-----------|------|----------------------------------------------------------|
| `userID`  | TEXT | Primary key matching the user identifier used on orders. |
| `balance` | REAL | Current cash balance in rupees.                          |

Balance rules:

- `deposit()` inserts or increments the account balance.
- `withdraw()` fails if the account does not have enough cash.
- BUY orders check `price * quantity` before fraud scoring.
- Buying power is checked per order; funds are not reserved for resting orders.

## `transactions`

Append-only money movement ledger. Successful deposits, withdrawals, and trade settlements write here automatically.

```sql
CREATE TABLE IF NOT EXISTS transactions (
    txnID        INTEGER PRIMARY KEY AUTOINCREMENT,
    userID       TEXT NOT NULL,
    type         TEXT NOT NULL,
    amount       REAL NOT NULL,
    balanceAfter REAL NOT NULL,
    timestamp    INTEGER NOT NULL,
    note         TEXT
);
```

| Column         | Type    | Notes                                                |
|----------------|---------|------------------------------------------------------|
| `txnID`        | INTEGER | Database-assigned primary key.                       |
| `userID`       | TEXT    | User whose balance changed.                          |
| `type`         | TEXT    | `DEPOSIT`, `WITHDRAW`, `TRADE_BUY`, or `TRADE_SELL`. |
| `amount`       | REAL    | Positive amount. Direction is represented by `type`. |
| `balanceAfter` | REAL    | User balance immediately after the transaction.      |
| `timestamp`    | INTEGER | Unix epoch seconds.                                  |
| `note`         | TEXT    | Optional context such as counterparty information.   |

Trade settlement writes two transaction rows inside a database transaction:

| Type         | User   | Meaning                   |
|--------------|--------|---------------------------|
| `TRADE_BUY`  | Buyer  | Cash paid to seller.      |
| `TRADE_SELL` | Seller | Cash received from buyer. |

If either side of settlement fails, the engine rolls back the settlement transaction.

## Dashboard contract

The Python dashboard treats SQLite as a read-only event stream. It should not mutate any table. Its refresh signal is based on monotonic table IDs, especially `max(orderID)`, `max(logID)`, and `max(txnID)`.

Because WAL mode is enabled by C++, the dashboard can read while the engine writes.

## Deliberate boundaries

- There is no position table, so SELL orders are not checked against share ownership.
- Cash is not reserved for open BUY orders.
- `risk_log` is the only durable blacklist source.
- User identity is free text; there is no authentication table.
