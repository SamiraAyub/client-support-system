#include "services/ReportService.h"

namespace csms {

SystemReport ReportService::generate() const {
    session_.requireLogin();

    SystemReport report;
    report.totalClients = repository_.countClients(true);
    report.activeClients = repository_.countClients(false);
    report.archivedClients = report.totalClients - report.activeClients;

    // Start from zero for every possible value so the report always lists all of
    // them, even those with no cases.
    for (const CaseStatus s : {CaseStatus::Open, CaseStatus::InProgress, CaseStatus::Resolved,
                               CaseStatus::Closed}) {
        report.casesByStatus[s] = 0;
    }
    for (const Priority p : {Priority::Low, Priority::Medium, Priority::High}) {
        report.casesByPriority[p] = 0;
    }
    for (const auto& [status, count] : repository_.casesByStatus()) {
        report.casesByStatus[status] = count;
        report.totalCases += count;
    }
    for (const auto& [priority, count] : repository_.casesByPriority()) {
        report.casesByPriority[priority] = count;
    }

    report.unassignedCases = repository_.countUnassignedCases();
    report.staffWorkloads = repository_.staffWorkloads();
    return report;
}

} // namespace csms
