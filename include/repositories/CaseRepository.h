#pragma once
// SQL for the SupportCases table.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "database/Database.h"
#include "models/SupportCase.h"

namespace csms {

// Every field is optional; the ones that are set are combined with AND.
struct CaseFilter {
    std::optional<std::int64_t> caseId;
    std::optional<std::int64_t> clientId;
    std::optional<std::int64_t> assignedStaffId;
    std::optional<CaseStatus> status;
    std::optional<Priority> priority;
};

class CaseRepository {
public:
    explicit CaseRepository(Database& db) : db_(db) {}

    // Returns the generated ID. The client (and assignee, if any) must exist,
    // otherwise the foreign keys make this throw DatabaseError.
    std::int64_t insert(const SupportCase& supportCase);

    std::optional<SupportCase> findById(std::int64_t id) const;

    // Ordered by ID.
    std::vector<SupportCase> search(const CaseFilter& filter) const;

    // Both throw NotFoundError if the case does not exist.
    void updateStatus(std::int64_t id, CaseStatus status, const std::string& updatedAt);
    void updateAssignment(std::int64_t id, std::optional<std::int64_t> staffId,
                          const std::string& updatedAt);

private:
    Database& db_;
};

} // namespace csms
