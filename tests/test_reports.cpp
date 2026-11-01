#include <gtest/gtest.h>

#include "common/Exceptions.h"
#include "repositories/ReportRepository.h"
#include "services/ReportService.h"
#include "ServiceFixture.h"

using namespace csms;

class ReportTest : public test_support::ServiceFixture {
protected:
    ReportRepository reportRepo{db};
    ReportService reportService{reportRepo, session};
};

TEST_F(ReportTest, EmptySystemReportsZerosForEverything) {
    const SystemReport r = reportService.generate();
    EXPECT_EQ(r.totalClients, 0);
    EXPECT_EQ(r.totalCases, 0);
    EXPECT_EQ(r.unassignedCases, 0);
    ASSERT_EQ(r.casesByStatus.size(), 4u);       // every status is listed...
    ASSERT_EQ(r.casesByPriority.size(), 3u);     // ...and every priority
    for (const auto& [status, count] : r.casesByStatus) EXPECT_EQ(count, 0);
    for (const auto& [priority, count] : r.casesByPriority) EXPECT_EQ(count, 0);
}

TEST_F(ReportTest, CountsClientsIncludingArchivedOnes) {
    addClient("a@example.com");
    addClient("b@example.com");
    const Client c = addClient("c@example.com");
    clientService.archiveClient(c.id);

    const SystemReport r = reportService.generate();
    EXPECT_EQ(r.totalClients, 3);
    EXPECT_EQ(r.activeClients, 2);
    EXPECT_EQ(r.archivedClients, 1);
}

TEST_F(ReportTest, CountsCasesByStatus) {
    const Client c = addClient();
    addCaseWithStatus(c.id, CaseStatus::Open);
    addCaseWithStatus(c.id, CaseStatus::Open);
    addCaseWithStatus(c.id, CaseStatus::InProgress);
    addCaseWithStatus(c.id, CaseStatus::Resolved);
    addCaseWithStatus(c.id, CaseStatus::Closed);
    addCaseWithStatus(c.id, CaseStatus::Closed);
    addCaseWithStatus(c.id, CaseStatus::Closed);

    const SystemReport r = reportService.generate();
    EXPECT_EQ(r.totalCases, 7);
    EXPECT_EQ(r.casesByStatus.at(CaseStatus::Open), 2);
    EXPECT_EQ(r.casesByStatus.at(CaseStatus::InProgress), 1);
    EXPECT_EQ(r.casesByStatus.at(CaseStatus::Resolved), 1);
    EXPECT_EQ(r.casesByStatus.at(CaseStatus::Closed), 3);
}

TEST_F(ReportTest, CountsCasesByPriorityIncludingZeroes) {
    const Client c = addClient();
    addCase(c.id, Priority::High);
    addCase(c.id, Priority::High);
    addCase(c.id, Priority::Low);

    const SystemReport r = reportService.generate();
    EXPECT_EQ(r.casesByPriority.at(Priority::High), 2);
    EXPECT_EQ(r.casesByPriority.at(Priority::Medium), 0);   // none, still listed
    EXPECT_EQ(r.casesByPriority.at(Priority::Low), 1);
}

TEST_F(ReportTest, ReopenedCasesAreCountedUnderTheirCurrentStatus) {
    const SupportCase sc = addCaseWithStatus(addClient().id, CaseStatus::Resolved);
    caseService.reopenCase(sc.id, "again");
    const SystemReport r = reportService.generate();
    EXPECT_EQ(r.casesByStatus.at(CaseStatus::Resolved), 0);
    EXPECT_EQ(r.casesByStatus.at(CaseStatus::Open), 1);
}

TEST_F(ReportTest, StaffWorkloadListsEveryAgentEvenWithoutCases) {
    addStaff("idle", StaffRole::SupportAgent);
    const SystemReport r = reportService.generate();

    ASSERT_EQ(r.staffWorkloads.size(), 2u);   // fixture agent + idle; admin is not an agent
    for (const auto& w : r.staffWorkloads) {
        EXPECT_EQ(w.totalAssigned, 0);
        EXPECT_EQ(w.openWork, 0);
    }
}

TEST_F(ReportTest, StaffWorkloadCountsAssignedCasesPerPerson) {
    const Staff second = addStaff("second", StaffRole::SupportAgent);
    const Client c = addClient();

    addCaseWithStatus(c.id, CaseStatus::InProgress);    // agent: open work
    addCaseWithStatus(c.id, CaseStatus::Resolved);      // agent: finished
    addCaseWithStatus(c.id, CaseStatus::Closed);        // agent: finished
    const SupportCase forSecond = addCase(c.id);
    caseService.assignCase(forSecond.id, second.id);    // second: 1 open
    addCase(c.id);                                      // unassigned

    const SystemReport r = reportService.generate();
    EXPECT_EQ(r.unassignedCases, 1);

    StaffWorkload agentLoad, secondLoad;
    for (const auto& w : r.staffWorkloads) {
        if (w.staffId == agent.id) agentLoad = w;
        if (w.staffId == second.id) secondLoad = w;
    }
    EXPECT_EQ(agentLoad.totalAssigned, 3);
    EXPECT_EQ(agentLoad.openWork, 1);
    EXPECT_EQ(secondLoad.totalAssigned, 1);
    EXPECT_EQ(secondLoad.openWork, 1);
}

TEST_F(ReportTest, DeactivatedStaffWithCasesStayInTheReport) {
    addCaseWithStatus(addClient().id, CaseStatus::InProgress);
    staffService.deactivateStaff(agent.id);

    const SystemReport r = reportService.generate();
    bool found = false;
    for (const auto& w : r.staffWorkloads) {
        if (w.staffId == agent.id) {
            found = true;
            EXPECT_FALSE(w.active);
            EXPECT_EQ(w.totalAssigned, 1);
        }
    }
    EXPECT_TRUE(found);   // historical references are preserved in reports
}

TEST_F(ReportTest, ReportRequiresLogin) {
    session.end();
    EXPECT_THROW(reportService.generate(), AuthorizationError);
}

TEST_F(ReportTest, AgentsMayViewReports) {
    loginAs(agent);
    EXPECT_NO_THROW(reportService.generate());
}
