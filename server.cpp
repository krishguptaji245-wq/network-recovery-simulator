
#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <queue>
#include <functional>
#include <limits>
#include <algorithm>
#include <sstream>
#include <regex>
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
    for (int i = 0; i < N; i++) {
        if (names[i] == name) return i;
    }
    return -1;
}

void addEdge(int a, int b, int delay) {
    if (a < 0 || b < 0 || a == b || delay <= 0) return;

    for (auto& e : graph[a]) {
        if (e.to == b) {
            e.delay = delay;
            e.active = true;

            for (auto& reverse : graph[b]) {
                if (reverse.to == a) {
                    reverse.delay = delay;
                    reverse.active = true;
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

    for (auto& e : graph[a]) {
        if (e.to == b) {
            e.active = active;
            found = true;
        }
    }

    for (auto& e : graph[b]) {
        if (e.to == a) {
            e.active = active;
            found = true;
        }
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

    out += "\"";
    return out;
}

string getValue(const string& body, const string& key) {
    regex pattern(
        "\"" + key +
        "\"\\s*:\\s*(?:\"([^\"]*)\"|([0-9]+))"
    );

    smatch match;

    if (regex_search(body, match, pattern)) {
        return match[1].matched
            ? match[1].str()
            : match[2].str();
    }

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
    int active = 0;
    int failed = 0;

    ostringstream out;

    out << "{\"nodes\":[";

    for (int i = 0; i < N; i++) {
        if (i) out << ",";

        out << "{\"id\":"
            << jsonString(names[i])
            << "}";
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
        << jsonString(failed ? "Degraded" : "Healthy")
        << "}";

    return out.str();
}

string runAlgorithm(
    const string& algorithm,
    int source,
    int target
) {
    if (source < 0 || target < 0) {
        return "{\"error\":\"Invalid source or target device\"}";
    }

    vector<int> parent(N, -1);
    vector<int> distance(N, numeric_limits<int>::max());

    // BFS: minimum number of connections
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

    // Dijkstra: minimum total delay
    else if (algorithm == "dijkstra") {
        using Pair = pair<int, int>;

        priority_queue<
            Pair,
            vector<Pair>,
            greater<Pair>
        > pq;

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

    // DFS: traversal from source
    else if (algorithm == "dfs") {
        vector<bool> visited(N, false);
        vector<int> order;

        function<void(int)> visit = [&](int u) {
            visited[u] = true;
            order.push_back(u);

            for (const auto& e : graph[u]) {
                if (e.active && !visited[e.to]) {
                    visit(e.to);
                }
            }
        };

        visit(source);

        ostringstream out;

        out << "{\"found\":true,"
            << "\"algorithm\":\"dfs\","
            << "\"path\":[";

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

    if (distance[target] == numeric_limits<int>::max()) {
        return "{\"found\":false,"
               "\"message\":\"No available route\"}";
    }

    vector<int> path;

    for (int v = target; v != -1; v = parent[v]) {
        path.push_back(v);
    }

    reverse(path.begin(), path.end());

    ostringstream out;

    out << "{\"found\":true,"
        << "\"algorithm\":" << jsonString(algorithm)
        << ",\"path\":[";

    for (size_t i = 0; i < path.size(); i++) {
        if (i) out << ",";
        out << jsonString(names[path[i]]);
    }

    out << "],\"cost\":" << distance[target];

    if (algorithm == "bfs") {
        out << ",\"hops\":" << distance[target];
    } else {
        out << ",\"totalDelay\":" << distance[target];
    }

    out << "}";

    return out.str();
}

int main() {
    initializeNetwork();

    httplib::Server server;

    server.Get(
        "/api/health",
        [](const httplib::Request&, httplib::Response& res) {
            res.set_content(
                "{\"status\":\"ok\"}",
                "application/json"
            );
        }
    );

    server.Get(
        "/api/network",
        [](const httplib::Request&, httplib::Response& res) {
            res.set_content(networkJson(), "application/json");
        }
    );

    server.Post(
        "/api/connection/add",
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

    server.Post(
        "/api/connection/fail",
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

    server.Post(
        "/api/connection/restore",
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

    server.Post(
        "/api/algorithm",
        [](const httplib::Request& req, httplib::Response& res) {
            string algorithm = getValue(req.body, "algorithm");
            int source = deviceIndex(getValue(req.body, "source"));
            int target = deviceIndex(getValue(req.body, "target"));

            string result = runAlgorithm(algorithm, source, target);
            res.set_content(result, "application/json");
        }
    );

    server.Post(
        "/api/reset",
        [](const httplib::Request&, httplib::Response& res) {
            initializeNetwork();
            res.set_content(networkJson(), "application/json");
        }
    );

    server.set_mount_point("/", "./public");

    int port = 8080;

    if (const char* p = getenv("PORT")) {
        port = atoi(p);
    }

    cout << "Network Recovery Simulator server starting...\n";

    if (!server.listen("0.0.0.0", port)) {
        cerr << "Server failed to start.\n";
        return 1;
    }

    return 0;
}
