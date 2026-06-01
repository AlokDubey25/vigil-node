# pragma once

struct Trade{
    int tradeID = 0;
    
    int buyOrderID = 0;
    int sellOrderID = 0;

    double price = 0.0;

    int quantity = 0;

    long long timestamp = 0;

    bool butFilled = false;
    bool sellFilled = false;
};
