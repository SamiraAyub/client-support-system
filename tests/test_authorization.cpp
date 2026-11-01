// Authorization restrictions: who may do what. Rules are enforced in the
// SERVICE layer, so they hold no matter which front end calls them.

#include <gtest/gtest.h>

#include "common/Exceptions.h"
#include "ServiceFixture.h"

using namespace csms;

class AuthorizationTest : public test_support::ServiceFixture {
protected:
    // Calls every operation that needs a login and expects AuthorizationError.
    void expectEverythingRequiresLogin() {
        EXPECT_THROW(clientService.registerClient("A", "a@example.com", "5551234567"), AuthorizationError);
        EXPECT_THROW(clientService.getClient(1), AuthorizationError);
        EXPECT_THROW(clientService.updateClient(1, "A", "a@example.com", "5551234567"), AuthorizationError);
        EXPECT_THROW(clientService.searchClients("a"), AuthorizationError);
        EXPECT_THROW(clientService.listClients(), AuthorizationError);
        EXPECT_THROW(clientService.archiveClient(1), AuthorizationError);
        EXPECT_THROW(clientService.restoreClient(1), AuthorizationError);

        EXPECT_THROW(caseService.createCase(1, "T", "", Priority::Low), AuthorizationError);
        EXPECT_THROW(caseService.getCase(1), AuthorizationError);
        EXPECT_THROW(caseService.searchCases({}), AuthorizationError);
        EXPECT_THROW(caseService.assignCase(1, 1), AuthorizationError);
        EXPECT_THROW(caseService.changeStatus(1, CaseStatus::InProgress, ""), AuthorizationError);
        EXPECT_THROW(caseService.reopenCase(1, "x"), AuthorizationError);
        EXPECT_THROW(caseService.getHistory(1), AuthorizationError);

        EXPECT_THROW(staffService.createStaff("abc", "A", StaffRole::SupportAgent, "LongEnough1"), AuthorizationError);
        EXPECT_THROW(staffService.getStaff(1), AuthorizationError);
        EXPECT_THROW(staffService.listStaff(true), AuthorizationError);
        EXPECT_THROW(staffService.updateStaff(1, "A", StaffRole::SupportAgent), AuthorizationError);
        EXPECT_THROW(staffService.deactivateStaff(1), AuthorizationError);
        EXPECT_THROW(staffService.reactivateStaff(1), AuthorizationError);
        EXPECT_THROW(staffService.resetPassword(1, "LongEnough1"), AuthorizationError);
        EXPECT_THROW(staffService.changeOwnPassword("a", "LongEnough1"), AuthorizationError);
    }
};

TEST_F(AuthorizationTest, NothingWorksWhenLoggedOut) {
    session.end();
    expectEverythingRequiresLogin();
}

TEST_F(AuthorizationTest, LoggedOutAttemptsChangeNothing) {
    const Client c = addClient();
    session.end();
    EXPECT_THROW(clientService.archiveClient(c.id), AuthorizationError);
    EXPECT_THROW(clientService.registerClient("B", "b@example.com", "5551234567"), AuthorizationError);
    loginAs(admin);
    EXPECT_FALSE(clientService.getClient(c.id).archived);
    EXPECT_EQ(clientService.listClients(true).size(), 1u);
}

TEST_F(AuthorizationTest, AgentsCannotManageStaff) {
    loginAs(agent);
    EXPECT_THROW(staffService.createStaff("newbie", "New", StaffRole::SupportAgent, "LongEnough1"),
                 AuthorizationError);
    EXPECT_THROW(staffService.updateStaff(admin.id, "Hacked", StaffRole::SupportAgent),
                 AuthorizationError);
    EXPECT_THROW(staffService.deactivateStaff(admin.id), AuthorizationError);
    EXPECT_THROW(staffService.reactivateStaff(admin.id), AuthorizationError);
    EXPECT_THROW(staffService.resetPassword(admin.id, "HackedPass99"), AuthorizationError);

    loginAs(admin);   // nothing was changed
    EXPECT_TRUE(staffService.getStaff(admin.id).active);
    EXPECT_EQ(staffService.getStaff(admin.id).fullName, "Full admin");
    EXPECT_EQ(staffRepo.countAll(), 2);
}

TEST_F(AuthorizationTest, AgentsCannotArchiveOrRestoreClients) {
    const Client c = addClient();
    loginAs(agent);
    EXPECT_THROW(clientService.archiveClient(c.id), AuthorizationError);
    loginAs(admin);
    clientService.archiveClient(c.id);
    loginAs(agent);
    EXPECT_THROW(clientService.restoreClient(c.id), AuthorizationError);
    EXPECT_TRUE(clientService.getClient(c.id).archived);
}

TEST_F(AuthorizationTest, AgentsCanDoDayToDaySupportWork) {
    loginAs(agent);
    const Client c = clientService.registerClient("Ann", "ann2@example.com", "5551234567");
    EXPECT_NO_THROW(clientService.updateClient(c.id, "Ann B", "ann2@example.com", "5551234567"));
    EXPECT_NO_THROW(clientService.searchClients("Ann"));

    const SupportCase sc = caseService.createCase(c.id, "Help", "", Priority::Low);
    EXPECT_NO_THROW(caseService.assignCase(sc.id, agent.id));
    EXPECT_NO_THROW(caseService.changeStatus(sc.id, CaseStatus::InProgress, "start"));
    EXPECT_NO_THROW(caseService.getHistory(sc.id));
    EXPECT_NO_THROW(staffService.listStaff(false));   // read-only staff access
}

TEST_F(AuthorizationTest, AdministratorsCanArchiveAndManageStaff) {
    const Client c = addClient();
    EXPECT_NO_THROW(clientService.archiveClient(c.id));
    EXPECT_NO_THROW(clientService.restoreClient(c.id));
    EXPECT_NO_THROW(staffService.createStaff("newbie", "New", StaffRole::SupportAgent, "LongEnough1"));
}

TEST_F(AuthorizationTest, PermissionsFollowTheCurrentSessionNotTheEarlierOne) {
    const Client c = addClient();
    loginAs(admin);
    EXPECT_NO_THROW(clientService.archiveClient(c.id));
    loginAs(agent);   // switch user: admin rights must not linger
    EXPECT_THROW(clientService.restoreClient(c.id), AuthorizationError);
}

TEST_F(AuthorizationTest, HistoryRecordsTheActingUserNotAnArbitraryOne) {
    const Client c = addClient();
    loginAs(agent);
    const SupportCase sc = caseService.createCase(c.id, "T", "", Priority::Low);
    EXPECT_EQ(caseService.getHistory(sc.id)[0].changedByStaffId, agent.id);
}
