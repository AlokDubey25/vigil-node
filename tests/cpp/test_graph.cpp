# define CATCH_CONFIG_MAIN
# include "catch.hpp"
# include "../../cpp/include/graph.h"

TEST_CASE("no edges - no cycle"){
    Graph g;
    REQUIRE(g.isInCycle("I1") == false);
    REQUIRE(g.getDegree("I1") == 0);   
}

TEST_CASE("one-way trade - no cycle"){
    Graph g;
    g.addEdge("I1", "I2");  // i.e. I1 bought from I2
    REQUIRE(g.isInCycle("I1") == false);
    REQUIRE(g.isInCycle("I2") == false);
    REQUIRE(g.getDegree("I1") == 1);
    REQUIRE(g.getDegree("I2") == 0);
}

TEST_CASE("direct 2 hop cycle - classic wash trade A buys B then B buys A"){
    Graph g;
    g.addEdge("I1", "I2");   // I1 bought form I2
    g.addEdge("I2", "I1");  // I2 bought from I1   <- its WASH TRADE !!

    SECTION("both users flagged"){
        REQUIRE(g.isInCycle("I1") == true);
        REQUIRE(g.isInCycle("I2") == true);
    }
    SECTION("getCircularTraders return both"){
        REQUIRE(g.getCircularTraders().size() == 2);
    }
}

TEST_CASE("3-hp ring A -> B -> C -> A cordinated wash trade"){
    Graph g;
    g.addEdge("I1", "I2");
    g.addEdge("I2", "I3");
    g.addEdge("I3", "I1");      // cloases the ring

    REQUIRE(g.isInCycle("I1") == true);
    REQUIRE(g.isInCycle("I2") == true);
    REQUIRE(g.isInCycle("I3") == true);
}

TEST_CASE("innocent bystander outside the ring is not flagged"){
    Graph g;
    g.addEdge("I1", "I2");
    g.addEdge("I2", "I1");       // I1 and I2 are cycling
    g.addEdge("I3", "I4");      // I3 just bought from I4, no cycle

    REQUIRE(g.isInCycle("I1") == true);
    REQUIRE(g.isInCycle("I3") == false);
    REQUIRE(g.isInCycle("I4") == false);
}

TEST_CASE("degree counts unique counterparties only"){
    Graph g;
    g.addEdge("I1", "I2");
    g.addEdge("I1", "I3");
    g.addEdge("I1", "I2");      // duplicate - unordered_set duplicates

    REQUIRE(g.getDegree("I1") == 2);    // I2 and I3, not 3
}