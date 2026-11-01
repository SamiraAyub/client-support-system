#include "models/Enums.h"

#include "common/Exceptions.h"

namespace csms {

std::string toString(Priority p) {
    switch (p) {
        case Priority::Low:    return "Low";
        case Priority::Medium: return "Medium";
        case Priority::High:   return "High";
    }
    throw ValidationError("Unknown priority value");
}

std::string toString(CaseStatus s) {
    switch (s) {
        case CaseStatus::Open:       return "Open";
        case CaseStatus::InProgress: return "InProgress";
        case CaseStatus::Resolved:   return "Resolved";
        case CaseStatus::Closed:     return "Closed";
    }
    throw ValidationError("Unknown case status value");
}

std::string toString(StaffRole r) {
    switch (r) {
        case StaffRole::Administrator: return "Administrator";
        case StaffRole::SupportAgent:  return "SupportAgent";
    }
    throw ValidationError("Unknown staff role value");
}

std::string toDisplayString(Priority p) { return toString(p); }

std::string toDisplayString(CaseStatus s) {
    return s == CaseStatus::InProgress ? "In Progress" : toString(s);
}

std::string toDisplayString(StaffRole r) {
    return r == StaffRole::SupportAgent ? "Support Agent" : toString(r);
}

Priority priorityFromString(const std::string& text) {
    if (text == "Low")    return Priority::Low;
    if (text == "Medium") return Priority::Medium;
    if (text == "High")   return Priority::High;
    throw ValidationError("Invalid priority '" + text + "' (expected Low, Medium or High)");
}

CaseStatus caseStatusFromString(const std::string& text) {
    if (text == "Open")       return CaseStatus::Open;
    if (text == "InProgress") return CaseStatus::InProgress;
    if (text == "Resolved")   return CaseStatus::Resolved;
    if (text == "Closed")     return CaseStatus::Closed;
    throw ValidationError("Invalid status '" + text +
                          "' (expected Open, InProgress, Resolved or Closed)");
}

StaffRole staffRoleFromString(const std::string& text) {
    if (text == "Administrator") return StaffRole::Administrator;
    if (text == "SupportAgent")  return StaffRole::SupportAgent;
    throw ValidationError("Invalid role '" + text +
                          "' (expected Administrator or SupportAgent)");
}

} // namespace csms
