#pragma once
// Owns the SQLite connection (RAII) and the schema lifecycle.
//
// - Foreign-key enforcement is switched on for every connection.
// - Pass ":memory:" for a throw-away database (used heavily by the tests).
// - Non-copyable and non-movable: repositories hold a reference to it, so it
//   must stay at a stable address and outlive everything that uses it.

#include <cstdint>
#include <string>

#include "database/Statement.h"

struct sqlite3;

namespace csms {

class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Create tables and indexes if missing. Safe to call on every start-up.
    // Throws DatabaseError if the file was created by a newer schema version.
    void initializeSchema();

    // Run SQL that contains NO user-supplied values (schema, PRAGMA, BEGIN...).
    // For anything involving user data use prepare() and bind parameters.
    void execute(const std::string& sql);

    Statement prepare(const std::string& sql);

    std::int64_t lastInsertRowId() const;

    // Rows modified by the most recently completed INSERT / UPDATE / DELETE.
    int changes() const;
    int schemaVersion();
    bool foreignKeysEnabled();
    bool inTransaction() const noexcept { return transactionActive_; }

private:
    friend class Transaction;   // Transaction toggles transactionActive_

    sqlite3* db_ = nullptr;
    bool transactionActive_ = false;
};

} // namespace csms
