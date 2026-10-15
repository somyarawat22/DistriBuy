#include <iostream>
#include <string>
#include <unordered_map>
#include <memory>
#include <pqxx/pqxx>
#include <unistd.h>
#include <thread>
#include <chrono>
#include "cluster.h"

class NodeConnection {
public:
    NodeConnection(const NodeConfig& config, const std::string& user, const std::string& password)
        : config_(config) {
        conn_str_ =
            "host=" + config.host +
            " port=" + std::to_string(config.port) +
            " user=" + user +
            " password=" + password +
            " dbname=postgres";
        try_reconnect();
    }

    bool try_reconnect() {
        try {
            conn_ = std::make_unique<pqxx::connection>(conn_str_);
            alive_ = true;
            return true;
        } catch (const std::exception&) {
            conn_.reset();
            alive_ = false;
            return false;
        }
    }
    
    bool ping() {
        if (!conn_ || !conn_->is_open()) {
            alive_ = try_reconnect();
            if (!alive_) {
                return false;
            }
        }
        try {
            pqxx::nontransaction n(*conn_);
            n.exec("SELECT 1");
            alive_ = true;
        } catch (const std::exception&) {
            alive_ = false;
        }
        return alive_;
    }
    
    bool is_alive() const {
        return alive_;
    }

    const NodeConfig& config() const {
        return config_;
    }

    pqxx::connection& raw_connection() {
        return *conn_;
    }

    
    std::size_t query_table(const std::string& table) {
        if (!is_alive()) {
            std::cerr << "  [ERROR] Node " << config_.node_id << " is not connected.\n";
            return 0;
        }
        try {
            pqxx::work txn(*conn_);
            pqxx::result r = txn.exec("SELECT * FROM " + table + ";");
            for (const auto& row : r) {
                std::string line;
                for (const auto& field : row) {
                    line += field.c_str();
                    line += " | ";
                }
                std::cout << "    " << line << '\n';
            }
            return r.size();
        } catch (const std::exception& e) {
            std::cerr << "  [ERROR] Query failed on node " << config_.node_id
                       << ": " << e.what() << '\n';
            return 0;
        }
    }

    std::size_t query_domain_table() {
        return query_table(table_for_domain(config_.domain));
    }

private:
    NodeConfig config_;
    std::string conn_str_;
    std::unique_ptr<pqxx::connection> conn_;
    bool alive_ = false;
};

class Coordinator {
public:
    explicit Coordinator(const ClusterMetadata& metadata)
        : replica_map_(build_replica_map()) {
        for (const auto& node_config : metadata.nodes) {
            connections_.emplace(
                node_config.domain,
                std::make_unique<NodeConnection>(node_config, PG_USER, PG_PASSWORD)
            );
        }
        }
    void route_and_query(NodeDomain domain) {
        auto it = connections_.find(domain);
        if (it == connections_.end()) {
            std::cerr << "No node registered for domain " << domain_name(domain) << '\n';
            return;
        }
        NodeConnection& primary = *it->second;
        std::cout << "Routing to " << domain_name(domain) << " node ("
                   << primary.config().endpoint() << ")\n";

        if (primary.is_alive()) {
            std::cout << "  Status: reachable (primary)\n";
            std::size_t rows = primary.query_domain_table();
            std::cout << "  Rows returned: " << rows << '\n';
            return;
        }

        // --- Primary is down: attempt automatic failover to the replica ---
        std::cout << "  Status: PRIMARY UNREACHABLE - attempting failover...\n";

        auto replica_domain_it = replica_map_.find(domain);
        if (replica_domain_it == replica_map_.end()) {
            std::cout << "  No replica configured for " << domain_name(domain) << ". Read failed.\n";
            return;
        }

        NodeDomain replica_domain = replica_domain_it->second;
        auto replica_conn_it = connections_.find(replica_domain);
        if (replica_conn_it == connections_.end() || !replica_conn_it->second->is_alive()) {
            std::cout << "  Replica node (" << domain_name(replica_domain)
                       << "'s node) is ALSO unreachable. Read failed - no surviving copy.\n";
            return;
        }

        NodeConnection& replica = *replica_conn_it->second;
        std::cout << "  FAILOVER: serving from replica on " << domain_name(replica_domain)
                   << "'s node (" << replica.config().endpoint() << "), "
                   << "which holds a replicated copy of " << domain_name(domain) << " data\n";
        std::size_t rows = replica.query_table(table_for_domain(domain));
        std::cout << "  Rows returned (from replica): " << rows << '\n';
    }

    bool place_order(int user_id, int product_id, int quantity) {
        auto orders_it = connections_.find(NodeDomain::Orders);
        auto products_it = connections_.find(NodeDomain::Products);

        if (orders_it == connections_.end() || products_it == connections_.end()) {
            std::cout << "  [2PC] Missing node registration for Orders or Products.\n";
            return false;
        }

        NodeConnection& orders_conn = *orders_it->second;
        NodeConnection& products_conn = *products_it->second;

        if (!orders_conn.is_alive() || !products_conn.is_alive()) {
            std::cout << "  [2PC] Aborting: one or more required nodes are unreachable "
                       << "(Orders: " << (orders_conn.is_alive() ? "up" : "DOWN")
                       << ", Products: " << (products_conn.is_alive() ? "up" : "DOWN") << "). "
                       << "2PC requires writing to the real owner node - a replica cannot "
                       << "be written to, only read from.\n";
            return false;
        }

        static int txn_counter = 0;
        ++txn_counter;
        std::string txn_orders = "order_" + std::to_string(txn_counter) + "_orders";
        std::string txn_products = "order_" + std::to_string(txn_counter) + "_products";

        bool orders_prepared = false;
        bool products_prepared = false;

        std::cout << "\n[2PC] Phase 1: PREPARE\n";

        try {
            pqxx::work txn(orders_conn.raw_connection());
            txn.exec(
                "INSERT INTO orders (user_id, product_id, quantity, status) VALUES (" +
                std::to_string(user_id) + ", " + std::to_string(product_id) + ", " +
                std::to_string(quantity) + ", 'pending')"
            );
            txn.exec("PREPARE TRANSACTION '" + txn_orders + "'");
            orders_prepared = true;
            std::cout << "  Orders node: PREPARE succeeded\n";
        } catch (const std::exception& e) {
            std::cout << "  Orders node: PREPARE failed (" << e.what() << ")\n";
        }
