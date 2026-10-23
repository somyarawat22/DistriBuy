#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>
#include <unordered_map>
#include <ctime>
#include <cstdlib>
#include <unistd.h>
#include <pqxx/pqxx>
enum class NodeState { Starting, Ready };
enum class NodeDomain { Users, Products, Orders };
enum class TxState { Begin, Commit, Abort };
const char* state_name(NodeState state) {
    switch (state) {
        case NodeState::Starting:
            return "starting";
        case NodeState::Ready:
            return "ready";
    }
    return "unknown";
}
const char* domain_name(NodeDomain domain) {
    switch (domain) {
        case NodeDomain::Users:
            return "Users";
        case NodeDomain::Products:
            return "Products";
        case NodeDomain::Orders:
            return "Orders";
    }
    return "Unknown";
}

const char* tx_state_name(TxState state) {
    switch (state) {
        case TxState::Begin:
            return "begin";
        case TxState::Commit:
            return "commit";
        case TxState::Abort:
            return "abort";
    }
    return "unknown";
}

struct NodeConfig {
    int node_id;
    std::string host;
    int port;
    NodeDomain domain;
    bool is_valid() const {
        return node_id > 0 && !host.empty() && port > 0 && port <= 65535;
    }

    std::string endpoint() const {
        return host + ':' + std::to_string(port);
    }
};

const NodeConfig* find_peer(const std::vector<NodeConfig>& peers, int node_id) {
    for (const NodeConfig& peer : peers) {
        if (peer.node_id == node_id) {
            return &peer;
        }
    }
    return nullptr;
}

bool peers_are_valid(const NodeConfig& local,
                     const std::vector<NodeConfig>& peers) {
    std::unordered_set<int> peer_ids;
    for (const NodeConfig& peer : peers) {
        if (!peer.is_valid() || peer.node_id == local.node_id ||
            !peer_ids.insert(peer.node_id).second) {
            return false;
        }
    }
    return true;
}

struct LivenessInfo {
    long last_heartbeat_time = 0;
    bool has_heartbeat = false;

    void mark_alive(long now) {
        last_heartbeat_time = now;
        has_heartbeat = true;
    }
};

const long HEARTBEAT_TIMEOUT_SECONDS = 10;

bool is_alive(const LivenessInfo& liveness, long now) {
    if (!liveness.has_heartbeat) {
        return false;
    }
    return (now - liveness.last_heartbeat_time) <= HEARTBEAT_TIMEOUT_SECONDS;
}

struct Node {
    NodeConfig config;
    std::vector<NodeConfig> peers;

    bool is_valid() const {
        return config.is_valid() && peers_are_valid(config, peers);
    }

    std::vector<std::string> peer_endpoints() const {
        std::vector<std::string> endpoints;
        for (const auto& peer : peers) {
            endpoints.push_back(peer.endpoint());
        }
        return endpoints;
    }