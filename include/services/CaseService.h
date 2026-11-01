#pragma once
// Business rules for support cases.
//
//  * every operation needs a logged-in user (the Session);
//  * cases can only be created for existing, non-archived clients;
//  * cases can only be assigned to ACTIVE SUPPORT AGENTS;
//  * status changes follow the state machine in CaseWorkflow.h;
//  * every creation / status change / reopen writes a CaseHistory row IN THE SAME
//    DATABASE TRANSACTION as the change itself, so the case and its audit trail can
//    never disagree: either both are saved or neither is.

#include <cstdint>
#include <string>
#include <vector>

#include "database/Database.h"
#include "models/CaseHistoryEntry.h"
#include "models/SupportCase.h"
#include "repositories/CaseRepository.h"
#include "repositories/ClientRepository.h"
#include "repositories/HistoryRepository.h"
#include "repositories/StaffRepository.h"
#include "services/Session.h"

namespace csms {

class CaseService {
public:
    CaseService(Database& db, CaseRepository& cases, HistoryRepository& history,
                ClientRepository& clients, StaffRepository& staff, const Session& session)
        : db_(db), cases_(cases), history_(history), clients_(clients), staff_(staff),
          session_(session) {}

    // Throws AuthorizationError, ValidationError (bad text, archived client),
    // NotFoundError (unknown client).
    SupportCase createCase(std::int64_t clientId, const std::string& title,
                           const std::string& description, Priority priority);

    SupportCase getCase(std::int64_t caseId) const;
    std::vector<SupportCase> searchCases(const CaseFilter& filter) const;

    // Throws NotFoundError (case or staff), ValidationError (closed case, staff
    // deactivated, or staff is not a support agent).
    SupportCase assignCase(std::int64_t caseId, std::int64_t staffId);

    // Ordinary status change. Throws InvalidTransitionError for moves the workflow
    // forbids (including Resolved -> Open, which needs reopenCase) and for moving
    // an unassigned case to InProgress.
    SupportCase changeStatus(std::int64_t caseId, CaseStatus newStatus, const std::string& note);

    // Resolved -> Open. A reason is mandatory. Throws InvalidTransitionError if
    // the case is not Resolved.
    SupportCase reopenCase(std::int64_t caseId, const std::string& reason);

    // Complete history, oldest first. Throws NotFoundError for an unknown case.
    std::vector<CaseHistoryEntry> getHistory(std::int64_t caseId) const;

private:
    SupportCase loadCase(std::int64_t caseId) const;
    void recordStatusChange(const SupportCase& current, CaseStatus newStatus,
                            const std::string& note, std::int64_t actorId);

    Database& db_;
    CaseRepository& cases_;
    HistoryRepository& history_;
    ClientRepository& clients_;
    StaffRepository& staff_;
    const Session& session_;
};

} // namespace csms
