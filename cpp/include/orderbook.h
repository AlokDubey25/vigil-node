# pragma once
# include <map>
# include <functional>
# include <string>
# include "order.h"

class OrderBook{
public:
    // taking and printing to terminal 
    bool addOrder(const Order& order);
    void printBook() const;

    // Return -1 if book side is empty as (Best price for both bidder and taker)
    long long getBestBid() const;
    long long getBestAsk() const;

    // USer readbale best prices (as divided by 100)
    double getBestBidPrice() const;
    double getBestAskPrice() const;

    // Checking if both ain't be empty
    bool hasBuys() const;
    bool hasSells() const;

private:
    // Buying at highest and Selling at lowest
    std::map<long long, Order,
            std::greater<long long>> buyBook_;
    std::map<long long, Order> sellBook_;

    // Convert price in rupees to paise key
    static long long toPaise(double price){
        return static_cast<long long>(price * 100 + 0.5);
    }

}; 
