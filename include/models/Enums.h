#pragma once
// Domain enumerations.
//
// Each enum has two string forms:
//   toString()         - stable identifier stored in the database
//   toDisplayString()  - human-friendly text for the CLI and reports
// fromString() parses the stored form and throws ValidationError otherwise.

#include <string>

namespace csms {

enum class Priority { Low, Medium, High };

enum class CaseStatus { Open, InProgress, Resolved, Closed };

enum class StaffRole { Administrator, SupportAgent };

std::string toString(Priority p);
std::string toString(CaseStatus s);
std::string toString(StaffRole r);

std::string toDisplayString(Priority p);
std::string toDisplayString(CaseStatus s);
std::string toDisplayString(StaffRole r);

Priority priorityFromString(const std::string& text);
CaseStatus caseStatusFromString(const std::string& text);
StaffRole staffRoleFromString(const std::string& text);

} // namespace csms
