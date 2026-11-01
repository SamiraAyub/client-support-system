#include "services/ClientService.h"

#include "common/Exceptions.h"
#include "common/TimeUtils.h"
#include "common/Validation.h"

namespace csms {

namespace {

std::string idText(std::int64_t id) { return std::to_string(id); }

} // namespace

Client ClientService::registerClient(const std::string& name, const std::string& email,
                                     const std::string& phone) {
    session_.requireLogin();
    Client client;
    client.name = validateName(name);
    client.email = validateEmail(email);
    client.phone = validatePhone(phone);
    client.registeredAt = currentTimestampUtc();
    client.archived = false;

    if (const auto existing = repository_.findByEmail(client.email)) {
        throw DuplicateError("A client with email '" + client.email +
                             "' already exists (ID " + idText(existing->id) + ")");
    }

    client.id = repository_.insert(client);
    return client;
}

Client ClientService::getClient(std::int64_t id) const {
    session_.requireLogin();
    const auto client = repository_.findById(id);
    if (!client) {
        throw NotFoundError("Client " + idText(id) + " does not exist");
    }
    return *client;
}

Client ClientService::updateClient(std::int64_t id, const std::string& name,
                                   const std::string& email, const std::string& phone) {
    session_.requireLogin();
    // Existence is checked before input: an unknown ID is the more basic problem.
    Client client = getClient(id);
    if (client.archived) {
        throw ValidationError("Client " + idText(id) +
                              " is archived; restore the client before editing");
    }

    client.name = validateName(name);
    client.email = validateEmail(email);
    client.phone = validatePhone(phone);

    // A different client holding this email is a duplicate; the same client
    // keeping (or re-casing) its own email is fine.
    if (const auto other = repository_.findByEmail(client.email); other && other->id != id) {
        throw DuplicateError("A client with email '" + client.email +
                             "' already exists (ID " + idText(other->id) + ")");
    }

    repository_.update(client);
    return client;
}

void ClientService::archiveClient(std::int64_t id) {
    session_.requireAdministrator();
    const Client client = getClient(id);
    if (client.archived) {
        throw ValidationError("Client " + idText(id) + " is already archived");
    }
    repository_.setArchived(id, true);
}

void ClientService::restoreClient(std::int64_t id) {
    session_.requireAdministrator();
    const Client client = getClient(id);
    if (!client.archived) {
        throw ValidationError("Client " + idText(id) + " is not archived");
    }
    repository_.setArchived(id, false);
}

std::vector<Client> ClientService::searchClients(const std::string& text,
                                                 bool includeArchived) const {
    session_.requireLogin();
    return repository_.search(trim(text), includeArchived);
}

std::vector<Client> ClientService::listClients(bool includeArchived) const {
    session_.requireLogin();
    return repository_.search("", includeArchived);
}

} // namespace csms
