// End-to-end CLI tests: scripted keyboard input goes in, terminal text comes
// out, and the database is checked afterwards. Nothing is mocked.

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

#include "cli/Application.h"
#include "cli/Console.h"
#include "cli/Format.h"
#include "common/Exceptions.h"
#include "repositories/ReportRepository.h"
#include "services/ReportService.h"
#include "ServiceFixture.h"

using namespace csms;
using test_support::kTestPassword;

namespace {

// Joins lines into the text a user would type (each followed by Enter).
std::string typed(const std::vector<std::string>& lines) {
    std::string s;
    for (const auto& l : lines) s += l + "\n";
    return s;
}

class CliTest : public test_support::InMemoryDbTest {
protected:
    // Runs the application with the given keystrokes and returns everything it printed.
    std::string run(const std::vector<std::string>& lines) {
        std::istringstream in(typed(lines));
        std::ostringstream out;
        Console console(in, out);
        Application app(console, {session, auth, staff, clients, cases, reports});
        app.run();
        return out.str();
    }

    void seedAdmin() { staff.bootstrapAdministrator("root", "Root User", kTestPassword); }

    void seedAgent() {
        session.start(*staffRepo.findByUsername("root"));
        staff.createStaff("jane", "Jane Doe", StaffRole::SupportAgent, "JanePass123");
        session.end();
    }

    static bool contains(const std::string& text, const std::string& part) {
        return text.find(part) != std::string::npos;
    }

    PasswordHasher hasher = PasswordHasher::lowCostForTests();
    Session session;
    ClientRepository clientRepo{db};
    StaffRepository staffRepo{db};
    CaseRepository caseRepo{db};
    HistoryRepository historyRepo{db};
    ReportRepository reportRepo{db};
    AuthService auth{staffRepo, hasher, session};
    StaffService staff{staffRepo, hasher, session};
    ClientService clients{clientRepo, session};
    CaseService cases{db, caseRepo, historyRepo, clientRepo, staffRepo, session};
    ReportService reports{reportRepo, session};
};

} // namespace

// ------------------------------------------------------ console helpers

TEST(ParseIntegerTest, AcceptsWholeNumbersOnly) {
    EXPECT_EQ(parseInteger("42"), 42);
    EXPECT_EQ(parseInteger("-7"), -7);
    EXPECT_FALSE(parseInteger("").has_value());
    EXPECT_FALSE(parseInteger("12abc").has_value());
    EXPECT_FALSE(parseInteger("abc").has_value());
    EXPECT_FALSE(parseInteger("1.5").has_value());
    EXPECT_FALSE(parseInteger("99999999999999999999999").has_value());   // overflow
}

TEST(ConsoleTest, AskIntRepromptsUntilValid) {
    std::istringstream in("abc\n\n12x\n7\n");
    std::ostringstream out;
    Console console(in, out);
    EXPECT_EQ(console.askInt("ID"), 7);
    EXPECT_NE(out.str().find("whole number"), std::string::npos);
}

TEST(ConsoleTest, AskOptionalIntAllowsBlank) {
    std::istringstream in("\n5\n");
    std::ostringstream out;
    Console console(in, out);
    EXPECT_FALSE(console.askOptionalInt("ID").has_value());
    EXPECT_EQ(console.askOptionalInt("ID"), 5);
}

TEST(ConsoleTest, ConfirmRepromptsUntilYesOrNo) {
    std::istringstream in("maybe\nY\nno\n");
    std::ostringstream out;
    Console console(in, out);
    EXPECT_TRUE(console.confirm("Sure?"));
    EXPECT_FALSE(console.confirm("Sure?"));
}

TEST(ConsoleTest, AskChoiceRejectsOutOfRangeAndText) {
    std::istringstream in("9\nfoo\n-1\n2\n");
    std::ostringstream out;
    Console console(in, out);
    EXPECT_EQ(console.askChoice("Menu", {"a", "b"}), 2);
    EXPECT_NE(out.str().find("Invalid choice"), std::string::npos);
}

