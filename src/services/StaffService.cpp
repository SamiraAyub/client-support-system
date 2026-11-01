#include "services/StaffService.h"

#include "common/Exceptions.h"
#include "common/TimeUtils.h"
#include "common/Validation.h"

namespace csms {

namespace {
std::string idText(std::int64_t id) { return std::to_string(id); }
} // namespace

Staff StaffService::load(std::int64_t id) const {
    const auto found = staff_.findById(id);
    if (!found) {
        throw NotFoundError("Staff member " + idText(id) + " does not exist");
    }
    return *found;
}

bool StaffService::hasStaff() const {
    return staff_.countAll() > 0;
}

Staff StaffService::bootstrapAdministrator(const std::string& username,
                                           const std::string& fullName,
                                           const std::string& password) {
    if (staff_.countAll() > 0) {
        throw AuthorizationError("Setup is already complete; log in as an administrator instead");
    }
    Staff s;
    s.username = validateUsername(username);
    s.fullName = validateName(fullName);
    s.role = StaffRole::Administrator;
    s.passwordHash = hasher_.hash(validatePassword(password));
    s.active = true;
    s.createdAt = currentTimestampUtc();
    s.id = staff_.insert(s);
    return s;
}

Staff StaffService::createStaff(const std::string& username, const std::string& fullName,
                                StaffRole role, const std::string& password) {
    session_.requireAdministrator();

    Staff s;
    s.username = validateUsername(username);
    s.fullName = validateName(fullName);
    s.role = role;
    s.passwordHash = hasher_.hash(validatePassword(password));
    s.active = true;
    s.createdAt = currentTimestampUtc();

    if (staff_.findByUsername(s.username)) {
        throw DuplicateError("Username '" + s.username + "' is already taken");
    }
    s.id = staff_.insert(s);
    return s;
}

Staff StaffService::getStaff(std::int64_t id) const {
    session_.requireLogin();
    return load(id);
}

std::vector<Staff> StaffService::listStaff(bool includeInactive) const {
    session_.requireLogin();
    return staff_.listAll(includeInactive);
}

Staff StaffService::updateStaff(std::int64_t id, const std::string& fullName, StaffRole role) {
    const Staff& actor = session_.requireAdministrator();
    Staff target = load(id);
    const std::string cleanName = validateName(fullName);

    if (role != target.role) {
        if (target.id == actor.id) {
            throw ValidationError("You cannot change your own role");
        }
        const bool demotingAnActiveAdmin =
            target.role == StaffRole::Administrator && target.active;
        if (demotingAnActiveAdmin && staff_.countActiveAdministrators() <= 1) {
            throw ValidationError("At least one active administrator must remain");
        }
    }

    staff_.update(id, cleanName, role);
    target.fullName = cleanName;
    target.role = role;
    return target;
}

void StaffService::deactivateStaff(std::int64_t id) {
    const Staff& actor = session_.requireAdministrator();
    const Staff target = load(id);

    if (target.id == actor.id) {
        throw ValidationError("You cannot deactivate your own account");
    }
    if (!target.active) {
        throw ValidationError("Staff member '" + target.username + "' is already deactivated");
    }
    if (target.role == StaffRole::Administrator && staff_.countActiveAdministrators() <= 1) {
        throw ValidationError("At least one active administrator must remain");
    }
    staff_.setActive(id, false);
}

void StaffService::reactivateStaff(std::int64_t id) {
    session_.requireAdministrator();
    const Staff target = load(id);
    if (target.active) {
        throw ValidationError("Staff member '" + target.username + "' is already active");
    }
    staff_.setActive(id, true);
}

void StaffService::resetPassword(std::int64_t id, const std::string& newPassword) {
    session_.requireAdministrator();
    load(id);   // NotFoundError for an unknown ID
    staff_.setPasswordHash(id, hasher_.hash(validatePassword(newPassword)));
}

void StaffService::changeOwnPassword(const std::string& currentPassword,
                                     const std::string& newPassword) {
    const std::int64_t myId = session_.requireLogin().id;
    const Staff me = load(myId);
    if (!hasher_.verify(me.passwordHash, currentPassword)) {
        throw AuthenticationError("The current password is incorrect");
    }
    staff_.setPasswordHash(myId, hasher_.hash(validatePassword(newPassword)));
}

} // namespace csms
