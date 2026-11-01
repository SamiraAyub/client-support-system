// Phase 1 tests: connection management, schema integrity, prepared statements
// and the Transaction guard. Fixture data is inserted with raw SQL on purpose -
// repositories do not exist yet.

#include <gtest/gtest.h>

#include <string>

#include "common/Exceptions.h"
#include "database/Database.h"
#include "database/Schema.h"
#include "database/Statement.h"
#include "database/Transaction.h"
#include "TestHelpers.h"

using namespace csms;

namespace {

constexpr const char* kTime = "2026-01-01T00:00:00Z";

std::int64_t countRows(Database& db, const std::string& table) {
    Statement s = db.prepare("SELECT COUNT(*) FROM " + table);   // table name is a test literal
    s.step();
    return s.columnInt(0);
}

bool tableExists(Database& db, const std::string& name) {
    Statement s = db.prepare("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name=?1");
    s.bindText(1, name);
    s.step();
    return s.columnInt(0) == 1;
}

class SchemaTest : public ::testing::Test {
protected:
    void SetUp() override { db.initializeSchema(); }

    void insertClient(const std::string& email = "ann@example.com") {
        Statement s = db.prepare(
            "INSERT INTO Clients(name, email, phone, registered_at) VALUES('Ann', ?1, '555', ?2)");
        s.bindText(1, email).bindText(2, kTime);
        s.step();
    }

    void insertStaff(const std::string& username = "agent1") {
        Statement s = db.prepare(
            "INSERT INTO Staff(username, full_name, role, password_hash, created_at) "
            "VALUES(?1, 'Agent One', 'SupportAgent', 'hash', ?2)");
        s.bindText(1, username).bindText(2, kTime);
        s.step();
    }

    void insertCase(std::int64_t clientId, const std::string& priority = "Medium") {
        Statement s = db.prepare(
            "INSERT INTO SupportCases(client_id, title, priority, created_at, updated_at) "
            "VALUES(?1, 'Printer down', ?2, ?3, ?3)");
        s.bindInt(1, clientId).bindText(2, priority).bindText(3, kTime);
        s.step();
    }

    Database db{":memory:"};
};

} // namespace

// ---------------------------------------------------------------- connection

TEST(DatabaseTest, ForeignKeysAreEnabledOnNewConnections) {
    Database db(":memory:");
    EXPECT_TRUE(db.foreignKeysEnabled());
}

TEST(DatabaseTest, EmptyPathIsRejected) {
    EXPECT_THROW(Database(""), DatabaseError);
}

TEST(DatabaseTest, UnopenablePathThrowsDatabaseError) {
    EXPECT_THROW(Database("/nonexistent_dir_for_csms/sub/test.db"), DatabaseError);
}

TEST(DatabaseTest, MalformedSqlThrowsDatabaseError) {
    Database db(":memory:");
    EXPECT_THROW(db.execute("THIS IS NOT SQL"), DatabaseError);
    EXPECT_THROW(db.prepare("SELECT * FROM missing_table"), DatabaseError);
}

// -------------------------------------------------------------------- schema

TEST(DatabaseTest, FreshDatabaseHasVersionZeroUntilInitialised) {
    Database db(":memory:");
    EXPECT_EQ(db.schemaVersion(), 0);
    db.initializeSchema();
    EXPECT_EQ(db.schemaVersion(), kSchemaVersion);
}

TEST_F(SchemaTest, CreatesAllRequiredTables) {
    EXPECT_TRUE(tableExists(db, "Clients"));
    EXPECT_TRUE(tableExists(db, "Staff"));
    EXPECT_TRUE(tableExists(db, "SupportCases"));
    EXPECT_TRUE(tableExists(db, "CaseHistory"));
}

TEST_F(SchemaTest, InitialisationIsIdempotentAndKeepsData) {
    insertClient();
    EXPECT_NO_THROW(db.initializeSchema());
    EXPECT_EQ(countRows(db, "Clients"), 1);
}

TEST_F(SchemaTest, NewerSchemaVersionIsRejected) {
    db.execute("PRAGMA user_version = 999");
    EXPECT_THROW(db.initializeSchema(), DatabaseError);
}

// --------------------------------------------------------------- constraints

TEST_F(SchemaTest, CaseWithUnknownClientViolatesForeignKey) {
    try {
        insertCase(/*clientId=*/12345);
        FAIL() << "Expected DatabaseError";
    } catch (const DatabaseError& e) {
        EXPECT_TRUE(e.isConstraintViolation());
    }
}

TEST_F(SchemaTest, DuplicateEmailIsRejectedCaseInsensitively) {
    insertClient("ann@example.com");
    try {
        insertClient("ANN@Example.COM");
        FAIL() << "Expected DatabaseError";
    } catch (const DatabaseError& e) {
        EXPECT_TRUE(e.isConstraintViolation());
    }
    EXPECT_EQ(countRows(db, "Clients"), 1);
}

