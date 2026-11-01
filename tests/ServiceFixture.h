#pragma once
// Shared fixture for service-level tests: an in-memory database, every
// repository and service wired together exactly like main() does, an
// administrator and a support agent in the database, and the administrator
// logged in. Tests call loginAs() / session.end() to act as someone else.

#include <string>

#include "TestHelpers.h"
#include "common/Validation.h"
#include "repositories/CaseRepository.h"
#include "repositories/ClientRepository.h"
#include "repositories/HistoryRepository.h"
#include "repositories/StaffRepository.h"
#include "services/AuthService.h"
#include "services/CaseService.h"
#include "services/ClientService.h"
#include "services/PasswordHasher.h"
#include "services/Session.h"
#include "services/StaffService.h"

namespace csms::test_support {

constexpr const char* kTestPassword = "CorrectHorse9!";

class ServiceFixture : public InMemoryDbTest {
protected:
    void SetUp() override {
        InMemoryDbTest::SetUp();
        admin = addStaff("admin", StaffRole::Administrator);
        agent = addStaff("agent", StaffRole::SupportAgent);
        session.start(admin);
    }

    Staff addStaff(const std::string& username, StaffRole role, bool active = true) {
        Staff s;
        s.username = username;
        s.fullName = "Full " + username;
        s.role = role;
        s.passwordHash = hasher.hash(kTestPassword);
        s.active = active;
        s.createdAt = "2026-01-01T00:00:00Z";
        s.id = staffRepo.insert(s);
        return s;
    }

    Client addClient(const std::string& email = "ann@example.com") {
        Client c;
        c.name = "Ann Lee";
        c.email = email;
        c.phone = "555-123-4567";
        c.registeredAt = "2026-01-01T00:00:00Z";
        c.id = clientRepo.insert(c);
        return c;
    }

    SupportCase addCase(std::int64_t clientId, Priority priority = Priority::Medium) {
        return caseService.createCase(clientId, "Printer down", "It will not print", priority);
    }

    // Assign to the fixture's agent and walk the case to the wanted status.
    SupportCase addCaseWithStatus(std::int64_t clientId, CaseStatus target) {
        SupportCase c = addCase(clientId);
        if (target == CaseStatus::Open) return c;
        caseService.assignCase(c.id, agent.id);
        caseService.changeStatus(c.id, CaseStatus::InProgress, "");
        if (target == CaseStatus::InProgress) return caseService.getCase(c.id);
        caseService.changeStatus(c.id, CaseStatus::Resolved, "");
        if (target == CaseStatus::Resolved) return caseService.getCase(c.id);
        caseService.changeStatus(c.id, CaseStatus::Closed, "");
        return caseService.getCase(c.id);
    }

    void loginAs(const Staff& user) { session.start(user); }

    std::int64_t countRows(const std::string& table) {
        Statement s = db.prepare("SELECT COUNT(*) FROM " + table);   // test-only literal
        s.step();
        return s.columnInt(0);
    }

    Staff admin;
    Staff agent;
    PasswordHasher hasher = PasswordHasher::lowCostForTests();
    Session session;
    ClientRepository clientRepo{db};
    StaffRepository staffRepo{db};
    CaseRepository caseRepo{db};
    HistoryRepository historyRepo{db};
    ClientService clientService{clientRepo, session};
    StaffService staffService{staffRepo, hasher, session};
    AuthService authService{staffRepo, hasher, session};
    CaseService caseService{db, caseRepo, historyRepo, clientRepo, staffRepo, session};
};

} // namespace csms::test_support
