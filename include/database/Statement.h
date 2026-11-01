#pragma once
// RAII wrapper around a SQLite prepared statement.
//
// All user-supplied values must be passed through the bind*() methods and never
// concatenated into SQL text. That is what prevents SQL injection.
//
// Parameter indexes are 1-based and match "?1", "?2", ... placeholders.
// A Statement must not outlive the Database that created it.

#include <cstdint>
#include <optional>
#include <string>

struct sqlite3;
struct sqlite3_stmt;

namespace csms {

class Statement {
public:
    Statement(sqlite3* db, const std::string& sql);
    ~Statement();

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;

    // Binding (returns *this so calls can be chained).
    Statement& bindInt(int index, std::int64_t value);
    Statement& bindText(int index, const std::string& value);
    Statement& bindNull(int index);
    Statement& bindOptionalInt(int index, const std::optional<std::int64_t>& value);

    // Advance to the next row. Returns true if a row is available, false when
    // the statement has finished. Throws DatabaseError on failure.
    bool step();

    // Reuse the statement with new parameter values.
    void reset();

    // Column access (0-based) for the current row.
    std::int64_t columnInt(int column) const;
    std::string columnText(int column) const;
    bool columnIsNull(int column) const;
    std::optional<std::int64_t> columnOptionalInt(int column) const;

private:
    sqlite3* database() const;

    sqlite3_stmt* stmt_ = nullptr;
};

} // namespace csms
