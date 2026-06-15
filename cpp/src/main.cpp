# include <iostream>
# include <ctime>
# include "../include/order.h"
# include "../include/orderbook.h"
# include "../include/trade.h"
# include "../include/database.h"
# include "../include/feature_extractor.h"
# include "../include/bridge.h"
using namespace std;

int main(){
    // 01 :- so data enter first and after rescan it continue to engine 
    DatabaseHandler db("vigil.db");
    if (!db.isOpen()) {return 1; }
    
    // load previously blocked users 
    auto blacklist = db.loadBlacklist();
    cout<< "[ENIGINE]" << blacklist.size() << " users on permanent blacklist\n";
        

    // 02 :- Python bridge which is added now - it launched once, stays runnig
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

        // b. compute mid price from current book state
        double midPrice = 0.0;
        if (book.hasBuys() && book.hasSells()){
            midPrice = (book.getBestBidPrice() + book.getBestAskPrice()) / 2.0;
        }

        // c. extract 6 featuews and print them
        FeatureVector fv = extractor.extract(o, midPrice);
        fv.print(o.userID);
        cout<< "            json:   " << fv.toJSON() << "\n";

        // d. rule based check - catches obvious threats before ML exists
        if (fv.isSuspicious()){
            cout<< "            [!] sus - flagged (ML scores)\n";
        }
        
        // e. add to order book
        book.addOrder(o);
    };

    cout<< "\n   Inserting orders + extracting features   \n";
    
    // _________HERE WE INSERTED ORDERS FOR NOW BEING____________
    // NEW DATA FOR ENTRIES FOR CHECKING ENGINE WITH ALL TEST PASS
    insertOrder(makeOrder(1, "I1001", 104.00, 10, "BUY"));
    insertOrder(makeOrder(2, "I1002", 103.50, 5, "BUY"));
    insertOrder(makeOrder(3, "I1003", 101.00, 20, "BUY"));
    insertOrder(makeOrder(4, "I1004", 103.00, 8, "SELL"));
    insertOrder(makeOrder(5, "I1005", 103.25, 5, "SELL"));
    insertOrder(makeOrder(6, "I1006", 106.0, 10, "SELL"));


    // 04 :- continuous loop to match the trade...

    int tradeCount = 0;
    cout << "MATCHING ENGINE RUNNING\n";

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


    return 0;  // destructor file here when db is closed

}