TEST_F(SchemaTest, DuplicateUsernameIsRejected) {
    insertStaff("agent1");
    EXPECT_THROW(insertStaff("AGENT1"), DatabaseError);
}

TEST_F(SchemaTest, InvalidPriorityIsRejectedByCheckConstraint) {
    insertClient();
    EXPECT_THROW(insertCase(1, "Urgent"), DatabaseError);
}

TEST_F(SchemaTest, BlankClientNameIsRejected) {
    Statement s = db.prepare(
        "INSERT INTO Clients(name, email, phone, registered_at) VALUES('   ', 'x@y.com', '1', ?1)");
    s.bindText(1, kTime);
    EXPECT_THROW(s.step(), DatabaseError);
}

TEST_F(SchemaTest, NullInNotNullColumnIsRejected) {
    Statement s = db.prepare(
        "INSERT INTO Clients(name, email, phone, registered_at) VALUES('Ann', ?1, '1', ?2)");
    s.bindNull(1).bindText(2, kTime);
    EXPECT_THROW(s.step(), DatabaseError);
}

TEST_F(SchemaTest, ReferencedClientCannotBeDeleted) {
    insertClient();
    insertCase(1);
    EXPECT_THROW(db.execute("DELETE FROM Clients WHERE id = 1"), DatabaseError);
    EXPECT_EQ(countRows(db, "Clients"), 1);
}

TEST_F(SchemaTest, ReferencedStaffCannotBeDeleted) {
    insertClient();
    insertStaff();
    Statement s = db.prepare(
        "INSERT INTO SupportCases(client_id, title, priority, assigned_staff_id, created_at, updated_at) "
        "VALUES(1, 'T', 'Low', 1, ?1, ?1)");
    s.bindText(1, kTime);
    s.step();
    EXPECT_THROW(db.execute("DELETE FROM Staff WHERE id = 1"), DatabaseError);
}

TEST_F(SchemaTest, HistoryRequiresExistingCaseAndStaff) {
    Statement s = db.prepare(
        "INSERT INTO CaseHistory(case_id, changed_by_staff_id, new_status, changed_at) "
        "VALUES(99, 99, 'Open', ?1)");
    s.bindText(1, kTime);
    EXPECT_THROW(s.step(), DatabaseError);
}

// -------------------------------------------------------- prepared statements

TEST_F(SchemaTest, BoundValuesAreTreatedAsDataNotSql) {
    const std::string hostile = "Robert'); DROP TABLE Clients;--";
    Statement ins = db.prepare(
        "INSERT INTO Clients(name, email, phone, registered_at) VALUES(?1, 'r@x.com', '1', ?2)");
    ins.bindText(1, hostile).bindText(2, kTime);
    ins.step();

    EXPECT_TRUE(tableExists(db, "Clients"));   // still there

    Statement sel = db.prepare("SELECT name FROM Clients WHERE email = 'r@x.com'");
    ASSERT_TRUE(sel.step());
    EXPECT_EQ(sel.columnText(0), hostile);     // stored verbatim
}

TEST_F(SchemaTest, OptionalIntegersMapToNull) {
    insertClient();
    Statement ins = db.prepare(
        "INSERT INTO SupportCases(client_id, title, priority, assigned_staff_id, created_at, updated_at) "
        "VALUES(1, 'T', 'Low', ?1, ?2, ?2)");
    ins.bindOptionalInt(1, std::nullopt).bindText(2, kTime);
    ins.step();

    Statement sel = db.prepare("SELECT assigned_staff_id FROM SupportCases");
    ASSERT_TRUE(sel.step());
    EXPECT_TRUE(sel.columnIsNull(0));
    EXPECT_FALSE(sel.columnOptionalInt(0).has_value());
}

TEST_F(SchemaTest, StepIteratesOverAllRowsThenReturnsFalse) {
    insertClient("a@x.com");
    insertClient("b@x.com");
    insertClient("c@x.com");
    Statement sel = db.prepare("SELECT email FROM Clients ORDER BY id");
    int rows = 0;
    while (sel.step()) ++rows;
    EXPECT_EQ(rows, 3);
}

TEST_F(SchemaTest, ResetAllowsStatementReuse) {
    insertClient("a@x.com");
    insertClient("b@x.com");
    Statement sel = db.prepare("SELECT email FROM Clients WHERE id = ?1");
    sel.bindInt(1, 1);
    ASSERT_TRUE(sel.step());
    EXPECT_EQ(sel.columnText(0), "a@x.com");

    sel.reset();
    sel.bindInt(1, 2);
    ASSERT_TRUE(sel.step());
    EXPECT_EQ(sel.columnText(0), "b@x.com");
}

TEST_F(SchemaTest, BindingOutOfRangeThrows) {
    Statement s = db.prepare("SELECT ?1");
    EXPECT_THROW(s.bindInt(5, 1), DatabaseError);
}

