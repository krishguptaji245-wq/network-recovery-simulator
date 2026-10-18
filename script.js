
const svg = document.getElementById("networkGraph");
const $ = id => document.getElementById(id);

let edges = [];
let devices = [1, 2, 3, 4, 5];
let route = [];

const positions = {
    1: [150, 85],
    2: [350, 55],
    3: [550, 135],
    4: [220, 285],
    5: [475, 285]
};

function log(message) {
    const li = document.createElement("li");
    li.textContent = new Date().toLocaleTimeString() + " — " + message;
    $("activityLog").prepend(li);
}

async function api(url, method = "GET", data = null) {
    const options = { method, headers: {} };

    if (data !== null) {
        options.headers["Content-Type"] = "application/json";
        options.body = JSON.stringify(data);
    }

    const response = await fetch(url, options);
    const result = await response.json();

    if (!response.ok) {
        throw new Error(result.error || "API request failed");
    }

    return result;
}

function showError(error) {
    $("result").textContent = "Error: " + error.message;
    log("API error: " + error.message);
}

function syncNetwork(data) {
    devices = data.nodes.map(node => Number(node.id.slice(1)));

    edges = data.edges.map(edge => ({
        a: Number(edge.from.slice(1)),
        b: Number(edge.to.slice(1)),
        delay: edge.delay,
        active: edge.active
    }));

    populateDevices();
    updateStats(data);
    renderGraph();
}

async function loadNetwork() {
    try {
        const data = await api("/api/network");
        syncNetwork(data);
        log("Network loaded from C++ backend.");
    } catch (error) {
        showError(error);
    }
}

function getEdge(a, b) {
    return edges.find(e =>
        (e.a === a && e.b === b) ||
        (e.a === b && e.b === a)
    );
}

function renderGraph() {
    svg.replaceChildren();
    const ns = "http://www.w3.org/2000/svg";

    function element(tag, attrs = {}) {
        const el = document.createElementNS(ns, tag);
        Object.entries(attrs).forEach(([key, value]) =>
            el.setAttribute(key, value)
        );
        svg.appendChild(el);
        return el;
    }

    edges.forEach(e => {
        const [x1, y1] = positions[e.a];
        const [x2, y2] = positions[e.b];

        let color = e.active ? "#35d49a" : "#ff647c";
        let width = 2;

        for (let i = 0; i < route.length - 1; i++) {
            if (
                (route[i] === e.a && route[i + 1] === e.b) ||
                (route[i] === e.b && route[i + 1] === e.a)
            ) {
                color = "#ffd166";
                width = 4;
            }
        }

        element("line", {
            x1, y1, x2, y2,
            stroke: color,
            "stroke-width": width,
            "stroke-dasharray": e.active ? "none" : "7 6"
        });

        const label = element("text", {
            x: (x1 + x2) / 2,
            y: (y1 + y2) / 2 - 8,
            fill: "#d9e5ff",
            "font-size": 12,
            "text-anchor": "middle"
        });
        label.textContent = e.delay + "ms";
    });

    devices.forEach(v => {
        const [x, y] = positions[v];

        element("circle", {
            cx: x, cy: y, r: 25,
            fill: route.includes(v) ? "#765b22" : "#173866",
            stroke: route.includes(v) ? "#ffd166" : "#5795ff",
            "stroke-width": 2
        });

        const label = element("text", {
            x, y: y + 5,
            fill: "#ffffff",
            "font-size": 13,
            "font-weight": 700,
            "text-anchor": "middle"
        });
        label.textContent = "D" + v;
    });
}

function updateStats(data) {
    $("deviceCount").textContent = data.totalDevices;
    $("connectionCount").textContent = data.activeConnections;
    $("failedCount").textContent = data.failedConnections;

    $("networkStatus").textContent = data.status;
    $("networkStatus").style.color =
        data.failedConnections ? "#ff647c" : "#35d49a";
}

function populateDevices() {
    ["deviceA", "deviceB"].forEach(id => {
        const select = $(id);
        const previous = Number(select.value);

        select.replaceChildren();

        devices.forEach(v => {
            const option = document.createElement("option");
            option.value = v;
            option.textContent = "Device " + v;
            select.appendChild(option);
        });

        if (devices.includes(previous)) select.value = previous;
    });

    if (devices.length > 1 &&
        $("deviceA").value === $("deviceB").value) {
        $("deviceB").selectedIndex = 1;
    }
}

