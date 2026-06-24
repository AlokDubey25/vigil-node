# pragma once
# include <string>
# include <unordered_map>
# include <unordered_set>
# include <vector>
# include <iostream>
# include <iomanip>
# include <queue>
# include <sstream>
using namespace std;

class Graph {
public:
    // edge for buyes to trade with seller
    void addEdge(const string& buyer, const string& seller);

    // it will be true if user is in a trading cycle (like kinda wash trade)
    bool isInCycle(const string& userID) const;

    // for number of times unique counterparties this user has traded with
    int getDegree(const string& userID) const;

    // all users currently in a cycle
    vector<string> getCircularTraders() const;

    // debug - print full trade network
    void print() const;

    // network based fraud scrore 0.0-1.0 where 0.5 if in a cycle + 0.1/0.2 for high degree
    // capped at 1.0
    double getNetworkScore(const string& userID) const;

    // BFS : it will be used to find everyonein fraud ring, not just direct pairs
    vector<string> getConnectedComponent(const string& userID) const;

    // sting for logging and dashboard
    string getSummary() const;

    // configures the cycle detection score weight
    void setCycleBaseScore(double score) { cycleBase_ = score;}


    

private:
    // adjust user - set of users has brought FROM
    unordered_map<string, unordered_set<string>> adj_;

    // DFS - we can get from current back to staet in <= maxDepth hops may be or may be not...
    bool dfsCycle(const string& start, 
                  const string& current,
                  unordered_set<string>& visited,
                  int depth, int maxDepth) const;

    // same as settings.example.json
    double cycleBase_ = 0.5;
};