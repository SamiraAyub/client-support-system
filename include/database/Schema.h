#pragma once
// Database schema definition. Bump kSchemaVersion whenever the schema changes.

namespace csms {

constexpr int kSchemaVersion = 1;

// All CREATE TABLE / CREATE INDEX statements (idempotent: IF NOT EXISTS).
const char* schemaSql();

} // namespace csms
