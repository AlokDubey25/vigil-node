# define CATCH_CONFIG_MAIN
# include "catch.hpp"
# include "../../cpp/include/config.h"
# include <fstream>
# include <cstdio> 
using namespace std;

// a temp settings file, test, delete 
static const string TMP = "/tmp/vigil)test_settings.json";

TEST_CASE("Config reads  values from a JSON file"){
    // for a known settings file
    {
        ofstream f(TMP);
        f << R"({"fraud_threshold":0.75, "window_size":50, "mode":"test"})";
    }

    Config cfg(TMP);
    REQUIRE(cfg.isLoaded() == true);

    SECTION("reads double correctly"){
        REQUIRE(cfg.getDouble("fraud_threshold", 0.0) == Approx(0.75));
    }
    SECTION("reads int correctly"){
        REQUIRE(cfg.getInt("window_size", 0) == 50);
    }
    SECTION("reads string correctly"){
        REQUIRE(cfg.getString("mode", "") == "test");
    }
    SECTION("missing key returns fallback - not an error"){
        REQUIRE(cfg.getDouble("not_a_key", 42.0) == Approx(42.0));
        REQUIRE(cfg.getInt   ("not_a_key", 99)   == 99);
        REQUIRE(cfg.getString("not_a_key", "x")  == "x");
    }

    remove(TMP.c_str());
}

TEST_CASE("Config handles missing file gracefully"){
    Config cfg("/this/path/does/not/exist.json");
    REQUIRE(cfg.isLoaded() == false);

    // all fallbacks work when not loaded
    REQUIRE(cfg.getDouble("any", 9.9) == Approx(9.9));
    REQUIRE(cfg.getInt   ("any", 7)   == 7);
    REQUIRE(cfg.getString("any", "z") == "z");
}