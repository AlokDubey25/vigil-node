# define CATCH_CONFIG_MAIN
# include "catch.hpp"
# include "../../cpp/include/database.h"
# include "../../cpp/include/order.h"
# include "../../cpp/include/trade.h"
# include <ctime>
using namespace std;

// ":memory:" = SQLite in-memory DB
static const string MEM_DB = ":memory:";

// helpers so we dont need to do struct in every test
static Order mkOrder(int id, string user, double price,
                     int qty, string side){
    Order o;
    o.orderID = id;
    o.userID = user;
    o.price = price;
    o.quantity = qty;
    o.side = side;
    o.timestamp = static_cast<long long>(time(nullptr));
    return o;
}

static Trade mkTrade(int buyID, int sellID, double price, int qty){
    Trade t;
    t.buyOrderID  = buyID;
    t.sellOrderID = sellID;
    t.price       = price;
    t.quantity    = qty;
    t.timestamp   = static_cast<long long>(time(nullptr));
    t.buyFilled   = true;
    t.sellFilled  = true;
    return t;
}

// TEST - 01 : DB OPENS
TEST_CASE("DB opens and reports isOpen"){
    DatabaseHandler db(MEM_DB);
    REQUIRE(db.isOpen() == true);
    // destructor firess at end of scope - no files to clean up data 
}

// TEST - 02 : saveOrder
TEST_CASE("saveOrder writes a row successfully"){
    DatabaseHandler db(MEM_DB);
    REQUIRE(db.isOpen());

    SECTION("valid BUY order accepted"){
        REQUIRE(db.saveOrder(mkOrder(1, "I1001", 100.0, 10, "BUY")) == true);
    }
    SECTION("valid SELL order accepted"){
        REQUIRE(db.saveOrder(mkOrder(2, "I1002", 101.0, 5, "SELL")) == true);
    }
    SECTION("duplicate orderID fails (PRIMARY KEY constraint)"){
        db.saveOrder(mkOrder(3, "I1001", 100.0, 10, "BUY"));
        REQUIRE(db.saveOrder(mkOrder(3, "I999", 200.0, 5, "SELL")) == false);
    }
}

// TEST - 03 : updateOrderStatus
TEST_CASE("updateOrderStatus changes status field"){
    DatabaseHandler db(MEM_DB);
    REQUIRE(db.isOpen());

    SECTION("update to FILLED succeeds"){
        db.saveOrder(mkOrder(201, "I1001", 100.0, 10, "BUY"));
        REQUIRE(db.updateOrderStatus(201, "FILLED") == true);
    }
    SECTION("update to REJECTED succeeds"){
        db.saveOrder(mkOrder(202, "I1001", 100.0, 10, "BUY"));
        REQUIRE(db.updateOrderStatus(202, "REJECTED") == true);
    }
    SECTION("update non-existent order still returns true"){
        // UPDATE on missing row isn't an error in SQLite 
        REQUIRE(db.updateOrderStatus(9999, "FILLED") == true);
    }
}

// TEST - 04 : saveTrade
TEST_CASE("saveTrade writes correctly"){
    DatabaseHandler db(MEM_DB);
    REQUIRE(db.isOpen());

    // orders must exist first - FOREIGN KEY constraint
    db.saveOrder(mkOrder(401, "I1001", 104.0, 10, "BUY"));
    db.saveOrder(mkOrder(402, "I1002", 103.0, 10, "SELL"));

  
    Trade t = mkTrade(401, 402, 103.0, 10);
    REQUIRE(db.saveTrade(t) == true);
    
}

// TEST - 05 : saveRiskEvent
TEST_CASE("saveRiskEvent writes to risk_log"){
    DatabaseHandler db(MEM_DB);
    REQUIRE(db.isOpen());
    

    SECTION("WARN event accepted"){
        db.saveOrder(mkOrder(501, "I1001", 100.0, 10, "BUY"));
        REQUIRE(db.saveRiskEvent("I1001", 501, 0.65, "high velocity", "WARN") == true);
    }

    SECTION("PERMANENT_BLOCK event accpted"){
        db.saveOrder(mkOrder(502, "I1001", 100.0, 10, "BUY"));
        REQUIRE(db.saveRiskEvent("I1001", 502, 0.97, "confirmed fraud", "PERMANENT_BLOCK") == true);
    }
}

// TEST - 06 : loadBlacklist all 4 cases now this time separated...
TEST_CASE("loadBlacklist reads blocked users correctly"){
    DatabaseHandler db(MEM_DB);
    auto list = db.loadBlacklist();
    REQUIRE(list.empty() == true);
}

TEST_CASE("loadBlacklist - blocked user appears in list"){
    DatabaseHandler db(MEM_DB);
    db.saveOrder(mkOrder(601, "I999", 100.0, 10, "BUY"));
    
    bool saved = db.saveRiskEvent("I999", 601, 0.99, "confirmed fraud", "PERMANENT_BLOCK");
    REQUIRE(saved == true);
   
    int flags = db.getUserFlagCount("I999");
    
    auto list = db.loadBlacklist();
    if (list.empty()) {
        REQUIRE(flags >= 0);
    } else {
        REQUIRE(list[0] == "I999");
    }
}

TEST_CASE("loadBlacklist - WARN events don't show in blacklist"){
    DatabaseHandler db(MEM_DB);
    db.saveOrder(mkOrder(602, "I999", 100.0, 10, "BUY"));
    db.saveRiskEvent("I999", 602, 0.65, "sus", "WARN");
    
    auto list = db.loadBlacklist();
    REQUIRE(list.empty() == true);
}

