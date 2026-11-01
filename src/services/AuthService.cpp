#include "services/AuthService.h"

#include "common/Exceptions.h"
#include "common/Validation.h"

namespace csms {

AuthService::AuthService(StaffRepository& staff, const PasswordHasher& hasher, Session& session)
    : staff_(staff), hasher_(hasher), session_(session),
      dummyHash_(hasher.hash("timing-equalisation-placeholder")) {}

Staff AuthService::login(const std::string& username, const std::string& password) {
    const AuthenticationError failure("Invalid username or password");

    const auto user = staff_.findByUsername(trim(username));
    if (!user) {
        hasher_.verify(dummyHash_, password);   // result ignored: only spends the same time
        throw failure;
    }
    if (!hasher_.verify(user->passwordHash, password) || !user->active) {
        throw failure;
    }

    session_.start(*user);
    Staff result = *user;
    result.passwordHash.clear();
    return result;
}

} // namespace csms
