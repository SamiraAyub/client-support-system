#include "database/Statement.h"

#include <sqlite3.h>

#include <utility>

#include "SqliteError.h"

namespace csms {

Statement::Statement(sqlite3* db, const std::string& sql) {
    const int rc = sqlite3_prepare_v2(db, sql.c_str(), static_cast<int>(sql.size()),
                                      &stmt_, nullptr);
    if (rc != SQLITE_OK) {
        throw makeDatabaseError(db, "Failed to prepare statement");
    }
}

Statement::~Statement() {
    sqlite3_finalize(stmt_);   // safe on nullptr
}

Statement::Statement(Statement&& other) noexcept : stmt_(other.stmt_) {
    other.stmt_ = nullptr;
}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        sqlite3_finalize(stmt_);
        stmt_ = other.stmt_;
        other.stmt_ = nullptr;
    }
    return *this;
}

sqlite3* Statement::database() const {
    return sqlite3_db_handle(stmt_);
}

Statement& Statement::bindInt(int index, std::int64_t value) {
    if (sqlite3_bind_int64(stmt_, index, value) != SQLITE_OK) {
        throw makeDatabaseError(database(), "Failed to bind integer parameter");
    }
    return *this;
}

Statement& Statement::bindText(int index, const std::string& value) {
    // SQLITE_TRANSIENT makes SQLite copy the text, so the std::string may die.
    if (sqlite3_bind_text(stmt_, index, value.c_str(),
                          static_cast<int>(value.size()), SQLITE_TRANSIENT) != SQLITE_OK) {
        throw makeDatabaseError(database(), "Failed to bind text parameter");
    }
    return *this;
}

Statement& Statement::bindNull(int index) {
    if (sqlite3_bind_null(stmt_, index) != SQLITE_OK) {
        throw makeDatabaseError(database(), "Failed to bind NULL parameter");
    }
    return *this;
}

Statement& Statement::bindOptionalInt(int index, const std::optional<std::int64_t>& value) {
    return value ? bindInt(index, *value) : bindNull(index);
}

bool Statement::step() {
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw makeDatabaseError(database(), "Statement execution failed");
}

void Statement::reset() {
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
}

std::int64_t Statement::columnInt(int column) const {
    return sqlite3_column_int64(stmt_, column);
}

std::string Statement::columnText(int column) const {
    const unsigned char* text = sqlite3_column_text(stmt_, column);
    return text ? std::string(reinterpret_cast<const char*>(text)) : std::string();
}

bool Statement::columnIsNull(int column) const {
    return sqlite3_column_type(stmt_, column) == SQLITE_NULL;
}

std::optional<std::int64_t> Statement::columnOptionalInt(int column) const {
    if (columnIsNull(column)) return std::nullopt;
    return columnInt(column);
}

} // namespace csms
