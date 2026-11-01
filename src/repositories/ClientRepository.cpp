#include "repositories/ClientRepository.h"

#include "common/Exceptions.h"

namespace csms {

namespace {

// Column order used by every SELECT so mapRow() can rely on fixed positions.
constexpr const char* kSelectColumns =
    "SELECT id, name, email, phone, registered_at, is_archived FROM Clients";

Client mapRow(const Statement& row) {
    Client c;
    c.id = row.columnInt(0);
    c.name = row.columnText(1);
    c.email = row.columnText(2);
    c.phone = row.columnText(3);
    c.registeredAt = row.columnText(4);
    c.archived = row.columnInt(5) != 0;
    return c;
}

// Make user text safe for use inside LIKE: '%', '_' and '\' lose their
// wildcard meaning (the query declares '\' as the escape character).
std::string escapeLike(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        if (c == '\\' || c == '%' || c == '_') out += '\\';
        out += c;
    }
    return out;
}

} // namespace

std::int64_t ClientRepository::insert(const Client& client) {
    Statement stmt = db_.prepare(
        "INSERT INTO Clients (name, email, phone, registered_at, is_archived) "
        "VALUES (?1, ?2, ?3, ?4, ?5)");
    stmt.bindText(1, client.name)
        .bindText(2, client.email)
        .bindText(3, client.phone)
        .bindText(4, client.registeredAt)
        .bindInt(5, client.archived ? 1 : 0);

    try {
        stmt.step();
    } catch (const DatabaseError& e) {
        if (e.isUniqueViolation()) {
            throw DuplicateError("A client with email '" + client.email + "' already exists");
        }
        throw;
    }
    return db_.lastInsertRowId();
}

std::optional<Client> ClientRepository::findById(std::int64_t id) const {
    Statement stmt = db_.prepare(std::string(kSelectColumns) + " WHERE id = ?1");
    stmt.bindInt(1, id);
    if (!stmt.step()) return std::nullopt;
    return mapRow(stmt);
}

std::optional<Client> ClientRepository::findByEmail(const std::string& email) const {
    // The column is declared COLLATE NOCASE, so '=' is already case-insensitive.
    Statement stmt = db_.prepare(std::string(kSelectColumns) + " WHERE email = ?1");
    stmt.bindText(1, email);
    if (!stmt.step()) return std::nullopt;
    return mapRow(stmt);
}

std::vector<Client> ClientRepository::search(const std::string& text,
                                             bool includeArchived) const {
    Statement stmt = db_.prepare(
        std::string(kSelectColumns) +
        " WHERE (?1 = '' OR name  LIKE ?2 ESCAPE '\\'"
        "               OR email LIKE ?2 ESCAPE '\\'"
        "               OR phone LIKE ?2 ESCAPE '\\')"
        "   AND (?3 = 1 OR is_archived = 0)"
        " ORDER BY name COLLATE NOCASE, id");
    stmt.bindText(1, text)
        .bindText(2, "%" + escapeLike(text) + "%")
        .bindInt(3, includeArchived ? 1 : 0);

    std::vector<Client> results;
    while (stmt.step()) {
        results.push_back(mapRow(stmt));
    }
    return results;
}

void ClientRepository::update(const Client& client) {
    Statement stmt = db_.prepare(
        "UPDATE Clients SET name = ?1, email = ?2, phone = ?3 WHERE id = ?4");
    stmt.bindText(1, client.name)
        .bindText(2, client.email)
        .bindText(3, client.phone)
        .bindInt(4, client.id);

    try {
        stmt.step();
    } catch (const DatabaseError& e) {
        if (e.isUniqueViolation()) {
            throw DuplicateError("A client with email '" + client.email + "' already exists");
        }
        throw;
    }
    if (db_.changes() == 0) {
        throw NotFoundError("Client " + std::to_string(client.id) + " does not exist");
    }
}

void ClientRepository::setArchived(std::int64_t id, bool archived) {
    Statement stmt = db_.prepare("UPDATE Clients SET is_archived = ?1 WHERE id = ?2");
    stmt.bindInt(1, archived ? 1 : 0).bindInt(2, id);
    stmt.step();
    if (db_.changes() == 0) {
        throw NotFoundError("Client " + std::to_string(id) + " does not exist");
    }
}

} // namespace csms
