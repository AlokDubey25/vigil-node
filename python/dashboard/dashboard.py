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
REFRESH_SECS = 1


# PANEL - 01 : ENGINE STATS
def build_stats_panel() -> Panel:
    stats = get_engine_stats()
    t = Table.grid(padding=(0, 2))
    t.add_column(style="bold cyan")
    t.add_column(style="bold white", justify="right")

    t.add_row(" 📋  Total Orders",    str(stats.get("total_orders", 0)))
    t.add_row(" ⚡  Trades Executed", str(stats.get("total_orders", stats.get("total_orders", 0))))
    t.add_row(" 🚫  Orders Blocked",   str(stats.get("blocked", 0)))
    t.add_row(" 📊  Fraud Rate",
               f"[red]{stats['fraud_rate']}%[/red]" \
               if stats["fraud_rate"] > 5 else
               f"[green]{stats['fraud_rate']}%[/green]")        # can we make this optimize or better...??
    
    t.add_row(" 👤  Active Users", str(stats["active_users"]))

    return Panel(t, title="[bold cyan] 📊 Engine Stats [/bold cyan]",
                 border_style="cyan", expand=True)
    
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
        
        return Panel(t, title="[bold blue] 🛒 Recent Orders [/bold blue]",
                     border_style="blue", expand=True)
    
# PANEL - 03 : RISK LOG
def build_risk_panel() -> Panel:
    events = get_risk_log(8)
    t = Table(box=box.SIMPLE, show_header=True,
              header_style="bold red", padding=(0, 1))
    t.add_column("User",    width=10)
    t.add_column("Score",   justify="right", width=7)
    t.add_column("Action",  width=16)
    t.add_column("Reason",  width=24)

    for e in events:
        action_s = {
            "PERMANENT_BLOCK": "[bold red]PERMANENT_BLOCK[/]",
            "TEMP_BLOCK":      "[bold orange]TEMP_BLOCK[/]",
            "WARN":            "[yellow]WARN[/]",
        }.get(e["action"], e["action"])
        t.add_row(e["userID"], 
                  f"{e['fraudScore']:.3f}",
                  action_s, e["reason"][:24])
        
        return Panel(t, title="[bold red] ⚠️ RISK Log [/bold red]",
                     border_style="red", expand=True)
    
# PANEL - 04 : ORDER BOOK SNAPSHOT 
def build_book_panel() -> Panel:
    book = get_book_snapshot(5)

    def make_side(orders, side):
        t = Table(box=box.SIMPLE, show_header=True, expand=True)
        color = "green" if side == "BUY" else "red"
        t.add_column(f"[{color}]{side}[/]", justify="right", width=9)
        t.add_column("Qty", justify="right", width=5)
        t.add_column("USer", width=8)

        for o in orders:
            t.add_row(f"{o['price']:.2f}",
                       str(o["quantity"]), o["userID"])
        
        if not orders:
            t.add_row("-", "-", "-")
        return t
    
    buy_t  = make_side(book["BUY"], "BUY")
    sell_t = make_side(book["SELL"], "SELL")

    return Panel(Columns([buy_t, sell_t]),
                 title="[bold magenta] 📖 Order Book [/bold magenta]",
                 border_style="magenta", expand=True)



'''
# MAIN : BUILDING FULL LAYOUT AND RUN LIVE REFRESH
def build_layout():
    return Layout(
        Layout(
            Layout(build_stats_panel(), name="stats"),
            Layout(build_orders_panel(), name="orders"),
            direction="horizontal", ratio=1
        ),
        Layout(
            Layout(build_risk_panel(), name="risk"),
            Layout(build_book_panel(), name="book"),
            direction="horizontal", ratio=1,
        ),
        direction="vertical"
    )
'''

def build_layout():
    root = Layout()

    root.split_column(
        Layout(name="top_row", ratio=1),
        Layout(name="bottom_row", ratio=1)
    )

    root["top_row"].split_row(
        Layout(build_stats_panel(), name="stats"),
        Layout(build_orders_panel(), name="orders")
    )
    root["bottom_row"].split_row(
        Layout(build_risk_panel(), name="risk"),
        Layout(build_book_panel(), name="book")
    )
    
    return root



def main():
    console = Console()
    now = datetime.now().strftime("%H:%M:%S")
    console.print(f"[bold cyan]Vigil Node Dashboard[/] - started {now}")
    console.print("Ctrl+C to exit\n")

    try:
        with Live(build_layout(), console=console,
                  refresh_per_second=1, screen=False) as live:
            while True:
                time.sleep(REFRESH_SECS)
                live.update(build_layout())
    except KeyboardInterrupt:
        console.print("\n[dim]Dashboard stopped[/dim]")


if __name__ == "__main__":
    main()
