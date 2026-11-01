#include "repositories/HistoryRepository.h"

namespace csms {

std::int64_t HistoryRepository::insert(const CaseHistoryEntry& e) {
    Statement stmt = db_.prepare(
        "INSERT INTO CaseHistory (case_id, changed_by_staff_id, previous_status, new_status, "
        "note, changed_at) VALUES (?1, ?2, ?3, ?4, ?5, ?6)");
    stmt.bindInt(1, e.caseId).bindInt(2, e.changedByStaffId);
    if (e.previousStatus) stmt.bindText(3, toString(*e.previousStatus)); else stmt.bindNull(3);
    stmt.bindText(4, toString(e.newStatus)).bindText(5, e.note).bindText(6, e.changedAt);
    stmt.step();
    return db_.lastInsertRowId();
}

std::vector<CaseHistoryEntry> HistoryRepository::findByCase(std::int64_t caseId) const {
    Statement stmt = db_.prepare(
        "SELECT id, case_id, changed_by_staff_id, previous_status, new_status, note, changed_at "
        "FROM CaseHistory WHERE case_id = ?1 ORDER BY id");
    stmt.bindInt(1, caseId);

    std::vector<CaseHistoryEntry> result;
    while (stmt.step()) {
        CaseHistoryEntry e;
        e.id = stmt.columnInt(0);
        e.caseId = stmt.columnInt(1);
        e.changedByStaffId = stmt.columnInt(2);
        if (!stmt.columnIsNull(3)) e.previousStatus = caseStatusFromString(stmt.columnText(3));
        e.newStatus = caseStatusFromString(stmt.columnText(4));
        e.note = stmt.columnText(5);
        e.changedAt = stmt.columnText(6);
        result.push_back(std::move(e));
    }
    return result;
}

} // namespace csms
