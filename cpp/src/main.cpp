# include <iostream>
# include <iomanip>
# include <ctime>
# include <unordered_set>
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

void printUsage() {
    cout << "Usage: ./build/vigil <command> [args]\n"
         << "Commands:\n"
         << "  run           run the demo order set\n"
         << "  interactive   guided menu — buy, sell, deposit, withdraw\n"
         << "  history <uid> last 10 transactions for a user\n"
         << "  reset         wipe vigil.db and start fresh\n"
         << "  --help        this message\n";   
}

int main(int argc, char* argv[]){

    if (argc < 2) { printUsage(); return 1; }

    string cmd = argv[1];
    if (cmd == "history")     return runHistory(argc, argv);
    if (cmd == "reset")       return runReset();       ← fix 6: now handled
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
    const double THRESHOLD    = cfg.getDouble("fraud_threshold",       0.80);
    const int    TEMP_AT      = cfg.getInt   ("blacklist_temp_threshold", 3);
    const int    PERM_AT      = cfg.getInt   ("blacklist_perm_threshold", 5);
    const double ML_WEIGHT    = cfg.getDouble("ml_weight",             0.70);
    const double GRAPH_WEIGHT = cfg.getDouble("graph_weight",          0.30);
    const double CYCLE_BASE   = cfg.getDouble("cycle_base_score",      0.50);


    cout<< Color::dim("[CONFIG] threshold=" + to_string(THRESHOLD)
                      + "  temp_at=" + to_string(TEMP_AT)
                      + "  perm_at=" + to_string(PERM_AT)) << "\n";



    // 02 :- so data enter first and after rescan it continue to engine 
    DatabaseHandler db("vigil.db");
    if (!db.isOpen()) {return 1; }
    
    // oad previously blocked users into unordered_set for O(1)
    auto blacklist = db.loadBlacklist();
    unordered_set<string> blocked(blacklist.begin(), blacklist.end());
    cout<< "[ENIGINE]" << blocked.size() << " users on permanent blacklist\n";

    

    // 03 :- Python bridge which is added now - it launched once, stays runnig
    // run ./vigil from project root so the path resolves correctly
    Bridge bridge("python3 python/bridge/scorer.py");
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


    auto makeOrder = [](int id, const string& user,
                        double price, int qty,
                        const string& side){
        
        Order o;
        o.orderID = id;
        o.userID = user;
        o.price = price;
        o.quantity = qty;
        o.side = side;
        o.timestamp = static_cast<long long>(time(nullptr));
        return o;
    };


    // 04 :- it will hepl to detemine escalation action like
    // prevFlags = how many times already flagged (from risk_log)
    // thisFlag  = prevFlags + 1 (current incident)
    auto escalate = [&](const string& userID) -> string {
        int prevFlags = db.getUserFlagCount(userID);
        int thisFlag  = prevFlags + 1;

        if (thisFlag < TEMP_AT) return "WARN";
        if (thisFlag < PERM_AT) return "TEMP_BLOCK";
        
        return "PERMANENT_BLOCK";
    };


    // 05 :- inset orders
    // now every insert will save to main db , extract features, adds to book
    auto insertOrder = [&](const Order& o){
        // a. Save to DB first
        if (!db.saveOrder(o))
            cerr<< "[WARN] order " << o.orderID << " not saved to DB\n";

        // this to remeber who owns this order for downstram graph anallystics
        orderUsers[o.orderID] = o.userID;

        // b. blacklist check - fastest path, O(1)
        if (blocked.count(o.userID)) {
            cout<< "[BLOCKEd]" << o.userID << " - on permanent blacklist\n";
            db.updateOrderStatus(o.orderID, "REJECTED");
            db.saveRiskEvent(o.userID, o.orderID, 1.0, "permanent blacklist", "REJECT");

            return;
        }

        // c. compute mid price from current book state
        double midPrice = 0.0;
        if (book.hasBuys() && book.hasSells()){
            midPrice = (book.getBestBidPrice() + book.getBestAskPrice()) / 2.0;
        }

        // d. extract 6 featuews and print them
        FeatureVector fv = extractor.extract(o, midPrice);
        fv.print(o.userID);

        // e. ML score form py bridge
        double mlScore = bridge.score(fv.toJSON());
               
        // f. graph network score form trading history
        double graphScore = tradeGraph.getNetworkScore(o.userID);

        // g. combined score - but ML is weighted heavier (cuz its trained)
        // here graph will adds signal from network patterns which ML cna't see through
        double combined = (mlScore * ML_WEIGHT) + (graphScore * GRAPH_WEIGHT);

        cout<< "    [ML]    score="
            << fixed << setprecision(4) << mlScore << "\n"
            << "    [GRAPH] score=" << graphScore  << "\n"
            << "    [FINAL] score=" << combined    << "\n";





        // h. verdict against combined score
        if (combined > THRESHOLD){
            string action = escalate(o.userID);

            cout<< "[FLAGGED] " << o.userID
                << " score = "  << combined
                << " action = " << action << "\n";

            db.updateOrderStatus(o.orderID, "REJECTED");
            db.saveRiskEvent(o.userID, o.orderID, combined, "Combined ML+graph score", action);

            return;
        }

        // i. add to order book
        cout << "[ALLOWED] " << o.userID << "\n"; 
        book.addOrder(o);
    };


    // TEST ORDERS 

    cout<< "\n===== processing orders =====\n";
    
    insertOrder(makeOrder(1, "I1001", 104.00, 10, "BUY"));
    insertOrder(makeOrder(2, "I1002", 103.50,  5, "BUY"));
    insertOrder(makeOrder(3, "I1003", 101.00, 20, "BUY"));
    insertOrder(makeOrder(4, "I1004", 103.00,  8, "SELL"));
    insertOrder(makeOrder(5, "I1005", 103.25,  5, "SELL"));
    insertOrder(makeOrder(6, "I1006", 106.00, 10, "SELL"));


    cout << "\n==== wash trading demo orders ====\n";

    // round 1: I2001 buys from I2002
    insertOrder(makeOrder(10, "I2001", 105.00, 5, "BUY"));
    insertOrder(makeOrder(11, "I2002", 104.50, 5, "SELL"));

    // round 2: I2002 buys back from I2001  ← completes the cycle!
    insertOrder(makeOrder(12, "I2002", 105.00, 5, "BUY"));
    insertOrder(makeOrder(13, "I2001", 104.50, 5, "SELL"));

    // 06 :- continuous loop to match the trade...

    int tradeCount = 0;
    cout << "\n==== MATCHING ENGINE RUNNING(only allowed orders) ====\n";

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

    cout<< Color::bold("\033[1m==== Transaction History: " + userID + " ====\033[0m") << "\n";
    for (const auto& r : records) {
        bool isOutFlow = (r.type == "WITHDRAW" || r.type == "TRADE_BUY");

        string sign = isOutFlow ? "-" : "+";
        string colored = isOutFlow ? Color::red(sign + "Rd. ") : Color::green(sign + "Rs. ");

        cout<< " " << r.type << " " << colored
            << fixed << setprecision(2) << r.amount
            << "  -> balance Rs." << r.balanceAfter;
        
        if (!r.note.empty()) cout << "  (" << r.note << ")";
        cout << "\n";
    }
    return 0;
}

void runInteractive(DatabaseHandler& db) {
    cout<< "\033[36==== Live Session Started. Type commands below (e.g. 'history I2001' or 'exit') ====\033[0m\n";

    while (cout<< "vigil> " && getline(cin, line)) {
        if (line == "exit" || line == "quit") {
            break;
        }

        if (line.rfind("history ", 0) == 0) {
            string uid = line.substr(8);
            auto records = db.getTransactionsForUser(uid, 10);

            if (records.empty()) {
                cout<< "\033[90mno transactions for " << uid << "\033[0m\n";
            } else {
                for (const auto& r : records) {
                    bool isOutFlow = (r.type == "WINDOW" || r.type == "TRADE_BUY")
                    string sign = isOutFlow ? "-" : "+";
                    string colored = isOutFlow ? ("\033[31m" + sign + "Rs.") : ("\033[32m" + sign + "Rs.");
                    
                    cout<< "  " << r.type << "  " << colored 
                        << fixed << setprecision(2) << r.amount << "\033[0m"
                        << "  -> Rs." << r.balanceAfter << "\n";
                }
            }
            continue;
        }
        if (!line.empty()) {
            cout<< "    Unkown command. Supported commands: history <userID>\n";
        }
    }
}