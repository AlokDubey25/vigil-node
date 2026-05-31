# include <iostream>
# include "../include/order.h"
# include "../include/orderbook.h"
using namespace std;

int main(){
    OrderBook book;

    auto makeOrder = [](int id, const string& user,
                        double price, const string& side){
        
        Order o;
        o.orderID = id;
        o.userID = user;
        o.price = price;
        o.side = side;
        return o;
    };
    
    // Inseting 3 BUY orders for now
    book.addOrder(makeOrder(1, "I1001", 101.50, "BUY"));
    book.addOrder(makeOrder(2, "I1002", 100.00, "BUY"));
    book.addOrder(makeOrder(3, "I1003", 102.75, "BUY"));

    // Insering 3 SELL orders
    book.addOrder(makeOrder(4, "I1004", 103.50, "SELL"));
    book.addOrder(makeOrder(5, "I1005", 103.00, "SELL"));

    // Printing it...
    book.printBook();

    // Print best prices
    cout<< "Best bid: " << book.getBestBid() << "\n";
    cout<< "Best ask: " << book.getBestask() << "\n";
    cout<< "Spread:   "
        << book.getBestAsk() - book.getBestBid()
        << "\n";

    return 0;
} 
