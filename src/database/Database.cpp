#include "database/Database.h"

#include <sqlite3.h>

#include "common/Exceptions.h"
#include "database/Schema.h"
#include "database/Transaction.h"
#include "SqliteError.h"

namespace csms {

// isConstraintViolation() in Exceptions.h relies on this value.
static_assert(SQLITE_CONSTRAINT == 19, "SQLITE_CONSTRAINT changed; update DatabaseError");
static_assert(SQLITE_CONSTRAINT_UNIQUE == 2067, "update DatabaseError::isUniqueViolation");
static_assert(SQLITE_CONSTRAINT_PRIMARYKEY == 1555, "update DatabaseError::isUniqueViolation");

Database::Database(const std::string& path) {
    if (path.empty()) {
        throw DatabaseError("Database path must not be empty");
    }

    const int rc = sqlite3_open_v2(path.c_str(), &db_,
                                   SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc != SQLITE_OK) {
        const std::string detail = db_ ? sqlite3_errmsg(db_) : "out of memory";
        sqlite3_close(db_);
        db_ = nullptr;
        throw DatabaseError("Cannot open database '" + path + "': " + detail, rc);
    }

    try {
        sqlite3_extended_result_codes(db_, 1);   // e.g. SQLITE_CONSTRAINT_UNIQUE
        sqlite3_busy_timeout(db_, 5000);         // wait up to 5s if the file is locked
        execute("PRAGMA foreign_keys = ON");
        if (!foreignKeysEnabled()) {
            throw DatabaseError("SQLite build does not support foreign keys");
        }
    } catch (...) {
        sqlite3_close(db_);
        db_ = nullptr;
        throw;
    }
}

Database::~Database() {
    sqlite3_close(db_);   // safe on nullptr; all Statements are finalized by now
}

void Database::execute(const std::string& sql) {
    char* errMsg = nullptr;
    const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        const std::string detail = errMsg ? errMsg : sqlite3_errmsg(db_);
        sqlite3_free(errMsg);
        throw DatabaseError("SQL execution failed: " + detail, rc);
    }
}

Statement Database::prepare(const std::string& sql) {
    return Statement(db_, sql);
}

std::int64_t Database::lastInsertRowId() const {
    return sqlite3_last_insert_rowid(db_);
}

int Database::changes() const {
    return sqlite3_changes(db_);
}

int Database::schemaVersion() {
    Statement stmt = prepare("PRAGMA user_version");
    stmt.step();
    return static_cast<int>(stmt.columnInt(0));
}

bool Database::foreignKeysEnabled() {
    Statement stmt = prepare("PRAGMA foreign_keys");
    stmt.step();
    return stmt.columnInt(0) == 1;
}

void Database::initializeSchema() {
    const int existing = schemaVersion();
    if (existing > kSchemaVersion) {
        throw DatabaseError("Database schema version " + std::to_string(existing) +
                            " is newer than this application supports (" +
                            std::to_string(kSchemaVersion) + ")");
    }

    // All-or-nothing: a half-created schema is never left behind.
    Transaction tx(*this);
    execute(schemaSql());
    execute("PRAGMA user_version = " + std::to_string(kSchemaVersion));
    tx.commit();
}

} // namespace csms
