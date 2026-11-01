#include "cli/StaffMenu.h"

#include "cli/Format.h"

namespace csms {

namespace {
// 1 = Support Agent, 2 = Administrator, 0 = cancel (returns false).
bool pickRole(Console& console, StaffRole& role) {
    switch (console.askChoice("Role", {"Support Agent", "Administrator"}, "Cancel")) {
        case 1: role = StaffRole::SupportAgent; return true;
        case 2: role = StaffRole::Administrator; return true;
        default: return false;
    }
}
} // namespace

void StaffMenu::run() {
    for (;;) {
        const int choice = console_.askChoice(
            "Staff Management (administrator)",
            {"List staff", "Create staff account", "Edit name / role", "Deactivate staff",
             "Reactivate staff", "Reset a user's password"});
        if (choice == 0) return;

        console_.runGuarded([&] {
            switch (choice) {
                case 1: listStaff(); break;
                case 2: createStaff(); break;
                case 3: editStaff(); break;
                case 4: deactivateStaff(); break;
                case 5: reactivateStaff(); break;
                case 6: resetPassword(); break;
                default: break;
            }
        });
    }
}

void StaffMenu::listStaff() {
    printStaff(console_.out(), staff_.listStaff(true));
}

void StaffMenu::createStaff() {
    const std::string username = console_.ask("Username");
    const std::string fullName = console_.ask("Full name");
    StaffRole role = StaffRole::SupportAgent;
    if (!pickRole(console_, role)) {
        console_.println("Cancelled.");
        return;
    }
    const std::string password = console_.askNewPassword();
    const Staff s = staff_.createStaff(username, fullName, role, password);
    console_.println("Created " + toDisplayString(s.role) + " '" + s.username + "' (ID " +
                     std::to_string(s.id) + ").");
}

void StaffMenu::editStaff() {
    const Staff current = staff_.getStaff(console_.askInt("Staff ID"));
    console_.println("Currently: " + current.fullName + ", " + toDisplayString(current.role));

    const std::string name = console_.ask("New full name [" + current.fullName + "]");
    StaffRole role = current.role;
    if (console_.confirm("Change the role?")) {
        if (!pickRole(console_, role)) role = current.role;
    }
    staff_.updateStaff(current.id, name.empty() ? current.fullName : name, role);
    console_.println("Staff member #" + std::to_string(current.id) + " updated.");
}

void StaffMenu::deactivateStaff() {
    const Staff s = staff_.getStaff(console_.askInt("Staff ID"));
    if (console_.confirm("Deactivate '" + s.username + "'? (they keep their history but cannot log in)")) {
        staff_.deactivateStaff(s.id);
        console_.println("'" + s.username + "' deactivated.");
    } else {
        console_.println("Cancelled.");
    }
}

void StaffMenu::reactivateStaff() {
    const std::int64_t id = console_.askInt("Staff ID");
    staff_.reactivateStaff(id);
    console_.println("Staff member #" + std::to_string(id) + " reactivated.");
}

void StaffMenu::resetPassword() {
    const Staff s = staff_.getStaff(console_.askInt("Staff ID"));
    const std::string password = console_.askNewPassword();
    staff_.resetPassword(s.id, password);
    console_.println("Password for '" + s.username + "' has been reset.");
}

} // namespace csms
