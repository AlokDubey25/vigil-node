# pragma once
# include <deque>
# include <unordered_map>
# include <vector>
# include <string>
# include <iostream>
# include <iomanip>
# include "order.h"
using namespace std;

// It will describe about one user's behaviour at one moment 
// like this have each and every features which will be req for next model
struct FeatureVector{
    double velocity         = 0.0;      // orders in last 60 sec
    double priceDeviation   = 0.0;     // distance from mid price
    double cancelRate       = 0.0;    // cacels/totals
    double sizeRatio        = 1.0;   // this qty/avg qty
    double timeBetween      = 0.0;  // avg sec btwn orders
    double repeatPriceRate  = 0.0; // fraction of history at same price


    // into vec for sending data to other engine for fraud detection
    vector<double> toVector() const{
        return {velocity, priceDeviation, cancelRate, sizeRatio, timeBetween, repeatPriceRate};
    }

    void print(const string& userID) const{
        cout<< "[FV]" << userID
            << "vel = "     << velocity
            << "pdev = "    << priceDeviation
            << "create = "  << cancelRate
            << "sratio = "  << sizeRatio
            << "tbetween = "<< timeBetween
            << "rpprice = " << repeatPriceRate
            << "\n";
    }

};


// and now this will extract each features from one to other model
class FeatureExtractor{
public:
    // past orders to remember as a history window
    static constexpr int WINDOW_SIZE = 100;

    // it will be main method to call for every order
    FeatureVector extract(const Order& order, double midPrice);          // midPrice : (bestBid + bestAsk) / 2 in rs. , 0 if unkown
    
    // as name suggest :
    void recordCancel(const string& userID);

private:
 
    unordered_map<string, deque<Order>> history_;   // for recent orders 
    unordered_map<string, int> cancelCount;        // how many times each user cancelled for cancelRate

    // now main part like this will help to calculate each features defined above
    double calcVelocity(const deque<Order>& orders, long long now) const;
    double calcPriceDeviation(const Order& current, double midPrice) const;
    double calcCancelRate(const string& userID, int totalOrders) const;
    double calcSizeRatio(const Order& current, const deque<Order>& orders) const;
    double calcTimeBetween(const deque<Order>& orders) const;
    double calcRepeatPriceRate(const Order& current, const deque<Order>& orders) const;

};