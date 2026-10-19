
#include "httplib.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <queue>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

struct Edge {
    int to, delay;
    bool active;
};

struct Device {
    string id, name;
};

httplib::Server server;

vector<Device> devices = {
    {"D1", "Device 1"}, {"D2", "Device 2"},
    {"D3", "Device 3"}, {"D4", "Device 4"},
    {"D5", "Device 5"}
};

vector<vector<Edge>> graph;

string getValue(const string& body, const string& key) {
    string token = key + "=";
    size_t p = body.find(token);
    if (p == string::npos) return "";
    p += token.size();

    size_t e = body.find('&', p);
    string s = body.substr(p, e == string::npos ? string::npos : e - p);
    string out;

    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') {
            out += ' ';
        } else if (s[i] == '%' && i + 2 < s.size()) {
            auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };

            int a = hex(s[i + 1]);
            int b = hex(s[i + 2]);

            if (a >= 0 && b >= 0) {
                out += char(a * 16 + b);
                i += 2;
            } else {
                out += s[i];
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

int deviceIndex(const string& id) {
    for (size_t i = 0; i < devices.size(); ++i)
        if (devices[i].id == id) return int(i);
    return -1;
}

void addEdge(int a, int b, int delay, bool active = true) {
    for (auto& e : graph[a]) {
        if (e.to == b) {
            e.delay = delay;
            e.active = active;

            for (auto& r : graph[b]) {
                if (r.to == a) {
                    r.delay = delay;
                    r.active = active;
                }
            }
            return;
        }
    }

    graph[a].push_back({b, delay, active});
    graph[b].push_back({a, delay, active});
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

void initializeNetwork() {
    graph.assign(devices.size(), {});

    addEdge(0, 1, 4, false);
    addEdge(0, 2, 2);
    addEdge(1, 2, 1);
    addEdge(1, 3, 5);
    addEdge(2, 3, 8);
    addEdge(3, 4, 3);
}

string networkJson() {
    int active = 0, failed = 0;
    ostringstream edges;
    bool first = true;

    for (size_t i = 0; i < graph.size(); ++i) {
        for (const auto& e : graph[i]) {
            if (int(i) >= e.to) continue;

            if (e.active) ++active;
            else ++failed;

            if (!first) edges << ',';
            first = false;

            edges << "{\"from\":\"" << devices[i].id
                  << "\",\"to\":\"" << devices[e.to].id
                  << "\",\"delay\":" << e.delay
                  << ",\"active\":"
                  << (e.active ? "true" : "false") << '}';
        }
    }

    ostringstream out;
    out << "{\"devices\":[";

    for (size_t i = 0; i < devices.size(); ++i) {
        if (i) out << ',';

        out << "{\"id\":\"" << devices[i].id
            << "\",\"name\":\"" << devices[i].name << "\"}";
    }

    out << "],\"edges\":[" << edges.str()
        << "],\"activeConnections\":" << active
        << ",\"failedConnections\":" << failed
        << ",\"status\":\""
        << (failed ? "Degraded" : "Healthy") << "\"}";

    return out.str();
}

vector<int> bfs(int s, int t) {
    vector<int> parent(devices.size(), -1);
    vector<bool> seen(devices.size(), false);
    queue<int> q;

    seen[s] = true;
    q.push(s);

    while (!q.empty()) {
        int u = q.front();
        q.pop();

        if (u == t) break;

        for (const auto& e : graph[u]) {
            if (e.active && !seen[e.to]) {
                seen[e.to] = true;
                parent[e.to] = u;
                q.push(e.to);
            }
        }
    }

    if (!seen[t]) return {};

    vector<int> path;
    for (int v = t; v != -1; v = parent[v])
        path.push_back(v);

    reverse(path.begin(), path.end());
    return path;
}

vector<int> dfs(int s, int t) {
    vector<int> parent(devices.size(), -1);
    vector<bool> seen(devices.size(), false);
    vector<int> st = {s};

    seen[s] = true;

    while (!st.empty()) {
        int u = st.back();
        st.pop_back();

        if (u == t) break;

        for (auto it = graph[u].rbegin(); it != graph[u].rend(); ++it) {
            if (it->active && !seen[it->to]) {
                seen[it->to] = true;
                parent[it->to] = u;
                st.push_back(it->to);
            }
        }
    }

    if (!seen[t]) return {};

    vector<int> path;
    for (int v = t; v != -1; v = parent[v])
        path.push_back(v);

    reverse(path.begin(), path.end());
    return path;
}

pair<vector<int>, int> dijkstra(int s, int t) {
    const int INF = numeric_limits<int>::max() / 4;

    vector<int> dist(devices.size(), INF);
    vector<int> parent(devices.size(), -1);

    using P = pair<int, int>;
    priority_queue<P, vector<P>, greater<P>> q;

    dist[s] = 0;
    q.push({0, s});

    while (!q.empty()) {
        auto [d, u] = q.top();
        q.pop();

        if (d != dist[u]) continue;

        for (const auto& e : graph[u]) {
            if (e.active && dist[e.to] > d + e.delay) {
                dist[e.to] = d + e.delay;
                parent[e.to] = u;
                q.push({dist[e.to], e.to});
            }
        }
    }

    if (dist[t] == INF) return {{}, -1};

    vector<int> path;
    for (int v = t; v != -1; v = parent[v])
        path.push_back(v);

    reverse(path.begin(), path.end());
    return {path, dist[t]};
}

string pathJson(const vector<int>& path) {
    ostringstream out;
    out << '[';

    for (size_t i = 0; i < path.size(); ++i) {
        if (i) out << ',';
        out << '"' << devices[path[i]].id << '"';
    }

    out << ']';
    return out.str();
}

string runAlgorithm(const string& algorithm, int s, int t) {
    if (s < 0 || t < 0)
        return "{\"error\":\"Invalid source or target\"}";

    vector<int> path;
    int delay = -1;
    string name = algorithm;

    if (algorithm == "BFS" || algorithm == "bfs") {
        path = bfs(s, t);
        name = "BFS";
    } else if (algorithm == "DFS" || algorithm == "dfs") {
        path = dfs(s, t);
        name = "DFS";
    } else if (algorithm == "Dijkstra" ||
               algorithm == "DIJKSTRA" ||
               algorithm == "dijkstra") {
        auto result = dijkstra(s, t);
        path = result.first;
        delay = result.second;
        name = "Dijkstra";
    } else {
        return "{\"error\":\"Unknown algorithm\"}";
    }

    if (path.empty()) {
        return "{\"algorithm\":\"" + name +
               "\",\"found\":false,\"path\":[],"
               "\"message\":\"No active path found\"}";
    }

    if (delay < 0) {
        delay = 0;

        for (size_t i = 1; i < path.size(); ++i) {
            for (const auto& e : graph[path[i - 1]]) {
                if (e.to == path[i] && e.active) {
                    delay += e.delay;
                    break;
                }
            }
        }
    }

    return "{\"algorithm\":\"" + name +
           "\",\"found\":true,\"path\":" + pathJson(path) +
           ",\"totalDelay\":" + to_string(delay) + "}";
}

string dsaReportJson() {
    return "{\"note\":\"Live recovery uses graph algorithms; "
           "supplementary DSA examples are separate.\","
           "\"algorithms\":[\"BFS\",\"DFS\",\"Dijkstra\"],"
           "\"graphRepresentation\":\"Adjacency list\"}";
}

int main() {
    initializeNetwork();

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
            int a = deviceIndex(getValue(req.body, "from"));
            int b = deviceIndex(getValue(req.body, "to"));
            string dt = getValue(req.body, "delay");

            if (a < 0 || b < 0 || a == b || dt.empty()) {
                res.status = 400;
                res.set_content(
                    "{\"error\":\"Invalid devices or delay\"}",
                    "application/json"
                );
                return;
            }

            int delay;
            try {
                size_t used = 0;
                delay = stoi(dt, &used);
                if (used != dt.size()) throw invalid_argument("delay");
            } catch (...) {
                res.status = 400;
                res.set_content(
                    "{\"error\":\"Delay must be a positive integer\"}",
                    "application/json"
                );
                return;
            }

            if (delay <= 0 || delay > 1000000) {
                res.status = 400;
                res.set_content(
                    "{\"error\":\"Invalid delay\"}",
                    "application/json"
                );
                return;
            }

            addEdge(a, b, delay, true);
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

            string result = runAlgorithm(algorithm, source, target);

            if (result.find("\"error\"") != string::npos)
                res.status = 400;

            res.set_content(result, "application/json");
        }
    );

    server.Post("/api/reset",
        [](const httplib::Request&, httplib::Response& res) {
            initializeNetwork();
            res.set_content(networkJson(), "application/json");
        }
    );

    server.set_mount_point("/", "./public");

    int port = 10000;

    if (const char* p = getenv("PORT")) {
        try {
            port = stoi(p);
        } catch (...) {
            port = 10000;
        }
    }

    cout << "Network Recovery Simulator starting on port "
         << port << "...\n";

    if (!server.listen("0.0.0.0", port)) {
        cerr << "Server failed to start.\n";
        return 1;
    }

    return 0;
}
