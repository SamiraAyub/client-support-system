#pragma once
#include <cstdint>
#include <string>

namespace csms {

// A customer who can raise support cases. Clients are archived, never deleted.
struct Client {
    std::int64_t id = 0;        // 0 until saved; assigned by the database
    std::string name;
    std::string email;          // unique, case-insensitive
    std::string phone;
    std::string registeredAt;   // ISO 8601 UTC
    bool archived = false;
};

} // namespace csms
