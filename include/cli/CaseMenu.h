#pragma once
#include <optional>

#include "cli/Console.h"
#include "cli/Format.h"
#include "services/CaseService.h"
#include "services/ClientService.h"
#include "services/StaffService.h"

namespace csms {

// Support-case screens. ClientService and StaffService are used only to turn
// IDs into names for display.
class CaseMenu {
public:
    CaseMenu(Console& console, CaseService& cases, ClientService& clients, StaffService& staff)
        : console_(console), cases_(cases), clients_(clients), staff_(staff) {}
    void run();

private:
    void createCase();
    void viewCase();
    void searchCases();
    void assignCase();
    void changeStatus();
    void reopenCase();

    Names loadNames() const;
    std::optional<Priority> pickPriority(const std::string& zeroLabel);
    std::optional<CaseStatus> pickStatus(const std::string& zeroLabel);

    Console& console_;
    CaseService& cases_;
    ClientService& clients_;
    StaffService& staff_;
};

} // namespace csms
