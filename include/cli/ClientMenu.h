#pragma once
#include "cli/Console.h"
#include "services/ClientService.h"

namespace csms {

// Client management screens. Talks only to ClientService - never to SQL.
class ClientMenu {
public:
    ClientMenu(Console& console, ClientService& clients) : console_(console), clients_(clients) {}
    void run();

private:
    void registerClient();
    void viewClient();
    void searchClients();
    void listClients();
    void updateClient();
    void archiveClient();
    void restoreClient();

    Console& console_;
    ClientService& clients_;
};

} // namespace csms
