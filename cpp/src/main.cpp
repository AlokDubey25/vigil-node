# include <iostream>
# include <iomanip>
# include <ctime>
# include <unordered_set>
# include <algorithm>
# include <chrono>
# include <cstdio>
# include <vector>
# include <fcntl.h>
# include "../include/order.h"
# include "../include/orderbook.h"
# include "../include/trade.h"
# include "../include/database.h"
# include "../include/feature_extractor.h"
# include "../include/bridge.h"
# include "../include/config.h"
# include "../include/graph.h"
# include "../include/colors.h"
using namespace std;

int  runHistory(int argc, char* argv[]);
int  runReset();
int  runBenchmark(int argc, char* argv[]);                          
void runInteractive(DatabaseHandler& db,  
                     Bridge& bridge, OrderBook& book,
                     FeatureExtractor& extractor, Graph& tradeGraph,
                     unordered_set<string>& blocked,
                     unordered_map<int, string>& orderUsers,
                     double THRESHOLD, double ML_WEIGHT,
                     double GRAPH_WEIGHT, int TEMP_AT, int PERM_AT);

Order makeOrder(int id, const string& user,
                double price, int qty, const string& side);

string escalateAction(DatabaseHandler& db, const string& userID,
                       int tempAt, int permAt);

void processOrder(const Order& o,
                   DatabaseHandler& db, OrderBook& book,
                   FeatureExtractor& extractor, Bridge& bridge,
                   Graph& tradeGraph,
                   unordered_set<string>& blocked,
                   unordered_map<int, string>& orderUsers,
                   double THRESHOLD, double ML_WEIGHT,
                   double GRAPH_WEIGHT, int TEMP_AT, int PERM_AT);

void runMatchLoop(OrderBook& book, DatabaseHandler& db,
                   Graph& tradeGraph,
                   unordered_map<int, string>& orderUsers);

bool cancelOrderByID(int orderID, OrderBook& book, DatabaseHandler& db,
                      FeatureExtractor& extractor,
                      unordered_map<int, string>& orderUsers);

void printUsage() {
    cout << "Usage: ./build/vigil <command> [args]\n"
         << "Commands:\n"
         << "  run           run the demo order set\n"
         << "  interactive   guided menu — buy, sell, deposit, withdraw, and more\n"
         << "  history <uid> last 10 transactions for a user\n"
         << "  reset         wipe vigil.db and start fresh\n"
         << "  --help        this message\n"  
         << "  benchmark [--n N]  measure orders/sec at each pipeline stage\n"; 
}

