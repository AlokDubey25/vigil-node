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
using namespace std;




int main(){

    // 01 :- config: reads settings.json before anything else
    Config cfg("config/settings.json");
    if (!cfg.isLoaded())
        cerr<< "[WARN] using hardcoded defaults\n";

    // read all thresholds from config with safe fallbacks
    const double THRESHOLD = cfg.getDouble("fraud_threshold",       0.80);
    const int    TEMP_AT   = cfg.getInt   ("blacklist_temp_threshold", 3);
    const int    PERM_AT   = cfg.getInt   ("blacklist_perm_threshold", 5);

    cout<< "[CONFIG] fraud_threshold = " << THRESHOLD
        << "    temp_at = " << TEMP_AT
        << "    perm_at = " << PERM_AT << "\n";



    // 02 :- so data enter first and after rescan it continue to engine 
    DatabaseHandler db("vigil.db");
    if (!db.isOpen()) {return 1; }
    
    // oad previously blocked users into unordered_set for O(1)
    auto blacklist = db.loadBlacklist();
    unordered_set<string> blocked(blacklist.begin(), blacklist.end());
    cout<< "[ENIGINE]" << blacklist.size() << " users on permanent blacklist\n";

/*
    // demo -> add a known-bad user so we can see blocking in action in production this comes from past sessions in risk_log
    {
        Order demo = {99, "U_FRAUD_BOT", 100.0, 1, "BUY",
                      static_cast<long long>(time(nullptr)),
                      "PENDING", 0.0, false};
        
        db.saveOrder(demo);
        db.saveRiskEvent("U_FRAUD_BOT", 99, 0.99, "confirmed fraud bot", "PERMANENT_BLOCK");

        // reload so U_FRAUD_BOT appears in set
        blacklist = db.loadBlacklist();
        blocked.clear();
        blocked.insert(blacklist.begin(), blacklist.end());
    }
*/
    
        

    // 03 :- Python bridge which is added now - it launched once, stays runnig
    // run ./vigil from project root so the path resolves correctly
    Bridge bridge("python3 python/bridge/scorer.py");
    if (!bridge.isReady()){
        cerr<< "[WARN] Bridge not ready - using fallback score\n";
    }

    
    OrderBook book;
    FeatureExtractor extractor;
    Graph tradeGraph;

    unordered_map<int, string> orderUsers;

    auto historicPairs = db.getTradePairs();
    for (const auto& [buyer, seller] : historicPairs) {
        tradeGraph.addEdge(buyer, seller);  // it will rebuild irderUsers from DB for graph - partial
    }
    if (!historicPairs.empty()) {
        cout<< "[GRAPH] rebuilt"
            << historicPairs.size()
            << " edges from DB history\n";
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
        double combined = (mlScore * 0.7) + (graphScore * 0.3);

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

        // g. add to order book
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
        if (book.getBestBid() < book.getBestAsk()) {
            break;
        }
        Trade t = book.matchOrders();
        if (t.quantity == 0) {
            break; 
        }
        db.saveTrade(t);
    
        if (t.buyFilled)  db.updateOrderStatus(t.buyOrderID, "FILLED");
        if (t.sellFilled) db.updateOrderStatus(t.sellOrderID, "FILLED");
        tradeCount++;
    

    // Graph : look up user IDs and add edge
        string buyer = orderUsers.count(t.buyOrderID) ? orderUsers[t.buyOrderID] : "?";
        string seller = orderUsers.count(t.sellOrderID) ? orderUsers[t.sellOrderID] : "?";
        
        tradeGraph.addEdge(buyer, seller);
        cout<< "[GRAPH] edge: "<< buyer << " -> " << seller << "\n";

        // check both ends of new edge for cycles
        for (const auto& uid : {buyer, seller}){
            if (tradeGraph.isInCycle(uid)) {
                cout<< "[GRAPH] ▲ WASH TRADE DETECTED: "
                    << uid << " is in a circular pattern!!\n";
                db.saveRiskEvent(uid, t.buyOrderID, 0.9, "circular trading network", "WARN");
            }
        }
    }



    // PRINT FULL GRAPH SNAPSHOT
    cout<< "\n[GRAPH] trading network:\n";
    tradeGraph.print();
    auto circular = tradeGraph.getCircularTraders();
    if (!circular.empty()){
        cout<< "[GRAPH] " << circular.size()
            << " users flagged for circular trading\n";
    }

    return 0;
}