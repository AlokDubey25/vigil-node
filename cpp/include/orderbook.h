# pragma once
# include <map>
# include <functional>
# include <string>
# include "order.h"

class OrderBook{
public:
    // taking and printing to terminal 
    void addOrder(const Order& order);
    void printBook(const);

    // Best price for both bidder and taker
    double getBestBid() const;
    double getBestAsk() const;

    // Checking if both ain't be empty
    bool hasBuys() const;
    bool hasSells() const;

private:
    // Buying at highest and Selling at lowest
    std::map<double, Order,
            std::greater<double>> buyBook_;
    std::map<double, Order> sellBook_;

}; 
