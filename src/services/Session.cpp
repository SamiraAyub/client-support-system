#include "services/Session.h"

#include "common/Exceptions.h"

namespace csms {

void Session::start(const Staff& user) {
    Staff copy = user;
    copy.passwordHash.clear();   // the session never needs (or keeps) the hash
    user_ = std::move(copy);
}

const Staff& Session::requireLogin() const {
    if (!user_) {
        throw AuthorizationError("You must be logged in to do that");
    }
    return *user_;
}

const Staff& Session::requireAdministrator() const {
    const Staff& user = requireLogin();
    if (user.role != StaffRole::Administrator) {
        throw AuthorizationError("Administrator privileges are required for this operation");
    }
    return user;
}

} // namespace csms
