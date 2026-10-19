
const svg = document.getElementById("networkGraph");
const $ = id => document.getElementById(id);

let edges = [];
let devices = [];
let route = [];

const positions = {
    1: [150, 85],
    2: [350, 55],
    3: [550, 135],
    4: [220, 285],
    5: [475, 285]
};

function log(message) {
    const logElement = $("activityLog");
    if (!logElement) return;

    const li = document.createElement("li");
    li.textContent =
        new Date().toLocaleTimeString() + " — " + message;

    logElement.prepend(li);
}

async function api(url, method = "GET", data = null) {
    const options = {
        method,
        headers: {}
    };

    if (data !== null) {
        options.headers["Content-Type"] = "application/json";
        options.body = JSON.stringify(data);
    }

    const response = await fetch(url, options);
    const text = await response.text();

    let result;

    try {
        result = text ? JSON.parse(text) : {};
    } catch {
        throw new Error("Server returned an invalid response.");
    }

    if (!response.ok) {
        throw new Error(result.error || "API request failed: " + response.status);
    }

    return result;
}

function showError(error) {
    if ($("result")) {
        $("result").textContent = "Error: " + error.message;
    }

    log("API error: " + error.message);
}

// FIX: Backend returns data.devices, not data.nodes.
function syncNetwork(data) {
    if (!Array.isArray(data.devices) || !Array.isArray(data.edges)) {
        throw new Error("Unexpected network API response format.");
    }

    devices = data.devices.map(device =>
        Number(String(device.id).replace("D", ""))
    );

    edges = data.edges.map(edge => ({
        a: Number(String(edge.from).replace("D", "")),
        b: Number(String(edge.to).replace("D", "")),
        delay: Number(edge.delay),
        active: Boolean(edge.active)
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

function populateDevices() {
    ["deviceA", "deviceB"].forEach(id => {
        const select = $(id);
        if (!select) return;

        const previous = Number(select.value);
        select.replaceChildren();

        devices.forEach(device => {
            const option = document.createElement("option");
            option.value = device;
            option.textContent = "Device " + device;
            select.appendChild(option);
        });

        if (devices.includes(previous)) {
            select.value = String(previous);
        }
    });

    const deviceA = $("deviceA");
    const deviceB = $("deviceB");

    if (
        deviceA &&
        deviceB &&
        devices.length > 1 &&
        deviceA.value === deviceB.value
    ) {
        deviceB.value = String(
            devices.find(device => String(device) !== deviceA.value)
        );
    }
}

function updateStats(data) {
    if ($("deviceCount")) {
        $("deviceCount").textContent = devices.length;
    }

    if ($("connectionCount")) {
        $("connectionCount").textContent =
            data.activeConnections ?? edges.filter(e => e.active).length;
    }

    if ($("failedCount")) {
        $("failedCount").textContent =
            data.failedConnections ?? edges.filter(e => !e.active).length;
    }

    if ($("networkStatus")) {
        $("networkStatus").textContent = data.status || "Unknown";
        $("networkStatus").style.color =
            (data.failedConnections ?? 0) > 0
                ? "#ff647c"
                : "#35d49a";
    }
}

function renderGraph() {
    if (!svg) return;

    svg.replaceChildren();

    const ns = "http://www.w3.org/2000/svg";

    function element(tag, attrs = {}) {
        const el = document.createElementNS(ns, tag);

        Object.entries(attrs).forEach(([key, value]) => {
            el.setAttribute(key, value);
        });

        svg.appendChild(el);
        return el;
    }

    edges.forEach(edge => {
        const p1 = positions[edge.a];
        const p2 = positions[edge.b];

        if (!p1 || !p2) return;

        const [x1, y1] = p1;
        const [x2, y2] = p2;

        let color = edge.active ? "#35d49a" : "#ff647c";
        let width = 2;

        for (let i = 0; i < route.length - 1; i++) {
            if (
                (route[i] === edge.a && route[i + 1] === edge.b) ||
                (route[i] === edge.b && route[i + 1] === edge.a)
            ) {
                color = "#ffd166";
                width = 4;
            }
        }

        element("line", {
            x1, y1, x2, y2,
            stroke: color,
            "stroke-width": width,
            "stroke-dasharray": edge.active ? "none" : "7 6"
        });

        const label = element("text", {
            x: (x1 + x2) / 2,
            y: (y1 + y2) / 2 - 8,
            fill: "#d9e5ff",
            "font-size": 12,
            "text-anchor": "middle"
        });

        label.textContent = edge.delay + "ms";
    });

    devices.forEach(device => {
        const point = positions[device];
        if (!point) return;

        const [x, y] = point;
        const onRoute = route.includes(device);

        element("circle", {
            cx: x,
            cy: y,
            r: 25,
            fill: onRoute ? "#765b22" : "#173866",
            stroke: onRoute ? "#ffd166" : "#5795ff",
            "stroke-width": 2
        });

        const label = element("text", {
            x,
            y: y + 5,
            fill: "#ffffff",
            "font-size": 13,
            "font-weight": 700,
            "text-anchor": "middle"
        });

        label.textContent = "D" + device;
    });
}

function getSelected() {
    const a = Number($("deviceA")?.value);
    const b = Number($("deviceB")?.value);

    if (!devices.includes(a) || !devices.includes(b)) {
        $("result").textContent = "Please select two devices.";
        return null;
    }

    if (a === b) {
        $("result").textContent =
            "Please select two different devices.";
        return null;
    }

    return [a, b];
}

function deviceName(device) {
    return "D" + device;
}

async function refreshAfterChange(message) {
    const data = await api("/api/network");

    route = [];
    syncNetwork(data);
    log(message);
}

async function addConnection() {
    const selected = getSelected();
    if (!selected) return;

    const [a, b] = selected;
    const delay = Number($("delay").value);

    if (!Number.isInteger(delay) || delay <= 0) {
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
            "Connection updated successfully.";
    } catch (error) {
        showError(error);
    }
}

async function failConnection() {
    const selected = getSelected();
    if (!selected) return;

    const [a, b] = selected;

    try {
        await api("/api/connection/fail", "POST", {
            from: deviceName(a),
            to: deviceName(b)
        });

        await refreshAfterChange(
            "Connection failed: " + a + " ↔ " + b
        );

        $("result").textContent = "Connection failed successfully.";
    } catch (error) {
        showError(error);
    }
}

async function restoreConnection() {
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

        $("result").textContent = "Connection restored successfully.";
    } catch (error) {
        showError(error);
    }
}

async function runAlgorithm() {
    const selected = getSelected();
    if (!selected) return;

    const [a, b] = selected;
    const algorithm = $("algorithm").value;

    try {
        if (algorithm === "dfs") {
            const adjacency = {};

            devices.forEach(device => {
                adjacency[device] = [];
            });

            edges.filter(edge => edge.active).forEach(edge => {
                adjacency[edge.a].push(edge.b);
                adjacency[edge.b].push(edge.a);
            });

            const visited = new Set();
            const order = [];

            function visit(device) {
                visited.add(device);
                order.push(device);

                adjacency[device].forEach(next => {
                    if (!visited.has(next)) {
                        visit(next);
                    }
                });
            }

            visit(a);

            route = order;
            renderGraph();

            $("result").textContent =
                "DFS traversal from Device " + a + ": " +
                order.map(deviceName).join(" → ");

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
            route = result.path.map(name =>
                Number(String(name).replace("D", ""))
            );

            if (algorithm === "bfs") {
                $("result").textContent =
                    "BFS minimum-hop route: " +
                    result.path.join(" → ") +
                    " | Hops: " + result.hops;
            } else {
                $("result").textContent =
                    "Dijkstra minimum-delay route: " +
                    result.path.join(" → ") +
                    " | Total delay: " + result.totalDelay + " ms";
            }
        }

        renderGraph();
        log(algorithm.toUpperCase() + " executed.");
    } catch (error) {
        showError(error);
    }
}

async function resetNetwork() {
    try {
        await api("/api/reset", "POST");
        route = [];
        await loadNetwork();

        $("result").textContent = "Default network restored.";
        log("Network reset by C++ backend.");
    } catch (error) {
        showError(error);
    }
}

$("addBtn")?.addEventListener("click", addConnection);
$("failBtn")?.addEventListener("click", failConnection);
$("restoreBtn")?.addEventListener("click", restoreConnection);
$("runBtn")?.addEventListener("click", runAlgorithm);
$("resetBtn")?.addEventListener("click", resetNetwork);

loadNetwork();
