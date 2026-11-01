#include "cli/ClientMenu.h"

#include "cli/Format.h"

namespace csms {

void ClientMenu::run() {
    for (;;) {
        const int choice = console_.askChoice(
            "Client Management",
            {"Register new client", "View client by ID", "Search clients", "List all clients",
             "Update client", "Archive client (administrator)",
             "Restore archived client (administrator)"});
        if (choice == 0) return;

        console_.runGuarded([&] {
            switch (choice) {
                case 1: registerClient(); break;
                case 2: viewClient(); break;
                case 3: searchClients(); break;
                case 4: listClients(); break;
                case 5: updateClient(); break;
                case 6: archiveClient(); break;
                case 7: restoreClient(); break;
                default: break;
            }
        });
    }
}

void ClientMenu::registerClient() {
    const std::string name = console_.ask("Full name");
    const std::string email = console_.ask("Email");
    const std::string phone = console_.ask("Phone");
    const Client c = clients_.registerClient(name, email, phone);
    console_.println("Registered client #" + std::to_string(c.id) + ": " + c.name);
}

void ClientMenu::viewClient() {
    printClientDetails(console_.out(), clients_.getClient(console_.askInt("Client ID")));
}

void ClientMenu::searchClients() {
    const std::string text = console_.ask("Search text (name, email or phone)");
    const bool archived = console_.confirm("Include archived clients?");
    printClients(console_.out(), clients_.searchClients(text, archived));
}

void ClientMenu::listClients() {
    const bool archived = console_.confirm("Include archived clients?");
    printClients(console_.out(), clients_.listClients(archived));
}

void ClientMenu::updateClient() {
    const Client current = clients_.getClient(console_.askInt("Client ID"));
    printClientDetails(console_.out(), current);
    console_.println("Press Enter to keep the current value.");

    auto askKeep = [&](const std::string& label, const std::string& existing) {
        const std::string answer = console_.ask(label + " [" + existing + "]");
        return answer.empty() ? existing : answer;
    };
    const std::string name = askKeep("Name", current.name);
    const std::string email = askKeep("Email", current.email);
    const std::string phone = askKeep("Phone", current.phone);

    clients_.updateClient(current.id, name, email, phone);
    console_.println("Client #" + std::to_string(current.id) + " updated.");
}

void ClientMenu::archiveClient() {
    const Client c = clients_.getClient(console_.askInt("Client ID"));
    if (console_.confirm("Archive '" + c.name + "'? (the record is kept, not deleted)")) {
        clients_.archiveClient(c.id);
        console_.println("Client #" + std::to_string(c.id) + " archived.");
    } else {
        console_.println("Cancelled.");
    }
}

void ClientMenu::restoreClient() {
    const std::int64_t id = console_.askInt("Client ID");
    clients_.restoreClient(id);
    console_.println("Client #" + std::to_string(id) + " restored.");
}

} // namespace csms
