#include <libpq-fe.h>

#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

struct NodeConfig {
    std::string name;
    std::string host;
    std::string database;
};

struct NodeStatus {
    std::string name;
    bool healthy;
    std::string detail;
};

NodeStatus check_node(const NodeConfig& node) {
    const char* password = std::getenv("POSTGRES_PASSWORD");
    const char* user = std::getenv("POSTGRES_USER");

    const std::string connection_info =
        "host=" + node.host +
        " port=5432 dbname=" + node.database +
        " user=" + (user == nullptr ? "postgres" : user) +
        " password=" + (password == nullptr ? "postgres" : password) +
        " connect_timeout=2";

    PGconn* connection = PQconnectdb(connection_info.c_str());
    if (connection == nullptr || PQstatus(connection) != CONNECTION_OK) {
        const std::string detail = connection == nullptr
            ? "connection allocation failed"
            : PQerrorMessage(connection);
        if (connection != nullptr) {
            PQfinish(connection);
        }
        return {node.name, false, detail};
    }

    PGresult* result = PQexec(connection, "SELECT 1");
    const bool query_succeeded = result != nullptr && PQresultStatus(result) == PGRES_TUPLES_OK;
    const std::string detail = query_succeeded ? "ready" : PQerrorMessage(connection);

    if (result != nullptr) {
        PQclear(result);
    }
    PQfinish(connection);
    return {node.name, query_succeeded, detail};
}

void print_status(const std::vector<NodeStatus>& statuses) {
    for (const NodeStatus& status : statuses) {
        std::cout << "node=" << status.name
                  << " status=" << (status.healthy ? "ACTIVE" : "FAILED")
                  << " detail=" << status.detail << '\n';
    }

    std::cout << "route users= node1 standby=node2\n"
              << "route products_inventory= node2 standby=node3\n"
              << "route orders_payments= node3 standby=node1\n";
}

void print_demo_snapshot(const std::vector<NodeConfig>& nodes) {
    for (const NodeConfig& node : nodes) {
        const char* password = std::getenv("POSTGRES_PASSWORD");
        const char* user = std::getenv("POSTGRES_USER");
        const std::string connection_info =
            "host=" + node.host +
            " port=5432 dbname=" + node.database +
            " user=" + (user == nullptr ? "postgres" : user) +
            " password=" + (password == nullptr ? "postgres" : password) +
            " connect_timeout=2";

        PGconn* connection = PQconnectdb(connection_info.c_str());
        if (connection == nullptr || PQstatus(connection) != CONNECTION_OK) {
            std::cout << "store_view node=" << node.name << " status=unavailable\n";
            if (connection != nullptr) {
                PQfinish(connection);
            }
            continue;
        }

        const std::string query = node.name == "node1"
            ? "SELECT 'customers' AS area, COUNT(*)::text AS count FROM users UNION ALL SELECT 'carts', COUNT(*)::text FROM carts UNION ALL SELECT 'replicated orders', COUNT(*)::text FROM orders"
            : node.name == "node2"
                ? "SELECT 'products' AS area, COUNT(*)::text AS count FROM products UNION ALL SELECT 'inventory items', COUNT(*)::text FROM inventory UNION ALL SELECT 'replicated customers', COUNT(*)::text FROM users"
                : "SELECT 'orders' AS area, COUNT(*)::text AS count FROM orders UNION ALL SELECT 'payments', COUNT(*)::text FROM payments UNION ALL SELECT 'replicated products', COUNT(*)::text FROM products";

        PGresult* result = PQexec(connection, query.c_str());
        if (result != nullptr && PQresultStatus(result) == PGRES_TUPLES_OK) {
            for (int row = 0; row < PQntuples(result); ++row) {
                std::cout << "store_view node=" << node.name
                          << " area=" << PQgetvalue(result, row, 0)
                          << " count=" << PQgetvalue(result, row, 1) << '\n';
            }
        } else {
            std::cout << "store_view node=" << node.name << " status=query_failed\n";
        }

        if (result != nullptr) {
            PQclear(result);
        }
        PQfinish(connection);
    }
}

int main(int argc, char** argv) {
    const bool run_once = argc > 1 && std::string(argv[1]) == "--once";
    const bool run_demo = argc > 1 && std::string(argv[1]) == "--demo";
    const std::vector<NodeConfig> nodes = {
        {"node1", "node1", "ecommerce_node1"},
        {"node2", "node2", "ecommerce_node2"},
        {"node3", "node3", "ecommerce_node3"}
    };

    do {
        std::vector<std::future<NodeStatus>> checks;
        for (const NodeConfig& node : nodes) {
            checks.push_back(std::async(std::launch::async, check_node, node));
        }

        std::vector<NodeStatus> statuses;
        bool all_healthy = true;
        for (std::future<NodeStatus>& check : checks) {
            statuses.push_back(check.get());
            all_healthy = all_healthy && statuses.back().healthy;
        }

        print_status(statuses);
        if (run_demo) {
            print_demo_snapshot(nodes);
        }
        if (run_once || run_demo) {
            return all_healthy ? EXIT_SUCCESS : EXIT_FAILURE;
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    } while (true);
}