#pragma once
// Private helper shared by the database layer's .cpp files.
// Keeps sqlite3.h out of the public headers.

#include <sqlite3.h>

#include <string>

#include "common/Exceptions.h"

namespace csms {

inline DatabaseError makeDatabaseError(sqlite3* db, const std::string& context) {
    const std::string detail = db ? sqlite3_errmsg(db) : "unknown error";
    const int code = db ? sqlite3_extended_errcode(db) : 0;
    return DatabaseError(context + ": " + detail, code);
}

} // namespace csms
