# include "../include/graph.h"
using namespace std;

// edge for buyes to trade with seller
void Graph::addEdge(const string& buyer, const string& seller) {
    adj_[buyer].insert(seller);
    // point to be noted that seller has an entry even with no outgoing edges
    if (adj_.find(seller) == adj_.end())
        adj_[seller] = {};
}

// it will be true if user is in a trading cycle (like kinda wash trade)
bool  Graph::isInCycle(const string& userID) const{
    if (adj_.find(userID) == adj_.end()) return false;

    unordered_set<string> visited;
    visited.insert(userID);

    return dfsCycle(userID, userID, visited, 0, 3);

}

// for number of times unique counterparties this user has traded with
int Graph::getDegree(const string& userID) const{
    auto it = adj_.find(userID);
    if (it == adj_.end()) return 0;
    return static_cast<int>(it -> second.size());
}

// all users currently in a cycle
vector<string> Graph::getCircularTraders() const{
    vector<string> result;
    for (const auto& [user, _] : adj_) {
        if (isInCycle(user))
            result.push_back(user);
    }
    return result;
}

// debug - print full trade network
void Graph::print() const{
    if (adj_.empty()) {
        cout << "[GRAPH] no trades recorded yet\n";
        return;
    }
    cout<< "[GRAPH] trading network: \n";
    for (const auto& [user, neighbors] : adj_) {
        cout<< "  " << user << " -> ";
        for (const auto& n : neighbors)
            cout<< n << " ";
        if (isInCycle(user))
            cout << "  ▲ caution: CYCLE";
        cout << "\n";
    }
}


bool Graph::dfsCycle(const string& start, const string& current, unordered_set<string>& visited, int depth, int maxDepth) const{

    if (depth > maxDepth) return false;
    auto it = adj_.find(current);
    if (it == adj_.end()) return false;

    for (const auto& neighbor : it->second) {
        // if cycle forund then we'll back at start after at least one hop
        if (neighbor == start && depth > 0) return true;
        if (visited.count(neighbor)) continue;

        visited.insert(neighbor);
        if (dfsCycle(start, neighbor, visited, depth + 1, maxDepth))
            return true;
        
        visited.erase(neighbor);
    }

    return false;
}