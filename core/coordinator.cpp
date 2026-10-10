#include <iostream>
#include <string>
#include <unordered_map>
#include <memory>
#include <pqxx/pqxx>
#include <unistd.h>
#include <thread>
#include <chrono>
#include "cluster.h"

// The Coordinator is the "brain" described in the README: the application
// layer talks only to this process, never to a node directly. It holds a
// live connection to every node at once, routes each request to the node
// that owns the relevant domain, and — as of this version — automatically
// redirects reads to a replica node when the primary is unreachable.

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
// Attempts to (re)establish the connection. Used both at startup and
    // by the heartbeat loop when a previously-dead node might have come
    // back up — without this, a node that failed once would stay marked
    // dead forever, even after a real recovery.
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
