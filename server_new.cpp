#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <queue>
#include <stack>
#include <functional>
#include <limits>
#include <algorithm>
#include <sstream>
#include <regex>
#include <cmath>
#include <utility>
#include "httplib.h"

using namespace std;

struct Edge {
    int to;
    int delay;
    bool active;
};

const int N = 5;
vector<vector<Edge>> graph(N);
string names[N] = {"D1", "D2", "D3", "D4", "D5"};

int deviceIndex(const string& name) {
    for (int i = 0; i < N; i++)
        if (names[i] == name) return i;
    return -1;
}

void addEdge(int a, int b, int delay) {
    if (a < 0 || b < 0 || a == b || delay <= 0) return;

    for (auto& e : graph[a]) {
        if (e.to == b) {
            e.delay = delay;
            e.active = true;

            for (auto& r : graph[b]) {
                if (r.to == a) {
                    r.delay = delay;
                    r.active = true;
                }
            }
            return;
        }
    }

    graph[a].push_back({b, delay, true});
    graph[b].push_back({a, delay, true});
}

bool setEdgeState(int a, int b, bool active) {
    if (a < 0 || b < 0 || a == b) return false;

    bool found = false;

    for (auto& e : graph[a])
        if (e.to == b) {
            e.active = active;
            found = true;
        }

    for (auto& e : graph[b])
        if (e.to == a) {
            e.active = active;
            found = true;
        }

    return found;
}

string jsonString(const string& s) {
    string out = "\"";

    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n') out += "\\n";
        else out += c;
    }

    return out + "\"";
}

string getValue(const string& body, const string& key) {
    regex pattern(
        "\"" + key +
        "\"\\s*:\\s*(?:\"([^\"]*)\"|([0-9]+))"
    );

    smatch match;

    if (regex_search(body, match, pattern))
        return match[1].matched ? match[1].str() : match[2].str();

    return "";
}

void initializeNetwork() {
    for (auto& list : graph) list.clear();

    addEdge(0, 1, 5);
    addEdge(0, 3, 8);
    addEdge(1, 2, 4);
    addEdge(2, 4, 6);
    addEdge(3, 4, 3);
}

string networkJson() {
    int active = 0, failed = 0;
    ostringstream out;

    out << "{\"nodes\":[";

    for (int i = 0; i < N; i++) {
        if (i) out << ",";
        out << "{\"id\":" << jsonString(names[i]) << "}";
    }

    out << "],\"edges\":[";
    bool first = true;

    for (int i = 0; i < N; i++) {
        for (const auto& e : graph[i]) {
            if (i >= e.to) continue;

            if (e.active) active++;
            else failed++;

            if (!first) out << ",";
            first = false;

            out << "{\"from\":" << jsonString(names[i])
                << ",\"to\":" << jsonString(names[e.to])
                << ",\"delay\":" << e.delay
                << ",\"active\":" << (e.active ? "true" : "false")
                << "}";
        }
    }

    out << "],\"totalDevices\":" << N
        << ",\"activeConnections\":" << active
        << ",\"failedConnections\":" << failed
        << ",\"status\":"
        << jsonString(failed ? "Degraded" : "Healthy") << "}";

    return out.str();
}

