#pragma once
// Login / logout.
//
// Security notes
//  * Passwords are verified against the stored Argon2id hash; they are never
//    stored, logged or compared as plain text.
//  * Every failure (unknown user, wrong password, deactivated account) produces
//    the SAME message, so an attacker cannot discover which usernames exist.
//  * For an unknown username a dummy verification is still performed, so the
//    response takes about as long as for a real user (no timing side-channel).

#include <string>

#include "models/Staff.h"
#include "repositories/StaffRepository.h"
#include "services/PasswordHasher.h"
#include "services/Session.h"

namespace csms {

class AuthService {
public:
    AuthService(StaffRepository& staff, const PasswordHasher& hasher, Session& session);

    // Starts the session and returns the user (without password hash).
    // Throws AuthenticationError on any failure.
    Staff login(const std::string& username, const std::string& password);

    void logout() noexcept { session_.end(); }

private:
    StaffRepository& staff_;
    const PasswordHasher& hasher_;
    Session& session_;
    std::string dummyHash_;   // used to equalise timing for unknown usernames
};

} // namespace csms
