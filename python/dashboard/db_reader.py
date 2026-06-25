import sqlite3, os
from typing import List, Dict, Any

DB_PATH = os.path.join(os.path.abspath(os.path.join(os.path.dirname(__file__), "../..")), "vigil.db")

def _connect():
    """Open a read-only connection. Safe while C++ engine writes."""
    if not os.path.exists(DB_PATH):
        return None
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn

def get_engine_stats() -> Dict[str, Any]:
    """Panel 1 - headline numbers."""
    conn = _connect()
    if not conn:
        return {"total_orders":0, "total_trades":0, 
                "blocked":0, "fraud_rate":0.0, "active_users":0}
    try:
        curr = conn.cursor()
        total   = curr.execute("SELECT COUNT(*) FROM orders").fetchone()[0]
        trades  = curr.execute("SELECT COUNT(*) FROM trades").fetchone()[0]
        blocked = curr.execute("SELECT COUNT(*) FROM orders WHERE status='REJECTED'").fetchone()[0]
        users   = curr.execute("SELECT COUNT(DISTINCT userID) FROM orders").fetchone()[0]
        rate    = round(blocked / total * 100, 1) if total > 0 else 0.0
        return {"total_orders":total, "total_trades":trades,
                "blocked":blocked, "fraud_rate":rate,
                "active_users":users}
    finally:
        conn.close()

def get_recent_orders(limit: int = 10) -> List[Dict]:
    """Panel 2 - latest orders."""
    conn = _connect()
    if not conn : return []
    try:
        rows = conn.execute(
            """SELECT orderID, userID, price, quantity, side, status
               FROM orders ORDER BY orderID DESC LIMIT ?""", (limit,)
        ).fetchall()
        return [dict(r) for r in rows]
    finally:
        conn.close()

def get_risk_log(limit: int = 10) -> List[Dict]:
    """Panel 3 - latest risk events."""
    conn = _connect()
    if not conn: return[]
    try:
        rows = conn.execute(
            """SELECT logID, userID, fraudScore, action, reason
               FROM risk_log ORDER BY logID DESC LIMIT ?""", (limit,)
        ).fetchall()
        return [dict(r) for r in rows]
    finally:
        conn.close()

def get_book_snapshot(limit: int = 5) -> Dict[str, List[Dict]]:
    """Panel 4 - pending orders by side, sorted by price."""
    conn = _connect()
    if not conn: return {"BUY":[], "SELL":[]}
    try:
        buys = conn.execute(
            """SELECT price, quantity, userID FROM orders
               WHERE side='BUY' AND status='PENDING'
               ORDER BY price DESC LIMIT ?""", (limit,)
        ).fetchall()
        sells = conn.execute(
            """SELECt price, quantity, userID FROM orders
               WHERE side='SELL' AND status='PENDING'
               ORDER BY price DESC LIMIT ?""", (limit,)
        ).fetchall()
        return {"BUY" :[dict(r) for r in buys],
                "SELL":[dict(r) for r in sells]}
    finally:
        conn.close()
