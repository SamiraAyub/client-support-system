// Repository tests run against a real in-memory SQLite database: they verify
// the SQL itself, not a mock.

#include <gtest/gtest.h>

#include <string>

#include "common/Exceptions.h"
#include "repositories/ClientRepository.h"
#include "TestHelpers.h"

using namespace csms;

namespace {

Client makeClient(const std::string& name, const std::string& email,
                  const std::string& phone = "555-123-4567") {
    Client c;
    c.name = name;
    c.email = email;
    c.phone = phone;
    c.registeredAt = "2026-01-01T00:00:00Z";
    return c;
}

class ClientRepositoryTest : public test_support::InMemoryDbTest {
protected:
    ClientRepository repo{db};
};

} // namespace

TEST_F(ClientRepositoryTest, InsertAssignsUniqueIncreasingIds) {
    const auto a = repo.insert(makeClient("Ann", "ann@example.com"));
    const auto b = repo.insert(makeClient("Bob", "bob@example.com"));
    EXPECT_GT(a, 0);
    EXPECT_GT(b, a);
}

TEST_F(ClientRepositoryTest, FindByIdReturnsAllStoredFields) {
    const auto id = repo.insert(makeClient("Ann Lee", "ann@example.com", "+1 555 123 4567"));
    const auto found = repo.findById(id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->id, id);
    EXPECT_EQ(found->name, "Ann Lee");
    EXPECT_EQ(found->email, "ann@example.com");
    EXPECT_EQ(found->phone, "+1 555 123 4567");
    EXPECT_EQ(found->registeredAt, "2026-01-01T00:00:00Z");
    EXPECT_FALSE(found->archived);
}

TEST_F(ClientRepositoryTest, FindByIdReturnsEmptyForUnknownId) {
    EXPECT_FALSE(repo.findById(999).has_value());
}

TEST_F(ClientRepositoryTest, FindByEmailIsCaseInsensitive) {
    const auto id = repo.insert(makeClient("Ann", "Ann@Example.com"));
    const auto found = repo.findByEmail("ANN@example.COM");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->id, id);
    EXPECT_FALSE(repo.findByEmail("nobody@example.com").has_value());
}

TEST_F(ClientRepositoryTest, DuplicateEmailThrowsDuplicateError) {
    repo.insert(makeClient("Ann", "ann@example.com"));
    EXPECT_THROW(repo.insert(makeClient("Other Ann", "ANN@EXAMPLE.COM")), DuplicateError);
    EXPECT_EQ(repo.search("", true).size(), 1u);
}

TEST_F(ClientRepositoryTest, UpdateChangesNamePhoneAndEmail) {
    const auto id = repo.insert(makeClient("Ann", "ann@example.com"));
    Client c = *repo.findById(id);
    c.name = "Ann Lee";
    c.email = "ann.lee@example.com";
    c.phone = "+44 20 7946 0000";
    repo.update(c);

    const auto after = repo.findById(id);
    EXPECT_EQ(after->name, "Ann Lee");
    EXPECT_EQ(after->email, "ann.lee@example.com");
    EXPECT_EQ(after->phone, "+44 20 7946 0000");
    EXPECT_EQ(after->registeredAt, "2026-01-01T00:00:00Z");   // untouched
}

TEST_F(ClientRepositoryTest, UpdateUnknownClientThrowsNotFound) {
    Client ghost = makeClient("Ghost", "ghost@example.com");
    ghost.id = 4242;
    EXPECT_THROW(repo.update(ghost), NotFoundError);
}

TEST_F(ClientRepositoryTest, UpdateToAnotherClientsEmailThrowsDuplicate) {
    repo.insert(makeClient("Ann", "ann@example.com"));
    const auto bobId = repo.insert(makeClient("Bob", "bob@example.com"));
    Client bob = *repo.findById(bobId);
    bob.email = "ann@example.com";
    EXPECT_THROW(repo.update(bob), DuplicateError);
    EXPECT_EQ(repo.findById(bobId)->email, "bob@example.com");   // unchanged
}

