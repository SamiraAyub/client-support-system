#include "cli/Application.h"

#include "cli/CaseMenu.h"
#include "cli/ClientMenu.h"
#include "cli/Format.h"
#include "cli/StaffMenu.h"
#include "common/Exceptions.h"

namespace csms {

void Application::run() {
    try {
        console_.println("==========================================");
        console_.println("   Client Support Management System");
        console_.println("==========================================");

        // A brand-new database has no accounts: create the first administrator
        // (no default password is ever shipped).
        while (!services_.staff.hasStaff()) {
            console_.runGuarded([&] { firstRunSetup(); });
        }

        int failedAttempts = 0;
        for (;;) {
            const LoginResult result = loginScreen();
            if (result == LoginResult::Quit) break;
            if (result == LoginResult::Failed) {
                if (++failedAttempts >= kMaxLoginAttempts) {
                    console_.println("Too many failed login attempts. Exiting.");
                    break;
                }
                continue;
            }
            failedAttempts = 0;
            mainMenu();
        }
        console_.println("Goodbye.");
    } catch (const InputClosedError&) {
        console_.println();
        console_.println("Input closed. Goodbye.");
    }
}

void Application::firstRunSetup() {
    console_.println();
    console_.println("First-time setup: no accounts exist yet.");
    console_.println("Create the first administrator account.");
    const std::string username = console_.ask("Username");
    const std::string fullName = console_.ask("Full name");
    const std::string password = console_.askNewPassword();
    const Staff admin = services_.staff.bootstrapAdministrator(username, fullName, password);
    console_.println("Administrator '" + admin.username + "' created. You can now log in.");
}

Application::LoginResult Application::loginScreen() {
    console_.println();
    console_.println("--- Login ---");
    const std::string username = console_.ask("Username (blank to quit)");
    if (username.empty()) return LoginResult::Quit;
    const std::string password = console_.askSecret("Password");

    try {
        const Staff user = services_.auth.login(username, password);
        console_.println("Welcome, " + user.fullName + ".");
        return LoginResult::LoggedIn;
    } catch (const AuthenticationError& e) {
        console_.println(std::string("Login failed: ") + e.what());
        return LoginResult::Failed;
    }
}

void Application::mainMenu() {
    for (;;) {
        const Staff& user = services_.session.requireLogin();
        const int choice = console_.askChoice(
            "Main Menu - " + user.username + " (" + toDisplayString(user.role) + ")",
            {"Clients", "Support cases", "Reports", "Staff management (administrator)",
             "Change my password"},
            "Log out");

        if (choice == 0) {
            services_.auth.logout();
            console_.println("Logged out.");
            return;
        }

        switch (choice) {
            case 1: ClientMenu(console_, services_.clients).run(); break;
            case 2: CaseMenu(console_, services_.cases, services_.clients, services_.staff).run(); break;
            case 3: console_.runGuarded([&] { showReport(); }); break;
            case 4:
                console_.runGuarded([&] {
                    services_.session.requireAdministrator();   // friendly early check
                    StaffMenu(console_, services_.staff).run();
                });
                break;
            case 5: console_.runGuarded([&] { changeOwnPassword(); }); break;
            default: break;
        }
    }
}

void Application::showReport() {
    printReport(console_.out(), services_.reports.generate());
}

void Application::changeOwnPassword() {
    const std::string current = console_.askSecret("Current password");
    const std::string next = console_.askNewPassword();
    services_.staff.changeOwnPassword(current, next);
    console_.println("Your password has been changed.");
}

} // namespace csms
