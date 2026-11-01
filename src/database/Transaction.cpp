#include "database/Transaction.h"

#include "common/Exceptions.h"
#include "database/Database.h"

namespace csms {

Transaction::Transaction(Database& db) : db_(db) {
    if (db_.transactionActive_) {
        throw DatabaseError("A transaction is already active (nesting is not supported)");
    }
    db_.execute("BEGIN");
    db_.transactionActive_ = true;
}

Transaction::~Transaction() {
    if (!finished_) {
        try {
            db_.execute("ROLLBACK");
        } catch (...) {
            // Destructors must not throw. If ROLLBACK fails SQLite has already
            // discarded the transaction (e.g. after a fatal I/O error).
        }
        db_.transactionActive_ = false;
    }
}

void Transaction::commit() {
    if (finished_) {
        throw DatabaseError("Transaction has already been finished");
    }
    db_.execute("COMMIT");   // if this throws, the destructor will roll back
    finished_ = true;
    db_.transactionActive_ = false;
}

void Transaction::rollback() {
    if (finished_) {
        throw DatabaseError("Transaction has already been finished");
    }
    db_.execute("ROLLBACK");
    finished_ = true;
    db_.transactionActive_ = false;
}

} // namespace csms
