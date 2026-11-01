#pragma once
// SQL for the Staff table. Staff are never deleted: they are deactivated, so
// cases and history that refer to them stay valid.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "database/Database.h"
#include "models/Staff.h"

namespace csms {

class StaffRepository {
public:
    explicit StaffRepository(Database& db) : db_(db) {}

    // Throws DuplicateError if the username exists (case-insensitive).
    std::int64_t insert(const Staff& staff);

    std::optional<Staff> findById(std::int64_t id) const;
    std::optional<Staff> findByUsername(const std::string& username) const;

    // Ordered by username (case-insensitive).
    std::vector<Staff> listAll(bool includeInactive) const;

    // The following throw NotFoundError if the ID does not exist.
    void update(std::int64_t id, const std::string& fullName, StaffRole role);
    void setActive(std::int64_t id, bool active);
    void setPasswordHash(std::int64_t id, const std::string& passwordHash);

    int countAll() const;
    int countActiveAdministrators() const;

private:
    std::int64_t scalar(const std::string& sql) const;

    Database& db_;
};

} // namespace csms
