import time, sys, os
from datetime import datetime

from rich.console   import Console
from rich.table     import Table
from rich.panel     import Panel
from rich.layout    import Layout
from rich.live      import Live
from rich.text      import Text
from rich.columns   import Columns
from rich           import box

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "../..")))

from python.dashboard.db_reader import (
    get_engine_stats, get_recent_orders,
    get_risk_log, get_book_snapshot
)
REFRESH_SECS = 2


# PANEL - 01 : ENGINE STATS
def build_stats_panel() -> Panel:
    stats = get_engine_stats()
    t = Table.grid(padding=(0, 2))
    t.add_column(style="bold cyan")
    t.add_column(style="bold white")

    t.add_row("• Total Orders",    str(stats["total_orders"]))
    t.add_row("• Trades Executed", str(stats["total_orders"]))
    t.add_row("• Orders Blocked",   str(stats["blocked"]))
    t.add_row("• Fraud Rate",
               f"[red]{stats['fraud_rate']}%[/red]" \
               if stats["fraud_rate"] > 5 else
               f"[green]{stats['fraud_rate']}%[/green]")
    
    t.add_row("• Active Users", str(stats["active_users"]))

    return Panel(t, title="[bold cyan]Engine Stats[/bold cyan]",
                 boarder_style="cyan")
    
# PANEL - 02 : RECENT ORDERS
def build_orders_panel() -> Panel:
    orders = get_recent_orders(8)
    t = Table(box=box.SIMPLE, show_header=True,
              header_style="bold blue", padding=(0, 1))
    t.add_column("ID",    style="dim",     width=5)
    t.add_column("User",  width=10)
    t.add_column("Price", justify="right", width=8)
    t.add_column("Qty",   justify="right", width=5)
    t.add_column("Side",  width=5)
    t.add_column("Status",width=10)

    for o in orders:
        side_s = f"[green]{o['side']}[/]" \
                 if o["side"] == "BUY" else f"[red]{o['side']}[/]"
        status_s = {
            "FILLED":   "[green]FILLED[/]",
            "REJECTED": "[red]BLOCKED[/]",
            "PENDING":  "[yellow]PENDING[/]",
        }.get(o["status"], o["status"])
        t.add_row(str(o["orderID"]), o["userID"],
                  f"{o['price']:.2f}", str(o["quantity"]),
                  side_s, status_s)
        
        return Panel(t, title="[bold blue]Recent Orders[/bold blue]",
                     border_style="blue")