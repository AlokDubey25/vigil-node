# include <iostream>
# include <ctime>
# include "../include/order.h"
# include "../include/orderbook.h"
# include "../include/trade.h"
# include "../include/database.h"
# include "../include/feature_extractor.h"
using namespace std;

int main(){
    // so data enter first and after rescan it continue to engine 
    DatabaseHandler db("vigil.db");
    if (!db.isOpen()){
        cerr<< "[FATAL] Database conncetion failded... Shutting down.\n";
        return 1;
    }
    
    // load previously blocked users 
    auto blacklist = db.loadBlacklist();
    if (!blacklist.empty()){
        cout<< "[ENIGINE]" << blacklist.size()
            << " users on permanent blacklist\n";
        for (const auto& uid : blacklist){
            cout<< "  blocked:  " << uid << "\n";
        }
    }else{
        cout<< "[ENGINE] no blocked users on record as per last data\n";
    }

    
    OrderBook book;

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

    //  inset orders
    // now every insert will save to main db , extract features, adds to book
    auto insertOrder = [&](const Order& o){
        // 01. Save to DB first
        if (!db.saveOrder(o))
            cerr<< "[WARN] order " << o.orderID << " not saved to DB\n";

        // 02. compute mid price from current book state
        double midPrice = 0.0;
        if (book.hasBuys() && book.hasSells()){
            midPrice = (book.getBestBidPrice() + book.getBestAskPrice()) / 2.0;
        }

        // 03. extract 6 featuews and print them
        FeatureVector fv = extractor.extract(o, midPrice);
        fv.print(o.userID);

        // 04. add to order book
        book.addOrder(o);
    }

    cout<< "\n   Inserting orders + extracting features   \n";
    
    // _________HERE WE INSERTED ORDERS FOR NOW BEING____________
    // NEW DATA FOR ENTRIES FOR CHECKING ENGINE WITH ALL TEST PASS
    insertOrder(makeOrder(1, "I1001", 104.00, 10, "BUY"));
    insertOrder(makeOrder(2, "I1002", 103.50, 5, "BUY"));
    insertOrder(makeOrder(3, "I1003", 101.00, 20, "BUY"));
    insertOrder(makeOrder(4, "I1004", 103.00, 8, "SELL"));
    insertOrder(makeOrder(5, "I1005", 103.25, 5, "SELL"));
    insertOrder(makeOrder(6, "I1006", 106.0, 10, "SELL"));
    // U1001 places a second order — this one will have real features
    insertOrder(makeOrder(7, "U1001", 104.50, 8,  "BUY"));

    // Printing it... before matching
    cout<< "\n   BOOK STATE   \n";
    book.printBook();

    // continuous loop to match the trade...

    int tradeCount = 0;
    cout << "MATCHING ENGINE RUNNING\n";

    while (book.hasBuys() && book.hasSells()) {
        if (book.getBestBid() < book.getBestAsk()) {
            cout << "[ENGINE]-positive : No more matches possible\n";
            break;
        }
        Trade t = book.matchOrders();
        if (t.quantity == 0) {
            break; 
        }
        db.saveTrade(t)
   
        // pull it out if its fully consumed or pending for partial fills 
        if (t.buyFilled)  db.updateOrderStatus(t.buyOrderID, "FILLED");
        if (t.sellFilled) db.updateOrderStatus(t.sellOrderID, "FILLED");
        tradeCount++;
    }

    cout<< "\n[ENGINE] session done for now... trades execuded successfully as: "
        << tradeCount << "\n";
        
    return 0;  // destructor file here when db is closed

}