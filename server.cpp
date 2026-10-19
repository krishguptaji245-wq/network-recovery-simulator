
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <queue>
#include <stack>
#include <algorithm>
#include <limits>
#include <cstdlib>
#include <cctype>
#include "httplib.h"

using namespace std;
using namespace httplib;

const int N = 5;
const int INF = numeric_limits<int>::max() / 4;

struct Edge {
    int to;
    int delay;
    bool active;
};

struct Device {
    string id;
    string name;
};

vector<Device> devices = {
    {"D1", "Device 1"},
    {"D2", "Device 2"},
    {"D3", "Device 3"},
    {"D4", "Device 4"},
    {"D5", "Device 5"}
};

vector<vector<Edge>> graph(N);

// ---------- JSON / REQUEST HELPERS ----------

string jsonEscape(const string& value) {
    string result;

    for (char c : value) {
        if (c == '"') result += "\\\"";
        else if (c == '\\') result += "\\\\";
        else if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else if (c == '\t') result += "\\t";
        else result += c;
    }

    return result;
}

string getValue(const string& body, const string& key) {
    // Read JSON string or numeric values.
    string token = "\"" + key + "\"";
    size_t p = body.find(token);

    if (p != string::npos) {
        p = body.find(':', p);
        if (p == string::npos) return "";

        ++p;

        while (p < body.size() &&
               isspace(static_cast<unsigned char>(body[p]))) {
            ++p;
        }

        if (p >= body.size()) return "";

        if (body[p] == '"') {
            ++p;
            string result;

            while (p < body.size()) {
                if (body[p] == '\\' && p + 1 < body.size()) {
                    result += body[p + 1];
                    p += 2;
                } else if (body[p] == '"') {
                    return result;
                } else {
                    result += body[p++];
                }
            }

            return "";
        }

        size_t end = body.find_first_of(",}", p);
        string value = body.substr(
            p, end == string::npos ? string::npos : end - p
        );

        size_t first = value.find_first_not_of(" \t\r\n");
        if (first == string::npos) return "";

        size_t last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    // Also support form-encoded values such as source=D1.
    string formToken = key + "=";
    p = body.find(formToken);

    if (p == string::npos) return "";

    p += formToken.size();

    size_t end = body.find('&', p);
    string value = body.substr(
        p, end == string::npos ? string::npos : end - p
    );

    string result;

    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '+') {
            result += ' ';
        } else if (value[i] == '%' && i + 2 < value.size()) {
            try {
                result += static_cast<char>(
                    stoi(value.substr(i + 1, 2), nullptr, 16)
                );
                i += 2;
            } catch (...) {
                result += value[i];
            }
        } else {
            result += value[i];
        }
    }

    return result;
}

int deviceIndex(string id) {
    // Accept D1, d1, 1, and Device 1.
    if (!id.empty() && (id[0] == 'D' || id[0] == 'd')) {
        id.erase(0, 1);
    }

    if (id.size() >= 7 && id.substr(0, 7) == "Device ") {
        id = id.substr(7);
    }

    try {
        size_t used = 0;
        int number = stoi(id, &used);

        if (used != id.size()) return -1;
        if (number < 1 || number > N) return -1;

        return number - 1;
    } catch (...) {
        return -1;
    }
}

string deviceId(int index) {
    if (index < 0 || index >= N) return "";
    return devices[index].id;
}

// ---------- NETWORK MANAGEMENT ----------

void addEdge(int a, int b, int delay, bool active = true) {
    if (a < 0 || a >= N || b < 0 || b >= N || a == b) {
        return;
    }

    for (auto& edge : graph[a]) {
        if (edge.to == b) {
            edge.delay = delay;
            edge.active = active;

            for (auto& reverse : graph[b]) {
                if (reverse.to == a) {
                    reverse.delay = delay;
                    reverse.active = active;
                }
            }

            return;
        }
    }

    graph[a].push_back({b, delay, active});
    graph[b].push_back({a, delay, active});
}

bool setEdgeState(int a, int b, bool active) {
    if (a < 0 || a >= N || b < 0 || b >= N || a == b) {
        return false;
    }

    bool found = false;

    for (auto& edge : graph[a]) {
        if (edge.to == b) {
            edge.active = active;
            found = true;
        }
    }

    for (auto& edge : graph[b]) {
        if (edge.to == a) {
            edge.active = active;
            found = true;
        }
    }

    return found;
}

