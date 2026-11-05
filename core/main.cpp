#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>
#include <unordered_map>
#include <ctime>
#include <cstdlib>
#include <unistd.h>
#include <pqxx/pqxx>

std::string pg_user_env() {
    const char* v = std::getenv("PG_USER");
    return v ? v : "postgres";
}
std::string pg_password_env() {
    const char* v = std::getenv("PG_PASSWORD");
    if (!v || !*v) {
        std::cerr << "PG_PASSWORD is not set. Export it before running.\n";
        std::cerr.flush();
        _exit(1);
    }
    return v;
}
