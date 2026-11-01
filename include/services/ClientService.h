#pragma once
// Service layer: business rules for client management.
//
//  * validates and normalises all input (see common/Validation.h)
//  * prevents duplicate clients (same email, case-insensitive)
//  * archives instead of deleting; archived clients are read-only until restored
//  * requires a logged-in user; archiving / restoring is Administrator-only
//  * reports problems with domain exceptions that the CLI shows to the user
//
// The service knows nothing about SQL or the console.
//
// Note on duplicate checks: the service looks for an existing email first so it
// can give a helpful message (including the existing client's ID). The UNIQUE
// constraint in the schema remains the final safety net if two writers race.

#include <cstdint>
#include <string>
#include <vector>

#include "models/Client.h"
#include "repositories/ClientRepository.h"
#include "services/Session.h"

namespace csms {

class ClientService {
public:
    ClientService(ClientRepository& repository, const Session& session)
        : repository_(repository), session_(session) {}

    // Every method below throws AuthorizationError when nobody is logged in.

    // Throws ValidationError, DuplicateError.
    Client registerClient(const std::string& name, const std::string& email,
                          const std::string& phone);

    // Throws NotFoundError.
    Client getClient(std::int64_t id) const;

    // Throws NotFoundError, ValidationError (bad input, or client is archived),
    // DuplicateError (email belongs to a different client).
    Client updateClient(std::int64_t id, const std::string& name, const std::string& email,
                        const std::string& phone);

    // Administrator only. Throws NotFoundError; ValidationError if already archived.
    void archiveClient(std::int64_t id);

    // Administrator only. Undo an archive (e.g. archived by mistake). Throws NotFoundError;
    // ValidationError if the client is not archived.
    void restoreClient(std::int64_t id);

    // Matches name, email or phone (case-insensitive substring).
    std::vector<Client> searchClients(const std::string& text, bool includeArchived = false) const;
    std::vector<Client> listClients(bool includeArchived = false) const;

private:
    ClientRepository& repository_;
    const Session& session_;
};

} // namespace csms