string runAlgorithm(const string& algorithm, int source, int target) {
    if (source < 0 || target < 0)
        return "{\"error\":\"Invalid source or target device\"}";

    vector<int> parent(N, -1);
    vector<int> distance(N, numeric_limits<int>::max());

    if (algorithm == "bfs") {
        queue<int> q;
        vector<bool> visited(N, false);

        q.push(source);
        visited[source] = true;
        distance[source] = 0;

        while (!q.empty()) {
            int u = q.front();
            q.pop();

            for (const auto& e : graph[u]) {
                if (!e.active || visited[e.to]) continue;

                visited[e.to] = true;
                parent[e.to] = u;
                distance[e.to] = distance[u] + 1;
                q.push(e.to);
            }
        }
    }
    else if (algorithm == "dijkstra") {
        using Pair = pair<int, int>;
        priority_queue<Pair, vector<Pair>, greater<Pair>> pq;

        distance[source] = 0;
        pq.push({0, source});

        while (!pq.empty()) {
            Pair current = pq.top();
            pq.pop();

            int cost = current.first;
            int u = current.second;

            if (cost != distance[u]) continue;

            for (const auto& e : graph[u]) {
                if (!e.active) continue;

                if (distance[u] + e.delay < distance[e.to]) {
                    distance[e.to] = distance[u] + e.delay;
                    parent[e.to] = u;
                    pq.push({distance[e.to], e.to});
                }
            }
        }
    }
    else if (algorithm == "dfs") {
        vector<bool> visited(N, false);
        vector<int> order;

        function<void(int)> visit = [&](int u) {
            visited[u] = true;
            order.push_back(u);

            for (const auto& e : graph[u])
                if (e.active && !visited[e.to])
                    visit(e.to);
        };

        visit(source);

        ostringstream out;
        out << "{\"found\":true,\"algorithm\":\"dfs\",\"path\":[";

        for (size_t i = 0; i < order.size(); i++) {
            if (i) out << ",";
            out << jsonString(names[order[i]]);
        }

        out << "],\"cost\":" << order.size() - 1 << "}";
        return out.str();
    }
    else {
        return "{\"error\":\"Use bfs, dijkstra or dfs\"}";
    }

    if (distance[target] == numeric_limits<int>::max())
        return "{\"found\":false,\"message\":\"No available route\"}";

    vector<int> path;

    for (int v = target; v != -1; v = parent[v])
        path.push_back(v);

    reverse(path.begin(), path.end());

    ostringstream out;
    out << "{\"found\":true,\"algorithm\":"
        << jsonString(algorithm) << ",\"path\":[";

    for (size_t i = 0; i < path.size(); i++) {
        if (i) out << ",";
        out << jsonString(names[path[i]]);
    }

    out << "],\"cost\":" << distance[target];

    if (algorithm == "bfs")
        out << ",\"hops\":" << distance[target];
    else
        out << ",\"totalDelay\":" << distance[target];

    out << "}";
    return out.str();
}
// Adjacency Matrix
vector<vector<int>> buildAdjacencyMatrix() {
    vector<vector<int>> matrix(N, vector<int>(N, 0));

    for (int u = 0; u < N; u++)
        for (const auto& e : graph[u])
            if (e.active) matrix[u][e.to] = e.delay;

    return matrix;
}

// Binary Search Tree
struct BSTNode {
    int key;
    BSTNode *left, *right;

    BSTNode(int value) : key(value), left(nullptr), right(nullptr) {}
};

BSTNode* bstInsert(BSTNode* root, int key) {
    if (!root) return new BSTNode(key);

    if (key < root->key)
        root->left = bstInsert(root->left, key);
    else if (key > root->key)
        root->right = bstInsert(root->right, key);

    return root;
}

bool bstSearch(BSTNode* root, int key) {
    while (root) {
        if (root->key == key) return true;
        root = key < root->key ? root->left : root->right;
    }
    return false;
}

int bstMin(BSTNode* root) {
    while (root && root->left) root = root->left;
    return root ? root->key : -1;
}

int bstMax(BSTNode* root) {
    while (root && root->right) root = root->right;
    return root ? root->key : -1;
}

void bstInorder(BSTNode* root, vector<int>& result) {
    if (!root) return;
    bstInorder(root->left, result);
    result.push_back(root->key);
    bstInorder(root->right, result);
}

void bstReverseInorder(BSTNode* root, vector<int>& result) {
    if (!root) return;
    bstReverseInorder(root->right, result);
    result.push_back(root->key);
    bstReverseInorder(root->left, result);
}

void deleteBST(BSTNode* root) {
    if (!root) return;
    deleteBST(root->left);
    deleteBST(root->right);
    delete root;
}

// AVL Tree
struct AVLNode {
    int key, height;
    AVLNode *left, *right;

    AVLNode(int value)
        : key(value), height(1), left(nullptr), right(nullptr) {}
};

int avlHeight(AVLNode* node) {
    return node ? node->height : 0;
}

int avlBalance(AVLNode* node) {
    return node ? avlHeight(node->left) - avlHeight(node->right) : 0;
}

void updateAVLHeight(AVLNode* node) {
    node->height = 1 + max(avlHeight(node->left), avlHeight(node->right));
}

AVLNode* rotateRight(AVLNode* y) {
    AVLNode* x = y->left;
    AVLNode* t2 = x->right;

    x->right = y;
    y->left = t2;

    updateAVLHeight(y);
    updateAVLHeight(x);
    return x;
}

AVLNode* rotateLeft(AVLNode* x) {
    AVLNode* y = x->right;
    AVLNode* t2 = y->left;

    y->left = x;
    x->right = t2;

    updateAVLHeight(x);
    updateAVLHeight(y);
    return y;
}

AVLNode* avlInsert(AVLNode* root, int key) {
    if (!root) return new AVLNode(key);

    if (key < root->key)
        root->left = avlInsert(root->left, key);
    else if (key > root->key)
        root->right = avlInsert(root->right, key);
    else
        return root;

    updateAVLHeight(root);
    int balance = avlBalance(root);

    if (balance > 1 && key < root->left->key)
        return rotateRight(root);

    if (balance < -1 && key > root->right->key)
        return rotateLeft(root);

    if (balance > 1 && key > root->left->key) {
        root->left = rotateLeft(root->left);
        return rotateRight(root);
    }

    if (balance < -1 && key < root->right->key) {
        root->right = rotateRight(root->right);
        return rotateLeft(root);
    }

    return root;
}

