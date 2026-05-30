# include <iostream>
# include "../include/order.h"
using namespace std;

int main(){
    Order o;
    o.orderID = 1;
    o.userID = "I25MA014";
    o.price = 100.50;
    o.side - "BUY";
    o.timestamp = 1700000000;

    cout << "Order " << o.orderID
         << " | " << o.userID
         << " | " << o.side
         << " | " << o.price << " rs\n";
    
    return 0;
} 
