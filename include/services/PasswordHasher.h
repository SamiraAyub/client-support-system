#pragma once
// Password hashing, isolated in one small class.
//
// This wraps libsodium's crypto_pwhash_str (Argon2id) - an established,
// audited implementation. NO cryptography is implemented in this project.
//
//  * hash() returns a self-describing string such as
//        $argon2id$v=19$m=65536,t=2,p=1$<salt>$<hash>
//    The random salt and the cost parameters are inside the string, so it is the
//    only thing that has to be stored, and each call gives a different result.
//  * verify() re-derives the hash and compares in constant time.
//
// Swapping the algorithm or library later means changing only this class.
// Only this class (PasswordHasher.cpp) includes libsodium.

#include <cstddef>
#include <string>

namespace csms {

class PasswordHasher {
public:
    // Production strength (libsodium's "interactive" limits: ~64 MiB, 2 passes).
    static PasswordHasher interactive();

    // Smallest allowed cost. ONLY for unit tests, so they run quickly.
    static PasswordHasher lowCostForTests();

    // Throws CsmsError if libsodium cannot hash (e.g. out of memory).
    std::string hash(const std::string& password) const;

    // True if `password` matches `encodedHash`. Never throws for a wrong
    // password or a malformed hash - both simply return false.
    bool verify(const std::string& encodedHash, const std::string& password) const;

private:
    PasswordHasher(unsigned long long opsLimit, std::size_t memLimit);

    unsigned long long opsLimit_;
    std::size_t memLimit_;
};

} // namespace csms
