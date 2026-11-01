#include "repositories/CaseRepository.h"

#include "common/Exceptions.h"

namespace csms {

namespace {

constexpr const char* kSelectColumns =
    "SELECT id, client_id, title, description, priority, status, assigned_staff_id, "
    "created_at, updated_at FROM SupportCases";

SupportCase mapRow(const Statement& row) {
    SupportCase c;
    c.id = row.columnInt(0);
    c.clientId = row.columnInt(1);
    c.title = row.columnText(2);
    c.description = row.columnText(3);
    c.priority = priorityFromString(row.columnText(4));
    c.status = caseStatusFromString(row.columnText(5));
    c.assignedStaffId = row.columnOptionalInt(6);
    c.createdAt = row.columnText(7);
    c.updatedAt = row.columnText(8);
    return c;
}

} // namespace

std::int64_t CaseRepository::insert(const SupportCase& c) {
    Statement stmt = db_.prepare(
        "INSERT INTO SupportCases (client_id, title, description, priority, status, "
        "assigned_staff_id, created_at, updated_at) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)");
    stmt.bindInt(1, c.clientId)
        .bindText(2, c.title)
        .bindText(3, c.description)
        .bindText(4, toString(c.priority))
        .bindText(5, toString(c.status))
        .bindOptionalInt(6, c.assignedStaffId)
        .bindText(7, c.createdAt)
        .bindText(8, c.updatedAt);
    stmt.step();
    return db_.lastInsertRowId();
}

std::optional<SupportCase> CaseRepository::findById(std::int64_t id) const {
    Statement stmt = db_.prepare(std::string(kSelectColumns) + " WHERE id = ?1");
    stmt.bindInt(1, id);
    if (!stmt.step()) return std::nullopt;
    return mapRow(stmt);
}

std::vector<SupportCase> CaseRepository::search(const CaseFilter& f) const {
    // Fixed SQL text: "?N IS NULL OR column = ?N" switches a filter off when its
    // parameter is NULL, so no SQL is ever assembled from user input.
    Statement stmt = db_.prepare(std::string(kSelectColumns) +
                                 " WHERE (?1 IS NULL OR id = ?1)"
                                 "   AND (?2 IS NULL OR client_id = ?2)"
                                 "   AND (?3 IS NULL OR status = ?3)"
                                 "   AND (?4 IS NULL OR priority = ?4)"
                                 "   AND (?5 IS NULL OR assigned_staff_id = ?5)"
                                 " ORDER BY id");
    stmt.bindOptionalInt(1, f.caseId).bindOptionalInt(2, f.clientId);
    if (f.status) stmt.bindText(3, toString(*f.status)); else stmt.bindNull(3);
    if (f.priority) stmt.bindText(4, toString(*f.priority)); else stmt.bindNull(4);
    stmt.bindOptionalInt(5, f.assignedStaffId);

    std::vector<SupportCase> result;
    while (stmt.step()) result.push_back(mapRow(stmt));
    return result;
}

void CaseRepository::updateStatus(std::int64_t id, CaseStatus status,
                                  const std::string& updatedAt) {
    Statement stmt = db_.prepare("UPDATE SupportCases SET status = ?1, updated_at = ?2 WHERE id = ?3");
    stmt.bindText(1, toString(status)).bindText(2, updatedAt).bindInt(3, id);
    stmt.step();
    if (db_.changes() == 0) {
        throw NotFoundError("Case " + std::to_string(id) + " does not exist");
    }
}

void CaseRepository::updateAssignment(std::int64_t id, std::optional<std::int64_t> staffId,
                                      const std::string& updatedAt) {
    Statement stmt = db_.prepare(
        "UPDATE SupportCases SET assigned_staff_id = ?1, updated_at = ?2 WHERE id = ?3");
    stmt.bindOptionalInt(1, staffId).bindText(2, updatedAt).bindInt(3, id);
    stmt.step();
    if (db_.changes() == 0) {
        throw NotFoundError("Case " + std::to_string(id) + " does not exist");
    }
}

} // namespace csms
