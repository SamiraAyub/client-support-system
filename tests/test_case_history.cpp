// Phase 4: case history and transactional consistency.

#include <gtest/gtest.h>

#include <regex>

#include "common/Exceptions.h"
#include "ServiceFixture.h"

using namespace csms;

class CaseHistoryTest : public test_support::ServiceFixture {};

TEST_F(CaseHistoryTest, CreatingACaseWritesTheFirstHistoryEntry) {
    const SupportCase c = addCase(addClient().id);
    const auto history = caseService.getHistory(c.id);
    ASSERT_EQ(history.size(), 1u);
    EXPECT_EQ(history[0].caseId, c.id);
    EXPECT_FALSE(history[0].previousStatus.has_value());
    EXPECT_EQ(history[0].newStatus, CaseStatus::Open);
    EXPECT_EQ(history[0].changedByStaffId, admin.id);
    EXPECT_EQ(history[0].note, "Case created");
    EXPECT_EQ(history[0].changedAt, c.createdAt);
}

TEST_F(CaseHistoryTest, EveryStatusChangeIsRecordedWithAllRequiredFields) {
    const SupportCase c = addCase(addClient().id);
    caseService.assignCase(c.id, agent.id);

    loginAs(agent);
    caseService.changeStatus(c.id, CaseStatus::InProgress, "Looking into it");
    caseService.changeStatus(c.id, CaseStatus::Resolved, "Replaced the toner");

    const auto history = caseService.getHistory(c.id);
    ASSERT_EQ(history.size(), 3u);   // created + 2 changes

    const CaseHistoryEntry& e = history[1];
    EXPECT_EQ(e.previousStatus, CaseStatus::Open);
    EXPECT_EQ(e.newStatus, CaseStatus::InProgress);
    EXPECT_EQ(e.changedByStaffId, agent.id);          // the staff member responsible
    EXPECT_EQ(e.note, "Looking into it");
    EXPECT_TRUE(std::regex_match(e.changedAt,
                                 std::regex(R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$)")));

    EXPECT_EQ(history[2].previousStatus, CaseStatus::InProgress);
    EXPECT_EQ(history[2].newStatus, CaseStatus::Resolved);
    EXPECT_EQ(history[2].note, "Replaced the toner");
}

TEST_F(CaseHistoryTest, HistoryIsReturnedOldestFirstAndCompleteForTheWholeLifecycle) {
    const SupportCase c = addCaseWithStatus(addClient().id, CaseStatus::Closed);
    caseService.getHistory(c.id);
    const auto history = caseService.getHistory(c.id);
    ASSERT_EQ(history.size(), 4u);   // created, InProgress, Resolved, Closed
    EXPECT_EQ(history[0].newStatus, CaseStatus::Open);
    EXPECT_EQ(history[1].newStatus, CaseStatus::InProgress);
    EXPECT_EQ(history[2].newStatus, CaseStatus::Resolved);
    EXPECT_EQ(history[3].newStatus, CaseStatus::Closed);
    for (std::size_t i = 1; i < history.size(); ++i) {
        EXPECT_LT(history[i - 1].id, history[i].id);
        EXPECT_EQ(history[i].previousStatus, history[i - 1].newStatus);   // chain is unbroken
    }
}

TEST_F(CaseHistoryTest, ReopenIsRecordedWithItsReason) {
    const SupportCase c = addCaseWithStatus(addClient().id, CaseStatus::Resolved);
    caseService.reopenCase(c.id, "Customer reports it broke again");

    const auto history = caseService.getHistory(c.id);
    const CaseHistoryEntry& last = history.back();
    EXPECT_EQ(last.previousStatus, CaseStatus::Resolved);
    EXPECT_EQ(last.newStatus, CaseStatus::Open);
    EXPECT_EQ(last.note, "Customer reports it broke again");
}

TEST_F(CaseHistoryTest, RejectedOperationsLeaveNoHistoryBehind) {
    const SupportCase c = addCase(addClient().id);
    EXPECT_THROW(caseService.changeStatus(c.id, CaseStatus::Closed, ""), InvalidTransitionError);
    EXPECT_THROW(caseService.changeStatus(c.id, CaseStatus::InProgress, ""),   // unassigned
                 InvalidTransitionError);
    EXPECT_THROW(caseService.reopenCase(c.id, "x"), InvalidTransitionError);
    EXPECT_EQ(caseService.getHistory(c.id).size(), 1u);   // only the creation entry
}

TEST_F(CaseHistoryTest, AssignmentDoesNotCreateHistoryBecauseItIsNotAStatusChange) {
    const SupportCase c = addCase(addClient().id);
    caseService.assignCase(c.id, agent.id);
    EXPECT_EQ(caseService.getHistory(c.id).size(), 1u);
}

TEST_F(CaseHistoryTest, HistoryOfUnknownCaseThrowsNotFound) {
    EXPECT_THROW(caseService.getHistory(777), NotFoundError);
}

TEST_F(CaseHistoryTest, HistoriesOfDifferentCasesAreKeptSeparate) {
    const Client client = addClient();
    const SupportCase a = addCase(client.id);
    const SupportCase b = addCaseWithStatus(client.id, CaseStatus::Resolved);
    EXPECT_EQ(caseService.getHistory(a.id).size(), 1u);
    EXPECT_EQ(caseService.getHistory(b.id).size(), 3u);
}

// ------------------------------------------------------- transaction rollback

// Rollback is tested for real, without mocks: a "ghost" user whose ID does not
// exist in the Staff table makes the history INSERT violate its foreign key
// *after* the case row has already been updated inside the same transaction.

namespace {
Staff makeGhost() {
    Staff ghost;
    ghost.id = 99999;
    ghost.username = "ghost";
    ghost.role = StaffRole::Administrator;
    return ghost;
}
}  // namespace

TEST_F(CaseHistoryTest, FailedHistoryWriteRollsBackTheStatusChange) {
    const SupportCase c = addCaseWithStatus(addClient().id, CaseStatus::InProgress);
    const auto historyBefore = caseService.getHistory(c.id).size();
    const std::string updatedBefore = caseService.getCase(c.id).updatedAt;

    loginAs(makeGhost());
    EXPECT_THROW(caseService.changeStatus(c.id, CaseStatus::Resolved, "x"), DatabaseError);

    loginAs(admin);
    const SupportCase after = caseService.getCase(c.id);
    EXPECT_EQ(after.status, CaseStatus::InProgress);          // UPDATE was undone
    EXPECT_EQ(after.updatedAt, updatedBefore);
    EXPECT_EQ(caseService.getHistory(c.id).size(), historyBefore);
    EXPECT_FALSE(db.inTransaction());                          // transaction fully closed
}

TEST_F(CaseHistoryTest, FailedHistoryWriteRollsBackCaseCreation) {
    const Client client = addClient();
    loginAs(makeGhost());
    EXPECT_THROW(addCase(client.id), DatabaseError);

    EXPECT_EQ(countRows("SupportCases"), 0);                  // INSERT was undone
    EXPECT_EQ(countRows("CaseHistory"), 0);
    EXPECT_FALSE(db.inTransaction());
}

TEST_F(CaseHistoryTest, FailedReopenRollsBackToResolved) {
    const SupportCase c = addCaseWithStatus(addClient().id, CaseStatus::Resolved);
    loginAs(makeGhost());
    EXPECT_THROW(caseService.reopenCase(c.id, "reason"), DatabaseError);
    loginAs(admin);
    EXPECT_EQ(caseService.getCase(c.id).status, CaseStatus::Resolved);
}

TEST_F(CaseHistoryTest, ServiceWorksNormallyAfterARolledBackOperation) {
    const SupportCase c = addCaseWithStatus(addClient().id, CaseStatus::InProgress);
    loginAs(makeGhost());
    EXPECT_THROW(caseService.changeStatus(c.id, CaseStatus::Resolved, ""), DatabaseError);

    loginAs(admin);
    EXPECT_EQ(caseService.changeStatus(c.id, CaseStatus::Resolved, "ok").status,
              CaseStatus::Resolved);
}

// ---------------------------------------------------------------- persistence

TEST(CaseHistoryPersistenceTest, CasesAndHistorySurviveARestart) {
    test_support::TempDbPath file;
    std::int64_t caseId = 0;

    {   // run 1
        Database db(file.str());
        db.initializeSchema();
        StaffRepository staff(db);
        ClientRepository clients(db);
        CaseRepository cases(db);
        HistoryRepository history(db);
        Session session;

        Staff a; a.username = "agent"; a.fullName = "Agent"; a.passwordHash = "h";
        a.createdAt = "2026-01-01T00:00:00Z";
        a.id = staff.insert(a);
        Client c; c.name = "Ann"; c.email = "ann@example.com"; c.phone = "5551234567";
        c.registeredAt = "2026-01-01T00:00:00Z";
        c.id = clients.insert(c);

        Staff boss; boss.username = "boss"; boss.fullName = "Boss"; boss.role = StaffRole::Administrator;
        boss.passwordHash = "h"; boss.createdAt = "2026-01-01T00:00:00Z";
        boss.id = staff.insert(boss);
        session.start(boss);

        CaseService service(db, cases, history, clients, staff, session);
        caseId = service.createCase(c.id, "Router offline", "No lights", Priority::High).id;
        service.assignCase(caseId, a.id);
        service.changeStatus(caseId, CaseStatus::InProgress, "On it");
    }

    {   // run 2: brand-new connection to the same file
        Database db(file.str());
        db.initializeSchema();
        StaffRepository staff(db);
        ClientRepository clients(db);
        CaseRepository cases(db);
        HistoryRepository history(db);
        Session session;
        Staff boss = *staff.findByUsername("boss");
        session.start(boss);

        CaseService service(db, cases, history, clients, staff, session);
        const SupportCase c = service.getCase(caseId);
        EXPECT_EQ(c.title, "Router offline");
        EXPECT_EQ(c.status, CaseStatus::InProgress);
        EXPECT_TRUE(c.assignedStaffId.has_value());

        const auto entries = service.getHistory(caseId);
        ASSERT_EQ(entries.size(), 2u);
        EXPECT_EQ(entries[1].note, "On it");
    }
}
