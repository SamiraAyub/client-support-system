#pragma once
#include "cli/Console.h"
#include "services/StaffService.h"

namespace csms {

// Staff administration screens (the service enforces administrator-only access).
class StaffMenu {
public:
    StaffMenu(Console& console, StaffService& staff) : console_(console), staff_(staff) {}
    void run();

private:
    void listStaff();
    void createStaff();
    void editStaff();
    void deactivateStaff();
    void reactivateStaff();
    void resetPassword();

    Console& console_;
    StaffService& staff_;
};

} // namespace csms
