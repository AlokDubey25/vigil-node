import sqlite3, os, sys, pytest 

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
sys.path.insert(0, ROOT)

import python.dashboard.db_reader as db_reader

SCHEMA = """
CREATE TABLE orders(orderID INTEGER PRIMARY KEY, userID TEXT, price REAL,
    quantity INTEGER, side TEXT, timestamp INTEGER, status TEXT DEFAULT 'PENDING',
    blockScore REAL DEFAULT 0.0, fraudFlag INTEGER DEFAULT 0);
CREATE TABLE trades(tradeID INTEGER PRIMARY KEY AUTOINCREMENT, buyOrderID INTEGER,
    sellOrderID INTEGER, price REAL, quantity INTEGER, timestamp INTEGER);
CREATE TABLE risk_log(logID INTEGER PRIMARY KEY AUTOINCREMENT, userID TEXT,
    orderID INTEGER, fraudScore REAL, reason TEXT, action TEXT, timestamp INTEGER);
"""

@pytest.fixture
def temp_db(tmp_path, monkeypatch):
    db_path = tmp_path / "test_vigil.db"
    conn    = sqlite3.connect(str(db_path))
    conn.executescript(SCHEMA)
    conn.execute("INSERT INTO orders VALUES (1, 'I1', 100.0, 10, 'BUY', 1700000000, 'FILLED', 0.1, 0)")
    conn.execute("INSERT INTO orders VALUES (2, 'I2', 100.0, 10, 'SELL', 1700000000, 'FILLED', 0.1, 0)")
    conn.execute("INSERT INTO trades VALUES (1, 1, 2, 100.0, 10, 1700000000)")
    conn.execute("INSERT INTO risk_log VALUES (1, 'I1', 1, 0.9, 'circular trading ring detected', 'WARN', 1700000000)")
    conn.commit(); conn.close()

    # monkeypatch we're using 'cuz the module-level DB_PATH constant so _connect() uses our temp file
    monkeypatch.setattr(db_reader, "DB_PATH", str(db_path))
    return db_path

def test_engine_stats_counts(temp_db):
    stats = db_reader.get_engine_stats()
    assert stats["total_orders"] == 2
    assert stats["total_trades"] == 1

def test_recent_orders_returns_rows(temp_db):
    orders = db_reader.get_recent_orders(5)
    assert len(orders) == 2

def test_risk_log_returns_entry(temp_db):
    log = db_reader.get_risk_log(5)
    assert len(log) == 1
    assert log[0]["action"] == "WARN"

def test_graph_data_finds_ring_member(temp_db):
    data = db_reader.get_graph_data()
    assert ("I1", "I2") in data["edges"]
    assert "I1" in data["ring_members"]

def test_missing_db_returns_safe_defaults(monkeypatch):
    monkeypatch.setattr(db_reader, "DB_PATH", "/nonexistent/path.db")
    stats = db_reader.get_engine_stats()
    assert stats["total_orders"] == 0
    