int main(int argc, char* argv[]){

    if (argc < 2) { printUsage(); return 1; }

    string cmd = argv[1];
    if (cmd == "history")     return runHistory(argc, argv);
    if (cmd == "benchmark") return runBenchmark(argc, argv);
    if (cmd == "reset")       return runReset();      
    if (cmd == "--help")      { printUsage(); return 0; }
    if (cmd != "run" && cmd != "interactive") {
        cerr<< Color::red("[ERROR] unknown command: " + cmd) << "\n";
        printUsage(); return 1;
    }


    // 01 :- config: reads settings.json before anything else
    Config cfg("config/settings.json");
    if (!cfg.isLoaded())
        cerr<< "[WARN] using hardcoded defaults\n";

    // read all thresholds along with others from config with safe fallbacks
    const double THRESHOLD      = cfg.getDouble("fraud_threshold",       0.80);
    const int    TEMP_AT        = cfg.getInt   ("blacklist_temp_threshold", 3);
    const int    PERM_AT        = cfg.getInt   ("blacklist_perm_threshold", 5);
    const double ML_WEIGHT      = cfg.getDouble("ml_weight",             0.70);
    const double GRAPH_WEIGHT   = cfg.getDouble("graph_weight",          0.30);
    const double CYCLE_BASE     = cfg.getDouble("cycle_base_score",      0.50);
    const int    BRIDGE_TIMEOUT = cfg.getInt("bridge_timeout_ms", 100);

    cout<< Color::dim("[CONFIG] threshold=" + to_string(THRESHOLD)
                      + "  temp_at=" + to_string(TEMP_AT)
                      + "  perm_at=" + to_string(PERM_AT)) << "\n";



    // 02 :- so data enter first and after rescan it continue to engine 
    DatabaseHandler db("vigil.db");
    if (!db.isOpen()) {return 1; }
    
    // load previously blocked users into unordered_set for O(1)
    auto blacklist = db.loadBlacklist();
    unordered_set<string> blocked(blacklist.begin(), blacklist.end());
    cout<< "[ENIGINE]" << blocked.size() << " users on permanent blacklist\n";

    

    // 03 :- Python bridge which is added now - it launched once, stays runnig
    // run ./vigil from project root so the path resolves correctly
    Bridge bridge("python3 python/bridge/scorer.py", BRIDGE_TIMEOUT);
    if (!bridge.isReady()){
        cerr<< Color::yellow("[WARN] Bridge not ready - using fallback score") << "\n";
    }

    
    OrderBook book;
    FeatureExtractor extractor;
    Graph tradeGraph;

    tradeGraph.setCycleBaseScore(CYCLE_BASE);
    
    unordered_map<int, string> orderUsers;

    auto historicPairs = db.getTradePairs();
    for (const auto& [buyer, seller] : historicPairs) {
        tradeGraph.addEdge(buyer, seller);  // it will rebuild irderUsers from DB for graph - partial
    }
    if (cmd == "interactive") {
        runInteractive(db, bridge, book, extractor, tradeGraph,
                        blocked, orderUsers, THRESHOLD,
                        ML_WEIGHT, GRAPH_WEIGHT, TEMP_AT, PERM_AT);
        return 0;
    }


     auto insert = [&](const Order& o) {
        processOrder(o, db, book, extractor, bridge, tradeGraph,
                     blocked, orderUsers, THRESHOLD,
                     ML_WEIGHT, GRAPH_WEIGHT, TEMP_AT, PERM_AT);
    };

    cout << "\n===== processing orders =====\n";
    insert(makeOrder(1, "I1001", 104.00, 10, "BUY"));
    insert(makeOrder(2, "I1002", 103.50,  5, "BUY"));
    insert(makeOrder(3, "I1004", 103.00,  8, "SELL"));
    insert(makeOrder(4, "I1005", 103.25,  5, "SELL"));

    cout << "\n==== matching normal orders ====\n";
    runMatchLoop(book, db, tradeGraph, orderUsers);

    cout << "\n==== cancellation demo (drives cancelRate) ====\n";
    insert(makeOrder(30, "I3001", 99.00, 5, "BUY"));
    insert(makeOrder(31, "I3001", 98.50, 5, "BUY"));
    insert(makeOrder(32, "I3001", 98.00, 5, "BUY"));
    cancelOrderByID(31, book, db, extractor, orderUsers);
    cancelOrderByID(32, book, db, extractor, orderUsers);
    insert(makeOrder(33, "I3001", 97.50, 5, "BUY"));    


    // ── wash trading demo — prices far from the normal range so wash
    // traders match EACH OTHER, not the existing cheaper sells      ──    
    cout << "\n==== wash trading demo ====\n";
    insert(makeOrder(20, "I2001", 200.00,  5, "BUY"));
    insert(makeOrder(21, "I2002", 190.00,  5, "SELL"));
    runMatchLoop(book, db, tradeGraph, orderUsers);   

    insert(makeOrder(22, "I2002", 200.00,  5, "BUY"));
    insert(makeOrder(23, "I2001", 190.00,  5, "SELL"));
    runMatchLoop(book, db, tradeGraph, orderUsers);   

    cout << "\n[GRAPH] " << tradeGraph.getSummary() << "\n";
    tradeGraph.print();
    auto circular = tradeGraph.getCircularTraders();
    if (!circular.empty())
        cout << Color::red("[GRAPH] " + to_string(circular.size())
                         + " users flagged for circular trading") << "\n";
    return 0;
}


int runReset() {                                  
    const char* files[] = { "vigil.db", "vigil.db-wal", "vigil.db-shm" };
    for (const char* f : files) {
        if (std::remove(f) == 0)
            cout << Color::green("[RESET] removed " + string(f)) << "\n";
    }
    cout << "done — run './build/vigil run' to start fresh.\n";
    return 0;
}

