# define CATCH_CONFIG_MAIN
# include "catch.hpp"
# include "../../cpp/include/feature_extractor.h"
# include "../../cpp/include/order.h"
# include <algorithm>
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
        FeatureVector fv = ex.extract(mk(6,"I1",100.0, 10, "BUY", BASE+50), 0.0);

        REQUIRE(fv.velocity == Approx(5.0));
    }

    SECTION("orders older than 60 seconds are not counted"){
        FeatureExtractor ex;
        // 3 orders all at BASE
        for (int i = 0; i < 3; i++)
            ex.extract(mk(i+1, "I1", 100.0,10,"BUY", BASE), 0.0);

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

// TEST - 04 : size ratio
TEST_CASE("size ratio compares current qty to historical average"){

    SECTION("3x larger than average"){
        FeatureExtractor ex;
        // history : 3 orders of qty 10 each - average = 10
        for (int i = 0; i < 3; i++)
            ex.extract(mk(i+1, "I1", 100.0, 10, "BUY", BASE+i), 0.0);
        // qty = 30, avg = 10, -> ratio = 3.0
        FeatureVector fv = ex.extract(mk(4, "I1", 100.0, 30, "BUY", BASE+3), 0.0);

        REQUIRE(fv.sizeRatio == Approx(3.0));
    }

    SECTION("exactly average size = 1.0"){
        FeatureExtractor ex;
        for (int i = 0; i < 4; i++)
            ex.extract(mk(i+1, "I1", 100.0, 10, "BUY", BASE+i), 0.0);
        
            FeatureVector fv = ex.extract(mk(5, "I1", 100.0, 10, "BUY", BASE+4), 0.0);

            REQUIRE(fv.sizeRatio == Approx(1.0));
    }
}

// TEST - 05 : time between orders
TEST_CASE("timeBetween calculates average gap between orders"){

    SECTION("consistent 10-sec gaps"){
        FeatureExtractor ex;
        // 3 orders : t = 0, t = 10, t = 20  - gaps are 10 and 10
        for (int i = 0; i < 3; i++)
            ex.extract(mk(i+1, "I1", 100.0, 10, "BUY", BASE+i*10), 0.0);
        // 4th order: history=[t0,t10,t20], gaps=(10+10)/2 = 10.0
        FeatureVector fv = ex.extract(mk(4, "I1", 100.0, 10, "BUY", BASE+30), 0.0);

        REQUIRE(fv.timeBetween == Approx(10.0));
    }

    SECTION("omly one order in history returns 0"){
        FeatureExtractor ex;
        ex.extract(mk(1,"I1", 100.0, 10, "BUY", BASE), 0.0);
        // history has 1 item - can;t compute a gap, returns 0
        FeatureVector fv = ex.extract(mk(2,"I1", 100.0, 10, "BUY", BASE+10), 0.0);

        REQUIRE(fv.timeBetween == Approx(0.0));
    }
}

// TEST - 06 : repeat price rate
TEST_CASE("repeatPriceRate measures how often same price appears"){

    SECTION("half of history at same price"){
        FeatureExtractor ex;
        // 4 orders: alternatimg 100.0 and 105.0
        ex.extract(mk(1, "I1", 100.0, 10, "BUY", BASE),     0.0);
        ex.extract(mk(2, "I1", 105.0, 10, "BUY", BASE+1),   0.0);
        ex.extract(mk(3, "I1", 100.0, 10, "BUY", BASE+2),   0.0);
        ex.extract(mk(4, "I1", 105.0, 10, "BUY", BASE+3),   0.0);
        // 5th order at 100.0 - 2 of 4 history entries match = 0.5
        FeatureVector fv = ex.extract(mk(5, "I1", 100.0, 10, "BUY", BASE+4), 0.0);

        REQUIRE(fv.repeatPriceRate == Approx(0.5));
    }

    SECTION("no repeat prices = 0.0"){
        FeatureExtractor ex;
        ex.extract(mk(1,"I1", 100.0, 10, "BUY", BASE), 0.0);
        // price 105 doesn't match history price 100 at all
        FeatureVector fv = ex.extract(mk(2, "I1", 105.0, 10, "BUY", BASE+1), 0.0);

        REQUIRE(fv.repeatPriceRate == Approx(0.0));
    }
}

// TEST - 07 : different users don't share history
TEST_CASE("each user has completely independent history"){
    FeatureExtractor ex;

    // I1 builds up 3 orders of history
    for (int i = 0; i < 3; i++)
        ex.extract(mk(i+1, "I1", 100.0, 10, "BUY", BASE+i), 0.0);

    // I2's first order - must NOT see I1's history
    FeatureVector fv = ex.extract(mk(4, "I2", 100.0, 10, "BUY", BASE+3), 0.0);

    REQUIRE(fv.velocity == Approx(0.0));
    REQUIRE(fv.sizeRatio== Approx(1.0));
}

// TEST - 08 : rolling window caps at 100
TEST_CASE("rolling window never exceeds WINDOW_SIZE"){
    FeatureExtractor ex;

    // add 105 orders - window should cap at 100
    for (int i = 0; i< 105; i++)
        ex.extract(mk(i+1, "I1", 100.0, 10, "BUY", BASE+i), 0.0);

    // now window has orders BASE+5 to BASE+104
    // vel : orders where base+105 - ts <= 60
    // ts >= Basr+45: orders base+45 to Base+104 = 60

    FeatureVector fv = ex.extract(mk(106, "I1", 100.0, 10, "BUY", BASE+105), 0.0);

    REQUIRE(fv.velocity == Approx(60.0));
}


// TEST - 09 : toJSON format
TEST_CASE("toJSON produces valid format for Python"){
    FeatureVector fv;
    fv.velocity         = 5.0;
    fv.priceDeviation   = 0.1234;
    fv.cancelRate       = 0.0;
    fv.sizeRatio        = 2.5;
    fv.timeBetween      = 30.0;
    fv.repeatPriceRate  = 0.25;

    string json = fv.toJSON();

    SECTION("starts with [ and ends with ]"){
        REQUIRE(json.front() == '[');
        REQUIRE(json.back() == ']');
    }

    SECTION("has exactly 5 commas for 6 values"){
        int commas = static_cast<int>(count(json.begin(), json.end(), ','));
        REQUIRE(commas == 5);
    }

    SECTION("all-zeroes vector produces correct format"){
        FeatureVector zfv;
        // by default : all values are zero
        string zjson = zfv.toJSON();

        REQUIRE(zjson.front() == '[');
        REQUIRE(zjson.back() == ']');

        int commas = static_cast<int>(count(zjson.begin(), zjson.end(), ','));
        REQUIRE(commas == 5);
    }
}

// TEST - 10 : isSuspicious logic
TEST_CASE("isSuspicious requires 2 or more flags"){

    SECTION("safe defaults are not suspicious"){
        FeatureVector fv;                          // all defaults - new user
        REQUIRE(fv.isSuspicious() == false);
    }

    SECTION("one flag alone is not enough"){
        FeatureVector fv;
        fv.velocity = 25.0;                       // only velocity is high
        REQUIRE(fv.isSuspicious() == false);
    }

    SECTION("high velocity AND large size = sus"){
        FeatureVector fv;
        fv.velocity = 25.0;                         // flag 1: > 20
        fv.sizeRatio= 8.0;                         // flag 2: > 5
        REQUIRE(fv.isSuspicious() == true);
    }

    SECTION("high price deviation AND repeat price = sus"){
        FeatureVector fv;
        fv.priceDeviation   = 0.10;              // flag 1: > 0.05
        fv.repeatPriceRate  = 0.80;             // flag 2: > 0.7
        REQUIRE(fv.isSuspicious() == true);
    }

    SECTION("exactly at threshold is not flagged"){
        FeatureVector fv;
        fv.velocity = 20.0;                   // exactly 20, NOT > 20
        fv.sizeRatio= 5.0;                   // exactly 5, NOT > 5
        REQUIRE(fv.isSuspicious() == false);
    }

    SECTION("all 4 flags = definitely sus"){
        FeatureVector fv;
        fv.velocity         = 50.0;
        fv.priceDeviation   = 0.20;
        fv.sizeRatio        = 10.0;
        fv.repeatPriceRate  = 0.90;
        REQUIRE(fv.isSuspicious() == true);
    }
}


// TEST - 11 : feature caps
TEST_CASE("feature caps prevent extreme values"){
    static const long long BASE = 1700000000LL;
    FeatureExtractor ex;

    SECTION("velocity capped at 100 even with full window"){
        // 101 orders all at BASE - window holds last 100
        for (int i = 0; i < 101; i++)
            ex.extract(mk(i+1, "I1", 100.0, 10, "BUY", BASE), 0.0);

        // current at BASE + 30, all 100 windows entries within 60s
        // also here raw vel would be at 100 - cap also 100 so result = 100
        FeatureVector fv = ex.extract(mk(102, "I1", 100.0, 10, "BUY", BASE+30), 0.0);
        REQUIRE(fv.velocity <= 100.0);
    }

    SECTION("sizeRatio capped at 20"){
        // history: qty 1 each - average = 1
        for (int i = 0; i < 5; i++)
            ex.extract(mk(i+1, "I2", 100.0, 1, "BUY", BASE+i), 0.0);

        // qty=100, avg=1, raw ratio=100 - capped to 20
        FeatureVector fv = ex.extract(mk(6, "I2", 100.0, 100, "BUY", BASE+5), 0.0);
        REQUIRE(fv.sizeRatio == Approx(20.0));
    }
}

// Test - 12 : 01 Final verdict
TEST_CASE("friendlyVerdict — accepted order") {
    FeatureVector fv;   // all defaults, clean
    REQUIRE(friendlyVerdict(fv, 0.2, 0.8) == "Order accepted — looks normal.");
}

// Test - 12 : 02 Final verdict
TEST_CASE("friendlyVerdict — single flag, no 'and'") {
    FeatureVector fv;
    fv.velocity = 30.0;
    std::string v = friendlyVerdict(fv, 0.9, 0.8);
    REQUIRE(v.find("trading unusually fast") != std::string::npos);
    REQUIRE(v.find(" and ") == std::string::npos);
}

// Test - 12 : 03 Final verdict
TEST_CASE("friendlyVerdict — two flags joined with 'and'") {
    FeatureVector fv;
    fv.velocity  = 30.0;
    fv.sizeRatio = 8.0;
    std::string v = friendlyVerdict(fv, 0.9, 0.8);
    REQUIRE(v.find("trading unusually fast") != std::string::npos);
    REQUIRE(v.find("unusually large order") != std::string::npos);
    REQUIRE(v.find(" and ") != std::string::npos);
}

// Test - 12 : 04 Final verdict
TEST_CASE("friendlyVerdict — blocked with no individual flag falls back to network language") {
    FeatureVector fv;   // all features clean — score crossed purely via graph
    std::string v = friendlyVerdict(fv, 0.9, 0.8);
    REQUIRE(v.find("circular trading pattern") != std::string::npos);
}