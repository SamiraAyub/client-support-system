#include "repositories/ReportRepository.h"

namespace csms {

int ReportRepository::countClients(bool includeArchived) const {
    Statement stmt = db_.prepare("SELECT COUNT(*) FROM Clients WHERE (?1 = 1 OR is_archived = 0)");
    stmt.bindInt(1, includeArchived ? 1 : 0);
    stmt.step();
    return static_cast<int>(stmt.columnInt(0));
}

std::map<CaseStatus, int> ReportRepository::casesByStatus() const {
    Statement stmt = db_.prepare("SELECT status, COUNT(*) FROM SupportCases GROUP BY status");
    std::map<CaseStatus, int> result;
    while (stmt.step()) {
        result[caseStatusFromString(stmt.columnText(0))] = static_cast<int>(stmt.columnInt(1));
    }
    return result;
}

std::map<Priority, int> ReportRepository::casesByPriority() const {
    Statement stmt = db_.prepare("SELECT priority, COUNT(*) FROM SupportCases GROUP BY priority");
    std::map<Priority, int> result;
    while (stmt.step()) {
        result[priorityFromString(stmt.columnText(0))] = static_cast<int>(stmt.columnInt(1));
    }
    return result;
}

int ReportRepository::countUnassignedCases() const {
    Statement stmt = db_.prepare("SELECT COUNT(*) FROM SupportCases WHERE assigned_staff_id IS NULL");
    stmt.step();
    return static_cast<int>(stmt.columnInt(0));
}

std::vector<StaffWorkload> ReportRepository::staffWorkloads() const {
    Statement stmt = db_.prepare(
        "SELECT s.id, s.username, s.full_name, s.is_active, "
        "       COUNT(c.id), "
        "       COALESCE(SUM(CASE WHEN c.status IN ('Open', 'InProgress') THEN 1 ELSE 0 END), 0) "
        "FROM Staff s LEFT JOIN SupportCases c ON c.assigned_staff_id = s.id "
        "WHERE s.role = 'SupportAgent' OR c.id IS NOT NULL "
        "GROUP BY s.id "
        "ORDER BY s.full_name COLLATE NOCASE, s.id");

    std::vector<StaffWorkload> result;
    while (stmt.step()) {
        StaffWorkload w;
        w.staffId = stmt.columnInt(0);
        w.username = stmt.columnText(1);
        w.fullName = stmt.columnText(2);
        w.active = stmt.columnInt(3) != 0;
        w.totalAssigned = static_cast<int>(stmt.columnInt(4));
        w.openWork = static_cast<int>(stmt.columnInt(5));
        result.push_back(std::move(w));
    }
    return result;
}

} // namespace csms