int runBenchmark(int argc, char* argv[]) {
    int N = 5000;
    for (int i = 2; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--n" && i + 1 < argc) N = stoi(argv[++i]);
    }

    // ── silence all stdout + stderr during timing ──────────────────
    // save originals
    int saved_stdout = dup(STDOUT_FILENO);
    int saved_stderr = dup(STDERR_FILENO);
    int devnull      = open("/dev/null", O_WRONLY);

    auto silence = [&]() {
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        cout.flush(); cerr.flush();
    };
    auto restore = [&]() {
        cout.flush(); cerr.flush();
        dup2(saved_stdout, STDOUT_FILENO);
        dup2(saved_stderr, STDERR_FILENO);
    };

    auto measure = [](const string& label, int n, double secs) {
        cout << left  << setw(42) << label
             << right << setw(10) << fixed << setprecision(0) << (n / secs)
             << " orders/sec   ("
             << setprecision(1) << secs * 1000 << " ms)\n";
    };

    auto gen = [](int n) {
        vector<Order> v; v.reserve(n);
        for (int i = 0; i < n; i++)
            v.push_back(makeOrder(
                i + 1,
                "BENCH" + to_string(i % 20),
                100.0 + (i % 50) * 0.05,
                1,
                i % 2 == 0 ? "BUY" : "SELL"));
        return v;
    };

    // ── print header BEFORE silencing ─────────────────────────────
    cout << "\n" << Color::cyan("=== Vigil Node Benchmark — N=" + to_string(N) + " ===") << "\n\n";
    cout << left << setw(42) << "Stage"
         << right << setw(10) << "Throughput" << "   Time\n";
    cout << string(62, '-') << "\n";

    // ── stage 1: order book only ───────────────────────────────────
    {
        auto orders = gen(N);
        OrderBook book;

        silence();
        auto t0 = chrono::steady_clock::now();
        for (auto& o : orders) {
            book.addOrder(o);
            if (book.hasBuys() && book.hasSells() &&
                book.getBestBid() >= book.getBestAsk())
                book.matchOrders();
        }
        auto secs = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
        restore();

        measure("stage 1 — order book only:", N, secs);
    }

    // ── stage 2: + SQLite ─────────────────────────────────────────
    {
        remove("benchmark.db");
        remove("benchmark.db-wal");
        remove("benchmark.db-shm");

        auto orders = gen(N);

        silence();
        DatabaseHandler db("benchmark.db");
        OrderBook book;
        auto t0 = chrono::steady_clock::now();
        for (auto& o : orders) {
            db.saveOrder(o);
            book.addOrder(o);
            if (book.hasBuys() && book.hasSells() &&
                book.getBestBid() >= book.getBestAsk()) {
                Trade t = book.matchOrders();
                if (t.quantity) db.saveTrade(t);
            }
        }
        auto secs = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
        restore();

        remove("benchmark.db");
        remove("benchmark.db-wal");
        remove("benchmark.db-shm");

        measure("stage 2 — + SQLite persistence:", N, secs);
    }

    // ── stage 3: + feature extraction ────────────────────────────
    {
        auto orders = gen(N);
        OrderBook book;
        FeatureExtractor ex;

        silence();
        auto t0 = chrono::steady_clock::now();
        for (auto& o : orders) {
            double mid = (book.hasBuys() && book.hasSells())
                ? (book.getBestBidPrice() + book.getBestAskPrice()) / 2.0
                : 0.0;
            ex.extract(o, mid);
            book.addOrder(o);
            if (book.hasBuys() && book.hasSells() &&
                book.getBestBid() >= book.getBestAsk())
                book.matchOrders();
        }
        auto secs = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
        restore();

        measure("stage 3 — + feature extraction:", N, secs);
    }

    // ── stage 4: + ML bridge ──────────────────────────────────────
    {
        int BN = min(N, 200);
        auto orders = gen(BN);
        OrderBook book;
        FeatureExtractor ex;

        // silence completely — bridge startup, model load, timeout lines, all of it
        silence();
        Config cfg("config/settings.json");
        Bridge bridge("python3 python/bridge/scorer.py",
                      cfg.getInt("bridge_timeout_ms", 500));
        bool ready = bridge.isReady();
        restore();

        if (!ready) {
            cout << Color::yellow("stage 4 — + ML bridge:            ")
                 << "skipped (bridge not ready)\n";
        } else {
            silence();
            auto t0 = chrono::steady_clock::now();
            for (auto& o : orders) {
                double mid = (book.hasBuys() && book.hasSells())
                    ? (book.getBestBidPrice() + book.getBestAskPrice()) / 2.0
                    : 0.0;
                FeatureVector fv = ex.extract(o, mid);
                bridge.score(fv.toJSON());
                book.addOrder(o);
                if (book.hasBuys() && book.hasSells() &&
                    book.getBestBid() >= book.getBestAsk())
                    book.matchOrders();
            }
            auto secs = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
            restore();

            measure("stage 4 — + ML bridge (N=" + to_string(BN) + "):", BN, secs);
        }
    }

    // ── footer ────────────────────────────────────────────────────
    cout << string(62, '-') << "\n";
    cout << Color::dim("bottleneck is always stage 4 — one IPC round-trip per order\n\n");

    close(devnull);
    close(saved_stdout);
    close(saved_stderr);
}


