#ifndef SHARDCORE_CLUSTER_H
#define SHARDCORE_CLUSTER_H

#include <string>
#include <vector>
#include <unordered_map>

// This header holds the cluster topology definitions shared by both the
// per-node process (main.cpp) and the Coordinator (coordinator.cpp), so
// both always agree on which node owns which domain and which port it
// runs on.

enum class NodeDomain { Users, Products, Orders };
inline const char* domain_name(NodeDomain domain) {
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
inline std::string table_for_domain(NodeDomain domain) {
    switch (domain) {
        case NodeDomain::Users:
            return "users";
        case NodeDomain::Products:
            return "products";
        case NodeDomain::Orders:
            return "orders";
    }
    return "unknown";
}
