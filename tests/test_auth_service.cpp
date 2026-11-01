#include <gtest/gtest.h>

#include <string>

#include "common/Exceptions.h"
#include "ServiceFixture.h"

using namespace csms;
using test_support::kTestPassword;

class AuthServiceTest : public test_support::ServiceFixture {
protected:
    void SetUp() override {
        ServiceFixture::SetUp();
        session.end();   // start every test logged out
    }
};

TEST_F(AuthServiceTest, ValidCredentialsStartASession) {
    const Staff user = authService.login("agent", kTestPassword);
    EXPECT_EQ(user.id, agent.id);
    EXPECT_EQ(user.role, StaffRole::SupportAgent);
    EXPECT_TRUE(session.isLoggedIn());
    EXPECT_EQ(session.requireLogin().id, agent.id);
}

TEST_F(AuthServiceTest, ReturnedUserAndSessionNeverContainThePasswordHash) {
    const Staff user = authService.login("agent", kTestPassword);
    EXPECT_TRUE(user.passwordHash.empty());
    EXPECT_TRUE(session.requireLogin().passwordHash.empty());
}

TEST_F(AuthServiceTest, UsernameIsCaseInsensitiveAndTrimmed) {
    EXPECT_NO_THROW(authService.login("  AGENT ", kTestPassword));
}

TEST_F(AuthServiceTest, WrongPasswordIsRejectedAndNoSessionStarts) {
    EXPECT_THROW(authService.login("agent", "wrong-password"), AuthenticationError);
    EXPECT_FALSE(session.isLoggedIn());
}

TEST_F(AuthServiceTest, PasswordIsCaseSensitive) {
    EXPECT_THROW(authService.login("agent", "correcthorse9!"), AuthenticationError);
}

TEST_F(AuthServiceTest, UnknownUserAndWrongPasswordGiveTheSameMessage) {
    std::string unknownMessage, wrongPasswordMessage;
    try { authService.login("nobody", kTestPassword); } catch (const AuthenticationError& e) {
        unknownMessage = e.what();
    }
    try { authService.login("agent", "wrong-password"); } catch (const AuthenticationError& e) {
        wrongPasswordMessage = e.what();
    }
    EXPECT_FALSE(unknownMessage.empty());
    EXPECT_EQ(unknownMessage, wrongPasswordMessage);   // no username enumeration
}

TEST_F(AuthServiceTest, DeactivatedStaffCannotLogInEvenWithTheRightPassword) {
    staffRepo.setActive(agent.id, false);
    try {
        authService.login("agent", kTestPassword);
        FAIL() << "Expected AuthenticationError";
    } catch (const AuthenticationError& e) {
        EXPECT_EQ(std::string(e.what()), "Invalid username or password");
    }
    EXPECT_FALSE(session.isLoggedIn());
}

TEST_F(AuthServiceTest, EmptyCredentialsAreRejected) {
    EXPECT_THROW(authService.login("", ""), AuthenticationError);
    EXPECT_THROW(authService.login("agent", ""), AuthenticationError);
}

TEST_F(AuthServiceTest, LogoutEndsTheSessionAndBlocksFurtherOperations) {
    authService.login("admin", kTestPassword);
    ASSERT_TRUE(session.isLoggedIn());
    authService.logout();
    EXPECT_FALSE(session.isLoggedIn());
    EXPECT_THROW(clientService.listClients(), AuthorizationError);
    EXPECT_THROW(caseService.searchCases({}), AuthorizationError);
}

TEST_F(AuthServiceTest, LoggingInAsAnotherUserReplacesTheSession) {
    authService.login("agent", kTestPassword);
    authService.login("admin", kTestPassword);
    EXPECT_EQ(session.requireLogin().id, admin.id);
}

TEST_F(AuthServiceTest, FailedLoginDoesNotEndAnExistingSession) {
    authService.login("agent", kTestPassword);
    EXPECT_THROW(authService.login("admin", "wrong-password"), AuthenticationError);
    EXPECT_EQ(session.requireLogin().id, agent.id);
}

TEST(AuthPersistenceTest, AccountsAndPasswordsSurviveARestart) {
    test_support::TempDbPath file;
    PasswordHasher hasher = PasswordHasher::lowCostForTests();
    {
        Database db(file.str());
        db.initializeSchema();
        StaffRepository repo(db);
        Session session;
        StaffService staff(repo, hasher, session);
        staff.bootstrapAdministrator("root", "Root User", "LongEnough1");
    }
    {
        Database db(file.str());
        db.initializeSchema();
        StaffRepository repo(db);
        Session session;
        AuthService auth(repo, hasher, session);
        EXPECT_NO_THROW(auth.login("root", "LongEnough1"));
        EXPECT_THROW(auth.login("root", "wrong"), AuthenticationError);
    }
}
