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
using namespace std;


// fraud threshold - orders scoring above this are blocked
static constexpr double FRAUD_THRESHOLD = 0.80;


int main(){
    // 01 :- so data enter first and after rescan it continue to engine 
    DatabaseHandler db("vigil.db");
    if (!db.isOpen()) {return 1; }
    
    // load previously blocked users into unordered_set for O(1)
    auto blacklist = db.loadBlacklist();
    unordered_set<string> blocked(blacklist.begin(), blacklist.end());

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
    cout<< "[ENIGINE]" << blacklist.size() << " users on permanent blacklist\n";
        

    //AC 02 :- Python bridge which is added now - it launched once, stays runnig
    // run ./vigil from project root so the path resolves correctly
    Bridge bridge("python3 python/bridge/scorer.py");
    if (!bridge.isReady()){
        cerr<< "[WARN] Bridge not ready - using fallback score\n";
    }

    
    OrderBook book;
    FeatureExtractor extractor;

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

    // 03 :- inset orders
    // now every insert will save to main db , extract features, adds to book
    auto insertOrder = [&](const Order& o){
        // a. Save to DB first
        if (!db.saveOrder(o))
            cerr<< "[WARN] order " << o.orderID << " not saved to DB\n";


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

        // e. Send features to Py ML bridge and print score here...!!
        double mlScore = bridge.score(fv.toJSON());
        cout<< "        [ML] score: "<< fixed << setprecision(4) << mlScore << "\n";
               
        // f. verdict
        if (mlScore > FRAUD_THRESHOLD){
            cout<< "[BLOCKED] " << o.userID
                << " - ML score " << mlScore
                << " exceeds threshold "
                << FRAUD_THRESHOLD << "\n";

            db.updateOrderStatus(o.orderID, "REJECTED");
            db.saveRiskEvent(o.userID, o.orderID, mlScore, "ML score above threshold", "WARN");

            return;
        }

        // g. add to order book
        book.addOrder(o);
    };


    // TEST ORDERS 

    cout<< "\n===== processing orders =====\n";
    
    // these should pass - normal orders
    insertOrder(makeOrder(1, "I1001", 104.00, 10, "BUY"));
    insertOrder(makeOrder(2, "I1002", 103.50, 5, "BUY"));
    insertOrder(makeOrder(3, "I1003", 101.00, 20, "BUY"));

    // this should be blocked - on permanent blcklist
    insertOrder(makeOrder(4, "U_FRAUD_BOT", 103.00, 8, "SELL"));

    // these normal orders continue
    insertOrder(makeOrder(5, "I1005", 103.25, 5, "SELL"));
    insertOrder(makeOrder(6, "I1006", 106.00, 10, "SELL"));


    // 04 :- continuous loop to match the trade...

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
   
        // pull it out if its fully consumed or pending for partial fills 
        if (t.buyFilled)  db.updateOrderStatus(t.buyOrderID, "FILLED");
        if (t.sellFilled) db.updateOrderStatus(t.sellOrderID, "FILLED");
        tradeCount++;
    }

    cout<< "\n[ENGINE] session done for now... trades execuded successfully as: "
        << tradeCount << "\n";


    // final DB snapshot
    cout<< "[ENGINE] check risk_log for blocked orders\n";
    return 0;  // destructor file here when db is closed

}