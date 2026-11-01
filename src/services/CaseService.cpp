#include "services/CaseService.h"

#include "common/Exceptions.h"
#include "common/TimeUtils.h"
#include "common/Validation.h"
#include "database/Transaction.h"
#include "services/CaseWorkflow.h"

namespace csms {

namespace {

std::string idText(std::int64_t id) { return std::to_string(id); }

std::string allowedList(CaseStatus from) {
    const auto next = workflow::allowedNextStatuses(from);
    if (next.empty()) return "none (the case is final)";
    std::string text;
    for (std::size_t i = 0; i < next.size(); ++i) {
        if (i > 0) text += ", ";
        text += toDisplayString(next[i]);
    }
    return text;
}

} // namespace

SupportCase CaseService::loadCase(std::int64_t caseId) const {
    const auto found = cases_.findById(caseId);
    if (!found) {
        throw NotFoundError("Case " + idText(caseId) + " does not exist");
    }
    return *found;
}

SupportCase CaseService::createCase(std::int64_t clientId, const std::string& title,
                                    const std::string& description, Priority priority) {
    const std::int64_t actorId = session_.requireLogin().id;

    SupportCase c;
    c.title = validateRequiredText("Title", title, kMaxTitleLength);
    c.description = validateOptionalText("Description", description, kMaxDescriptionLength);
    c.priority = priority;

    const auto client = clients_.findById(clientId);
    if (!client) {
        throw NotFoundError("Client " + idText(clientId) + " does not exist");
    }
    if (client->archived) {
        throw ValidationError("Client " + idText(clientId) +
                              " is archived; restore the client before creating cases");
    }

    c.clientId = clientId;
    c.status = CaseStatus::Open;
    c.createdAt = currentTimestampUtc();
    c.updatedAt = c.createdAt;

    // Case row + "created" history row: all or nothing.
    Transaction tx(db_);
    c.id = cases_.insert(c);

    CaseHistoryEntry entry;
    entry.caseId = c.id;
    entry.changedByStaffId = actorId;
    entry.previousStatus = std::nullopt;     // nothing came before creation
    entry.newStatus = CaseStatus::Open;
    entry.note = "Case created";
    entry.changedAt = c.createdAt;
    history_.insert(entry);

    tx.commit();
    return c;
}

SupportCase CaseService::getCase(std::int64_t caseId) const {
    session_.requireLogin();
    return loadCase(caseId);
}

std::vector<SupportCase> CaseService::searchCases(const CaseFilter& filter) const {
    session_.requireLogin();
    return cases_.search(filter);
}

SupportCase CaseService::assignCase(std::int64_t caseId, std::int64_t staffId) {
    session_.requireLogin();

    const SupportCase current = loadCase(caseId);
    if (current.status == CaseStatus::Closed) {
        throw ValidationError("Case " + idText(caseId) + " is closed and cannot be reassigned");
    }

    const auto agent = staff_.findById(staffId);
    if (!agent) {
        throw NotFoundError("Staff member " + idText(staffId) + " does not exist");
    }
    if (!agent->active) {
        throw ValidationError("Staff member '" + agent->username +
                              "' is deactivated and cannot receive new assignments");
    }
    if (agent->role != StaffRole::SupportAgent) {
        throw ValidationError("Only support agents can be assigned to cases ('" +
                              agent->username + "' is an " + toDisplayString(agent->role) + ")");
    }

    if (current.assignedStaffId == staffId) {
        return current;   // already assigned to this agent: nothing to do
    }
    cases_.updateAssignment(caseId, staffId, currentTimestampUtc());
    return loadCase(caseId);
}

void CaseService::recordStatusChange(const SupportCase& current, CaseStatus newStatus,
                                     const std::string& note, std::int64_t actorId) {
    const std::string now = currentTimestampUtc();

    Transaction tx(db_);                              // begin
    cases_.updateStatus(current.id, newStatus, now);  // 1. change the case
    CaseHistoryEntry entry;                           // 2. record who/what/when
    entry.caseId = current.id;
    entry.changedByStaffId = actorId;
    entry.previousStatus = current.status;
    entry.newStatus = newStatus;
    entry.note = note;
    entry.changedAt = now;
    history_.insert(entry);
    tx.commit();                                      // both saved, or (on any
}                                                     // exception) both undone

SupportCase CaseService::changeStatus(std::int64_t caseId, CaseStatus newStatus,
                                      const std::string& note) {
    const std::int64_t actorId = session_.requireLogin().id;
    const std::string cleanNote = validateOptionalText("Note", note, kMaxNoteLength);
    const SupportCase current = loadCase(caseId);

    if (workflow::isReopen(current.status, newStatus)) {
        throw InvalidTransitionError(
            "A resolved case cannot be moved back to Open with a status change; "
            "use the explicit reopen operation");
    }
    if (!workflow::isAllowedTransition(current.status, newStatus)) {
        throw InvalidTransitionError("Cannot change a case from " +
                                     toDisplayString(current.status) + " to " +
                                     toDisplayString(newStatus) + ". Allowed: " +
                                     allowedList(current.status));
    }
    if (newStatus == CaseStatus::InProgress && !current.assignedStaffId) {
        throw InvalidTransitionError(
            "Assign the case to a support agent before moving it to In Progress");
    }

    recordStatusChange(current, newStatus, cleanNote, actorId);
    return loadCase(caseId);
}

SupportCase CaseService::reopenCase(std::int64_t caseId, const std::string& reason) {
    const std::int64_t actorId = session_.requireLogin().id;
    const std::string cleanReason = validateRequiredText("Reopen reason", reason, kMaxNoteLength);
    const SupportCase current = loadCase(caseId);

    if (current.status != CaseStatus::Resolved) {
        throw InvalidTransitionError(
            "Only resolved cases can be reopened (case " + idText(caseId) + " is " +
            toDisplayString(current.status) +
            (current.status == CaseStatus::Closed ? "; closed cases are final)" : ")"));
    }

    recordStatusChange(current, CaseStatus::Open, cleanReason, actorId);
    return loadCase(caseId);
}

std::vector<CaseHistoryEntry> CaseService::getHistory(std::int64_t caseId) const {
    session_.requireLogin();
    loadCase(caseId);   // NotFoundError for an unknown case
    return history_.findByCase(caseId);
}

} // namespace csms