Order makeOrder(int id, const string& user,
                double price, int qty,const string& side){
        
        Order o;
        o.orderID = id;
        o.userID = user;
        o.price = price;
        o.quantity = qty;
        o.side = side;
        o.timestamp = static_cast<long long>(time(nullptr));
        return o;
}


// 04 :- it will hepl to detemine escalation action like
// prevFlags = how many times already flagged (from risk_log)
// thisFlag  = prevFlags + 1 (current incident)
string escalateAction(DatabaseHandler& db, const string& userID,
                       int tempAt, int permAt) {
    int prevFlags = db.getUserFlagCount(userID);
    int thisFlag  = prevFlags + 1;
    if (thisFlag < tempAt) return "WARN";
    if (thisFlag < permAt) return "TEMP_BLOCK";
    return "PERMANENT_BLOCK";
}

void processOrder(const Order& o,
                   DatabaseHandler& db, OrderBook& book,
                   FeatureExtractor& extractor, Bridge& bridge,
                   Graph& tradeGraph,
                   unordered_set<string>& blocked,
                   unordered_map<int, string>& orderUsers,
                   double THRESHOLD, double ML_WEIGHT,
                   double GRAPH_WEIGHT, int TEMP_AT, int PERM_AT)
{
    if (!db.saveOrder(o))
        cerr << Color::yellow("[WARN] order " + to_string(o.orderID) + " not saved\n");

    orderUsers[o.orderID] = o.userID;

    // 1. Permanent Blacklist Check
    if (blocked.count(o.userID)) {
        cout << Color::red("[BLOCKED] " + o.userID + " — permanent blacklist(no scoring)") << "\n";
        cout << Color::yellow("  \"Order blocked — this user has been permanently banned for repeated fraud.\"") << "\n";  
        db.updateOrderStatus(o.orderID, "REJECTED");
        db.saveRiskEvent(o.userID, o.orderID, 1.0, "permanent blacklist", "REJECT");
        return;
    }

    // 2. Insufficient Funds Check
    if (o.side == "BUY") {
        double cost = o.price * o.quantity;
        double balance = db.getBalance(o.userID);
        if (balance < cost) {
            cout << Color::red("[REJECTED] " + o.userID + " — insufficient funds") << "\n";
            cout << Color::yellow("  \"Order blocked — not enough money in your account for this purchase.\"") << "\n";
            db.updateOrderStatus(o.orderID, "REJECTED");
            db.saveRiskEvent(o.userID, o.orderID, 0.0, "insufficient funds", "REJECT_FUNDS");
            return;
        }
    }



    double mid = 0.0;
    if (book.hasBuys() && book.hasSells())
        mid = (book.getBestBidPrice() + book.getBestAskPrice()) / 2.0;

    FeatureVector fv = extractor.extract(o, mid);
    fv.print(o.userID);

    double mlScore    = bridge.score(fv.toJSON());
    string explanation = bridge.getLastExplanation(); 
    double graphScore = tradeGraph.getNetworkScore(o.userID);
    double combined   = (mlScore * ML_WEIGHT) + (graphScore * GRAPH_WEIGHT);

    cout << "       [ML]    score=" << fixed << setprecision(4) << mlScore << "\n"
         << "       [GRAPH] score=" << graphScore << "\n"
         << "       [FINAL] score=" << combined   << "\n";
    
    if (!explanation.empty())
        cout << Color::dim("       [WHY]   " + explanation) << "\n";


    // 3. Plain-English Main Verdict Sentence (Console-only UX layer)
    string friendly = friendlyVerdict(fv, combined, THRESHOLD);
    cout << Color::yellow("  \"" + friendly + "\"") << "\n";


    if (combined > THRESHOLD) {
        string action = escalateAction(db, o.userID, TEMP_AT, PERM_AT);
        cout << Color::red("[FLAGGED] " + o.userID
                          + " combined=" + to_string(combined)
                          + " action="   + action) << "\n";
        db.updateOrderStatus(o.orderID, "REJECTED");

        string reason = explanation.empty() ? "combined ML+graph score" : explanation;
        db.saveRiskEvent(o.userID, o.orderID, combined, reason, action);

        if (action == "PERMANENT_BLOCK") blocked.insert(o.userID);
        return;
    }
    cout << Color::green("[ALLOWED] " + o.userID) << "\n";
    book.addOrder(o);
}


