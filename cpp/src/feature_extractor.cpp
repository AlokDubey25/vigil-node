# include "../include/feature_extractor.h"
# include <cmath>
# include <sstream>
# include <algorithm>
using namespace std;

// first extract for main method
FeatureVector FeatureExtractor::extract(const Order& order, double midPrice){

    FeatureVector fv;

    // create rolling window and history_[key] auto-creates empty deque if key is new a
    auto& window = history_[order.userID];

    // only compute features if we have history to work with
    // first-ever order from a user gets all-zero vector and that's fine    
    if (!window.empty()){
        fv.velocity         = calcVelocity(window, order.timestamp);
        fv.priceDeviation   = calcPriceDeviation(order, midPrice);
        fv.cancelRate       = calcCancelRate(order.userID, static_cast<int>(window.size()));
        fv.sizeRatio        = calcSizeRatio(order, window);
        fv.timeBetween      = calcTimeBetween(window);
        fv.repeatPriceRate  = calcRepeatPriceRate(order, window);
    }   

    // add this order to history AFTER computing features
    // this is to describe past behavior, not include this order
    window.push_back(order);

    // if we have more window size orders, drop oldest
    if (static_cast<int>(window.size()) > WINDOW_SIZE){
        window.pop_front();
    }


    // these numbers are fenerous to maximize as real fraud rarely  exceeds them
    fv.velocity         = min(fv.velocity,      100.0);
    fv.priceDeviation   = min(fv.priceDeviation,  1.0);
    fv.sizeRatio        = min(fv.sizeRatio,      20.0);
    fv.timeBetween      = min(fv.timeBetween,   300.0);
    fv.repeatPriceRate  = min(fv.repeatPriceRate, 1.0);


    return fv;
}

void FeatureExtractor::recordCancel(const string& userID) {
    cancelCount[userID]++;
}

// First feature :- velocity , count how many orders in last 60 sec
double FeatureExtractor::calcVelocity(const deque<Order>& orders, long long now)const{
    if (orders.empty()) return 0.0;

    int count = 0;
    const long long window = 60;   // 60 sec

    for (const auto& o : orders){
        if (now - o.timestamp <= window) count ++;
    }

    return static_cast<double>(count);
}

// Second feature :- price deviation , how deviated from mid price
double FeatureExtractor::calcPriceDeviation(const Order& current, double midPrice) const{
    
    // if no mid price then it can't deviate
    if (midPrice <= 0.0) return 0.0;

    return abs(current.price - midPrice) / midPrice;
}

// Third feature :- cancel rate , cancels  / total orders
double FeatureExtractor::calcCancelRate(const string& userID, int totalOrders) const{
    if (totalOrders == 0) return 0.0;

    // cacelcount_ might not have an entry for this user
    auto it = cancelCount.find(userID);
    if (it == cancelCount.end()) return 0.0;

    return static_cast<double>(it->second) / totalOrders;
}

// Fourth feature :- size ratio :- its 1.0 means exactly average , 5.0 means 5x bigger thna usual and so on...
double FeatureExtractor::calcSizeRatio(const Order& current, const deque<Order>& orders) const{
    if (orders.empty()) return 1.0;    

    double total = 0.0;
    for (const auto& o : orders) total += o.quantity;

    double avg = total / orders.size();
    if (avg <= 0.0) return 1.0;

    return static_cast<double>(current.quantity) / avg;
}

// Fifth feature :-  time between orders , very small speed like -> 0 then its bot speed trading
double FeatureExtractor::calcTimeBetween(const deque<Order>& orders) const{
    if (orders.size() < 2) return 0.0;

    long long totalGap = 0;
    int count = 0;

    for (size_t i = 1; i < orders.size(); i++){
        long long gap = orders[i].timestamp - orders[i - 1].timestamp;
        if (gap >= 0){
            totalGap += gap;
            count++;
        }
    }

    if (count == 0) return 0.0;
    return static_cast<double>(totalGap) / count;
}

// Sixth feature :- repeat price rate , when price matches current order price then high val = always placing orders at same level 
double FeatureExtractor::calcRepeatPriceRate(const Order& current, const deque<Order>& orders) const{
    if (orders.empty()) return 0.0;

    // compare using paise to avoid float comparison issue
    long long currentPaise = static_cast<long long>(current.price * 100 + 0.5);

    int matches = 0;
    for (const auto& o : orders){
        long long oPaise = static_cast<long long>(o.price * 100 + 0.5);
        if (currentPaise == oPaise) matches++;
    }

    return static_cast<double>(matches) / orders.size();
}

vector<double> FeatureVector::toVector() const {
    return {
        velocity,
        priceDeviation,
        cancelRate,
        sizeRatio,
        timeBetween,
        repeatPriceRate
    };
}

void FeatureVector::print(const string& userID) const {
    cout << "[FV] " << userID
         << fixed << setprecision(3)
         << " vel="      << velocity
         << " pdev="     << priceDeviation
         << " cancel="   << cancelRate
         << " sratio="   << sizeRatio
         << " tbetween=" << timeBetween
         << " rpprice="  << repeatPriceRate
         << "\n";
}

// toJSON for buildiing [v1,v2,v3,v4,v5,v6] - this exact format will go to py for fraud detection
string FeatureVector::toJSON() const{
    auto v = toVector();
    ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < v.size(); i++){
        if (i>0) oss << ",";
        oss << fixed << setprecision(4) << v[i];    // taking 4 decoimal points for precison
    }

    oss << "]";

    return oss.str();
}

// isSus precheck for ml it needs 2+ flags to avoid poitive from single noisy features
bool FeatureVector::isSuspicious() const{
    int flags = 0;

    if (velocity > 20.0)        flags++;     // > 20 orders last in 60s
    if (priceDeviation > 0.05)  flags++;    // > 5% away from market mid 
    if (sizeRatio > 5.0)        flags++;   // > 5x thierr avg order size
    if (repeatPriceRate > 0.7)  flags++;  // same price > 70% of history


    return flags >= 2;
    
}

string friendlyVerdict(const FeatureVector& fv, double combined, double threshold) {
    if (combined <= threshold)
        return "Order accepted — looks normal.";

    vector<string> reasons;
    if (fv.velocity > 20.0)         reasons.push_back("trading unusually fast");
    if (fv.priceDeviation > 0.05)   reasons.push_back("placing an order far from the market price");
    if (fv.sizeRatio > 5.0)         reasons.push_back("placing an unusually large order for this user");
    if (fv.repeatPriceRate > 0.7)   reasons.push_back("repeatedly hitting the exact same price level");
    if (fv.cancelRate > 0.7)        reasons.push_back("cancelling orders unusually often");

    // none of the individual ML features tripped — the score came mostly
    // from the graph side (wash trading ring), not any one behavioral signal
    if (reasons.empty())
        return "Order blocked — linked to a circular trading pattern with another account.";

    ostringstream oss;
    oss << "Order blocked — this looks suspicious: the user is ";
    for (size_t i = 0; i < reasons.size(); i++) {
        if (i > 0) oss << (i == reasons.size() - 1 ? " and " : ", ");
        oss << reasons[i];
    }
    oss << ".";
    return oss.str();
}