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
// A real liveness check: actually runs a trivial query against the
    // node rather than just checking whether the last-known connection
    // object still looks open. This is what the heartbeat loop calls
    // periodically. If the node is currently marked dead, this also
    // attempts a reconnect, so recovery is detected automatically.
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
