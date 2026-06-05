# define CATCH_CONFIG_MAIN
# include "catch.hpp"
# include "../../cpp/include/order.h"
# include "../../cpp/include/orderbook.h"
# include "../../cpp/include/trade.h"

//  multiple tests for validation and taking bids

// Build order im one line
static Order make(int id, const string& uid, double price, int qty, const string& side){
    Order o;
    o.orderID = id;
    o.userID = uid;
    o.price = price;
    o.quantity = qty;
    o.side = side;
    o.timestamp = 1700000000LL;
    return o;
}

// TEST - 01 : Validaton
TEST_CASE("addOrder rejects invalid input"){
    OrderBook book;

    SECTION("negative price"){
        REQUIRE(book.addOrder(make(1,"U1", -5.0, 10, "BUY")) == false);
    }
    SECTION("zero qty"){
        REQUIRE(book.addOrder(make(2,"U1", 100.0, 0, "BUY")) == false);
    }
    SECTION("empty uid"){
        REQUIRE(book.addOrder(make(3,"", 100.0, 10, "BUY")) == false);
    }
    SECTION("unkown side"){
        REQUIRE(book.addOrder(make(4,"U1", 100.0, 10, "HOLD")) == false);
    }
    SECTION("valid Order Accepted"){
        REQUIRE(book.addOrder(make(5,"U1", 5.0, 10, "BUY")) == true);
    }
}

// TEST - 02 : Sorting
TEST_CASE("order book sorts correcrtly"){
    SECTION("BUY side highest price first"){
        OrderBook book;
        book.addOrder(make(1, "U1", 100.0, 10, "BUY"));
        book.addOrder(make(2, "U2", 102.5, 5, "BUY"));
        book.addOrder(make(3, "U3", 101.0, 8, "BUY"));
        REQUIRE(book.getBestBidPrice() == Approx(102.5));
    }
    SECTION("SELL side lowest price first"){
        OrderBook book;
        book.addOrder(make(4, "U4", 103.0, 10, "SELL"));
        book.addOrder(make(5, "U5", 101.5, 5, "SELL"));
        book.addOrder(make(6, "U6", 102.0, 8, "SELL"));
        REQUIRE(book.getBestAskPrice() == Approx(101.5));
    }
}

// TEST - 03 : Matching
TEST_CASE("matchOrders executes correctly"){
    SECTION("no match when bid less than ask") {
        OrderBook book;
        book.addOrder(make(1,"U1",100.0,10,"BUY"));
        book.addOrder(make(2,"U2",101.0,10,"SELL"));
        Trade result = book.matchOrders();
        REQUIRE(result.quantity == 0);
    }
    SECTION("match fires when bid >= ask") {
        OrderBook book;
        book.addOrder(make(1,"U1",103.0,10,"BUY"));
        book.addOrder(make(2,"U2",102.0,10,"SELL"));
        Trade result = book.matchOrders();
        REQUIRE(result.quantity > 0);
    }
    SECTION("full fill remove both orders") {
        OrderBook book;
        book.addOrder(make(1,"U1",103.0,10,"BUY"));
        book.addOrder(make(2,"U2",102.0,10,"SELL"));
        book.matchOrders();
        REQUIRE(book.hasBuys() == false);
        REQUIRE(book.hasSells() == false);
    }
    SECTION("partial fill : buyer has remainder") {
        OrderBook book;
        book.addOrder(make(1,"U1",103.0,15,"BUY"));
        book.addOrder(make(2,"U2",102.0,10,"SELL"));
        book.matchOrders();
        REQUIRE(book.hasBuys() == true);
        REQUIRE(book.hasSells() == false);
    }
    SECTION("partially fill : seller has remainder") {
        OrderBook book;
        book.addOrder(make(1,"U1",102.0,10,"BUY"));
        book.addOrder(make(2,"U2",101.0,10,"SELL"));
        book.matchOrders();
        REQUIRE(book.hasBuys() == false);
        REQUIRE(book.hasSells() == true);
    }
}

// TEST - 04 : Empty Book Edge cases
TEST_CASE("empty book edge cases"){
    OrderBook book;
    SECTION("matchOrders on empty book return false"){
        Trade result = book.matchOrders();
        REQUIRE(result.quantity == 0);
    }
    SECTION("getBestBid return -1 when empty"){
        REQUIRE(book.getBestBid() == -1);
    }
    SECTION("getBestAsk return -1 when empty"){
        REQUIRE(book.getBestAsk() == -1);
    }
}

