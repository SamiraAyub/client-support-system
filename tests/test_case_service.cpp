#include <gtest/gtest.h>

#include "common/Exceptions.h"
#include "ServiceFixture.h"

using namespace csms;

class CaseServiceTest : public test_support::ServiceFixture {};

// ------------------------------------------------------------------- creation

TEST_F(CaseServiceTest, CreateCaseStoresAllFieldsAndStartsOpenAndUnassigned) {
    const Client client = addClient();
    const SupportCase c = caseService.createCase(client.id, "  Printer down  ",
                                                 " It will not print ", Priority::High);
    EXPECT_GT(c.id, 0);
    EXPECT_EQ(c.clientId, client.id);
    EXPECT_EQ(c.title, "Printer down");
    EXPECT_EQ(c.description, "It will not print");
    EXPECT_EQ(c.priority, Priority::High);
    EXPECT_EQ(c.status, CaseStatus::Open);
    EXPECT_FALSE(c.assignedStaffId.has_value());
    EXPECT_FALSE(c.createdAt.empty());
    EXPECT_EQ(c.createdAt, c.updatedAt);

    const SupportCase stored = caseService.getCase(c.id);
    EXPECT_EQ(stored.title, "Printer down");
    EXPECT_EQ(stored.createdAt, c.createdAt);
}

TEST_F(CaseServiceTest, CaseIdsAreUnique) {
    const Client client = addClient();
    EXPECT_NE(addCase(client.id).id, addCase(client.id).id);
}

TEST_F(CaseServiceTest, DescriptionMayBeEmptyButTitleMayNot) {
    const Client client = addClient();
    EXPECT_NO_THROW(caseService.createCase(client.id, "Title", "", Priority::Low));
    EXPECT_THROW(caseService.createCase(client.id, "   ", "d", Priority::Low), ValidationError);
    EXPECT_THROW(caseService.createCase(client.id, std::string(kMaxTitleLength + 1, 'x'), "d",
                                        Priority::Low), ValidationError);
    EXPECT_THROW(caseService.createCase(client.id, "T", std::string(kMaxDescriptionLength + 1, 'x'),
                                        Priority::Low), ValidationError);
}

TEST_F(CaseServiceTest, CreateCaseForUnknownClientFailsAndStoresNothing) {
    EXPECT_THROW(addCase(9999), NotFoundError);
    EXPECT_EQ(countRows("SupportCases"), 0);
    EXPECT_EQ(countRows("CaseHistory"), 0);
}

TEST_F(CaseServiceTest, CreateCaseForArchivedClientIsRejected) {
    const Client client = addClient();
    clientRepo.setArchived(client.id, true);
    EXPECT_THROW(addCase(client.id), ValidationError);
    EXPECT_EQ(countRows("SupportCases"), 0);
}

TEST_F(CaseServiceTest, GetUnknownCaseThrowsNotFound) {
    EXPECT_THROW(caseService.getCase(12345), NotFoundError);
}

// ----------------------------------------------------------------- assignment

TEST_F(CaseServiceTest, AssignCaseToActiveSupportAgent) {
    const SupportCase c = addCase(addClient().id);
    const SupportCase assigned = caseService.assignCase(c.id, agent.id);
    EXPECT_EQ(assigned.assignedStaffId, agent.id);
    EXPECT_EQ(caseService.getCase(c.id).assignedStaffId, agent.id);
    EXPECT_EQ(assigned.status, CaseStatus::Open);   // assignment alone does not change status
}

TEST_F(CaseServiceTest, CaseCanBeReassignedToAnotherAgent) {
    const Staff other = addStaff("agent2", StaffRole::SupportAgent);
    const SupportCase c = addCase(addClient().id);
    caseService.assignCase(c.id, agent.id);
    EXPECT_EQ(caseService.assignCase(c.id, other.id).assignedStaffId, other.id);
}

TEST_F(CaseServiceTest, AssigningToTheSameAgentTwiceIsHarmless) {
    const SupportCase c = addCase(addClient().id);
    caseService.assignCase(c.id, agent.id);
    EXPECT_NO_THROW(caseService.assignCase(c.id, agent.id));
}

TEST_F(CaseServiceTest, DeactivatedStaffCannotReceiveNewAssignments) {
    const Staff gone = addStaff("gone", StaffRole::SupportAgent, /*active=*/false);
    const SupportCase c = addCase(addClient().id);
    EXPECT_THROW(caseService.assignCase(c.id, gone.id), ValidationError);
    EXPECT_FALSE(caseService.getCase(c.id).assignedStaffId.has_value());
}

TEST_F(CaseServiceTest, AdministratorsCannotBeAssignedCases) {
    const SupportCase c = addCase(addClient().id);
    EXPECT_THROW(caseService.assignCase(c.id, admin.id), ValidationError);
}

