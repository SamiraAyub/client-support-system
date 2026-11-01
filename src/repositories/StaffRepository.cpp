#include "repositories/StaffRepository.h"

#include "common/Exceptions.h"

namespace csms {

namespace {

constexpr const char* kSelectColumns =
    "SELECT id, username, full_name, role, password_hash, is_active, created_at FROM Staff";

Staff mapRow(const Statement& row) {
    Staff s;
    s.id = row.columnInt(0);
    s.username = row.columnText(1);
    s.fullName = row.columnText(2);
    s.role = staffRoleFromString(row.columnText(3));
    s.passwordHash = row.columnText(4);
    s.active = row.columnInt(5) != 0;
    s.createdAt = row.columnText(6);
    return s;
}

} // namespace

std::int64_t StaffRepository::insert(const Staff& staff) {
    Statement stmt = db_.prepare(
        "INSERT INTO Staff (username, full_name, role, password_hash, is_active, created_at) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6)");
    stmt.bindText(1, staff.username)
        .bindText(2, staff.fullName)
        .bindText(3, toString(staff.role))
        .bindText(4, staff.passwordHash)
        .bindInt(5, staff.active ? 1 : 0)
        .bindText(6, staff.createdAt);
    try {
        stmt.step();
    } catch (const DatabaseError& e) {
        if (e.isUniqueViolation()) {
            throw DuplicateError("Username '" + staff.username + "' is already taken");
        }
        throw;
    }
    return db_.lastInsertRowId();
}

std::optional<Staff> StaffRepository::findById(std::int64_t id) const {
    Statement stmt = db_.prepare(std::string(kSelectColumns) + " WHERE id = ?1");
    stmt.bindInt(1, id);
    if (!stmt.step()) return std::nullopt;
    return mapRow(stmt);
}

std::optional<Staff> StaffRepository::findByUsername(const std::string& username) const {
    // username is declared COLLATE NOCASE, so '=' ignores letter case.
    Statement stmt = db_.prepare(std::string(kSelectColumns) + " WHERE username = ?1");
    stmt.bindText(1, username);
    if (!stmt.step()) return std::nullopt;
    return mapRow(stmt);
}

std::vector<Staff> StaffRepository::listAll(bool includeInactive) const {
    Statement stmt = db_.prepare(std::string(kSelectColumns) +
                                 " WHERE (?1 = 1 OR is_active = 1)"
                                 " ORDER BY username COLLATE NOCASE, id");
    stmt.bindInt(1, includeInactive ? 1 : 0);
    std::vector<Staff> result;
    while (stmt.step()) result.push_back(mapRow(stmt));
    return result;
}

void StaffRepository::update(std::int64_t id, const std::string& fullName, StaffRole role) {
    Statement stmt = db_.prepare("UPDATE Staff SET full_name = ?1, role = ?2 WHERE id = ?3");
    stmt.bindText(1, fullName).bindText(2, toString(role)).bindInt(3, id);
    stmt.step();
    if (db_.changes() == 0) {
        throw NotFoundError("Staff member " + std::to_string(id) + " does not exist");
    }
}

void StaffRepository::setActive(std::int64_t id, bool active) {
    Statement stmt = db_.prepare("UPDATE Staff SET is_active = ?1 WHERE id = ?2");
    stmt.bindInt(1, active ? 1 : 0).bindInt(2, id);
    stmt.step();
    if (db_.changes() == 0) {
        throw NotFoundError("Staff member " + std::to_string(id) + " does not exist");
    }
}

void StaffRepository::setPasswordHash(std::int64_t id, const std::string& passwordHash) {
    Statement stmt = db_.prepare("UPDATE Staff SET password_hash = ?1 WHERE id = ?2");
    stmt.bindText(1, passwordHash).bindInt(2, id);
    stmt.step();
    if (db_.changes() == 0) {
        throw NotFoundError("Staff member " + std::to_string(id) + " does not exist");
    }
}

std::int64_t StaffRepository::scalar(const std::string& sql) const {
    Statement stmt = db_.prepare(sql);   // callers pass constant SQL only
    stmt.step();
    return stmt.columnInt(0);
}

int StaffRepository::countAll() const {
    return static_cast<int>(scalar("SELECT COUNT(*) FROM Staff"));
}

int StaffRepository::countActiveAdministrators() const {
    return static_cast<int>(
        scalar("SELECT COUNT(*) FROM Staff WHERE role = 'Administrator' AND is_active = 1"));
}

} // namespace csms