void runMatchLoop(OrderBook& book, DatabaseHandler& db,
                   Graph& tradeGraph,
                   unordered_map<int, string>& orderUsers)
{
    while (book.hasBuys() && book.hasSells()) {
        if (book.getBestBid() < book.getBestAsk()) break;
        
        Trade t = book.matchOrders();

        if (t.quantity == 0) break; 
        
        db.saveTrade(t);
    
        if (t.buyFilled)  db.updateOrderStatus(t.buyOrderID, "FILLED");
        if (t.sellFilled) db.updateOrderStatus(t.sellOrderID, "FILLED");
    

    // Graph : look up user IDs and add edge
        string buyer = orderUsers.count(t.buyOrderID) ? orderUsers[t.buyOrderID] : "?";
        string seller = orderUsers.count(t.sellOrderID) ? orderUsers[t.sellOrderID] : "?";
        
        tradeGraph.addEdge(buyer, seller);
        cout<< Color::cyan("[GRAPH] edge: "+ buyer + " -> " + seller) << "\n";

        // check both ends of new edge for cycles
        bool ringDetected = tradeGraph.isInCycle(buyer) || tradeGraph.isInCycle(seller);

        if (ringDetected) {
            // find all connected to this ring via BFS
            auto component = tradeGraph.getConnectedComponent(buyer);
            cout<< Color::red("[RING] wash trade ring detected - "
                + to_string(component.size()) + " member") << "\n";

            for (const auto& member : component) {
                // only flag users confiremed to be in cycle
                if (tradeGraph.isInCycle(member)) {
                    cout << Color::red(" [RING] flagging " + member) << "\n";
                    db.saveRiskEvent(member, t.buyOrderID, 0.9, "circular trading ring detected", "WARN");
                } else {
                    // connected but not in cycle - may be innocent counterparty
                    cout<< "  [RING] connected: " << member <<"\n";
                }
            }
        }
    }
}

bool cancelOrderByID(int orderID, OrderBook& book, DatabaseHandler& db,
                      FeatureExtractor& extractor,
                      unordered_map<int, string>& orderUsers)
{
    if (!book.cancelOrder(orderID)) {
        cout << Color::yellow("[CANCEL] order " + to_string(orderID)
                              + " not found — already filled, rejected, or bad ID") << "\n";
        return false;
    }
    string userID = orderUsers.count(orderID) ? orderUsers[orderID] : "?";
    db.updateOrderStatus(orderID, "CANCELLED");
    extractor.recordCancel(userID);  
    cout << Color::cyan("[CANCEL] order " + to_string(orderID)
                        + " cancelled — " + userID + "'s cancel count updated") << "\n";
    return true;
}

int runHistory(int argc, char* argv[]) {
    if (argc < 3) {
        cerr<< Color::red("[ERROR] usage: ./build/vigil history ") << "\n";
        return 1;
    }
    string userID = argv[2];
    DatabaseHandler db("vigil.db");
    if (!db.isOpen()) return 1;

    auto records = db.getTransactionsForUser(userID, 10);
    if (records.empty()) {
        cout<< Color::dim("no transactions for " + userID) << "\n";
        return 0;
    }

    cout<< Color::bold("==== Transaction History: " + userID + " ====") << "\n";
    for (const auto& r : records) {
        bool out = (r.type == "WITHDRAW" || r.type == "TRADE_BUY");
        string sign    = out ? "-" : "+";
        string colored = out ? Color::red(sign + "Rs. ") : Color::green(sign + "Rs. ");
        cout << "  " << r.type << "  " << colored
             << fixed << setprecision(2) << r.amount
             << "  -> balance Rs." << r.balanceAfter;
        if (!r.note.empty()) cout << "  (" << r.note << ")";
        cout << "\n";
    }
    return 0;
}