TEST_F(ClientRepositoryTest, SetArchivedFlagsAndUnflagsClient) {
    const auto id = repo.insert(makeClient("Ann", "ann@example.com"));
    repo.setArchived(id, true);
    EXPECT_TRUE(repo.findById(id)->archived);
    repo.setArchived(id, false);
    EXPECT_FALSE(repo.findById(id)->archived);
}

TEST_F(ClientRepositoryTest, SetArchivedUnknownClientThrowsNotFound) {
    EXPECT_THROW(repo.setArchived(777, true), NotFoundError);
}

TEST_F(ClientRepositoryTest, ArchivedClientsRemainInTheDatabase) {
    const auto id = repo.insert(makeClient("Ann", "ann@example.com"));
    repo.setArchived(id, true);
    EXPECT_TRUE(repo.findById(id).has_value());   // never deleted
}

// --------------------------------------------------------------------- search

TEST_F(ClientRepositoryTest, SearchMatchesNameEmailAndPhoneCaseInsensitively) {
    repo.insert(makeClient("Alice Johnson", "alice@acme.com", "555-000-1111"));
    repo.insert(makeClient("Bob Smith", "bob@globex.com", "555-222-3333"));

    EXPECT_EQ(repo.search("JOHNSON", false).size(), 1u);     // name
    EXPECT_EQ(repo.search("globex", false).size(), 1u);      // email
    EXPECT_EQ(repo.search("222-33", false).size(), 1u);      // phone
    EXPECT_EQ(repo.search("555", false).size(), 2u);         // both
    EXPECT_TRUE(repo.search("zzz", false).empty());
}

TEST_F(ClientRepositoryTest, EmptySearchReturnsEveryoneOrderedByName) {
    repo.insert(makeClient("carol", "c@example.com"));
    repo.insert(makeClient("Alice", "a@example.com"));
    repo.insert(makeClient("bob", "b@example.com"));

    const auto all = repo.search("", false);
    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all[0].name, "Alice");
    EXPECT_EQ(all[1].name, "bob");
    EXPECT_EQ(all[2].name, "carol");
}

TEST_F(ClientRepositoryTest, SearchHidesArchivedUnlessRequested) {
    repo.insert(makeClient("Active", "a@example.com"));
    const auto old = repo.insert(makeClient("Archived", "b@example.com"));
    repo.setArchived(old, true);

    EXPECT_EQ(repo.search("", false).size(), 1u);
    EXPECT_EQ(repo.search("", true).size(), 2u);
    EXPECT_TRUE(repo.search("Archived", false).empty());
    EXPECT_EQ(repo.search("Archived", true).size(), 1u);
}

TEST_F(ClientRepositoryTest, LikeWildcardsInSearchTextAreTreatedLiterally) {
    repo.insert(makeClient("100% Legit", "legit@example.com"));
    repo.insert(makeClient("Ann", "ann@example.com"));
    repo.insert(makeClient("snake_case", "snake@example.com"));

    const auto percent = repo.search("%", false);
    ASSERT_EQ(percent.size(), 1u);
    EXPECT_EQ(percent[0].name, "100% Legit");

    const auto underscore = repo.search("_", false);
    ASSERT_EQ(underscore.size(), 1u);
    EXPECT_EQ(underscore[0].name, "snake_case");

    EXPECT_TRUE(repo.search("\\", false).empty());
}

TEST_F(ClientRepositoryTest, SqlInjectionInSearchTextIsHarmless) {
    repo.insert(makeClient("Ann", "ann@example.com"));
    EXPECT_TRUE(repo.search("'; DROP TABLE Clients; --", false).empty());
    EXPECT_TRUE(repo.search("' OR '1'='1", false).empty());
    EXPECT_EQ(repo.search("", false).size(), 1u);   // table intact, data intact
}

TEST_F(ClientRepositoryTest, SqlInjectionInStoredValuesIsStoredVerbatim) {
    const std::string hostile = "Robert'); DROP TABLE Clients;--";
    const auto id = repo.insert(makeClient(hostile, "bobby@example.com"));
    EXPECT_EQ(repo.findById(id)->name, hostile);
}
