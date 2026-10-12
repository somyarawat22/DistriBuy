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
