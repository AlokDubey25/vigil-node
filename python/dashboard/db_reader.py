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

               