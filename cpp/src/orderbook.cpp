# include "../include/orderbook.h"
# include <iostream>
# include <iomanip>
using namespace std;

void OrderBook::addOrder(const Order& order){
    if (order.side == "BUY"){
        buyBook_[order.price] = order;
    }else if (order.side == "SELL"){
        sellBook_[order.price] = order;
    }else{
        cerr<< "[ERROR] Unkown Side: "
            << order.side << "\n";
    }
}
    
void OrderBOok::printBook() const{
    cout << "\n=== ORDER BOOK ===\n";

    cout<< "  SELL side (lowest ask first)\n"
    for (const auto& [price, order] : sellBook_){
        cout<< "  ASK  "
            << fixed << setprecision(2)
            << price
            << "  [" << order.userID << "]\n";
    }

    cout << " --- spread ---\n";

    cout << "  BUY side (highest bid first)\n";
    for (const auto& [price, order] : buyBook_){
        cout<< "  BID  "
            << fixed << setprecision(2)
            << price
            << "  [" << order.userID << "]\n";
    }

    cout<< "=======================\n\n";
}

double OrderBook::getBestBid() const{
    if (buyBook_.empty()) return 0.0;
    return buyBook_.begin()->first;
}

double OrderBook::getBestAsk() const{
    if (sellBook_.empty()) return 0.0;
    return sellBook_.begin()->first;
}

bool OrderBook::hasBuys() const{
    return !buyBook_.empty();
}

bool OrderBook::hasSells() const{
    return !sellBook_.empty();
}
