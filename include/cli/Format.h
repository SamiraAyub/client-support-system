#pragma once
// Turns domain objects into readable terminal text. Pure output: no input, no
// database access.

#include <cstdint>
#include <iosfwd>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "models/CaseHistoryEntry.h"
#include "models/Client.h"
#include "models/Report.h"
#include "models/Staff.h"
#include "models/SupportCase.h"

namespace csms {

// ID -> display text lookups so tables can show names instead of bare numbers.
struct Names {
    std::map<std::int64_t, std::string> clients;   // client ID  -> client name
    std::map<std::int64_t, std::string> staff;     // staff ID   -> username

    std::string client(std::int64_t id) const;
    std::string staffMember(std::int64_t id) const;
    std::string assignee(const std::optional<std::int64_t>& id) const;   // "-" when unassigned
};

// "2026-11-01T14:30:00Z" -> "2026-11-01 14:30 UTC" (unchanged if not in that format).
std::string displayTime(const std::string& iso);

void printClients(std::ostream& out, const std::vector<Client>& clients);
void printClientDetails(std::ostream& out, const Client& client);

void printCases(std::ostream& out, const std::vector<SupportCase>& cases, const Names& names);
void printCaseDetails(std::ostream& out, const SupportCase& supportCase, const Names& names);
void printHistory(std::ostream& out, const std::vector<CaseHistoryEntry>& history,
                  const Names& names);

void printStaff(std::ostream& out, const std::vector<Staff>& staff);
void printReport(std::ostream& out, const SystemReport& report);

} // namespace csms
