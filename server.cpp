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

    // Extra DSA report endpoint; the dashboard does not need to display it.
    server.Get(
        "/api/dsa/report",
        [](const httplib::Request&, httplib::Response& res) {
            res.set_content(dsaReportJson(), "application/json");
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
