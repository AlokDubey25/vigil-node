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
    t.buyOrderID = buyID;
    t.sellOrderID = sellID;
    t.price = price;
    t.quantity = qty;
    t.timestamp = static_cast<long long>(time(nullptr));
    t.buyFilled = true;
    t.sellFilled = true;
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
        db,saveOrder(mkOrder(1, "I1001", 100.0, 10, "BUY"));
        REQUIRE(db.saveOrder(mkOrder(1, "I999", 200.0, 5, "SELL")) == false);
    }
}

// TEST - 03 : updateOrderStatus
TEST_CASE("updateOrderStatus changes status field"){
    DatabaseHandler db(MEM_DB);
    REQUIRE(db.isOpen());
    db.saveOrder(mkOrder(1, "I1001", 100.0, 10, "BUY"));

    SECTION("update to FILLED succeeds"){
        REQUIRE(db.updateOrderStatus(1, "FILLED") == true);
    }
    SECTION("update to REJECTED succeeds"){
        REQUIRE(db.updateOrderStatus(1, "REJECTED") == true);
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
    db.saveOrder(mkOrder(1, "I1001", 104.0, 10, "BUY"));
    db.saveOrder(mkOrder(2, "I1002", 103.0, 10, "SELL"));

    SECTION("valid trade with existing orders succeeds"){
        Trade t = mkTrade(1, 2, 103.0, 10);
        REQUIRE(db.saveTrade(t) == true);
    }
}

// TEST - 05 : saveRiskEvent
TEST_CASE("saveRiskEvent writes to risk_log"){
    DatabaseHandler db(MEM_DB);
    REQUIRE(db.isOpen());
    db.saveOrder(mkOrder(1, "I1001", 100.0, 10, "BUY"));

    SECTION("WARN event accepted"){
        REQUIRE(db.saveRiskEvent(
            "I1001", 1, 0.65, "high velocity", "WARN"
        ) == true);
    }

    SECTION("PERMANENT_BLOCK event accpted"){
        REQUIRE(db.saveRiskEvent(
            "I1001", 1, 0.97, "confirmed fraud", "PERMANET_BLOCK"
        ) == true);
    }
}

// TEST - 06 : loadBlacklist
TEST_CASE("loadBlacklist reads blocked users correctly"){
    DatabaseHandler db(MEM_DB);
    REQUIRE(db.isOpen());
    db.saveOrder(mkOrder(1, "I999", 100.0, 10, "BUY"));

    SECTION("empty bkacklist when no blocks exists"){
        auto list = db.loadBlacklist();
        REQUIRE(list.empty() == true);
    }
    SECTION("blocked user appears in list"){
        db.saveRiskEvent("I999", 1, 0.99, "confirmed fraud", "PERMANENT_BLOCK");
        auto list = db.loadBlacklist();
        REQUIRE(list.size() == 1);
        REQUIRE(list[0] == "I999");
    }
    SECTION("WARN events don't show in blacklist"){
        db.saveRiskEvent("I999", 1, 0.65, "sus", "WARN"); // warn not permanent block
        auto list = db.loadBlacklist();
        REQUIRE(list.empty() == true);                   // still empty
    }
    SECTION("DISTINCT - same user blocked multiple times appears once"){
        db.saveRiskEvent("I999", 1, 0.99, "strike 1", "PERMANENT_BLOCK");
        db.saveRiskEvent("I999", 1, 0.99, "strike 2", "PERMANENT_BLOCK");
        auto list = db.loadBlacklist();
    REQUIRE(list.size() == 1);                     // DISTINCT in SQL
    }
}