void runInteractive(DatabaseHandler& db, Bridge& bridge,
                     OrderBook& book, FeatureExtractor& extractor,
                     Graph& tradeGraph,
                     unordered_set<string>& blocked,
                     unordered_map<int, string>& orderUsers,
                     double THRESHOLD, double ML_WEIGHT,
                     double GRAPH_WEIGHT, int TEMP_AT, int PERM_AT)
{
    cout << Color::cyan("=== Vigil Node — Interactive Mode ===") << "\n";

    cout << "Enter your name to begin: ";
    string currentUser;
    getline(cin, currentUser);
    if (currentUser.empty()) currentUser = "GUEST";

    auto promptDouble = [](const string& label) -> double {
        cout << label;
        string line; getline(cin, line);
        try { return stod(line); } catch (...) { return -1.0; }
    };
    auto promptInt = [](const string& label) -> int {
        cout << label;
        string line; getline(cin, line);
        try { return stoi(line); } catch (...) { return -1; }
    };

    int nextID = 1000;

    while (true) {
        cout << "\n" << Color::bold("=== " + currentUser
                             + "  |  Rs."
                             + to_string(db.getBalance(currentUser)) + " ===") << "\n";
        cout << "1.Buy  2.Sell  3.Deposit  4.Withdraw  "
                "5.Balance  6.History  7.Book  8.Cancel  9.Switch  0.Quit\n> ";

        string choice;                  
        if (!getline(cin, choice)) break;
        if (choice == "0" || choice == "quit" || choice == "exit") break;

        if (choice == "1" || choice == "2") {
            string side = (choice == "1") ? "BUY" : "SELL";
            double price = promptDouble("  Price? Rs.");
            int    qty   = promptInt("  Quantity? ");
            if (price <= 0 || qty <= 0) {
                cout << Color::red("  invalid price or quantity") << "\n";
            } else {
                Order o = makeOrder(nextID++, currentUser, price, qty, side);
                processOrder(o, db, book, extractor, bridge, tradeGraph,
                             blocked, orderUsers, THRESHOLD,
                             ML_WEIGHT, GRAPH_WEIGHT, TEMP_AT, PERM_AT);
                runMatchLoop(book, db, tradeGraph, orderUsers);
            }
            continue;
        }
        if (choice == "3") {
            double amt = promptDouble("  Deposit amount? Rs.");
            if (amt <= 0 || !db.deposit(currentUser, amt))
                cout << Color::red("  deposit failed\n");
            else
                cout << Color::green("  deposited — new balance Rs."
                               + to_string(db.getBalance(currentUser))) << "\n";
            continue;
        }
        if (choice == "4") {
            double amt = promptDouble("  Withdraw amount? Rs.");
            if (amt <= 0 || !db.withdraw(currentUser, amt))
                cout << Color::red("  insufficient funds\n");
            else
                cout << Color::green("  withdrawn — balance Rs."
                               + to_string(db.getBalance(currentUser))) << "\n";
            continue;
        }
        if (choice == "5") {
            cout << "  balance: Rs." << fixed << setprecision(2)
                 << db.getBalance(currentUser) << "\n";
            continue;
        }
        if (choice == "6") {
            auto records = db.getTransactionsForUser(currentUser, 10);
            if (records.empty()) {
                cout << Color::dim("  no transactions yet\n");
            } else {
                for (const auto& r : records) {
                    bool out = (r.type == "WITHDRAW" || r.type == "TRADE_BUY"); 
                    string sign = out ? "-" : "+";
                    cout << "  " << r.type << "  " << sign << "Rs."
                         << fixed << setprecision(2) << r.amount
                         << "  -> Rs." << r.balanceAfter << "\n";
                }
            }
            continue;
        }
        if (choice == "7") { book.printBook(); continue; }
        if (choice == "8") {
            int id = promptInt("  Order ID to cancel? ");
            if (id <= 0) {
                cout << Color::red("  invalid order ID\n");
            } else {
                cancelOrderByID(id, book, db, extractor, orderUsers);
            }
            continue;
        }
        if (choice == "9") {
            cout << "  Enter new name: ";
            getline(cin, currentUser);
            if (currentUser.empty()) currentUser = "GUEST";
            continue;
        }
        cout << Color::red("  unknown option — pick a number from the menu\n");
    }
    cout << Color::dim("\nsession ended\n");
}