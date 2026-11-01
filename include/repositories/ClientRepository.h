#pragma once
// Repository layer: all SQL for the Clients table lives here.
//
// Responsibilities: run prepared statements and map rows <-> Client objects,
// translating database failures into domain exceptions (DuplicateError,
// NotFoundError). It does NOT validate user input (the service does) and it
// never reads from or prints to the console.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "database/Database.h"
#include "models/Client.h"

namespace csms {

class ClientRepository {
public:
    explicit ClientRepository(Database& db) : db_(db) {}

    // Inserts the client and returns the generated ID.
    // Throws DuplicateError if the email already exists (case-insensitive).
    std::int64_t insert(const Client& client);

    std::optional<Client> findById(std::int64_t id) const;
    std::optional<Client> findByEmail(const std::string& email) const;

    // Case-insensitive substring match on name, email or phone.
    // An empty `text` matches everyone. Archived clients are skipped unless
    // `includeArchived` is true. Results are ordered by name, then ID.
    std::vector<Client> search(const std::string& text, bool includeArchived) const;

    // Updates name, email and phone of the client with client.id.
    // Throws NotFoundError (no such ID) or DuplicateError (email taken).
    void update(const Client& client);

    // Throws NotFoundError if no client has this ID.
    void setArchived(std::int64_t id, bool archived);

private:
    Database& db_;
};

} // namespace csms
