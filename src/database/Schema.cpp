#include "database/Schema.h"

namespace csms {

// Design notes
//  * Timestamps are TEXT in ISO 8601 UTC (see common/TimeUtils.h).
//  * Booleans are INTEGER 0/1 guarded by CHECK constraints.
//  * Enum-like columns are TEXT guarded by CHECK constraints, so even a bug in
//    the application cannot store an invalid value.
//  * Foreign keys use ON DELETE RESTRICT: nothing referenced by history can be
//    deleted, which preserves historical staff / client references.
//  * Email and username are UNIQUE COLLATE NOCASE: "A@x.com" == "a@x.com".
const char* schemaSql() {
    return R"SQL(
CREATE TABLE IF NOT EXISTS Clients (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    name          TEXT    NOT NULL CHECK (length(trim(name)) > 0),
    email         TEXT    NOT NULL UNIQUE COLLATE NOCASE CHECK (length(trim(email)) > 0),
    phone         TEXT    NOT NULL,
    registered_at TEXT    NOT NULL,
    is_archived   INTEGER NOT NULL DEFAULT 0 CHECK (is_archived IN (0, 1))
);

CREATE TABLE IF NOT EXISTS Staff (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    username      TEXT    NOT NULL UNIQUE COLLATE NOCASE CHECK (length(trim(username)) > 0),
    full_name     TEXT    NOT NULL CHECK (length(trim(full_name)) > 0),
    role          TEXT    NOT NULL CHECK (role IN ('Administrator', 'SupportAgent')),
    password_hash TEXT    NOT NULL,
    is_active     INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0, 1)),
    created_at    TEXT    NOT NULL
);

CREATE TABLE IF NOT EXISTS SupportCases (
    id                INTEGER PRIMARY KEY AUTOINCREMENT,
    client_id         INTEGER NOT NULL REFERENCES Clients(id) ON DELETE RESTRICT,
    title             TEXT    NOT NULL CHECK (length(trim(title)) > 0),
    description       TEXT    NOT NULL DEFAULT '',
    priority          TEXT    NOT NULL CHECK (priority IN ('Low', 'Medium', 'High')),
    status            TEXT    NOT NULL DEFAULT 'Open'
                              CHECK (status IN ('Open', 'InProgress', 'Resolved', 'Closed')),
    assigned_staff_id INTEGER REFERENCES Staff(id) ON DELETE RESTRICT,
    created_at        TEXT    NOT NULL,
    updated_at        TEXT    NOT NULL
);

CREATE TABLE IF NOT EXISTS CaseHistory (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    case_id             INTEGER NOT NULL REFERENCES SupportCases(id) ON DELETE RESTRICT,
    changed_by_staff_id INTEGER NOT NULL REFERENCES Staff(id) ON DELETE RESTRICT,
    previous_status     TEXT CHECK (previous_status IS NULL OR
                                    previous_status IN ('Open', 'InProgress', 'Resolved', 'Closed')),
    new_status          TEXT NOT NULL
                             CHECK (new_status IN ('Open', 'InProgress', 'Resolved', 'Closed')),
    note                TEXT NOT NULL DEFAULT '',
    changed_at          TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_clients_name         ON Clients(name);
CREATE INDEX IF NOT EXISTS idx_cases_client         ON SupportCases(client_id);
CREATE INDEX IF NOT EXISTS idx_cases_status         ON SupportCases(status);
CREATE INDEX IF NOT EXISTS idx_cases_priority       ON SupportCases(priority);
CREATE INDEX IF NOT EXISTS idx_cases_assigned_staff ON SupportCases(assigned_staff_id);
CREATE INDEX IF NOT EXISTS idx_history_case_time    ON CaseHistory(case_id, changed_at);
CREATE INDEX IF NOT EXISTS idx_history_staff        ON CaseHistory(changed_by_staff_id);
)SQL";
}

} // namespace csms
