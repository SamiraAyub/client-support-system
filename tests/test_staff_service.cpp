#include <gtest/gtest.h>

#include "common/Exceptions.h"
#include "ServiceFixture.h"

using namespace csms;
using test_support::kTestPassword;

class StaffServiceTest : public test_support::ServiceFixture {
protected:
    // Raw column value, to prove what is really stored in the database.
    std::string storedHash(std::int64_t id) {
        Statement s = db.prepare("SELECT password_hash FROM Staff WHERE id = ?1");
        s.bindInt(1, id);
        s.step();
        return s.columnText(0);
    }
};

// ------------------------------------------------------------------ creating

TEST_F(StaffServiceTest, AdministratorCanCreateStaffOfEitherRole) {
    const Staff agentUser = staffService.createStaff("jane.doe", "Jane Doe", StaffRole::SupportAgent,
                                                     "LongEnough1");
    const Staff boss = staffService.createStaff("boss2", "Second Boss", StaffRole::Administrator,
                                                "LongEnough1");
    EXPECT_GT(agentUser.id, 0);
    EXPECT_EQ(agentUser.role, StaffRole::SupportAgent);
    EXPECT_TRUE(agentUser.active);
    EXPECT_EQ(boss.role, StaffRole::Administrator);
    EXPECT_EQ(staffService.getStaff(agentUser.id).fullName, "Jane Doe");
}

TEST_F(StaffServiceTest, OnlyThePasswordHashIsStoredNeverThePlaintext) {
    const Staff s = staffService.createStaff("jane", "Jane Doe", StaffRole::SupportAgent,
                                             "MySecretPass1");
    const std::string stored = storedHash(s.id);
    EXPECT_NE(stored, "MySecretPass1");
    EXPECT_EQ(stored.find("MySecretPass1"), std::string::npos);
    EXPECT_EQ(stored.rfind("$argon2id$", 0), 0u);
    EXPECT_TRUE(hasher.verify(stored, "MySecretPass1"));
}

TEST_F(StaffServiceTest, CreateRejectsDuplicateUsernamesCaseInsensitively) {
    staffService.createStaff("jane", "Jane", StaffRole::SupportAgent, "LongEnough1");
    EXPECT_THROW(staffService.createStaff("JANE", "Other", StaffRole::SupportAgent, "LongEnough1"),
                 DuplicateError);
    EXPECT_THROW(staffService.createStaff("admin", "Other", StaffRole::SupportAgent, "LongEnough1"),
                 DuplicateError);   // the fixture's administrator
}

TEST_F(StaffServiceTest, CreateValidatesUsernameNameAndPassword) {
    EXPECT_THROW(staffService.createStaff("ab", "Name", StaffRole::SupportAgent, "LongEnough1"),
                 ValidationError);                                   // too short
    EXPECT_THROW(staffService.createStaff("bad name", "Name", StaffRole::SupportAgent, "LongEnough1"),
                 ValidationError);                                   // space
    EXPECT_THROW(staffService.createStaff("-lead", "Name", StaffRole::SupportAgent, "LongEnough1"),
                 ValidationError);                                   // starts with symbol
    EXPECT_THROW(staffService.createStaff("okname", "  ", StaffRole::SupportAgent, "LongEnough1"),
                 ValidationError);
    EXPECT_THROW(staffService.createStaff("okname", "Name", StaffRole::SupportAgent, "short"),
                 ValidationError);                                   // password too short
    EXPECT_THROW(staffService.createStaff("okname", "Name", StaffRole::SupportAgent,
                                          std::string(kMaxPasswordLength + 1, 'x')),
                 ValidationError);
    EXPECT_EQ(staffRepo.countAll(), 2);                              // only fixture staff exist
}

// ----------------------------------------------------------------- bootstrap

TEST(StaffServiceBootstrapTest, FirstAdministratorCanBeCreatedWithoutLoginButOnlyOnce) {
    Database db(":memory:");
    db.initializeSchema();
    StaffRepository repo(db);
    PasswordHasher hasher = PasswordHasher::lowCostForTests();
    Session session;   // nobody logged in
    StaffService service(repo, hasher, session);

    EXPECT_FALSE(service.hasStaff());
    const Staff first = service.bootstrapAdministrator("root", "Root User", "LongEnough1");
    EXPECT_EQ(first.role, StaffRole::Administrator);
    EXPECT_TRUE(service.hasStaff());

    EXPECT_THROW(service.bootstrapAdministrator("root2", "Another", "LongEnough1"),
                 AuthorizationError);
    EXPECT_EQ(repo.countAll(), 1);
}

TEST(StaffServiceBootstrapTest, FailedBootstrapValidationLeavesSystemUninitialised) {
    Database db(":memory:");
    db.initializeSchema();
    StaffRepository repo(db);
    PasswordHasher hasher = PasswordHasher::lowCostForTests();
    Session session;
    StaffService service(repo, hasher, session);

    EXPECT_THROW(service.bootstrapAdministrator("root", "Root", "short"), ValidationError);
    EXPECT_FALSE(service.hasStaff());
}

