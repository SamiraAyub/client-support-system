#pragma once
// Staff management.
//
// Permissions
//   * Read operations (getStaff, listStaff) ...... any logged-in user
//   * changeOwnPassword .......................... any logged-in user
//   * everything else ............................ Administrators only
//   * bootstrapAdministrator ..................... nobody logged in; only while the
//                                                  Staff table is EMPTY
//
// Staff are never deleted: deactivating keeps every historical reference valid.
// Safety rules: you cannot change your own role or deactivate yourself, and the
// last active administrator can never be deactivated or demoted.

#include <cstdint>
#include <string>
#include <vector>

#include "models/Staff.h"
#include "repositories/StaffRepository.h"
#include "services/PasswordHasher.h"
#include "services/Session.h"

namespace csms {

class StaffService {
public:
    StaffService(StaffRepository& staff, const PasswordHasher& hasher, const Session& session)
        : staff_(staff), hasher_(hasher), session_(session) {}

    // True when at least one staff account exists.
    bool hasStaff() const;

    // First-run setup. Throws AuthorizationError if any staff already exist.
    Staff bootstrapAdministrator(const std::string& username, const std::string& fullName,
                                 const std::string& password);

    // Admin only. Throws ValidationError, DuplicateError.
    Staff createStaff(const std::string& username, const std::string& fullName, StaffRole role,
                      const std::string& password);

    Staff getStaff(std::int64_t id) const;
    std::vector<Staff> listStaff(bool includeInactive) const;

    // Admin only. Changes name and role.
    Staff updateStaff(std::int64_t id, const std::string& fullName, StaffRole role);

    // Admin only.
    void deactivateStaff(std::int64_t id);
    void reactivateStaff(std::int64_t id);
    void resetPassword(std::int64_t id, const std::string& newPassword);

    // Any logged-in user; must supply the current password.
    void changeOwnPassword(const std::string& currentPassword, const std::string& newPassword);

private:
    Staff load(std::int64_t id) const;

    StaffRepository& staff_;
    const PasswordHasher& hasher_;
    const Session& session_;
};

} // namespace csms
