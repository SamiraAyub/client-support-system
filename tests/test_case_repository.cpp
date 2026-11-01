#include <gtest/gtest.h>

#include "common/Exceptions.h"
#include "repositories/CaseRepository.h"
#include "repositories/ClientRepository.h"
#include "repositories/StaffRepository.h"
#include "TestHelpers.h"

using namespace csms;

namespace {

class CaseRepositoryTest : public test_support::InMemoryDbTest {
protected:
    void SetUp() override {
        InMemoryDbTest::SetUp();
        clientA = addClient("a@example.com");
        clientB = addClient("b@example.com");
        Staff s;
        s.username = "agent";
        s.fullName = "Agent";
        s.passwordHash = "h";
        s.createdAt = "2026-01-01T00:00:00Z";
        agentId = staff.insert(s);
    }

    std::int64_t addClient(const std::string& email) {
        Client c;
        c.name = "Client";
        c.email = email;
        c.phone = "5551234567";
        c.registeredAt = "2026-01-01T00:00:00Z";
        return clients.insert(c);
    }

    SupportCase make(std::int64_t clientId, Priority p = Priority::Medium,
                     const std::string& title = "Title") {
        SupportCase c;
        c.clientId = clientId;
        c.title = title;
        c.description = "Description";
        c.priority = p;
        c.status = CaseStatus::Open;
        c.createdAt = c.updatedAt = "2026-01-01T00:00:00Z";
        return c;
    }

    ClientRepository clients{db};
    StaffRepository staff{db};
    CaseRepository repo{db};
    std::int64_t clientA = 0, clientB = 0, agentId = 0;
};

} // namespace

TEST_F(CaseRepositoryTest, InsertAndFindRoundTripsAllFields) {
    SupportCase c = make(clientA, Priority::High, "Server down");
    c.assignedStaffId = agentId;
    const auto id = repo.insert(c);

    const auto found = repo.findById(id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->clientId, clientA);
    EXPECT_EQ(found->title, "Server down");
    EXPECT_EQ(found->description, "Description");
    EXPECT_EQ(found->priority, Priority::High);
    EXPECT_EQ(found->status, CaseStatus::Open);
    EXPECT_EQ(found->assignedStaffId, agentId);
    EXPECT_EQ(found->createdAt, "2026-01-01T00:00:00Z");
}

TEST_F(CaseRepositoryTest, UnassignedCaseHasNoAssignee) {
    const auto id = repo.insert(make(clientA));
    EXPECT_FALSE(repo.findById(id)->assignedStaffId.has_value());
}

TEST_F(CaseRepositoryTest, UnknownIdReturnsEmpty) {
    EXPECT_FALSE(repo.findById(404).has_value());
}

TEST_F(CaseRepositoryTest, InsertWithUnknownClientViolatesForeignKey) {
    try {
        repo.insert(make(/*clientId=*/9999));
        FAIL() << "Expected DatabaseError";
    } catch (const DatabaseError& e) {
        EXPECT_TRUE(e.isConstraintViolation());
        EXPECT_FALSE(e.isUniqueViolation());
    }
}

TEST_F(CaseRepositoryTest, InsertWithUnknownAssigneeViolatesForeignKey) {
    SupportCase c = make(clientA);
    c.assignedStaffId = 9999;
    EXPECT_THROW(repo.insert(c), DatabaseError);
}

TEST_F(CaseRepositoryTest, UpdateStatusAndAssignment) {
    const auto id = repo.insert(make(clientA));
    repo.updateStatus(id, CaseStatus::InProgress, "2026-02-02T00:00:00Z");
    repo.updateAssignment(id, agentId, "2026-03-03T00:00:00Z");

    const auto c = repo.findById(id);
    EXPECT_EQ(c->status, CaseStatus::InProgress);
    EXPECT_EQ(c->assignedStaffId, agentId);
    EXPECT_EQ(c->updatedAt, "2026-03-03T00:00:00Z");
    EXPECT_EQ(c->createdAt, "2026-01-01T00:00:00Z");

    repo.updateAssignment(id, std::nullopt, "2026-04-04T00:00:00Z");
    EXPECT_FALSE(repo.findById(id)->assignedStaffId.has_value());
}

TEST_F(CaseRepositoryTest, UpdatingUnknownCaseThrowsNotFound) {
    EXPECT_THROW(repo.updateStatus(1, CaseStatus::Closed, "t"), NotFoundError);
    EXPECT_THROW(repo.updateAssignment(1, std::nullopt, "t"), NotFoundError);
}

TEST_F(CaseRepositoryTest, EmptyFilterReturnsEveryCaseInIdOrder) {
    const auto a = repo.insert(make(clientA));
    const auto b = repo.insert(make(clientB));
    const auto all = repo.search({});
    ASSERT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0].id, a);
    EXPECT_EQ(all[1].id, b);
}

TEST_F(CaseRepositoryTest, FilterByEachCriterion) {
    const auto c1 = repo.insert(make(clientA, Priority::Low));
    const auto c2 = repo.insert(make(clientA, Priority::High));
    const auto c3 = repo.insert(make(clientB, Priority::High));
    repo.updateStatus(c2, CaseStatus::InProgress, "t");
    repo.updateAssignment(c3, agentId, "t");

    CaseFilter byId; byId.caseId = c2;
    ASSERT_EQ(repo.search(byId).size(), 1u);
    EXPECT_EQ(repo.search(byId)[0].id, c2);

    CaseFilter byClient; byClient.clientId = clientA;
    EXPECT_EQ(repo.search(byClient).size(), 2u);

    CaseFilter byStatus; byStatus.status = CaseStatus::InProgress;
    ASSERT_EQ(repo.search(byStatus).size(), 1u);
    EXPECT_EQ(repo.search(byStatus)[0].id, c2);

    CaseFilter byPriority; byPriority.priority = Priority::High;
    EXPECT_EQ(repo.search(byPriority).size(), 2u);

    CaseFilter byAgent; byAgent.assignedStaffId = agentId;
    ASSERT_EQ(repo.search(byAgent).size(), 1u);
    EXPECT_EQ(repo.search(byAgent)[0].id, c3);

    (void)c1;
}

TEST_F(CaseRepositoryTest, FiltersCombineWithAnd) {
    repo.insert(make(clientA, Priority::High));
    const auto target = repo.insert(make(clientB, Priority::High));
    repo.insert(make(clientB, Priority::Low));

    CaseFilter f;
    f.clientId = clientB;
    f.priority = Priority::High;
    const auto result = repo.search(f);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].id, target);

    f.status = CaseStatus::Closed;
    EXPECT_TRUE(repo.search(f).empty());
}
