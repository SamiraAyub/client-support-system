#pragma once
// Top-level CLI flow: first-run setup -> login -> main menu -> logout.

#include "cli/Console.h"
#include "services/AuthService.h"
#include "services/CaseService.h"
#include "services/ClientService.h"
#include "services/ReportService.h"
#include "services/Session.h"
#include "services/StaffService.h"

namespace csms {

// Everything the CLI needs, built once in main() (the composition root).
struct Services {
    Session& session;
    AuthService& auth;
    StaffService& staff;
    ClientService& clients;
    CaseService& cases;
    ReportService& reports;
};

class Application {
public:
    static constexpr int kMaxLoginAttempts = 5;

    Application(Console& console, Services services)
        : console_(console), services_(services) {}

    // Runs until the user quits, input ends, or too many logins fail.
    void run();

private:
    enum class LoginResult { LoggedIn, Failed, Quit };

    void firstRunSetup();
    LoginResult loginScreen();
    void mainMenu();
    void showReport();
    void changeOwnPassword();

    Console& console_;
    Services services_;
};

} // namespace csms
