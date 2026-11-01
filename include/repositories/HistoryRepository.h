#pragma once
// SQL for the CaseHistory table (append-only audit trail: no update, no delete).

#include <cstdint>
#include <vector>

#include "database/Database.h"
#include "models/CaseHistoryEntry.h"

namespace csms {

class HistoryRepository {
public:
    explicit HistoryRepository(Database& db) : db_(db) {}

    // Returns the new entry's ID. The case and staff member must exist.
    std::int64_t insert(const CaseHistoryEntry& entry);

    // Oldest first. Ordered by ID, which is strictly chronological (timestamps
    // only have one-second precision, so two entries can share one).
    std::vector<CaseHistoryEntry> findByCase(std::int64_t caseId) const;

private:
    Database& db_;
};

} // namespace csms