// ------------------------------------------------------------------ updating

TEST_F(StaffServiceTest, UpdateChangesNameAndRoleOfAnotherUser) {
    const Staff updated = staffService.updateStaff(agent.id, "Agent Smith", StaffRole::Administrator);
    EXPECT_EQ(updated.fullName, "Agent Smith");
    EXPECT_EQ(staffService.getStaff(agent.id).role, StaffRole::Administrator);
}

TEST_F(StaffServiceTest, AdministratorCannotChangeOwnRoleButCanRenameSelf) {
    EXPECT_THROW(staffService.updateStaff(admin.id, "Admin", StaffRole::SupportAgent),
                 ValidationError);
    EXPECT_NO_THROW(staffService.updateStaff(admin.id, "New Admin Name", StaffRole::Administrator));
    EXPECT_EQ(staffService.getStaff(admin.id).fullName, "New Admin Name");
}

TEST_F(StaffServiceTest, LastActiveAdministratorCannotBeDemoted) {
    Staff otherAdmin;            // a session user who is not in the DB as an active admin
    otherAdmin.id = 4242;
    otherAdmin.role = StaffRole::Administrator;
    loginAs(otherAdmin);
    EXPECT_THROW(staffService.updateStaff(admin.id, "Admin", StaffRole::SupportAgent),
                 ValidationError);
    EXPECT_EQ(staffService.getStaff(admin.id).role, StaffRole::Administrator);
}

TEST_F(StaffServiceTest, UpdateUnknownStaffThrowsNotFound) {
    EXPECT_THROW(staffService.updateStaff(999, "X", StaffRole::SupportAgent), NotFoundError);
}

// ------------------------------------------------------------ (de)activation

TEST_F(StaffServiceTest, DeactivateThenReactivate) {
    staffService.deactivateStaff(agent.id);
    EXPECT_FALSE(staffService.getStaff(agent.id).active);
    EXPECT_EQ(staffService.listStaff(false).size(), 1u);   // hidden from the active list
    EXPECT_EQ(staffService.listStaff(true).size(), 2u);    // but never deleted

    staffService.reactivateStaff(agent.id);
    EXPECT_TRUE(staffService.getStaff(agent.id).active);
}

TEST_F(StaffServiceTest, CannotDeactivateYourselfOrDeactivateTwice) {
    EXPECT_THROW(staffService.deactivateStaff(admin.id), ValidationError);
    staffService.deactivateStaff(agent.id);
    EXPECT_THROW(staffService.deactivateStaff(agent.id), ValidationError);
    EXPECT_THROW(staffService.reactivateStaff(admin.id), ValidationError);   // already active
    EXPECT_THROW(staffService.deactivateStaff(999), NotFoundError);
}

TEST_F(StaffServiceTest, LastActiveAdministratorCannotBeDeactivated) {
    Staff otherAdmin;
    otherAdmin.id = 4242;
    otherAdmin.role = StaffRole::Administrator;
    loginAs(otherAdmin);
    EXPECT_THROW(staffService.deactivateStaff(admin.id), ValidationError);
    EXPECT_TRUE(staffService.getStaff(admin.id).active);
}

TEST_F(StaffServiceTest, DeactivatedStaffRemainInTheDatabaseForHistory) {
    const SupportCase c = addCaseWithStatus(addClient().id, CaseStatus::InProgress);
    staffService.deactivateStaff(agent.id);
    EXPECT_TRUE(staffRepo.findById(agent.id).has_value());
    EXPECT_EQ(caseService.getCase(c.id).assignedStaffId, agent.id);
    EXPECT_EQ(caseService.getHistory(c.id).back().changedByStaffId, admin.id);
}

// ----------------------------------------------------------------- passwords

TEST_F(StaffServiceTest, ResetPasswordChangesWhatLoginAccepts) {
    staffService.resetPassword(agent.id, "BrandNewPass1");
    session.end();
    EXPECT_THROW(authService.login("agent", kTestPassword), AuthenticationError);
    EXPECT_NO_THROW(authService.login("agent", "BrandNewPass1"));
}

TEST_F(StaffServiceTest, ResetPasswordValidatesAndRejectsUnknownStaff) {
    EXPECT_THROW(staffService.resetPassword(agent.id, "short"), ValidationError);
    EXPECT_THROW(staffService.resetPassword(999, "BrandNewPass1"), NotFoundError);
}

TEST_F(StaffServiceTest, ChangeOwnPasswordRequiresTheCurrentPassword) {
    loginAs(*staffRepo.findById(agent.id));
    EXPECT_THROW(staffService.changeOwnPassword("wrong-password", "BrandNewPass1"),
                 AuthenticationError);
    EXPECT_THROW(staffService.changeOwnPassword(kTestPassword, "short"), ValidationError);

    staffService.changeOwnPassword(kTestPassword, "BrandNewPass1");
    session.end();
    EXPECT_NO_THROW(authService.login("agent", "BrandNewPass1"));
}
