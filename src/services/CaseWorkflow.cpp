#include "services/CaseWorkflow.h"

namespace csms::workflow {

bool isAllowedTransition(CaseStatus from, CaseStatus to) {
    switch (from) {
        case CaseStatus::Open:
            return to == CaseStatus::InProgress;
        case CaseStatus::InProgress:
            return to == CaseStatus::Open || to == CaseStatus::Resolved;
        case CaseStatus::Resolved:
            return to == CaseStatus::Closed;
        case CaseStatus::Closed:
            return false;
    }
    return false;
}

bool isReopen(CaseStatus from, CaseStatus to) {
    return from == CaseStatus::Resolved && to == CaseStatus::Open;
}

std::vector<CaseStatus> allowedNextStatuses(CaseStatus from) {
    std::vector<CaseStatus> result;
    for (const CaseStatus candidate : {CaseStatus::Open, CaseStatus::InProgress,
                                       CaseStatus::Resolved, CaseStatus::Closed}) {
        if (isAllowedTransition(from, candidate)) result.push_back(candidate);
    }
    return result;
}

} // namespace csms::workflow