bool avlSearch(AVLNode* root, int key) {
    while (root) {
        if (root->key == key) return true;
        root = key < root->key ? root->left : root->right;
    }
    return false;
}

void avlInorder(AVLNode* root, vector<int>& result) {
    if (!root) return;
    avlInorder(root->left, result);
    result.push_back(root->key);
    avlInorder(root->right, result);
}

void deleteAVL(AVLNode* root) {
    if (!root) return;
    deleteAVL(root->left);
    deleteAVL(root->right);
    delete root;
}

// Expression Tree
struct ExprNode {
    string token;
    ExprNode *left, *right;

    ExprNode(string value)
        : token(std::move(value)), left(nullptr), right(nullptr) {}
};

bool isOperatorToken(const string& token) {
    return token == "+" || token == "-" ||
           token == "*" || token == "/";
}

double evaluateExpression(ExprNode* root) {
    if (!root->left && !root->right)
        return stod(root->token);

    double a = evaluateExpression(root->left);
    double b = evaluateExpression(root->right);

    if (root->token == "+") return a + b;
    if (root->token == "-") return a - b;
    if (root->token == "*") return a * b;
    if (root->token == "/") return b == 0 ? 0 : a / b;

    return 0;
}

ExprNode* buildExpressionTree(const vector<string>& postfix) {
    stack<ExprNode*> st;

    for (const string& token : postfix) {
        if (!isOperatorToken(token)) {
            st.push(new ExprNode(token));
        }
        else {
            if (st.size() < 2) return nullptr;

            ExprNode* right = st.top();
            st.pop();

            ExprNode* left = st.top();
            st.pop();

            ExprNode* node = new ExprNode(token);
            node->left = left;
            node->right = right;
            st.push(node);
        }
    }

    if (st.size() != 1) return nullptr;
    return st.top();
}

void deleteExpressionTree(ExprNode* root) {
    if (!root) return;
    deleteExpressionTree(root->left);
    deleteExpressionTree(root->right);
    delete root;
}
// Fractional Knapsack: Greedy Method
struct KnapsackItem {
    int id;
    double value;
    double weight;
};

double fractionalKnapsack(vector<KnapsackItem> items, double capacity) {
    sort(items.begin(), items.end(),
        [](const KnapsackItem& a, const KnapsackItem& b) {
            return a.value / a.weight > b.value / b.weight;
        });

    double total = 0;

    for (const auto& item : items) {
        if (capacity <= 0) break;

        double take = min(capacity, item.weight);
        total += take * (item.value / item.weight);
        capacity -= take;
    }

    return total;
}

// 0/1 Knapsack: Dynamic Programming
int zeroOneKnapsack(
    const vector<int>& weights,
    const vector<int>& values,
    int capacity
) {
    vector<int> dp(capacity + 1, 0);

    for (size_t i = 0; i < weights.size(); i++) {
        for (int w = capacity; w >= weights[i]; w--) {
            dp[w] = max(dp[w], dp[w - weights[i]] + values[i]);
        }
    }

    return dp[capacity];
}

