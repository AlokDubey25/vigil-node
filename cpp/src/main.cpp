# include <iostream>
# include <ctime>
# include "../include/order.h"
# include "../include/orderbook.h"
# include "../include/trade.h"
using namespace std;

int main(){
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
    
    // _________HERE WE INSERTED ORDERS FOR NOW BEING____________
    // Inseting 3 BUY orders for now
    book.addOrder(makeOrder(1, "I1001", 101.50, 10, "BUY"));
    book.addOrder(makeOrder(2, "I1002", 100.00, 5, "BUY"));
    book.addOrder(makeOrder(3, "I1003", 102.75, 20, "BUY"));

    // Insering 3 SELL orders
    book.addOrder(makeOrder(4, "I1004", 103.50, 8, "SELL"));
    book.addOrder(makeOrder(5, "I1005", 103.00, 15, "SELL"));

    // Test validation - this to print reject msg
    book.addOrder(makeOrder(6, "I1006", -50.0, 1, "BUY"));
    book.addOrder(makeOrder(7, "", 100.0, 1, "BUY"));

    // Printing it...
    book.printBook();

    // continuous loop to match the trade...

    int tradeCount = 0;
    cout<< " MATCHING ENGINE RUNNING\n";
    
    while (book.hasBuys() && book.hasSells()){
        // check spread every times not just for starting
        if (book.hasBuys() < book.hasSells()){
            cout<< "[ENGINE] spread positive - no more matches\n";
            break;
        }

        Trade t = book.matchOrders();

        if (t.quantity == 0) break;

        tradeCount++;
    }

    cout<< "\n[ENGINE] session done for now... trades execuded successfully as: "
        << tradeCount << "\n";

    cout<< "\n BOOK AFTER MATCHING\n";
    book.printBook();

    return 0;
} 