TEST(ConsoleTest, EndOfInputThrowsInputClosedError) {
    std::istringstream in("");
    std::ostringstream out;
    Console console(in, out);
    EXPECT_THROW(console.ask("Name"), InputClosedError);
}

TEST(ConsoleTest, NewPasswordMustBeRepeatedIdentically) {
    {
        std::istringstream in("Secret123\nSecret123\n");
        std::ostringstream out;
        Console console(in, out);
        EXPECT_EQ(console.askNewPassword(), "Secret123");
    }
    {
        std::istringstream in("Secret123\nOther1234\n");
        std::ostringstream out;
        Console console(in, out);
        EXPECT_THROW(console.askNewPassword(), ValidationError);
    }
}

TEST(ConsoleTest, RunGuardedPrintsDomainErrorsInsteadOfThrowing) {
    std::istringstream in("");
    std::ostringstream out;
    Console console(in, out);
    EXPECT_NO_THROW(console.runGuarded([] { throw NotFoundError("Client 9 does not exist"); }));
    EXPECT_NO_THROW(console.runGuarded([] { throw std::runtime_error("boom"); }));
    EXPECT_NE(out.str().find("Error: Client 9 does not exist"), std::string::npos);
    EXPECT_NE(out.str().find("Unexpected error: boom"), std::string::npos);
    EXPECT_THROW(console.runGuarded([] { throw InputClosedError(); }), InputClosedError);
}

// --------------------------------------------------------- formatting

TEST(FormatTest, DisplayTimeIsReadable) {
    EXPECT_EQ(displayTime("2026-11-01T14:30:00Z"), "2026-11-01 14:30 UTC");
    EXPECT_EQ(displayTime("garbage"), "garbage");
}

TEST(FormatTest, LongTextIsTruncatedWithoutBreakingTheLayout) {
    Client c;
    c.id = 1;
    c.name = std::string(200, 'x');
    c.email = "a@example.com";
    c.phone = "5551234567";
    c.registeredAt = "2026-01-01T00:00:00Z";
    std::ostringstream out;
    printClients(out, {c});
    EXPECT_NE(out.str().find(".."), std::string::npos);
    EXPECT_EQ(out.str().find(std::string(50, 'x')), std::string::npos);
}

TEST(FormatTest, EmptyListsSayNothingWasFound) {
    std::ostringstream out;
    printClients(out, {});
    printCases(out, {}, Names{});
    EXPECT_NE(out.str().find("No clients found"), std::string::npos);
    EXPECT_NE(out.str().find("No cases found"), std::string::npos);
}

// ------------------------------------------------------ application flows

TEST_F(CliTest, FirstRunSetupCreatesAnAdministratorWhoCanThenLogIn) {
    const std::string out = run({
        "root", "Root User", "LongEnough1", "LongEnough1",   // setup
        "root", "LongEnough1",                                // login
        "0",                                                  // log out
        "",                                                   // quit at login prompt
    });
    EXPECT_TRUE(contains(out, "First-time setup"));
    EXPECT_TRUE(contains(out, "Administrator 'root' created"));
    EXPECT_TRUE(contains(out, "Welcome, Root User."));
    EXPECT_TRUE(contains(out, "Logged out."));
    EXPECT_TRUE(contains(out, "Goodbye."));
    EXPECT_EQ(staffRepo.countActiveAdministrators(), 1);
}

TEST_F(CliTest, SetupRetriesAfterInvalidInputInsteadOfCrashing) {
    const std::string out = run({
        "root", "Root User", "short1", "short1",            // password too short
        "root", "Root User", "LongEnough1", "LongEnough1",  // second try works
        "",                                                 // quit
    });
    EXPECT_TRUE(contains(out, "Error: Password must be at least 8 characters"));
    EXPECT_TRUE(contains(out, "Administrator 'root' created"));
}

TEST_F(CliTest, SetupDoesNotAppearWhenAccountsAlreadyExist) {
    seedAdmin();
    const std::string out = run({""});
    EXPECT_FALSE(contains(out, "First-time setup"));
    EXPECT_TRUE(contains(out, "--- Login ---"));
}

