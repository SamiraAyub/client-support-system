#pragma once
// All timestamps in the system are UTC, ISO 8601, second precision:
//     YYYY-MM-DDTHH:MM:SSZ      e.g. 2026-11-01T14:30:00Z
// Text in this format sorts chronologically, which keeps SQL ORDER BY simple.

#include <ctime>
#include <string>

namespace csms {

// Current time as an ISO 8601 UTC string.
std::string currentTimestampUtc();

// Format a specific time_t (exposed so tests are deterministic).
std::string formatTimestampUtc(std::time_t t);

} // namespace csms
