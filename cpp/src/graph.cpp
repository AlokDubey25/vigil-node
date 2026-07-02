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



vector<string> Graph::getConnectedComponent(const string& userID) const{
    vector<string> component;

    if (adj_.find(userID) == adj_.end()) return component;

    // build reverse adj onnce so we can traverse both dir
    unordered_map<string, unordered_set<string>> rev;
    for (const auto& [u, neighbours] : adj_)
        for (const auto& v : neighbours)
            rev[v].insert(u);

    // BFS - visit forward and reserve neighbors at each step
    unordered_set<string> visited;
    queue<string> bfsQ;
    visited.insert(userID);
    bfsQ.push(userID);

    while (!bfsQ.empty()) {
        auto curr = bfsQ.front(); bfsQ.pop();
        component.push_back(curr);

        // forward: who has curr bought from?
        auto fwd = adj_.find(curr);
        if (fwd != adj_.end()) {
            for (const auto& n : fwd -> second) 
                if (!visited.count(n)) { visited.insert(n); bfsQ.push(n); }
        }

        // reverse: who has brought from curr?
        auto r = rev.find(curr);
        if (r != rev.end()) {
            for (const auto& n : r-> second)
                if (!visited.count(n)) {visited.insert(n); bfsQ.push(n); }
        }
    }

    return component;
}

string Graph::getSummary() const {
    int nodes = static_cast<int>(adj_.size());
    int edges = 0;
    for (const auto& [_, n] : adj_) edges += static_cast<int>(n.size());
    auto circ = getCircularTraders();

    ostringstream oss;
    oss << "nodes=" << nodes
        << " edges=" << edges
        << " circular_traders=" << circ.size();

    return oss.str();
}

double Graph::getNetworkScore(const string& userID) const {
    double score = 0.0;
    if (isInCycle(userID)) score += cycleBase_;

    int deg = getDegree(userID);

    if (deg >= 3) score += 0.1;
    if (deg >= 5) score += 0.2;

    return min(score, 1.0);
}