TEST_F(CliTest, FullSupportWorkflowThroughTheMenus) {
    seedAdmin();
    const std::string out = run({
        "root", kTestPassword,
        // Staff management -> create an agent
        "4", "2", "jane", "Jane Doe", "1", "JanePass123", "JanePass123", "0",
        // Clients -> register
        "1", "1", "Ann Lee", "ann@example.com", "555-123-4567", "0",
        // Cases -> create (client 1, Medium), assign to staff 2, start work, view
        "2", "1", "1", "Printer down", "Will not print", "2",
        "4", "1", "2",
        "5", "1", "1", "Started diagnosis",
        "2", "1",
        "0",
        // Reports
        "3",
        "0",   // log out
        "",    // quit
    });

    EXPECT_TRUE(contains(out, "Created Support Agent 'jane'"));
    EXPECT_TRUE(contains(out, "Registered client #1: Ann Lee"));
    EXPECT_TRUE(contains(out, "Created case #1"));
    EXPECT_TRUE(contains(out, "now assigned to jane"));
    EXPECT_TRUE(contains(out, "Case #1 is now In Progress."));
    EXPECT_TRUE(contains(out, "Printer down"));
    EXPECT_TRUE(contains(out, "Case created"));            // history entry 1
    EXPECT_TRUE(contains(out, "Open -> In Progress"));     // history entry 2
    EXPECT_TRUE(contains(out, "Started diagnosis"));
    EXPECT_TRUE(contains(out, "Support System Report"));

    // The data really is in the database.
    session.start(*staffRepo.findByUsername("root"));
    const SupportCase c = cases.getCase(1);
    EXPECT_EQ(c.status, CaseStatus::InProgress);
    EXPECT_EQ(cases.getHistory(1).size(), 2u);
    EXPECT_EQ(clients.listClients().size(), 1u);
}

TEST_F(CliTest, InvalidMenuInputAndBadIdsAreReportedWithoutCrashing) {
    seedAdmin();
    const std::string out = run({
        "root", kTestPassword,
        "abc", "99",                 // junk in the main menu
        "1", "x",                    // clients menu, then junk inside it
        "2", "999",                  // view client 999 (does not exist)
        "0",                         // back to the main menu
        "2", "2", "404",             // cases menu -> view case 404 (does not exist)
        "0",                         // back to the main menu
        "0",                         // log out
        "",                          // quit
    });
    EXPECT_TRUE(contains(out, "Invalid choice"));
    EXPECT_TRUE(contains(out, "Error: Client 999 does not exist"));
    EXPECT_TRUE(contains(out, "Error: Case 404 does not exist"));
    EXPECT_TRUE(contains(out, "Goodbye."));
}

TEST_F(CliTest, InvalidClientDataShowsValidationMessagesAndSavesNothing) {
    seedAdmin();
    const std::string out = run({
        "root", kTestPassword,
        "1",                                              // main menu: clients
        "1", "Ann Lee", "not-an-email", "555-123-4567",   // register -> bad email
        "1", "Ann Lee", "ann@example.com", "abc",         // register -> bad phone
        "0",                                              // back
        "0",                                              // log out
        "",                                               // quit
    });
    EXPECT_TRUE(contains(out, "Error: Email must contain exactly one '@'"));
    EXPECT_TRUE(contains(out, "Error: Phone number may contain only digits"));
    session.start(*staffRepo.findByUsername("root"));
    EXPECT_TRUE(clients.listClients(true).empty());
}

TEST_F(CliTest, InvalidStatusTransitionIsExplainedAndNotApplied) {
    seedAdmin();
    seedAgent();
    session.start(*staffRepo.findByUsername("root"));
    const Client c = clients.registerClient("Ann", "ann@example.com", "5551234567");
    cases.createCase(c.id, "Broken", "", Priority::High);
    session.end();

    // An unassigned Open case cannot start work.
    const std::string out = run({
        "root", kTestPassword,
        "2", "5", "1", "1", "",      // change status of case 1 -> option 1 (In Progress)
        "0", "0", "",
    });
    EXPECT_TRUE(contains(out, "Error: Assign the case to a support agent"));
    session.start(*staffRepo.findByUsername("root"));
    EXPECT_EQ(cases.getCase(1).status, CaseStatus::Open);
    EXPECT_EQ(cases.getHistory(1).size(), 1u);
}

