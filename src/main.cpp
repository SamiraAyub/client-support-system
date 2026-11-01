// Composition root: builds every object once, wires the layers together and
// starts the CLI. No business logic lives here.
//
//   Database -> Repositories -> Services -> CLI

#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

#include "cli/Application.h"
#include "cli/Console.h"
#include "common/Exceptions.h"
#include "database/Database.h"
#include "repositories/CaseRepository.h"
#include "repositories/ClientRepository.h"
#include "repositories/HistoryRepository.h"
#include "repositories/ReportRepository.h"
#include "repositories/StaffRepository.h"
#include "services/AuthService.h"
#include "services/CaseService.h"
#include "services/ClientService.h"
#include "services/PasswordHasher.h"
#include "services/ReportService.h"
#include "services/Session.h"
#include "services/StaffService.h"

namespace {

void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [--db <path>]\n"
              << "  --db <path>   SQLite database file (default: data/csms.db)\n"
              << "  --help        show this message\n";
}

} // namespace

int main(int argc, char* argv[]) {
    std::string dbPath = "data/csms.db";   // relative to the current directory

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        }
        if (std::strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            dbPath = argv[++i];
        } else {
            std::cerr << "Unknown or incomplete argument: " << argv[i] << "\n";
            printUsage(argv[0]);
            return 2;
        }
    }

    try {
        if (dbPath != ":memory:") {
            const auto parent = std::filesystem::path(dbPath).parent_path();
            if (!parent.empty()) std::filesystem::create_directories(parent);
        }

        // Database layer
        csms::Database db(dbPath);
        db.initializeSchema();

        // Repository layer
        csms::ClientRepository clientRepo(db);
        csms::StaffRepository staffRepo(db);
        csms::CaseRepository caseRepo(db);
        csms::HistoryRepository historyRepo(db);
        csms::ReportRepository reportRepo(db);

        // Service layer
        const csms::PasswordHasher hasher = csms::PasswordHasher::interactive();
        csms::Session session;
        csms::AuthService auth(staffRepo, hasher, session);
        csms::StaffService staffService(staffRepo, hasher, session);
        csms::ClientService clientService(clientRepo, session);
        csms::CaseService caseService(db, caseRepo, historyRepo, clientRepo, staffRepo, session);
        csms::ReportService reportService(reportRepo, session);

        // Presentation layer
        csms::Console console(std::cin, std::cout, csms::Console::stdinIsTerminal());
        csms::Application app(console, {session, auth, staffService, clientService,
                                        caseService, reportService});
        app.run();
        return 0;
    } catch (const csms::CsmsError& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: cannot prepare the database location: " << e.code().message()
                  << " (" << e.path1().string() << ")\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << '\n';
        return 2;
    }
}
