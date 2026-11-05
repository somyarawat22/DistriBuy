#ifndef DISTRIBUY_CLUSTER_H
#define DISTRIBUY_CLUSTER_H

#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
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