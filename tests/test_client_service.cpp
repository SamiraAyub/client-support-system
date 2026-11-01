#include <gtest/gtest.h>

#include <regex>
#include <string>

#include "common/Exceptions.h"
#include "repositories/ClientRepository.h"
#include "services/ClientService.h"
#include "ServiceFixture.h"

using namespace csms;

namespace {

class ClientServiceTest : public test_support::ServiceFixture {
protected:
    ClientService& service = clientService;

    Client registerAnn() {
        return service.registerClient("Ann Lee", "ann@example.com", "+1 555 123 4567");
    }
};

} // namespace

// --------------------------------------------------------------- registration

TEST_F(ClientServiceTest, RegisterAssignsIdTimestampAndActiveStatus) {
    const Client c = registerAnn();
    EXPECT_GT(c.id, 0);
    EXPECT_FALSE(c.archived);
    EXPECT_TRUE(std::regex_match(c.registeredAt,
                                 std::regex(R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$)")));

    const Client stored = service.getClient(c.id);
    EXPECT_EQ(stored.name, "Ann Lee");
    EXPECT_EQ(stored.email, "ann@example.com");
    EXPECT_EQ(stored.phone, "+1 555 123 4567");
    EXPECT_EQ(stored.registeredAt, c.registeredAt);
}

TEST_F(ClientServiceTest, RegisterGeneratesDistinctIds) {
    const Client a = registerAnn();
    const Client b = service.registerClient("Bob", "bob@example.com", "555-222-3333");
    EXPECT_NE(a.id, b.id);
}

TEST_F(ClientServiceTest, RegisterTrimsInput) {
    const Client c = service.registerClient("  Ann Lee ", "  ann@example.com\t", " 555-123-4567 ");
    EXPECT_EQ(c.name, "Ann Lee");
    EXPECT_EQ(c.email, "ann@example.com");
    EXPECT_EQ(c.phone, "555-123-4567");
}

TEST_F(ClientServiceTest, RegisterRejectsInvalidInputAndStoresNothing) {
    EXPECT_THROW(service.registerClient("", "ann@example.com", "555-123-4567"), ValidationError);
    EXPECT_THROW(service.registerClient("Ann", "not-an-email", "555-123-4567"), ValidationError);
    EXPECT_THROW(service.registerClient("Ann", "ann@example.com", "abc"), ValidationError);
    EXPECT_TRUE(service.listClients(true).empty());
}

TEST_F(ClientServiceTest, RegisterRejectsDuplicateEmail) {
    const Client first = registerAnn();
    try {
        service.registerClient("Another Ann", "ann@example.com", "555-999-0000");
        FAIL() << "Expected DuplicateError";
    } catch (const DuplicateError& e) {
        // The message tells the user which existing client they collided with.
        EXPECT_NE(std::string(e.what()).find("ID " + std::to_string(first.id)), std::string::npos);
    }
    EXPECT_EQ(service.listClients(true).size(), 1u);
}

TEST_F(ClientServiceTest, DuplicateEmailCheckIgnoresCaseAndWhitespace) {
    registerAnn();
    EXPECT_THROW(service.registerClient("Ann Two", "  ANN@Example.COM ", "555-999-0000"),
                 DuplicateError);
}

TEST_F(ClientServiceTest, ArchivedClientsStillBlockDuplicateEmails) {
    const Client c = registerAnn();
    service.archiveClient(c.id);
    EXPECT_THROW(service.registerClient("Ann Again", "ann@example.com", "555-999-0000"),
                 DuplicateError);
}

// --------------------------------------------------------------------- lookup

TEST_F(ClientServiceTest, GetClientUnknownIdThrowsNotFound) {
    EXPECT_THROW(service.getClient(31337), NotFoundError);
}

// --------------------------------------------------------------------- update

TEST_F(ClientServiceTest, UpdateChangesFieldsAndKeepsRegistrationData) {
    const Client original = registerAnn();
    const Client updated = service.updateClient(original.id, "Ann Marie Lee",
                                                "annmarie@example.com", "555-000-9999");
    EXPECT_EQ(updated.id, original.id);
    EXPECT_EQ(updated.name, "Ann Marie Lee");

    const Client stored = service.getClient(original.id);
    EXPECT_EQ(stored.name, "Ann Marie Lee");
    EXPECT_EQ(stored.email, "annmarie@example.com");
    EXPECT_EQ(stored.phone, "555-000-9999");
    EXPECT_EQ(stored.registeredAt, original.registeredAt);
}

TEST_F(ClientServiceTest, UpdateAllowsKeepingOrRecasingOwnEmail) {
    const Client c = registerAnn();
    EXPECT_NO_THROW(service.updateClient(c.id, "Ann Lee", "ann@example.com", "555-123-4567"));
    EXPECT_NO_THROW(service.updateClient(c.id, "Ann Lee", "Ann@Example.com", "555-123-4567"));
    EXPECT_EQ(service.getClient(c.id).email, "Ann@Example.com");
}

TEST_F(ClientServiceTest, UpdateRejectsAnotherClientsEmail) {
    const Client ann = registerAnn();
    const Client bob = service.registerClient("Bob", "bob@example.com", "555-222-3333");
    try {
        service.updateClient(bob.id, "Bob", "ann@example.com", "555-222-3333");
        FAIL() << "Expected DuplicateError";
    } catch (const DuplicateError& e) {
        // Only the service's own pre-check names the clashing client; the
        // database constraint (the safety net) cannot know the other ID.
        EXPECT_NE(std::string(e.what()).find("ID " + std::to_string(ann.id)), std::string::npos);
    }
    EXPECT_EQ(service.getClient(bob.id).email, "bob@example.com");
}

TEST_F(ClientServiceTest, UpdateRejectsInvalidInputAndChangesNothing) {
    const Client c = registerAnn();
    EXPECT_THROW(service.updateClient(c.id, "  ", "ann@example.com", "555-123-4567"),
                 ValidationError);
    EXPECT_THROW(service.updateClient(c.id, "Ann", "bad@", "555-123-4567"), ValidationError);
    EXPECT_THROW(service.updateClient(c.id, "Ann", "ann@example.com", "12"), ValidationError);
    EXPECT_EQ(service.getClient(c.id).name, "Ann Lee");
}

TEST_F(ClientServiceTest, UpdateUnknownClientThrowsNotFound) {
    EXPECT_THROW(service.updateClient(999, "X", "x@example.com", "555-123-4567"), NotFoundError);
}

TEST_F(ClientServiceTest, ArchivedClientCannotBeUpdated) {
    const Client c = registerAnn();
    service.archiveClient(c.id);
    EXPECT_THROW(service.updateClient(c.id, "New Name", "ann@example.com", "555-123-4567"),
                 ValidationError);
    EXPECT_EQ(service.getClient(c.id).name, "Ann Lee");
}

// -------------------------------------------------------------------- archive

TEST_F(ClientServiceTest, ArchiveHidesClientFromDefaultListButKeepsTheRecord) {
    const Client c = registerAnn();
    service.archiveClient(c.id);

    EXPECT_TRUE(service.getClient(c.id).archived);               // still retrievable
    EXPECT_TRUE(service.listClients().empty());                  // hidden by default
    EXPECT_EQ(service.listClients(true).size(), 1u);             // visible on request
    EXPECT_TRUE(service.searchClients("Ann").empty());
    EXPECT_EQ(service.searchClients("Ann", true).size(), 1u);
}

TEST_F(ClientServiceTest, ArchivingTwiceOrUnknownClientFails) {
    const Client c = registerAnn();
    service.archiveClient(c.id);
    EXPECT_THROW(service.archiveClient(c.id), ValidationError);
    EXPECT_THROW(service.archiveClient(555), NotFoundError);
}

TEST_F(ClientServiceTest, RestoreReversesArchive) {
    const Client c = registerAnn();
    service.archiveClient(c.id);
    service.restoreClient(c.id);

    EXPECT_FALSE(service.getClient(c.id).archived);
    EXPECT_EQ(service.listClients().size(), 1u);
    EXPECT_NO_THROW(service.updateClient(c.id, "Ann L.", "ann@example.com", "555-123-4567"));
}

TEST_F(ClientServiceTest, RestoreRejectsActiveOrUnknownClient) {
    const Client c = registerAnn();
    EXPECT_THROW(service.restoreClient(c.id), ValidationError);
    EXPECT_THROW(service.restoreClient(555), NotFoundError);
}

// --------------------------------------------------------------------- search

TEST_F(ClientServiceTest, SearchTrimsTextAndMatchesAcrossFields) {
    service.registerClient("Alice Johnson", "alice@acme.com", "555-000-1111");
    service.registerClient("Bob Smith", "bob@globex.com", "555-222-3333");

    EXPECT_EQ(service.searchClients("  johnson ").size(), 1u);
    EXPECT_EQ(service.searchClients("globex").size(), 1u);
    EXPECT_EQ(service.searchClients("555").size(), 2u);
    EXPECT_EQ(service.searchClients("   ").size(), 2u);   // blank = everyone
}

// ---------------------------------------------------------------- persistence

TEST(ClientServicePersistenceTest, ClientsAndArchiveStatusSurviveRestart) {
    test_support::TempDbPath file;
    std::int64_t annId = 0;
    std::int64_t bobId = 0;

    {   // first "run" of the application
        Database db(file.str());
        db.initializeSchema();
        ClientRepository repo(db);
        Session session;
        Staff user;
        user.id = 1;
        user.role = StaffRole::Administrator;
        session.start(user);
        ClientService service(repo, session);
        annId = service.registerClient("Ann Lee", "ann@example.com", "555-123-4567").id;
        bobId = service.registerClient("Bob Roy", "bob@example.com", "555-222-3333").id;
        service.archiveClient(bobId);
    }

    {   // second "run": everything must still be there
        Database db(file.str());
        db.initializeSchema();
        ClientRepository repo(db);
        Session session;
        Staff user;
        user.id = 1;
        user.role = StaffRole::Administrator;
        session.start(user);
        ClientService service(repo, session);

        EXPECT_EQ(service.getClient(annId).name, "Ann Lee");
        EXPECT_FALSE(service.getClient(annId).archived);
        EXPECT_TRUE(service.getClient(bobId).archived);
        EXPECT_EQ(service.listClients().size(), 1u);
        EXPECT_EQ(service.listClients(true).size(), 2u);

        // Duplicate prevention also works against data from the previous run.
        EXPECT_THROW(service.registerClient("Ann Copy", "ann@example.com", "555-000-0000"),
                     DuplicateError);

        // New IDs continue after the old ones.
        const Client carol = service.registerClient("Carol", "carol@example.com", "555-444-5555");
        EXPECT_GT(carol.id, bobId);
    }
}
