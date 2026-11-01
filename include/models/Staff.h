#pragma once
#include <cstdint>
#include <string>

#include "models/Enums.h"

namespace csms {

// A user of the system. Staff are deactivated, never deleted, so historical
// case and history references always stay valid.
struct Staff {
    std::int64_t id = 0;
    std::string username;       // unique, case-insensitive
    std::string fullName;
    StaffRole role = StaffRole::SupportAgent;
    std::string passwordHash;   // encoded hash string; never a plaintext password
    bool active = true;
    std::string createdAt;      // ISO 8601 UTC
};

} // namespace csms
