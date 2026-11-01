#pragma once
// The case status state machine, kept as pure functions so it is trivial to
// test and to explain:
//
//     Open --------> InProgress --------> Resolved --------> Closed
//       ^                |                    |
//       +----------------+                    |
//       ^                                     |
//       +------------- reopen (explicit) -----+
//
// * Open -> InProgress, InProgress -> Open and InProgress -> Resolved and
//   Resolved -> Closed are ordinary transitions.
// * Resolved -> Open is only possible through CaseService::reopenCase().
// * Closed is final.

#include <vector>

#include "models/Enums.h"

namespace csms::workflow {

// Ordinary status changes (does NOT include reopening).
bool isAllowedTransition(CaseStatus from, CaseStatus to);

// The one special transition: Resolved -> Open.
bool isReopen(CaseStatus from, CaseStatus to);

// Statuses reachable from `from` by an ordinary change (for menus and messages).
std::vector<CaseStatus> allowedNextStatuses(CaseStatus from);

} // namespace csms::workflow