void initializeNetwork() {
    for (auto& list : graph) {
        list.clear();
    }

    addEdge(0, 1, 5);
    addEdge(0, 3, 8);
    addEdge(1, 2, 3);
    addEdge(1, 4, 7);
    addEdge(2, 4, 4);
    addEdge(3, 4, 6);
}

// ---------- NETWORK JSON ----------

string networkJson() {
    int activeConnections = 0;
    int failedConnections = 0;

    ostringstream out;

    out << "{\"devices\":[";

    for (int i = 0; i < N; ++i) {
        if (i > 0) out << ",";

        out << "{\"id\":\"" << devices[i].id
            << "\",\"name\":\"" << devices[i].name << "\"}";
    }

    out << "],\"nodes\":[";

    for (int i = 0; i < N; ++i) {
        if (i > 0) out << ",";

        out << "{\"id\":\"" << devices[i].id
            << "\",\"name\":\"" << devices[i].name << "\"}";
    }

    out << "],\"edges\":[";

    bool first = true;

    for (int a = 0; a < N; ++a) {
        for (const auto& edge : graph[a]) {
            // Report each undirected connection only once.
            if (a >= edge.to) continue;

            if (!first) out << ",";
            first = false;

            out << "{\"from\":\"" << devices[a].id
                << "\",\"to\":\"" << devices[edge.to].id
                << "\",\"delay\":" << edge.delay
                << ",\"active\":"
                << (edge.active ? "true" : "false") << "}";

            if (edge.active) ++activeConnections;
            else ++failedConnections;
        }
    }

    out << "],\"totalDevices\":" << N
        << ",\"activeConnections\":" << activeConnections
        << ",\"failedConnections\":" << failedConnections
        << ",\"status\":\""
        << (failedConnections > 0 ? "Degraded" : "Connected")
        << "\"}";

    return out.str();
}

// ---------- BFS: MINIMUM HOPS ----------

vector<int> bfs(int source, int target) {
    vector<int> parent(N, -1);
    vector<bool> visited(N, false);
    queue<int> q;

    visited[source] = true;
    q.push(source);

    while (!q.empty()) {
        int current = q.front();
        q.pop();

        if (current == target) break;

        for (const auto& edge : graph[current]) {
            if (edge.active && !visited[edge.to]) {
                visited[edge.to] = true;
                parent[edge.to] = current;
                q.push(edge.to);
            }
        }
    }

    if (!visited[target]) return {};

    vector<int> path;

    for (int v = target; v != -1; v = parent[v]) {
        path.push_back(v);
    }

    reverse(path.begin(), path.end());
    return path;
}

// ---------- DFS: TRAVERSAL ----------

void dfsVisit(
    int current,
    vector<bool>& visited,
    vector<int>& path
) {
    visited[current] = true;
    path.push_back(current);

    for (const auto& edge : graph[current]) {
        if (edge.active && !visited[edge.to]) {
            dfsVisit(edge.to, visited, path);
        }
    }
}

vector<int> dfs(int source, int target) {
    vector<bool> visited(N, false);
    vector<int> path;

    dfsVisit(source, visited, path);

    // DFS returns traversal order, including when target is unreachable.
    return path;
}

// ---------- DIJKSTRA: MINIMUM DELAY ----------

vector<int> dijkstra(int source, int target, int& totalDelay) {
    vector<int> distance(N, INF);
    vector<int> parent(N, -1);

    priority_queue<
        pair<int, int>,
        vector<pair<int, int>>,
        greater<pair<int, int>>
    > pq;

    distance[source] = 0;
    pq.push({0, source});

    while (!pq.empty()) {
        auto [currentDistance, current] = pq.top();
        pq.pop();

        if (currentDistance != distance[current]) continue;

        for (const auto& edge : graph[current]) {
            if (!edge.active) continue;

            int nextDistance = currentDistance + edge.delay;

            if (nextDistance < distance[edge.to]) {
                distance[edge.to] = nextDistance;
                parent[edge.to] = current;
                pq.push({nextDistance, edge.to});
            }
        }
    }

    if (distance[target] == INF) {
        totalDelay = -1;
        return {};
    }

    totalDelay = distance[target];

    vector<int> path;

    for (int v = target; v != -1; v = parent[v]) {
        path.push_back(v);
    }

    reverse(path.begin(), path.end());
    return path;
}

// ---------- ALGORITHM RESPONSE ----------