TEST_F(SchemaTest, LastInsertRowIdReflectsNewRow) {
    insertClient("a@x.com");
    insertClient("b@x.com");
    EXPECT_EQ(db.lastInsertRowId(), 2);
}

TEST_F(SchemaTest, GeneratedClientIdsAreUniqueAndIncreasing) {
    insertClient("a@x.com");
    const auto first = db.lastInsertRowId();
    insertClient("b@x.com");
    EXPECT_GT(db.lastInsertRowId(), first);
}

// -------------------------------------------------------------- transactions

TEST_F(SchemaTest, CommittedTransactionPersistsChanges) {
    {
        Transaction tx(db);
        insertClient();
        tx.commit();
    }
    EXPECT_EQ(countRows(db, "Clients"), 1);
    EXPECT_FALSE(db.inTransaction());
}

TEST_F(SchemaTest, TransactionRollsBackWhenScopeExitsWithoutCommit) {
    {
        Transaction tx(db);
        insertClient();
        EXPECT_TRUE(db.inTransaction());
    }   // no commit
    EXPECT_EQ(countRows(db, "Clients"), 0);
    EXPECT_FALSE(db.inTransaction());
}

TEST_F(SchemaTest, TransactionRollsBackWhenAnExceptionEscapes) {
    try {
        Transaction tx(db);
        insertClient("a@x.com");
        insertClient("A@X.COM");   // duplicate -> throws
        tx.commit();
    } catch (const DatabaseError&) {
    }
    EXPECT_EQ(countRows(db, "Clients"), 0);   // first insert was undone too
}

TEST_F(SchemaTest, ExplicitRollbackDiscardsChanges) {
    Transaction tx(db);
    insertClient();
    tx.rollback();
    EXPECT_EQ(countRows(db, "Clients"), 0);
}

TEST_F(SchemaTest, NestedTransactionsAreRejected) {
    Transaction outer(db);
    EXPECT_THROW(Transaction inner(db), DatabaseError);
    EXPECT_TRUE(db.inTransaction());   // outer is unaffected
}

TEST_F(SchemaTest, FinishedTransactionCannotBeReused) {
    Transaction tx(db);
    tx.commit();
    EXPECT_THROW(tx.commit(), DatabaseError);
    EXPECT_THROW(tx.rollback(), DatabaseError);
}

TEST_F(SchemaTest, NewTransactionCanStartAfterPreviousOneEnds) {
    { Transaction tx(db); tx.commit(); }
    { Transaction tx(db); insertClient(); tx.commit(); }
    EXPECT_EQ(countRows(db, "Clients"), 1);
}

// --------------------------------------------------------------- persistence

TEST(DatabasePersistenceTest, DataSurvivesClosingAndReopeningTheFile) {
    test_support::TempDbPath file;
    {
        Database db(file.str());
        db.initializeSchema();
        Statement s = db.prepare(
            "INSERT INTO Clients(name, email, phone, registered_at) VALUES('Ann', ?1, '555', ?2)");
        s.bindText(1, "ann@example.com").bindText(2, kTime);
        s.step();
    }   // connection closed

    Database reopened(file.str());
    reopened.initializeSchema();   // must not wipe anything
    EXPECT_EQ(reopened.schemaVersion(), kSchemaVersion);

    Statement sel = reopened.prepare("SELECT name FROM Clients WHERE email = 'ann@example.com'");
    ASSERT_TRUE(sel.step());
    EXPECT_EQ(sel.columnText(0), "Ann");
}

TEST(DatabasePersistenceTest, ForeignKeysAreEnforcedAfterReopening) {
    test_support::TempDbPath file;
    { Database db(file.str()); db.initializeSchema(); }

    Database reopened(file.str());
    EXPECT_TRUE(reopened.foreignKeysEnabled());
}

// ------------------------------------------------- Phase 2 additions to database layer

TEST_F(SchemaTest, UniqueViolationIsDistinguishedFromOtherConstraints) {
    insertClient("a@x.com");
    try {
        insertClient("a@x.com");
        FAIL() << "Expected DatabaseError";
    } catch (const DatabaseError& e) {
        EXPECT_TRUE(e.isConstraintViolation());
        EXPECT_TRUE(e.isUniqueViolation());
    }

    try {
        insertCase(/*clientId=*/999);   // foreign-key violation
        FAIL() << "Expected DatabaseError";
    } catch (const DatabaseError& e) {
        EXPECT_TRUE(e.isConstraintViolation());
        EXPECT_FALSE(e.isUniqueViolation());
    }
}

TEST_F(SchemaTest, ChangesReportsAffectedRowCount) {
    insertClient("a@x.com");
    insertClient("b@x.com");
    db.execute("UPDATE Clients SET phone = '999'");
    EXPECT_EQ(db.changes(), 2);
    db.execute("UPDATE Clients SET phone = '1' WHERE id = 12345");
    EXPECT_EQ(db.changes(), 0);
}
