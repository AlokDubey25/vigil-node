# pragma once
# include <string>
# include <vector>
# include"../vendor/sqlite3.h"
# include "order.h"
# include "trade.h"
using namespace std;

class DatabaseHandler{
public:
    // first open db file and create if there are none
    explicit DatabaseHandler(const string& dbPath);
    ~DatabaseHandler();

    // First check if db conncetion is open and healthy
    bool isOpen() const { return db_ != nullptr;}        // prreviously it was like nullptr}; which made semicolon outside of isOpen

    bool createTables();

    bool saveOrder(const Order& order);             // write in db
    bool saveTrade(const Trade& trade);
    bool saveRiskEvent(const string & userID,
                        int orderID,
                        double score,
                        const string& reason,
                        const string& action);
    bool updateOrderStatus(int orderID,
                            const string& status);

    vector<string> loadBlacklist();                 // reads from db

private:
        sqlite3* db_ = nullptr;

        // raw sql for ddl (table ,data and all)
        bool execSQL(const string& sql);
};