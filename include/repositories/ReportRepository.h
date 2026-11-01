#pragma once
// Read-only aggregate queries for the reports. All SQL stays in the repository layer.

#include <map>
#include <vector>

#include "database/Database.h"
#include "models/Enums.h"
#include "models/Report.h"

namespace csms {

class ReportRepository {
public:
    explicit ReportRepository(Database& db) : db_(db) {}

    int countClients(bool includeArchived) const;

    // Only statuses/priorities that occur in the data are returned; the
    // service fills in the zeros.
    std::map<CaseStatus, int> casesByStatus() const;
    std::map<Priority, int> casesByPriority() const;
    int countUnassignedCases() const;

    // Every support agent (even with zero cases), plus any other staff member who
    // has cases assigned. Ordered by full name.
    std::vector<StaffWorkload> staffWorkloads() const;

private:
    Database& db_;
};

} // namespace csms