TEST_F(CaseServiceTest, AssignRejectsUnknownCaseOrStaff) {
    const SupportCase c = addCase(addClient().id);
    EXPECT_THROW(caseService.assignCase(9999, agent.id), NotFoundError);
    EXPECT_THROW(caseService.assignCase(c.id, 9999), NotFoundError);
}

TEST_F(CaseServiceTest, ClosedCasesCannotBeReassigned) {
    const Staff other = addStaff("agent2", StaffRole::SupportAgent);
    const SupportCase c = addCaseWithStatus(addClient().id, CaseStatus::Closed);
    EXPECT_THROW(caseService.assignCase(c.id, other.id), ValidationError);
}

TEST_F(CaseServiceTest, DeactivatingAnAgentKeepsHistoricalAssignments) {
    const SupportCase c = addCase(addClient().id);
    caseService.assignCase(c.id, agent.id);
    staffRepo.setActive(agent.id, false);

    EXPECT_EQ(caseService.getCase(c.id).assignedStaffId, agent.id);   // reference preserved
    EXPECT_TRUE(staffRepo.findById(agent.id).has_value());            // record preserved
}

// -------------------------------------------------------------- status changes

TEST_F(CaseServiceTest, FullHappyPathOpenToClosed) {
    const SupportCase c = addCase(addClient().id);
    caseService.assignCase(c.id, agent.id);

    EXPECT_EQ(caseService.changeStatus(c.id, CaseStatus::InProgress, "").status,
              CaseStatus::InProgress);
    EXPECT_EQ(caseService.changeStatus(c.id, CaseStatus::Resolved, "Fixed").status,
              CaseStatus::Resolved);
    EXPECT_EQ(caseService.changeStatus(c.id, CaseStatus::Closed, "Customer confirmed").status,
              CaseStatus::Closed);
}

TEST_F(CaseServiceTest, InProgressCanGoBackToOpen) {
    const SupportCase c = addCaseWithStatus(addClient().id, CaseStatus::InProgress);
    EXPECT_EQ(caseService.changeStatus(c.id, CaseStatus::Open, "Waiting for parts").status,
              CaseStatus::Open);
}

TEST_F(CaseServiceTest, InvalidTransitionsAreRejectedAndLeaveStatusUnchanged) {
    const Client client = addClient();
    {   // Open can only go to InProgress
        const SupportCase c = addCase(client.id);
        EXPECT_THROW(caseService.changeStatus(c.id, CaseStatus::Resolved, ""), InvalidTransitionError);
        EXPECT_THROW(caseService.changeStatus(c.id, CaseStatus::Closed, ""), InvalidTransitionError);
        EXPECT_THROW(caseService.changeStatus(c.id, CaseStatus::Open, ""), InvalidTransitionError);
        EXPECT_EQ(caseService.getCase(c.id).status, CaseStatus::Open);
    }
    {   // InProgress cannot jump to Closed
        const SupportCase c = addCaseWithStatus(client.id, CaseStatus::InProgress);
        EXPECT_THROW(caseService.changeStatus(c.id, CaseStatus::Closed, ""), InvalidTransitionError);
        EXPECT_EQ(caseService.getCase(c.id).status, CaseStatus::InProgress);
    }
    {   // Resolved cannot go back to InProgress
        const SupportCase c = addCaseWithStatus(client.id, CaseStatus::Resolved);
        EXPECT_THROW(caseService.changeStatus(c.id, CaseStatus::InProgress, ""),
                     InvalidTransitionError);
    }
}

TEST_F(CaseServiceTest, ClosedCasesAreFinal) {
    const SupportCase c = addCaseWithStatus(addClient().id, CaseStatus::Closed);
    for (const CaseStatus s : {CaseStatus::Open, CaseStatus::InProgress, CaseStatus::Resolved,
                               CaseStatus::Closed}) {
        EXPECT_THROW(caseService.changeStatus(c.id, s, ""), InvalidTransitionError);
    }
    EXPECT_THROW(caseService.reopenCase(c.id, "please"), InvalidTransitionError);
    EXPECT_EQ(caseService.getCase(c.id).status, CaseStatus::Closed);
}

TEST_F(CaseServiceTest, CannotStartWorkOnAnUnassignedCase) {
    const SupportCase c = addCase(addClient().id);
    try {
        caseService.changeStatus(c.id, CaseStatus::InProgress, "");
        FAIL() << "Expected InvalidTransitionError";
    } catch (const InvalidTransitionError& e) {
        EXPECT_NE(std::string(e.what()).find("Assign"), std::string::npos);
    }
    EXPECT_EQ(caseService.getCase(c.id).status, CaseStatus::Open);
}

TEST_F(CaseServiceTest, ErrorMessageListsTheAllowedMoves) {
    const SupportCase c = addCase(addClient().id);
    try {
        caseService.changeStatus(c.id, CaseStatus::Closed, "");
        FAIL() << "Expected InvalidTransitionError";
    } catch (const InvalidTransitionError& e) {
        EXPECT_NE(std::string(e.what()).find("In Progress"), std::string::npos);
    }
}

