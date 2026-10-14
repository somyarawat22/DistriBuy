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
        // Fast, cached status — does NOT hit the network. Reflects the
    // result of the most recent ping() (or the initial connection
    // attempt if ping() has never been called yet).
    bool is_alive() const {
        return alive_;
    }

    const NodeConfig& config() const {
        return config_;
    }

    pqxx::connection& raw_connection() {
        return *conn_;
    }

    // Queries a specific table on this connection. Generalized (rather than
    // always querying this node's OWN domain table) so a replica node's
    // connection can also be used to read another domain's replicated data
    // during failover.
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
    }
