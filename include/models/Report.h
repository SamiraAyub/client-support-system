#pragma once
// Plain data returned by the report queries.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "models/Enums.h"

namespace csms {

// Workload of one support staff member.
struct StaffWorkload {
    std::int64_t staffId = 0;
    std::string username;
    std::string fullName;
    bool active = true;
    int totalAssigned = 0;     // every case ever assigned to this person
    int openWork = 0;          // of those: currently Open or In Progress
};

struct SystemReport {
    int totalClients = 0;      // every registered client, archived or not
    int activeClients = 0;
    int archivedClients = 0;

    int totalCases = 0;
    std::map<CaseStatus, int> casesByStatus;     // all four statuses always present
    std::map<Priority, int> casesByPriority;     // all three priorities always present
    int unassignedCases = 0;

    std::vector<StaffWorkload> staffWorkloads;
};

} // namespace csms
