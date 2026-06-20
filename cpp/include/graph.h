# pragma once
# include <string>
# include <unordered_map>
# include <unordered_set>
# include <vector>
# include <iostream>
# include <iomanip>
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


private:
    // adjust user - set of users has brought FROM
    unordered_map<string, unordered_set<string>> adj_;

    // DFS - we can get from current back to staet in <= maxDepth hops may be or may be not...
    bool dfsCycle(const string& start, 
                  const string& current,
                  unordered_set<string>& visited,
                  int depth, int maxDepth) const;
};