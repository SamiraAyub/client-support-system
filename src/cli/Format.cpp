#include "cli/Format.h"

#include <iomanip>
#include <ostream>

namespace csms {

namespace {

// Pads with spaces or truncates (with "..") to exactly `width` bytes, without
// cutting a multi-byte UTF-8 character in half.
std::string fit(const std::string& text, std::size_t width) {
    if (text.size() <= width) return text + std::string(width - text.size(), ' ');
    std::size_t cut = width > 2 ? width - 2 : width;
    while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) --cut;
    std::string out = text.substr(0, cut) + "..";
    if (out.size() < width) out += std::string(width - out.size(), ' ');
    return out;
}

void rule(std::ostream& out, std::size_t width) {
    out << std::string(width, '-') << '\n';
}

std::string yesNoStatus(bool archived) { return archived ? "Archived" : "Active"; }

} // namespace

// ------------------------------------------------------------------- Names

std::string Names::client(std::int64_t id) const {
    const auto it = clients.find(id);
    return it != clients.end() ? it->second : "#" + std::to_string(id);
}

std::string Names::staffMember(std::int64_t id) const {
    const auto it = staff.find(id);
    return it != staff.end() ? it->second : "#" + std::to_string(id);
}

std::string Names::assignee(const std::optional<std::int64_t>& id) const {
    return id ? staffMember(*id) : "-";
}

std::string displayTime(const std::string& iso) {
    if (iso.size() == 20 && iso[10] == 'T' && iso.back() == 'Z') {
        return iso.substr(0, 10) + " " + iso.substr(11, 5) + " UTC";
    }
    return iso;
}

// ----------------------------------------------------------------- clients

void printClients(std::ostream& out, const std::vector<Client>& clients) {
    if (clients.empty()) {
        out << "No clients found.\n";
        return;
    }
    out << fit("ID", 5) << fit("Name", 24) << fit("Email", 30) << fit("Phone", 18)
        << fit("Registered", 21) << "Status\n";
    rule(out, 105);
    for (const Client& c : clients) {
        out << fit(std::to_string(c.id), 5) << fit(c.name, 24) << fit(c.email, 30)
            << fit(c.phone, 18) << fit(displayTime(c.registeredAt), 21)
            << yesNoStatus(c.archived) << '\n';
    }
    out << clients.size() << " client(s)\n";
}

void printClientDetails(std::ostream& out, const Client& c) {
    out << "Client #" << c.id << '\n'
        << "  Name:       " << c.name << '\n'
        << "  Email:      " << c.email << '\n'
        << "  Phone:      " << c.phone << '\n'
        << "  Registered: " << displayTime(c.registeredAt) << '\n'
        << "  Status:     " << yesNoStatus(c.archived) << '\n';
}

// ------------------------------------------------------------------- cases

void printCases(std::ostream& out, const std::vector<SupportCase>& cases, const Names& names) {
    if (cases.empty()) {
        out << "No cases found.\n";
        return;
    }
    out << fit("ID", 5) << fit("Client", 20) << fit("Title", 30) << fit("Priority", 10)
        << fit("Status", 13) << "Assigned\n";
    rule(out, 90);
    for (const SupportCase& c : cases) {
        out << fit(std::to_string(c.id), 5) << fit(names.client(c.clientId), 20)
            << fit(c.title, 30) << fit(toDisplayString(c.priority), 10)
            << fit(toDisplayString(c.status), 13) << names.assignee(c.assignedStaffId) << '\n';
    }
    out << cases.size() << " case(s)\n";
}

void printCaseDetails(std::ostream& out, const SupportCase& c, const Names& names) {
    out << "Case #" << c.id << '\n'
        << "  Title:       " << c.title << '\n'
        << "  Client:      " << names.client(c.clientId) << " (ID " << c.clientId << ")\n"
        << "  Description: " << (c.description.empty() ? "-" : c.description) << '\n'
        << "  Priority:    " << toDisplayString(c.priority) << '\n'
        << "  Status:      " << toDisplayString(c.status) << '\n'
        << "  Assigned to: " << names.assignee(c.assignedStaffId) << '\n'
        << "  Created:     " << displayTime(c.createdAt) << '\n'
        << "  Updated:     " << displayTime(c.updatedAt) << '\n';
}

void printHistory(std::ostream& out, const std::vector<CaseHistoryEntry>& history,
                  const Names& names) {
    out << "History:\n";
    if (history.empty()) {
        out << "  (none)\n";
        return;
    }
    out << "  " << fit("When", 21) << fit("Change", 28) << fit("By", 16) << "Note\n";
    out << "  ";
    rule(out, 85);
    for (const CaseHistoryEntry& e : history) {
        const std::string from = e.previousStatus ? toDisplayString(*e.previousStatus) : "(new)";
        out << "  " << fit(displayTime(e.changedAt), 21)
            << fit(from + " -> " + toDisplayString(e.newStatus), 28)
            << fit(names.staffMember(e.changedByStaffId), 16) << e.note << '\n';
    }
}

// ------------------------------------------------------------------- staff

void printStaff(std::ostream& out, const std::vector<Staff>& staff) {
    if (staff.empty()) {
        out << "No staff found.\n";
        return;
    }
    out << fit("ID", 5) << fit("Username", 20) << fit("Name", 26) << fit("Role", 15) << "Status\n";
    rule(out, 75);
    for (const Staff& s : staff) {
        out << fit(std::to_string(s.id), 5) << fit(s.username, 20) << fit(s.fullName, 26)
            << fit(toDisplayString(s.role), 15) << (s.active ? "Active" : "Deactivated") << '\n';
    }
}

// ------------------------------------------------------------------ report

void printReport(std::ostream& out, const SystemReport& r) {
    out << "========== Support System Report ==========\n\n";

    out << "Clients\n"
        << "  Total registered: " << r.totalClients << '\n'
        << "  Active:           " << r.activeClients << '\n'
        << "  Archived:         " << r.archivedClients << "\n\n";

    out << "Cases by status (total " << r.totalCases << ")\n";
    for (const auto& [status, count] : r.casesByStatus) {
        out << "  " << fit(toDisplayString(status), 14) << std::setw(5) << count << '\n';
    }

    out << "\nCases by priority\n";
    for (const auto& [priority, count] : r.casesByPriority) {
        out << "  " << fit(toDisplayString(priority), 14) << std::setw(5) << count << '\n';
    }

    out << "\nCases assigned to each staff member\n";
    if (r.staffWorkloads.empty()) {
        out << "  (no support agents yet)\n";
    } else {
        out << "  " << fit("Staff", 26) << fit("Total", 8) << "Open / In Progress\n";
        out << "  ";
        rule(out, 52);
        for (const StaffWorkload& w : r.staffWorkloads) {
            const std::string label = w.fullName + (w.active ? "" : " (deactivated)");
            out << "  " << fit(label, 26) << fit(std::to_string(w.totalAssigned), 8)
                << w.openWork << '\n';
        }
    }
    out << "  Unassigned cases: " << r.unassignedCases << '\n';
    out << "\n===========================================\n";
}

} // namespace csms
