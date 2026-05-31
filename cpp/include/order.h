# pragma once
# include <string>
using namespace std;

struct Order{
    int    orderID = 0;
    string userID;
    double price = 0.0;
    int quantity = 1;
    string side;                  // for buying and selling stock
    long long timestamp = 0;
    string status = "PENDING";
    double blockScore = 0.0;     // for ML to stop fraud
    bool   fraudFlag = false;
};
