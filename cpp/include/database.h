# pragma once
# include <string>
# include <vector>
# include"../vendor/sqlite3.h"
# include "order.h"
# include "trade.h"
using namespace std;


struct TransactionRecord{
        int       txnID;
        string    userID;
        string    type;          // like Deposit , Withdraw, Trade_Buy, Trade_Sell...
        double    amount;       
        double    balanceAfter;
        long long timestamp;
        string    note;
    };

// one order row from the orders table (used by the `explain` command)
struct OrderRecord{
        bool      found = false;
        int       orderID = 0;
        string    userID;
        double    price = 0.0;
        int       quantity = 0;
        string    side;
        long long timestamp = 0;
        string    status;
        double    blockScore = 0.0;
        int       fraudFlag = 0;
    };

// one risk_log row for an order (score, reason, action)
struct RiskRecord{
        string    userID;
        int       orderID = 0;
        double    fraudScore = 0.0;
        string    reason;
        string    action;
        long long timestamp = 0;
    };
    
class DatabaseHandler{
public:
    // first open db file and create if there are none
    explicit DatabaseHandler(const string& dbPath);
    ~DatabaseHandler();

    // First check if db conncetion is open and healthy
    bool isOpen() const { return db_ != nullptr;}        // prreviously it was like nullptr}; which made semicolon outside of isOpen

    bool createTables();

    bool saveOrder(const Order& order);             // write in db
    bool saveTrade(Trade& trade);
    bool saveRiskEvent(const string & userID,
                        int orderID,
                        double score,
                        const string& reason,
                        const string& action);
    
    bool updateOrderStatus(int orderID,
                            const string& status);

    vector<string> loadBlacklist();                 // reads from db

    int getUserFlagCount(const string& userID);     // it is to decide WARN -> TEMP -> PERM

    vector<pair<string, string>> getTradePairs();    // it will return buyers and sellers userid for all trades and will be used at startup to rebuild trading network

    bool deposit(const string& userID, double amount, 
                 const string& type = "DEPOSIT", const string& note = "");
    
    bool withdraw(const string& userID, double amount,
                  const string& type = "WITHDRAW", const string& note = "");

    bool settleTrade(const string& buyerID, const string& sellerID, double amount);

    vector<TransactionRecord> getRecentTransactions(int limit = 10);
    vector<TransactionRecord> getTransactionsForUser(const string& userID, int limit = 10);

    // used by the `explain` command to rebuild an order's decision trail
    OrderRecord getOrder(int orderID);
    vector<RiskRecord> getRiskEventsForOrder(int orderID);

    double getBalance(const string& userID);

    private:
        sqlite3* db_ = nullptr;

        // raw sql for ddl (table ,data and all)
        bool execSQL(const string& sql);

        bool logTransaction(const string& userID, const string& type,
                            double amount, double balanceAfter, const string& note = "");
};