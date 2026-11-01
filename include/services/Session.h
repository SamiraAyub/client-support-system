#pragma once
// Who is using the application right now.
//
// One Session object is created in main() and handed (by reference) to every
// service that needs to know the acting user. That is explicit dependency
// injection, not global state, and it lets tests log in as any user.
//
// Only AuthService should call start() in production code; tests call it
// directly to act as a given staff member without hashing passwords.

#include <optional>

#include "models/Staff.h"

namespace csms {

class Session {
public:
    bool isLoggedIn() const noexcept { return user_.has_value(); }

    // The stored copy never contains the password hash.
    void start(const Staff& user);
    void end() noexcept { user_.reset(); }

    // Throws AuthorizationError("You must be logged in") when nobody is.
    const Staff& requireLogin() const;

    // Throws AuthorizationError unless the user is an Administrator.
    const Staff& requireAdministrator() const;

private:
    std::optional<Staff> user_;
};

} // namespace csms
