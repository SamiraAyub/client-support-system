#pragma once
// Scope guard for a database transaction (RAII).
//
//     {
//         Transaction tx(db);
//         ... several statements ...
//         tx.commit();          // if this line is never reached,
//     }                         // the destructor rolls everything back
//
// Nested transactions are not supported and throw DatabaseError, which keeps
// the rules simple: one unit of work = one transaction, opened by the service.

namespace csms {

class Database;

class Transaction {
public:
    explicit Transaction(Database& db);
    ~Transaction();

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    void commit();
    void rollback();

private:
    Database& db_;
    bool finished_ = false;
};

} // namespace csms
