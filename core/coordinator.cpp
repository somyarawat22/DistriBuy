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
