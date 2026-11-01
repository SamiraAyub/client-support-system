#pragma once
#include <cstdint>
#include <optional>
#include <string>

#include "models/Enums.h"

namespace csms {

// One row of a case's audit trail. previousStatus is empty for the entry that
// records the case's creation.
struct CaseHistoryEntry {
    std::int64_t id = 0;
    std::int64_t caseId = 0;
    std::int64_t changedByStaffId = 0;
    std::optional<CaseStatus> previousStatus;
    CaseStatus newStatus = CaseStatus::Open;
    std::string note;
    std::string changedAt;                         // ISO 8601 UTC
};

} // namespace csms