TEST_F(CaseServiceTest, StatusChangeRejectsUnknownCaseAndOverlongNote) {
    EXPECT_THROW(caseService.changeStatus(404, CaseStatus::InProgress, ""), NotFoundError);
    const SupportCase c = addCaseWithStatus(addClient().id, CaseStatus::InProgress);
    EXPECT_THROW(caseService.changeStatus(c.id, CaseStatus::Resolved,
                                          std::string(kMaxNoteLength + 1, 'x')),
                 ValidationError);
    EXPECT_EQ(caseService.getCase(c.id).status, CaseStatus::InProgress);
}

TEST_F(CaseServiceTest, StatusChangeUpdatesTheUpdatedTimestampButNotCreatedAt) {
    const SupportCase c = addCase(addClient().id);
    caseService.assignCase(c.id, agent.id);
    const SupportCase after = caseService.changeStatus(c.id, CaseStatus::InProgress, "");
    EXPECT_EQ(after.createdAt, c.createdAt);
    EXPECT_GE(after.updatedAt, c.updatedAt);
}

// --------------------------------------------------------------------- reopen

TEST_F(CaseServiceTest, ResolvedCaseCanBeReopenedExplicitly) {
    const SupportCase c = addCaseWithStatus(addClient().id, CaseStatus::Resolved);
    const SupportCase reopened = caseService.reopenCase(c.id, "Problem came back");
    EXPECT_EQ(reopened.status, CaseStatus::Open);
    EXPECT_EQ(reopened.assignedStaffId, agent.id);   // assignment is kept
}

TEST_F(CaseServiceTest, StatusChangeCannotBeUsedToReopen) {
    const SupportCase c = addCaseWithStatus(addClient().id, CaseStatus::Resolved);
    try {
        caseService.changeStatus(c.id, CaseStatus::Open, "oops");
        FAIL() << "Expected InvalidTransitionError";
    } catch (const InvalidTransitionError& e) {
        EXPECT_NE(std::string(e.what()).find("reopen"), std::string::npos);
    }
    EXPECT_EQ(caseService.getCase(c.id).status, CaseStatus::Resolved);
}

TEST_F(CaseServiceTest, OnlyResolvedCasesCanBeReopened) {
    const Client client = addClient();
    EXPECT_THROW(caseService.reopenCase(addCase(client.id).id, "why"), InvalidTransitionError);
    EXPECT_THROW(caseService.reopenCase(addCaseWithStatus(client.id, CaseStatus::InProgress).id, "why"),
                 InvalidTransitionError);
    EXPECT_THROW(caseService.reopenCase(404, "why"), NotFoundError);
}

TEST_F(CaseServiceTest, ReopenRequiresAReason) {
    const SupportCase c = addCaseWithStatus(addClient().id, CaseStatus::Resolved);
    EXPECT_THROW(caseService.reopenCase(c.id, ""), ValidationError);
    EXPECT_THROW(caseService.reopenCase(c.id, "   "), ValidationError);
    EXPECT_EQ(caseService.getCase(c.id).status, CaseStatus::Resolved);
}

TEST_F(CaseServiceTest, ReopenedCaseCanBeWorkedAgain) {
    const SupportCase c = addCaseWithStatus(addClient().id, CaseStatus::Resolved);
    caseService.reopenCase(c.id, "Came back");
    EXPECT_NO_THROW(caseService.changeStatus(c.id, CaseStatus::InProgress, ""));
}

// --------------------------------------------------------------------- search

TEST_F(CaseServiceTest, SearchFiltersByIdClientStatusPriorityAndAgent) {
    const Client c1 = addClient("one@example.com");
    const Client c2 = addClient("two@example.com");
    const SupportCase a = addCase(c1.id, Priority::Low);
    const SupportCase b = addCase(c1.id, Priority::High);
    const SupportCase c = addCase(c2.id, Priority::High);
    caseService.assignCase(b.id, agent.id);
    caseService.changeStatus(b.id, CaseStatus::InProgress, "");

    CaseFilter f;
    f.caseId = a.id;
    ASSERT_EQ(caseService.searchCases(f).size(), 1u);

    f = {}; f.clientId = c1.id;
    EXPECT_EQ(caseService.searchCases(f).size(), 2u);

    f = {}; f.status = CaseStatus::InProgress;
    ASSERT_EQ(caseService.searchCases(f).size(), 1u);
    EXPECT_EQ(caseService.searchCases(f)[0].id, b.id);

    f = {}; f.priority = Priority::High;
    EXPECT_EQ(caseService.searchCases(f).size(), 2u);

    f = {}; f.assignedStaffId = agent.id;
    ASSERT_EQ(caseService.searchCases(f).size(), 1u);

    f = {}; f.clientId = c2.id; f.priority = Priority::High;
    ASSERT_EQ(caseService.searchCases(f).size(), 1u);
    EXPECT_EQ(caseService.searchCases(f)[0].id, c.id);

    EXPECT_EQ(caseService.searchCases({}).size(), 3u);
}
