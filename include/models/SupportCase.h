#pragma once
#include <cstdint>
#include <optional>
#include <string>

#include "models/Enums.h"

namespace csms {

struct SupportCase {
    std::int64_t id = 0;
    std::int64_t clientId = 0;
    std::string title;
    std::string description;
    Priority priority = Priority::Medium;
    CaseStatus status = CaseStatus::Open;
    std::optional<std::int64_t> assignedStaffId;   // empty = unassigned
    std::string createdAt;                         // ISO 8601 UTC
    std::string updatedAt;                         // ISO 8601 UTC
};

} // namespace csms