function getSelected() {
    const a = Number($("deviceA").value);
    const b = Number($("deviceB").value);

    if (a === b) {
        $("result").textContent =
            "Please select two different devices.";
        return null;
    }

    return [a, b];
}

function deviceName(n) {
    return "D" + n;
}

async function refreshAfterChange(message) {
    const data = await api("/api/network");
    syncNetwork(data);
    route = [];
    renderGraph();
    log(message);
}

$("addBtn").addEventListener("click", async () => {
    const selected = getSelected();
    if (!selected) return;

    const [a, b] = selected;
    const delay = Number($("delay").value);

    if (!Number.isFinite(delay) || delay <= 0 ||
        !Number.isInteger(delay)) {
        $("result").textContent =
            "Enter a positive whole-number delay.";
        return;
    }

    try {
        await api("/api/connection/add", "POST", {
            from: deviceName(a),
            to: deviceName(b),
            delay
        });

        await refreshAfterChange(
            "Connection added/updated: " + a + " ↔ " + b
        );
        $("result").textContent =
            "Connection updated in C++ backend.";
    } catch (error) {
        showError(error);
    }
});

$("failBtn").addEventListener("click", async () => {
    const selected = getSelected();
    if (!selected) return;

    const [a, b] = selected;

    try {
        await api("/api/connection/fail", "POST", {
            from: deviceName(a),
            to: deviceName(b)
        });

        await refreshAfterChange("Connection failed: " + a + " ↔ " + b);
        $("result").textContent =
            "Connection failed in C++ backend.";
    } catch (error) {
        showError(error);
    }
});

$("restoreBtn").addEventListener("click", async () => {
    const selected = getSelected();
    if (!selected) return;

    const [a, b] = selected;

    try {
        await api("/api/connection/restore", "POST", {
            from: deviceName(a),
            to: deviceName(b)
        });

        await refreshAfterChange(
            "Connection restored: " + a + " ↔ " + b
        );
        $("result").textContent =
            "Connection restored in C++ backend.";
    } catch (error) {
        showError(error);
    }
});

async function runAlgorithm() {
    const selected = getSelected();
    if (!selected) return;

    const [a, b] = selected;
    const algorithm = $("algorithm").value;

    try {
        if (algorithm === "dfs") {
            // DFS remains in JavaScript for now.
            const adjacency = {};
            devices.forEach(v => adjacency[v] = []);

            edges.filter(e => e.active).forEach(e => {
                adjacency[e.a].push(e.b);
                adjacency[e.b].push(e.a);
            });

            const visited = new Set();
            const order = [];

            function visit(v) {
                visited.add(v);
                order.push(v);
                adjacency[v].forEach(next => {
                    if (!visited.has(next)) visit(next);
                });
            }

            visit(a);
            route = order;
            $("result").textContent =
                "DFS traversal from Device " + a + ": " +
                order.map(deviceName).join(" → ");

            renderGraph();
            log("DFS executed from Device " + a);
            return;
        }

        const result = await api("/api/algorithm", "POST", {
            algorithm,
            source: deviceName(a),
            target: deviceName(b)
        });

        if (!result.found) {
            route = [];
            $("result").textContent =
                result.message || "No route available.";
        } else {
            route = result.path.map(name => Number(name.slice(1)));

            if (algorithm === "bfs") {
                $("result").textContent =
                    "C++ BFS minimum-hop route: " +
                    result.path.join(" → ") +
                    " | Hops: " + result.hops;
            } else {
                $("result").textContent =
                    "C++ Dijkstra minimum-delay route: " +
                    result.path.join(" → ") +
                    " | Total delay: " + result.totalDelay + " ms";
            }
        }

        renderGraph();
        log(algorithm.toUpperCase() + " executed by C++ backend.");
    } catch (error) {
        showError(error);
    }
}

$("runBtn").addEventListener("click", runAlgorithm);

$("resetBtn").addEventListener("click", async () => {
    try {
        await api("/api/reset", "POST");
        await loadNetwork();
        route = [];
        renderGraph();
        log("Network reset by C++ backend.");
        $("result").textContent = "Default network restored.";
    } catch (error) {
        showError(error);
    }
});

loadNetwork();