// Extra DSA results; dashboard UI does not need to change.
string dsaReportJson() {
    vector<vector<int>> matrix = buildAdjacencyMatrix();

    BSTNode* bst = nullptr;
    AVLNode* avl = nullptr;

    for (int i = 1; i <= N; i++) {
        bst = bstInsert(bst, i);
        avl = avlInsert(avl, i);
    }

    vector<int> ascending, descending, avlSorted;
    bstInorder(bst, ascending);
    bstReverseInorder(bst, descending);
    avlInorder(avl, avlSorted);

    bool bstFound = bstSearch(bst, 3);
    bool avlFound = avlSearch(avl, 3);

    int minId = bstMin(bst);
    int maxId = bstMax(bst);

    deleteBST(bst);
    deleteAVL(avl);

    ExprNode* expression =
        buildExpressionTree({"5", "3", "+", "2", "*"});

    double expressionValue =
        expression ? evaluateExpression(expression) : 0;

    deleteExpressionTree(expression);

    double fractionalValue = fractionalKnapsack(
        {{1, 60, 10}, {2, 100, 20}, {3, 120, 30}}, 25
    );

    int zeroOneValue = zeroOneKnapsack(
        {2, 3, 4, 5}, {3, 4, 5, 6}, 5
    );

    ostringstream out;
    out << "{\"adjacencyMatrix\":[";

    for (int i = 0; i < N; i++) {
        if (i) out << ",";
        out << "[";

        for (int j = 0; j < N; j++) {
            if (j) out << ",";
            out << matrix[i][j];
        }

        out << "]";
    }

    out << "],\"bst\":{\"searchDevice3\":"
        << (bstFound ? "true" : "false")
        << ",\"minimumId\":" << minId
        << ",\"maximumId\":" << maxId
        << ",\"ascending\":[";

    for (size_t i = 0; i < ascending.size(); i++) {
        if (i) out << ",";
        out << ascending[i];
    }

    out << "],\"descending\":[";

    for (size_t i = 0; i < descending.size(); i++) {
        if (i) out << ",";
        out << descending[i];
    }

    out << "]},\"avl\":{\"searchDevice3\":"
        << (avlFound ? "true" : "false")
        << ",\"sortedIds\":[";

    for (size_t i = 0; i < avlSorted.size(); i++) {
        if (i) out << ",";
        out << avlSorted[i];
    }

    out << "]},\"expressionTree\":{\"postfix\":\"5 3 + 2 *\",\"result\":"
        << expressionValue
        << "},\"fractionalKnapsack\":{\"capacity\":25,\"maximumValue\":"
        << fractionalValue
        << "},\"zeroOneKnapsack\":{\"capacity\":5,\"maximumValue\":"
        << zeroOneValue << "}}";

    return out.str();
}

int main() {
    initializeNetwork();

    httplib::Server server;

    server.Get("/api/health",
        [](const httplib::Request&, httplib::Response& res) {
            res.set_content("{\"status\":\"ok\"}", "application/json");
        }
    );

    server.Get("/api/network",
        [](const httplib::Request&, httplib::Response& res) {
            res.set_content(networkJson(), "application/json");
        }
    );

    server.Get("/api/dsa/report",
        [](const httplib::Request&, httplib::Response& res) {
            res.set_content(dsaReportJson(), "application/json");
        }
    );

    server.Post("/api/connection/add",
        [](const httplib::Request& req, httplib::Response& res) {
            string from = getValue(req.body, "from");
            string to = getValue(req.body, "to");
            string delayText = getValue(req.body, "delay");

            int a = deviceIndex(from);
            int b = deviceIndex(to);

            if (a < 0 || b < 0 || a == b || delayText.empty()) {
                res.status = 400;
                res.set_content(
                    "{\"error\":\"Invalid devices or delay\"}",
                    "application/json"
                );
                return;
            }

            int delay = stoi(delayText);

            if (delay <= 0 || delay > 1000000) {
                res.status = 400;
                res.set_content(
                    "{\"error\":\"Delay must be positive\"}",
                    "application/json"
                );
                return;
            }

            addEdge(a, b, delay);
            res.set_content(networkJson(), "application/json");
        }
    );

    server.Post("/api/connection/fail",
        [](const httplib::Request& req, httplib::Response& res) {
            int a = deviceIndex(getValue(req.body, "from"));
            int b = deviceIndex(getValue(req.body, "to"));

            if (!setEdgeState(a, b, false)) {
                res.status = 404;
                res.set_content(
                    "{\"error\":\"Connection not found\"}",
                    "application/json"
                );
                return;
            }

            res.set_content(networkJson(), "application/json");
        }
    );

    server.Post("/api/connection/restore",
        [](const httplib::Request& req, httplib::Response& res) {
            int a = deviceIndex(getValue(req.body, "from"));
            int b = deviceIndex(getValue(req.body, "to"));

            if (!setEdgeState(a, b, true)) {
                res.status = 404;
                res.set_content(
                    "{\"error\":\"Connection not found\"}",
                    "application/json"
                );
                return;
            }

            res.set_content(networkJson(), "application/json");
        }
    );

    server.Post("/api/algorithm",
        [](const httplib::Request& req, httplib::Response& res) {
            string algorithm = getValue(req.body, "algorithm");
            int source = deviceIndex(getValue(req.body, "source"));
            int target = deviceIndex(getValue(req.body, "target"));

            res.set_content(
                runAlgorithm(algorithm, source, target),
                "application/json"
            );
        }
    );

    server.Post("/api/reset",
        [](const httplib::Request&, httplib::Response& res) {
            initializeNetwork();
            res.set_content(networkJson(), "application/json");
        }
    );

    server.set_mount_point("/", "./public");

    int port = 8080;
    if (const char* p = getenv("PORT")) port = atoi(p);

    cout << "Network Recovery Simulator server starting...\n";

    if (!server.listen("0.0.0.0", port)) {
        cerr << "Server failed to start.\n";
        return 1;
    }

    return 0;
}