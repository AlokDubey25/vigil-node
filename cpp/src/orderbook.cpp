# include "../include/orderbook.h"
# include <iostream>
# include <iomanip>
using namespace std;

bool OrderBook::addOrder(const Order& order){

    // Anything inappropriate get rejected 
    if (order.price <= 0.0){
        cerr<< "[REJECT] Invalid price: " << order.price << "\n";
        return false;
    }
    if (order.quantity <= 0){
        cerr<< "[REJECT] Invalid quantity: " << order.price << "\n";
        return false;
    }
    if (order.userID.empty()){
        cerr<< "[REJECT] Empty userID on order " << order.price << "\n";
        return false;
    }

    long long key = toPaise(order.price);


    if (order.side == "BUY"){
        buyBook_[key] = order;
    }else if (order.side == "SELL"){
        sellBook_[key] = order;
    }else{
        cerr<< "[REJJECTED] Unkown Side: " << order.side << "\n";
        return false;   
    }
    return true;
}
    

void OrderBook::printBook() const{
    cout << "\n=== ORDER BOOK ===\n";

    cout<< "  SELL side (lowest ask first)\n";
    for (const auto& [key, order] : sellBook_){
        cout<< "  ASK  "
            << fixed << setprecision(2)
            << key / 100.0
            << "  qty: " << order.quantity
            << "  [" << order.userID << "]\n";
    }

    cout << " --- spread ---\n";

    cout << "  BUY side (highest bid first)\n";
    for (const auto& [key, order] : buyBook_){
        cout<< "  BID  "
            << fixed << setprecision(2)
            << key / 100.0
            << "  qty: " << order.quantity
            << "  [" << order.userID << "]\n";
    }

    cout<< "=======================\n\n";
}

long long OrderBook::getBestBid() const{
    if (buyBook_.empty()) return -1;
    return buyBook_.begin()->first;
}
long long OrderBook::getBestAsk() const{
    if (sellBook_.empty()) return -1;
    return sellBook_.begin()->first;
}


double OrderBook::getBestBidPrice() const{
    return getBestBid() == -1 ? 0.0 : getBestBid() / 100.0;
}
double OrderBook::getBestAskPrice() const{
    return getBestAsk() == -1 ? 0.0 : getBestAsk() / 100.0;
}


bool OrderBook::hasBuys() const{
    return !buyBook_.empty();
}
bool OrderBook::hasSells() const{
    return !sellBook_.empty();
}
