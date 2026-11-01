#pragma once
// Project-wide exception hierarchy. Every error the application raises
// deliberately derives from CsmsError, so the CLI can catch one base type
// and show a readable message instead of crashing.

#include <stdexcept>
#include <string>

namespace csms {

class CsmsError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Bad user input (empty name, malformed email, unknown enum text, ...).
class ValidationError : public CsmsError {
public:
    using CsmsError::CsmsError;
};

// A referenced record does not exist (unknown client ID, ...).
class NotFoundError : public CsmsError {
public:
    using CsmsError::CsmsError;
};

// A uniqueness rule would be broken (e.g. duplicate client email).
class DuplicateError : public CsmsError {
public:
    using CsmsError::CsmsError;
};

// A case status change that the workflow does not allow.
class InvalidTransitionError : public CsmsError {
public:
    using CsmsError::CsmsError;
};

// Login failed (unknown user, wrong password, deactivated account).
class AuthenticationError : public CsmsError {
public:
    using CsmsError::CsmsError;
};

// The logged-in user is not allowed to perform the operation.
class AuthorizationError : public CsmsError {
public:
    using CsmsError::CsmsError;
};

// Failure reported by the database layer. Carries the SQLite result code so
// repositories can tell, for example, a constraint violation from other errors.
class DatabaseError : public CsmsError {
public:
    explicit DatabaseError(const std::string& message, int sqliteCode = 0)
        : CsmsError(message), code_(sqliteCode) {}

    int code() const noexcept { return code_; }

    // True for UNIQUE / FOREIGN KEY / NOT NULL / CHECK failures.
    // 19 is SQLITE_CONSTRAINT; extended codes keep it in the low byte
    // (verified by a static_assert in Database.cpp).
    bool isConstraintViolation() const noexcept { return (code_ & 0xFF) == 19; }

    // True only for UNIQUE / PRIMARY KEY violations (SQLITE_CONSTRAINT_UNIQUE = 2067,
    // SQLITE_CONSTRAINT_PRIMARYKEY = 1555; both verified in Database.cpp).
    bool isUniqueViolation() const noexcept { return code_ == 2067 || code_ == 1555; }

private:
    int code_;
};

} // namespace csms