TEST_F(CliTest, FailedLoginsAreRejectedAndTooManyEndTheProgram) {
    seedAdmin();
    std::vector<std::string> keys;
    for (int i = 0; i < Application::kMaxLoginAttempts; ++i) {
        keys.push_back("root");
        keys.push_back("wrong-password");
    }
    const std::string out = run(keys);
    EXPECT_TRUE(contains(out, "Login failed: Invalid username or password"));
    EXPECT_TRUE(contains(out, "Too many failed login attempts"));
    EXPECT_FALSE(session.isLoggedIn());
}

TEST_F(CliTest, SuccessfulLoginResetsTheFailureCounter) {
    seedAdmin();
    std::vector<std::string> keys;
    for (int i = 0; i < Application::kMaxLoginAttempts - 1; ++i) {
        keys.push_back("root");
        keys.push_back("wrong");
    }
    keys.insert(keys.end(), {"root", kTestPassword, "0",    // success, log out
                             "root", "wrong", "", });       // one more failure, then quit
    const std::string out = run(keys);
    EXPECT_FALSE(contains(out, "Too many failed login attempts"));
    EXPECT_TRUE(contains(out, "Welcome, Root User."));
}

TEST_F(CliTest, AgentsAreBlockedFromAdministratorFunctionsInTheMenus) {
    seedAdmin();
    seedAgent();
    session.start(*staffRepo.findByUsername("root"));
    const Client c = clients.registerClient("Ann", "ann@example.com", "5551234567");
    session.end();

    const std::string out = run({
        "jane", "JanePass123",
        "4",                         // staff management
        "1", "6", "1", "y", "0",     // client menu -> archive client 1
        "0",
        "0", "",
    });
    EXPECT_TRUE(contains(out, "Error: Administrator privileges are required"));
    session.start(*staffRepo.findByUsername("root"));
    EXPECT_FALSE(clients.getClient(c.id).archived);
}

TEST_F(CliTest, ArchiveAsksForConfirmationAndCanBeCancelled) {
    seedAdmin();
    session.start(*staffRepo.findByUsername("root"));
    const Client c = clients.registerClient("Ann", "ann@example.com", "5551234567");
    session.end();

    const std::string out = run({
        "root", kTestPassword,
        "1", "6", "1", "n", "0",     // say no
        "0", "0", "",
    });
    EXPECT_TRUE(contains(out, "Cancelled."));
    session.start(*staffRepo.findByUsername("root"));
    EXPECT_FALSE(clients.getClient(c.id).archived);
}

TEST_F(CliTest, LogoutRequiresLoggingInAgain) {
    seedAdmin();
    const std::string out = run({
        "root", kTestPassword, "0",     // log in, log out
        "root", "wrong",                // next login attempt must authenticate again
        "",
    });
    EXPECT_TRUE(contains(out, "Logged out."));
    EXPECT_TRUE(contains(out, "Login failed"));
    EXPECT_FALSE(session.isLoggedIn());
}

TEST_F(CliTest, ChangingOwnPasswordWorksThroughTheMenu) {
    seedAdmin();
    run({
        "root", kTestPassword,
        "5", kTestPassword, "NewPassw0rd!", "NewPassw0rd!",
        "0", "",
    });
    EXPECT_NO_THROW(auth.login("root", "NewPassw0rd!"));
    session.end();
    EXPECT_THROW(auth.login("root", kTestPassword), AuthenticationError);
}

TEST_F(CliTest, ProgramEndsCleanlyWhenInputRunsOutMidSession) {
    seedAdmin();
    std::string out;
    EXPECT_NO_THROW(out = run({"root", kTestPassword, "1", "1", "Ann"}));   // stops mid-form
    EXPECT_TRUE(contains(out, "Input closed. Goodbye."));
}

TEST_F(CliTest, PasswordsAreNeverPrintedBack) {
    seedAdmin();
    const std::string out = run({"root", kTestPassword, "0", ""});
    EXPECT_FALSE(contains(out, kTestPassword));
}