string runAlgorithm(
    const string& algorithm,
    int source,
    int target
) {
    if (source < 0 || source >= N ||
        target < 0 || target >= N) {
        return "{\"found\":false,\"error\":\"Invalid source or target\"}";
    }

    string name = algorithm;
    transform(name.begin(), name.end(), name.begin(),
        [](unsigned char c) { return static_cast<char>(tolower(c)); });

    vector<int> path;
    int totalDelay = -1;

    if (name == "bfs") {
        path = bfs(source, target);
    } else if (name == "dfs") {
        path = dfs(source, target);
    } else if (name == "dijkstra") {
        path = dijkstra(source, target, totalDelay);
    } else {
        return "{\"found\":false,\"error\":\"Unknown algorithm\"}";
    }

    if (path.empty()) {
        return "{\"found\":false,\"message\":\"No route available.\"}";
    }

    ostringstream out;

    out << "{\"found\":true,\"algorithm\":\"" << name << "\",\"path\":[";

    for (size_t i = 0; i < path.size(); ++i) {
        if (i > 0) out << ",";
        out << "\"" << deviceId(path[i]) << "\"";
    }

    out << "],\"hops\":" << static_cast<int>(path.size()) - 1
        << ",\"totalDelay\":" << totalDelay << "}";

    return out.str();
}

// ---------- MAIN SERVER ----------

int main() {
    initializeNetwork();

    Server server;

    // Serve frontend files.
    server.set_mount_point("/", "./public");

    server.Get("/api/health",
        [](const Request&, Response& res) {
            res.set_content(
                "{\"status\":\"ok\"}",
                "application/json"
            );
        }
    );

    server.Get("/api/network",
        [](const Request&, Response& res) {
            res.set_content(networkJson(), "application/json");
        }
    );

    server.Post("/api/connection/add",
        [](const Request& req, Response& res) {
            int a = deviceIndex(getValue(req.body, "from"));
            int b = deviceIndex(getValue(req.body, "to"));

            string delayText = getValue(req.body, "delay");
            int delay = 0;

            try {
                size_t used = 0;
                delay = stoi(delayText, &used);

                if (used != delayText.size()) {
                    throw invalid_argument("invalid delay");
                }
            } catch (...) {
                res.status = 400;
                res.set_content(
                    "{\"error\":\"Invalid connection delay\"}",
                    "application/json"
                );
                return;
            }

            if (a < 0 || b < 0 || a == b || delay <= 0) {
                res.status = 400;
                res.set_content(
                    "{\"error\":\"Invalid devices or delay\"}",
                    "application/json"
                );
                return;
            }

            addEdge(a, b, delay, true);
            res.set_content(networkJson(), "application/json");
        }
    );

    server.Post("/api/connection/fail",
        [](const Request& req, Response& res) {
            int a = deviceIndex(getValue(req.body, "from"));
            int b = deviceIndex(getValue(req.body, "to"));

            if (a < 0 || b < 0 || a == b) {
                res.status = 400;
                res.set_content(
                    "{\"error\":\"Invalid devices\"}",
                    "application/json"
                );
                return;
            }

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
        [](const Request& req, Response& res) {
            int a = deviceIndex(getValue(req.body, "from"));
            int b = deviceIndex(getValue(req.body, "to"));

            if (a < 0 || b < 0 || a == b) {
                res.status = 400;
                res.set_content(
                    "{\"error\":\"Invalid devices\"}",
                    "application/json"
                );
                return;
            }

            // Restore an existing connection, even if it is failed.
            if (!setEdgeState(a, b, true)) {
                res.status = 404;
                res.set_content(
                    "{\"error\":\"Connection not found. Add it first.\"}",
                    "application/json"
                );
                return;
            }

            res.set_content(networkJson(), "application/json");
        }
    );

    server.Post("/api/algorithm",
        [](const Request& req, Response& res) {
            string algorithm = getValue(req.body, "algorithm");
            int source = deviceIndex(getValue(req.body, "source"));
            int target = deviceIndex(getValue(req.body, "target"));

            string result = runAlgorithm(algorithm, source, target);

            if (result.find("\"error\"") != string::npos) {
                res.status = 400;
            }

            res.set_content(result, "application/json");
        }
    );

    server.Post("/api/reset",
        [](const Request&, Response& res) {
            initializeNetwork();
            res.set_content(networkJson(), "application/json");
        }
    );

    const char* portText = getenv("PORT");
    int port = portText ? atoi(portText) : 8080;

    if (port <= 0 || port > 65535) {
        port = 8080;
    }

    cout << "Network Recovery Simulator running on port "
         << port << endl;

    if (!server.listen("0.0.0.0", port)) {
        cerr << "Failed to start server on port " << port << endl;
        return 1;
    }

    return 0;
}
