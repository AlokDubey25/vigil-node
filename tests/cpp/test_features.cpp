# define CATCH_CONFIG_MAIN
# include "catch.hpp"
# include "../../cpp/include/feature_extractor.h"
# include "../../cpp/include/order.h"
using namespace std;

static const long long BASE = 1700000000LL;     // testing for nonw like will this be working or not

static Order mk(int id, const string& user, double price, int qty, const string& side, long long ts){

    Order o;
    o.orderID   = id;
    o.userID    = user;
    o.price     = price;
    o.quantity  = qty;
    o.side      = side;
    o.timestamp = ts;
    return o;
}

// TEST - 01 : first order = safe defaults
TEST_CASE("first order from new user returns safe defaults"){
    FeatureExtractor ex;
    FeatureVector fv = ex.extract(mk(1, "I1", 100.0, 10, "BUY", BASE), 0.0);

    REQUIRE(fv.velocity         == Approx(0.0));
    REQUIRE(fv.priceDeviation   == Approx(0.0));
    REQUIRE(fv.cancelRate       == Approx(0.0));
    REQUIRE(fv.sizeRatio        == Approx(1.0));   // 1.0 = average as perfectly avg
    REQUIRE(fv.timeBetween      == Approx(0.0));
    REQUIRE(fv.repeatPriceRate  == Approx(0.0));
}

// TEST - 02 : velocity
TEST_CASE("velocity counts orders within 60-second window"){

    SECTION("5 orders in last 60 seconds"){
        FeatureExtractor ex;
        // add 5 orders at t= bASE+0, +10,+20, +30,+40
        for (int i = 0; i < 5; i++)
            ex.extract(mk(i+1, "I1", 100.0, 10, "BUY", BASE + i*10), 0.0);

        // 6th order at BASE+50 - all 5 previous are within 60s
        FeatureVector fv = ex.extract(mk(6,"I1",100.0,10."BUY", BASE+50), 0.0);

        REQUIRE(fv.velocity == Approx(5.0));
    }

    SECTION("orders older than 60 seconds are not counted"){
        FeatureExtractor ex;
        // 3 orders all at BASE
        for (int i = 0; i < 3; i++)
            ex.extract(mk(i+1, "I1", 100.0,10,"BUY", BASE+70), 0.0);

        FeatureVector fv = ex.extract(mk(4,"I1",100.0,10,"BUY", BASE+70), 0.0);

        REQUIRE(fv.velocity == Approx(0.0));
    }

    SECTION("mixed : some in window, some outside"){
        FeatureExtractor ex;

        // 2 older orders at BASE , 2 recent orders at BASE+50
        ex.extract(mk(1,"I1", 100.0, 10, "BUY", BASE),     0.0);
        ex.extract(mk(2,"I1", 100.0, 10, "BUY", BASE),     0.0);
        ex.extract(mk(3,"I1", 100.0, 10, "BUY", BASE+50),  0.0);
        ex.extract(mk(4,"I1", 100.0, 10, "BUY", BASE+50),  0.0);

        // curent at BASE + 70 : BASE+50 orders are within 60s , BASE orders are not 
        FeatureVector fv = ex.extract(mk(5,"I1", 100.0, 10, "BUY", BASE+70), 0.0);

        REQUIRE(fv.velocity == Approx(2.0));
    }
}

// TEST - 03 : price deviation
TEST_CASE("price deviation measures distaance from mid-price"){

    SECTION("price 10 percent above mid"){
        FeatureExtractor ex;
        ex.extract(mk(1, "I1", 100.0, 10, "BUY", BASE), 100.0);
        // |110 - 100| / 100 = 0.1
        FeatureVector fv = ex.extract(mk(2, "I1", 110.0, 10, "BUY", BASE+1), 100.0);
        REQUIRE(fv.priceDeviation == Approx(0.1));
    }

    SECTION("price below mid also gives positive deviation"){
        FeatureExtractor ex;
        ex.extract(mk(1, "I1", 100.0, 10, "BUY", BASE), 100.0);
        // |90 - 100| / 100 = 0.1  
        FeatureVector fv = ex.extract(mk(2, "I1", 110.0, 10, "BUY", BASE+1), 0.0);
        REQUIRE(fv.priceDeviation == Approx(0.0));
    }
}

// 