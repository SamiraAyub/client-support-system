#include "cli/CaseMenu.h"

#include "services/CaseWorkflow.h"

namespace csms {

void CaseMenu::run() {
    for (;;) {
        const int choice = console_.askChoice(
            "Support Cases",
            {"Create case", "View case and its history", "Search / filter cases",
             "Assign case to a support agent", "Change case status", "Reopen a resolved case"});
        if (choice == 0) return;

        console_.runGuarded([&] {
            switch (choice) {
                case 1: createCase(); break;
                case 2: viewCase(); break;
                case 3: searchCases(); break;
                case 4: assignCase(); break;
                case 5: changeStatus(); break;
                case 6: reopenCase(); break;
                default: break;
            }
        });
    }
}

Names CaseMenu::loadNames() const {
    Names names;
    for (const Client& c : clients_.listClients(true)) names.clients[c.id] = c.name;
    for (const Staff& s : staff_.listStaff(true)) names.staff[s.id] = s.username;
    return names;
}

std::optional<Priority> CaseMenu::pickPriority(const std::string& zeroLabel) {
    switch (console_.askChoice("Priority", {"Low", "Medium", "High"}, zeroLabel)) {
        case 1: return Priority::Low;
        case 2: return Priority::Medium;
        case 3: return Priority::High;
        default: return std::nullopt;
    }
}

std::optional<CaseStatus> CaseMenu::pickStatus(const std::string& zeroLabel) {
    switch (console_.askChoice("Status", {"Open", "In Progress", "Resolved", "Closed"}, zeroLabel)) {
        case 1: return CaseStatus::Open;
        case 2: return CaseStatus::InProgress;
        case 3: return CaseStatus::Resolved;
        case 4: return CaseStatus::Closed;
        default: return std::nullopt;
    }
}

void CaseMenu::createCase() {
    const std::int64_t clientId = console_.askInt("Client ID");
    const std::string title = console_.ask("Title");
    const std::string description = console_.ask("Description");
    const auto priority = pickPriority("Cancel");
    if (!priority) {
        console_.println("Cancelled.");
        return;
    }
    const SupportCase c = cases_.createCase(clientId, title, description, *priority);
    console_.println("Created case #" + std::to_string(c.id) + " (status Open, unassigned).");
}

void CaseMenu::viewCase() {
    const SupportCase c = cases_.getCase(console_.askInt("Case ID"));
    const Names names = loadNames();
    printCaseDetails(console_.out(), c, names);
    console_.println();
    printHistory(console_.out(), cases_.getHistory(c.id), names);
}

void CaseMenu::searchCases() {
    console_.println("Leave a filter blank to ignore it.");
    CaseFilter filter;
    filter.caseId = console_.askOptionalInt("Case ID");
    filter.clientId = console_.askOptionalInt("Client ID");
    filter.assignedStaffId = console_.askOptionalInt("Assigned staff ID");
    filter.status = pickStatus("Any status");
    filter.priority = pickPriority("Any priority");
    printCases(console_.out(), cases_.searchCases(filter), loadNames());
}

void CaseMenu::assignCase() {
    // Show the people who can actually receive cases.
    std::vector<Staff> agents;
    for (const Staff& s : staff_.listStaff(false)) {
        if (s.role == StaffRole::SupportAgent) agents.push_back(s);
    }
    console_.println("Active support agents:");
    printStaff(console_.out(), agents);

    const std::int64_t caseId = console_.askInt("Case ID");
    const std::int64_t staffId = console_.askInt("Staff ID to assign");
    const SupportCase c = cases_.assignCase(caseId, staffId);
    console_.println("Case #" + std::to_string(c.id) + " is now assigned to " +
                     loadNames().assignee(c.assignedStaffId) + ".");
}

void CaseMenu::changeStatus() {
    const SupportCase c = cases_.getCase(console_.askInt("Case ID"));
    const auto next = workflow::allowedNextStatuses(c.status);
    if (next.empty()) {
        console_.println("Case #" + std::to_string(c.id) + " is " + toDisplayString(c.status) +
                         " and cannot change status any more.");
        return;
    }

    std::vector<std::string> labels;
    for (const CaseStatus s : next) labels.push_back(toDisplayString(s));
    const int choice = console_.askChoice(
        "Change status of case #" + std::to_string(c.id) + " (now " + toDisplayString(c.status) + ")",
        labels, "Cancel");
    if (choice == 0) {
        console_.println("Cancelled.");
        return;
    }

    const std::string note = console_.ask("Note (optional)");
    const SupportCase updated = cases_.changeStatus(c.id, next[static_cast<std::size_t>(choice - 1)], note);
    console_.println("Case #" + std::to_string(updated.id) + " is now " +
                     toDisplayString(updated.status) + ".");
}

void CaseMenu::reopenCase() {
    const std::int64_t id = console_.askInt("Case ID");
    const std::string reason = console_.ask("Reason for reopening");
    const SupportCase c = cases_.reopenCase(id, reason);
    console_.println("Case #" + std::to_string(c.id) + " reopened (status " +
                     toDisplayString(c.status) + ").");
}

} // namespace csms
