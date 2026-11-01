#include "services/PasswordHasher.h"

#include <sodium.h>

#include "common/Exceptions.h"

namespace csms {

namespace {

// sodium_init() is idempotent and thread-safe; it returns -1 only on failure.
void ensureSodiumInitialised() {
    if (sodium_init() < 0) {
        throw CsmsError("Failed to initialise the cryptography library (libsodium)");
    }
}

} // namespace

PasswordHasher::PasswordHasher(unsigned long long opsLimit, std::size_t memLimit)
    : opsLimit_(opsLimit), memLimit_(memLimit) {
    ensureSodiumInitialised();
}

PasswordHasher PasswordHasher::interactive() {
    return PasswordHasher(crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE);
}

PasswordHasher PasswordHasher::lowCostForTests() {
    return PasswordHasher(crypto_pwhash_OPSLIMIT_MIN, crypto_pwhash_MEMLIMIT_MIN);
}

std::string PasswordHasher::hash(const std::string& password) const {
    char out[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(out, password.c_str(), password.size(), opsLimit_, memLimit_) != 0) {
        throw CsmsError("Password hashing failed (the system may be out of memory)");
    }
    return std::string(out);
}

bool PasswordHasher::verify(const std::string& encodedHash, const std::string& password) const {
    // Reject anything that cannot be a libsodium hash string before calling it.
    if (encodedHash.empty() || encodedHash.size() >= crypto_pwhash_STRBYTES) return false;
    return crypto_pwhash_str_verify(encodedHash.c_str(), password.c_str(), password.size()) == 0;
}

} // namespace csms
