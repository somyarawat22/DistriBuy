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
    std::string summary() const {
        std::string result = "Node " + std::to_string(config.node_id) +
                             " (" + config.endpoint() + ") with " +
                             std::to_string(peers.size()) + " peer(s)";
        return result;
    }
};
struct ClusterMetadata {
    std::vector<NodeConfig> nodes;
    const NodeConfig* find_by_domain(NodeDomain domain) const {
        for (const auto& candidate : nodes) {
            if (candidate.domain == domain) {
                return &candidate;
            }
        }
        return nullptr;
    }
};
struct ReplicaMap {
    std::unordered_map<NodeDomain, NodeDomain> replication;
    void add(NodeDomain owner, NodeDomain replica) {
        replication[owner] = replica;
    }
};
// --- NEW: PgStore replaces the old in-memory Store. It wraps a real ---
// --- libpqxx connection to this node's own PostgreSQL cluster.       ---
class PgStore {
public:
    // Builds the connection string and opens a real TCP connection to
    // this node's Postgres instance (e.g. host=127.0.0.1 port=5433).
    PgStore(const std::string& host, int port,
            const std::string& user, const std::string& password,
            const std::string& dbname = "postgres") {
        std::string conn_str =
            "host=" + host +
            " port=" + std::to_string(port) +
            " user=" + user +
            " password=" + password +
            " dbname=" + dbname;
        conn_ = std::make_unique<pqxx::connection>(conn_str);
    }
    bool is_open() const {
        return conn_ && conn_->is_open();
    }
    // Runs a single INSERT/UPDATE/DELETE statement inside its own
    // transaction and commits it. Returns true on success.
    bool execute(const std::string& sql) {
        try {
            pqxx::work txn(*conn_);
            txn.exec(sql);
            txn.commit();
            return true;
        } catch (const std::exception& e) {
            std::cerr << "PgStore execute error: " << e.what() << '\n';
            return false;
        }
    }
    // Runs a SELECT and returns the number of rows found. Prints each
    // row's columns for now (useful for quick verification/demo output).
    std::size_t query_and_print(const std::string& sql) {
        try {
            pqxx::work txn(*conn_);
            pqxx::result r = txn.exec(sql);
            for (const auto& row : r) {
                std::string line;
                for (const auto& field : row) {
                    line += field.c_str();
                    line += " | ";
                }
                std::cout << "  " << line << '\n';
            }
            return r.size();
        } catch (const std::exception& e) {
            std::cerr << "PgStore query error: " << e.what() << '\n';
            return 0;
        }
    }

private:
    std::unique_ptr<pqxx::connection> conn_;
};
// --- END NEW ---

struct LogEntry {
    std::string key;
    std::string value;
    long timestamp;
};
struct TransactionLog {
    std::vector<LogEntry> entries;
    void record(const std::string& key, const std::string& value) {
        entries.push_back({key, value, (long)std::time(nullptr)});
    }
    std::size_t size() const {
        return entries.size();
    }
    std::string last_key() const {
        return entries.empty() ? "" : entries.back().key;
    }
};
