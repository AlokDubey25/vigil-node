#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "../../cpp/include/bridge.h"

TEST_CASE("bridge with dead process ends up not ready") {
    Bridge b("sleep 0.1", 200);
    REQUIRE(b.isReady() == false);
}

TEST_CASE("score() times out and returns fallback") {
    Bridge b("echo '{\"ready\":true}'; sleep 5", 100);
    REQUIRE(b.isReady() == true);
    REQUIRE(b.score("[1,0,0,1,0,0]") == Approx(Bridge::FALLBACK_SCORE));
}

TEST_CASE("score() parses a normal response") {
    Bridge b("echo '{\"ready\":true}'; read l; echo '{\"score\":0.42}'", 200);
    REQUIRE(b.isReady() == true);
    REQUIRE(b.score("[1,0,0,1,0,0]") == Approx(0.42));
}

TEST_CASE("score() clamps out-of-range values") {
    Bridge b("echo '{\"ready\":true}'; read l; echo '{\"score\":5.0}'", 200);
    REQUIRE(b.isReady() == true);
    double s = b.score("[1,0,0,1,0,0]");
    REQUIRE(s <= 1.0); REQUIRE(s >= 0.0);
}

TEST_CASE("score() falls back safely on malformed response") {
    Bridge b("echo '{\"ready\":true}'; read l; echo 'not json'", 200);
    REQUIRE(b.isReady() == true);
    REQUIRE(b.score("[1,0,0,1,0,0]") == Approx(Bridge::FALLBACK_SCORE));
}

TEST_CASE("getLastExplanation captures explanation field") {
    Bridge b("echo '{\"ready\":true}'; read l; "
             "echo '{\"score\":0.62,\"explanation\":\"velocity increased\"}'", 200);
    REQUIRE(b.isReady() == true);
    b.score("[1,0,0,1,0,0]");
    REQUIRE(b.getLastExplanation() == "velocity increased");
}

TEST_CASE("getLastExplanation is empty when no explanation sent") {
    Bridge b("echo '{\"ready\":true}'; read l; echo '{\"score\":0.1}'", 200);
    REQUIRE(b.isReady() == true);
    b.score("[1,0,0,1,0,0]");
    REQUIRE(b.getLastExplanation() == "");
}