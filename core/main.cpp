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
