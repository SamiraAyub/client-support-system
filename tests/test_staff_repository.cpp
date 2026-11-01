#include <gtest/gtest.h>

#include "common/Exceptions.h"
#include "repositories/StaffRepository.h"
#include "TestHelpers.h"

using namespace csms;

namespace {

Staff makeStaff(const std::string& username, StaffRole role = StaffRole::SupportAgent) {
    Staff s;
    s.username = username;
    s.fullName = "Name " + username;
    s.role = role;
    s.passwordHash = "hash";
    s.createdAt = "2026-01-01T00:00:00Z";
    return s;
}

class StaffRepositoryTest : public test_support::InMemoryDbTest {
protected:
    StaffRepository repo{db};
};

} // namespace

TEST_F(StaffRepositoryTest, InsertAndFindRoundTripsAllFields) {
    const auto id = repo.insert(makeStaff("alice", StaffRole::Administrator));
    const auto found = repo.findById(id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->username, "alice");
    EXPECT_EQ(found->fullName, "Name alice");
    EXPECT_EQ(found->role, StaffRole::Administrator);
    EXPECT_EQ(found->passwordHash, "hash");
    EXPECT_TRUE(found->active);
    EXPECT_EQ(found->createdAt, "2026-01-01T00:00:00Z");
}

TEST_F(StaffRepositoryTest, FindByUsernameIgnoresCase) {
    const auto id = repo.insert(makeStaff("Alice"));
    ASSERT_TRUE(repo.findByUsername("aLiCe").has_value());
    EXPECT_EQ(repo.findByUsername("ALICE")->id, id);
    EXPECT_FALSE(repo.findByUsername("bob").has_value());
    EXPECT_FALSE(repo.findById(999).has_value());
}

TEST_F(StaffRepositoryTest, DuplicateUsernameThrowsDuplicateError) {
    repo.insert(makeStaff("alice"));
    EXPECT_THROW(repo.insert(makeStaff("ALICE")), DuplicateError);
}

TEST_F(StaffRepositoryTest, UpdateChangesNameAndRole) {
    const auto id = repo.insert(makeStaff("alice"));
    repo.update(id, "Alice Cooper", StaffRole::Administrator);
    const auto after = repo.findById(id);
    EXPECT_EQ(after->fullName, "Alice Cooper");
    EXPECT_EQ(after->role, StaffRole::Administrator);
    EXPECT_THROW(repo.update(999, "X", StaffRole::SupportAgent), NotFoundError);
}

TEST_F(StaffRepositoryTest, SetActiveAndPasswordHash) {
    const auto id = repo.insert(makeStaff("alice"));
    repo.setActive(id, false);
    EXPECT_FALSE(repo.findById(id)->active);
    repo.setActive(id, true);
    EXPECT_TRUE(repo.findById(id)->active);

    repo.setPasswordHash(id, "new-hash");
    EXPECT_EQ(repo.findById(id)->passwordHash, "new-hash");

    EXPECT_THROW(repo.setActive(999, true), NotFoundError);
    EXPECT_THROW(repo.setPasswordHash(999, "x"), NotFoundError);
}

TEST_F(StaffRepositoryTest, ListAllOrdersByUsernameAndCanHideInactive) {
    repo.insert(makeStaff("carol"));
    const auto bob = repo.insert(makeStaff("Bob"));
    repo.insert(makeStaff("alice"));
    repo.setActive(bob, false);

    const auto everyone = repo.listAll(true);
    ASSERT_EQ(everyone.size(), 3u);
    EXPECT_EQ(everyone[0].username, "alice");
    EXPECT_EQ(everyone[1].username, "Bob");
    EXPECT_EQ(everyone[2].username, "carol");

    EXPECT_EQ(repo.listAll(false).size(), 2u);
}

TEST_F(StaffRepositoryTest, Counts) {
    EXPECT_EQ(repo.countAll(), 0);
    repo.insert(makeStaff("admin1", StaffRole::Administrator));
    const auto admin2 = repo.insert(makeStaff("admin2", StaffRole::Administrator));
    repo.insert(makeStaff("agent1"));
    EXPECT_EQ(repo.countAll(), 3);
    EXPECT_EQ(repo.countActiveAdministrators(), 2);
    repo.setActive(admin2, false);
    EXPECT_EQ(repo.countActiveAdministrators(), 1);
}