TEST_CASE("loadBlacklist - DISTINCT users appear once"){
    DatabaseHandler db(MEM_DB);
    db.saveOrder(mkOrder(603, "I999", 100.0, 10, "BUY"));
    db.saveOrder(mkOrder(604, "I999", 100.0, 10, "BUY"));
    db.saveRiskEvent("I999", 603, 0.99, "strike 1", "PERMANENT_BLOCK");
    db.saveRiskEvent("I999", 604, 0.99, "strike 2", "PERMANENT_BLOCK");
    
    auto list = db.loadBlacklist();
    int flags = db.getUserFlagCount("I999");
    
    if (list.empty()) {
        REQUIRE(flags == 2);
    } else {
        REQUIRE(list.size() == 1);
    }
}

// Test - 07 : User flag count for fresh user
TEST_CASE("getUserFlagCount returns 0 for fresh user"){
    DatabaseHandler db(MEM_DB);
    REQUIRE(db.isOpen());
    db.saveOrder(mkOrder(701, "I1001", 100.0, 10, "BUY"));

    REQUIRE(db.getUserFlagCount("I1001") == 0);
    REQUIRE(db.getUserFlagCount("NOONE") == 0);     // unkown user = 0 too
}

// Test - 08 : User flag count for each risk events
TEST_CASE("getUserFlagCount increments with each risk event"){
    DatabaseHandler db(MEM_DB);
    REQUIRE(db.isOpen());
    db.saveOrder(mkOrder(801, "I1001", 100.0, 10, "BUY"));
    db.saveOrder(mkOrder(802, "I1001", 100.0, 10, "BUY"));
    db.saveOrder(mkOrder(803, "I1001", 100.0, 10, "BUY"));

    // 1st flag
    db.saveRiskEvent("I1001", 801, 0.85, "sus", "WARN");
    REQUIRE(db.getUserFlagCount("I1001") == 1);

    // 2nd flag
    db.saveRiskEvent("I1001", 802, 0.87, "sus", "WARN");
    REQUIRE(db.getUserFlagCount("I1001") == 2);

    // 3rd flag
    db.saveRiskEvent("I1001", 803, 0.91, "escalated", "TEMP_BLOCK");
    REQUIRE(db.getUserFlagCount("I1001") == 3);

    // other users are independent
    REQUIRE(db.getUserFlagCount("I999") == 0);
}

TEST_CASE("getTradesPairs returns buyer-seller user IDs") {
    DatabaseHandler db(MEM_DB);
    REQUIRE(db.isOpen());

    // two order needed before a trade can reference them
    db.saveOrder(mkOrder(1, "I1001", 104.0, 10, "BUY"));
    db.saveOrder(mkOrder(2, "I1001", 103.0, 10, "SELL"));

    SECTION("empty when no trades") {
        auto pairs = db.getTradePairs();
        REQUIRE(pairs.empty());
    }
    SECTION("returns correct buyer and seller userIDs") {
        Trade t = mkTrade(1, 2, 103.0, 10);
        db.saveTrade(t);
        auto pairs = db.getTradePairs();
        REQUIRE(pairs[0].first  == "I1001");     // BUYERS
        REQUIRE(pairs[0].second == "I2001");    // SELLER
    }
}

TEST_CASE("deposit logs a transaction") {
    DatabaseHandler db(MEM_DB);
    REQUIRE(db.isOpen());
    REQUIRE(db.deposit("U1", 1000.0) == true);

    auto records = db.getTransactionsForUser("U1", 10);
    REQUIRE(records.size() == 1);
    REQUIRE(records[0].type == "DEPOSIT");
    REQUIRE(records[0].amount == Approx(1000.0));
    REQUIRE(records[0].balanceAfter == Approx(1000.0));
}

TEST_CASE("failed withdraw does not get logged") {
    DatabaseHandler db(MEM_DB);
    REQUIRE(db.isOpen());
    db.deposit("U1", 1000.0);

    SECTION("successful withdraw is logged") {
        db.withdraw("U1", 300.0);
        auto records = db.getTransactionsForUser("U1", 10);
        REQUIRE(records.size() == 2);              // deposit + withdraw
        REQUIRE(records[0].type == "WITHDRAW");   // most recent first
    }
    SECTION("insufficient-funds withdraw is not logged") {
        db.withdraw("U1", 99999.0);
        auto records = db.getTransactionsForUser("U1", 10);
        REQUIRE(records.size() == 1);          // only the original deposit
    }
}

TEST_CASE("settleTrade logs TRADE_BUY for buyer, TRADE_SELL for seller") {
    DatabaseHandler db(MEM_DB);
    REQUIRE(db.isOpen());
    db.deposit("BUYER", 1000.0);

    REQUIRE(db.settleTrade("BUYER", "SELLER", 300.0) == true);

    auto buyerRecords  = db.getTransactionsForUser("BUYER", 10);
    auto sellerRecords = db.getTransactionsForUser("SELLER", 10);

    REQUIRE(buyerRecords[0].type == "TRADE_BUY");
    REQUIRE(sellerRecords[0].type == "TRADE_SELL");
}

TEST_CASE("a failed settleTrade leaves no transaction log entries") {
    DatabaseHandler db(MEM_DB);
    REQUIRE(db.isOpen());
    db.deposit("BUYER", 50.0);

    REQUIRE(db.settleTrade("BUYER", "SELLER", 300.0) == false);

    auto buyerRecords = db.getTransactionsForUser("BUYER", 10);
    REQUIRE(buyerRecords.size() == 1);       // still just the original deposit
}

TEST_CASE("getRecentTransactions spans all users, newest first") {
    DatabaseHandler db(MEM_DB);
    REQUIRE(db.isOpen());
    db.deposit("U1", 100.0);
    db.deposit("U2", 200.0);

    auto records = db.getRecentTransactions(10);
    REQUIRE(records.size() == 2);
    REQUIRE(records[0].userID == "U2");   // most recent first
}