# Architecture

## Layers

| Layer | Folder | Responsibility | May depend on |
|---|---|---|---|
| Presentation | `cli/` | Menus, prompts, formatting | Services, models |
| Service | `services/` | Validation, business rules, permissions, transactions | Repositories, Database (for `Transaction`), models |
| Repository | `repositories/` | SQL for one table each; row <-> model mapping | Database, models |
| Database | `database/` | Connection, schema, prepared statements, transactions | SQLite |

Shared: `models/` (plain data structs, `enum class`es), `common/` (exceptions, validation, time).

**Rules:** the CLI never runs SQL and never includes repository/database headers; repositories
never read or print; services never print; `sqlite3.h` is included only inside `database/`;
there is no global mutable state.

## Composition root (`src/main.cpp`)

```
Database -> Repositories -> Services -> Console/Application
                              ^
                           Session (who is logged in), PasswordHasher
```
`main()` creates each object once and passes references through constructors. Services that
need to know the acting user receive a `const Session&`; only `AuthService` can start/end it.

## Database layer
- **`Database`** - RAII owner of `sqlite3*`; sets `PRAGMA foreign_keys = ON`, a busy timeout and
  extended result codes; `initializeSchema()` is idempotent and versioned via `user_version`.
- **`Statement`** - RAII prepared statement. Values are only ever *bound*. Parameters are
  1-based (`?1`, `?2`...). Move-only.
- **`Transaction`** - scope guard (`BEGIN` in the constructor, `ROLLBACK` in the destructor
  unless `commit()` ran). Nested transactions throw - one unit of work, one transaction.

## Repository layer
One class per table (`ClientRepository`, `StaffRepository`, `CaseRepository`,
`HistoryRepository`) plus `ReportRepository` for read-only aggregates. They translate low-level
failures into domain exceptions (`UNIQUE` -> `DuplicateError`, zero rows changed ->
`NotFoundError`). Search filters use fixed SQL such as `(?1 IS NULL OR id = ?1)` so no SQL text
is ever built from user input; `LIKE` wildcards in user text are escaped.

## Service layer
- `ClientService` - validation, duplicate rules, archive/restore (admin).
- `StaffService` - account management (admin), bootstrap, password changes, safety rules.
- `AuthService` - login/logout; identical failure message for every cause; dummy hash
  verification for unknown usernames to equalise timing.
- `CaseService` - case rules; `recordStatusChange` runs the case UPDATE and the history INSERT
  inside one `Transaction`.
- `CaseWorkflow` - the status state machine as pure functions (exhaustively unit-tested).
- `ReportService` - assembles `SystemReport`, filling zeroes for unused statuses/priorities.
- `PasswordHasher` - the only code that touches libsodium (Argon2id).

## Error handling
All deliberate errors derive from `CsmsError`: `ValidationError`, `NotFoundError`,
`DuplicateError`, `InvalidTransitionError`, `AuthenticationError`, `AuthorizationError`,
`DatabaseError`. `DatabaseError` carries the SQLite result code so repositories can tell a
UNIQUE violation from other failures. The CLI catches `CsmsError` in `Console::runGuarded`
and prints the message. End of input throws `InputClosedError`, which is not an error to
report - the program just exits.

## Case workflow

```
Open -> InProgress -> Resolved -> Closed (final)
  ^         |            |
  +---------+            |
  ^                      |
  +---- reopen() --------+   (explicit operation, reason required)
```
`Open -> InProgress` requires an assigned agent.

## Transaction example (status change)

```
Transaction tx(db);
  UPDATE SupportCases SET status = ?, updated_at = ? WHERE id = ?
  INSERT INTO CaseHistory (...)          -- if this fails (e.g. FK violation) ...
tx.commit();                             -- ... commit is never reached and the destructor
                                         -- rolls the UPDATE back as well
```
The tests prove this with a real foreign-key failure, not a mock.
