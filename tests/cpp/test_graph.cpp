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

TEST_CASE("getNetworkScore : zero for isolated user") {
    Graph g;
    REQUIRE(g.getNetworkScore("I1") == Approx(0.0));
}

TEST_CASE("getNetworkScore : cycle contributes 0.5") {
    Graph g;
    g.addEdge("I1", "I2");
    g.addEdge("I2","I1");       // wash trade

    double s = g.getNetworkScore("I1");
    REQUIRE(s >= 0.5);      // cycle at least 0.5
    REQUIRE(s <= 1.0);     // never Exceeds cap
}

TEST_CASE("getNetworkScore : degree adds score  without cycle") {
    Graph g;
    // U1 has 3 counterparties but no cycle
    g.addEdge("I1", "I2");
    g.addEdge("I1", "I3");
    g.addEdge("I1", "I4");

    double s = g.getNetworkScore("I1");
    REQUIRE(s == Approx(0.1));      // degree >= 3 adds 0.1, no cycle
}

TEST_CASE("getNetworkScore : high degree adds more") {
    Graph g;
    // U1 has 5r counterparties but no cycle
    for (int i = 2; i <= 6; i++)
        g.addEdge("I1", "I" + to_string(i));

    double s = g.getNetworkScore("I1");
    REQUIRE(s == Approx(0.3));      // deg >= 5: 0.1 + 0.2 = 0.3
}

TEST_CASE("getNetworkScore : capped at 1.0") {
    Graph g;
    // in cycle AND high degree = would exceed 1.0 without cap
    g.addEdge("I1", "I2"); g.addEdge("I2", "I1");       // cycle: +0.5
    g.addEdge("I1", "I3"); g.addEdge("I1", "I4");
    g.addEdge("I1", "I5"); g.addEdge("I1", "I6");       // deg 5+: +0.3

    // total would be 0.5+0.1+0.2 = 0.8 - below cap, but test it
    REQUIRE(g.getNetworkScore("I1") <= 1.0);
}

TEST_CASE("getConnectedComponent finds all nodes in one connected group") {
    Graph g;
    g.addEdge("I1", "I2");
    g.addEdge("I2", "I1");         // I1 and I2 in a cycle
    g.addEdge("I2", "I3");        // I3 connected to ring

    auto comp = g.getConnectedComponent("I1");
    REQUIRE(comp.size() == 3);  // I1, I2, I3
}

TEST_CASE("getConnectedComponent isolated node returns only itself") {
    Graph g;
    g.addEdge("I1", "I2");
    g.addEdge("I3", "I4");      // separate component

    auto comp = g.getConnectedComponent("I1");
    REQUIRE(comp.size() == 2);  // I1 and I2 only - no path to I3 or I4

    auto comp2 = g.getConnectedComponent("I3");
    REQUIRE(comp2.size() == 2); // I3 and I4 only
}

TEST_CASE("getConnectedComponet traverses reverse edges too") {
    Graph g;
    g.addEdge("I1", "I2");      // I1 -> I2 (directed)
    // starting from I2, reverse edge should find I1
    auto comp = g.getConnectedComponent("I2");
    REQUIRE(comp.size() == 2);  // BOTH I1 & I2
}

TEST_CASE("getSummary returns correct node and edge counts") {
    Graph g;
    g.addEdge("I1", "I2");
    g.addEdge("I2", "I1");

    string s = g.getSummary();
    REQUIRE(s.find("nodes=2") != string::npos);
    REQUIRE(s.find("edges=2") != string::npos);
    REQUIRE(s.find("circular_trades=2") != string::npos);
}