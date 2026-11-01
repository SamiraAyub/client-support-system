# Requirements

## Functional

### Clients
- Register clients with an auto-generated unique ID, name, email, phone and registration date.
- Update, search (name / email / phone) and view clients.
- Archive clients instead of deleting them; restore them if archived by mistake.
- Validate input; prevent duplicates (same email, case-insensitive, archived or not).

### Support cases
- Create cases for existing, non-archived clients.
- Fields: ID, title, description, priority (Low/Medium/High), status (Open/In Progress/
  Resolved/Closed), assigned staff member, created/updated timestamps.
- Assign cases to **active support agents** (administrators cannot be assigned cases; closed
  cases cannot be reassigned).
- Change status only along allowed transitions (see below).
- Search and filter by case ID, client, status, priority and assigned agent.
- Reopen a resolved case through an explicit operation that requires a reason.

### Case history
- Every creation and status change (including reopen) is stored: staff member responsible,
  previous status (empty on creation), new status, note, timestamp.
- The case change and its history row are saved in **one transaction**.
- Users can view a case's complete history, oldest first.

### Staff
- Administrator and Support Agent roles.
- Staff are deactivated, never deleted; deactivated staff cannot log in or receive new
  assignments; their historical references remain valid.

### Authentication and authorization
- Login / logout; first-run creation of the initial administrator (no default credentials).
- Passwords are stored only as Argon2id hashes (libsodium); no custom cryptography.
- Permission matrix:

| Operation | Logged out | Agent | Administrator |
|---|:-:|:-:|:-:|
| Clients: register, update, search, view | no | yes | yes |
| Clients: archive, restore | no | no | yes |
| Cases: create, view, search, assign, change status, reopen, history | no | yes | yes |
| Reports; view staff list | no | yes | yes |
| Staff: create, edit, deactivate, reactivate, reset password | no | no | yes |
| Change own password | no | yes | yes |

- Safety rules: nobody can deactivate themselves or change their own role; the last active
  administrator cannot be deactivated or demoted.

### Reports
Total registered clients (with active/archived split); cases by status; cases by priority;
cases assigned to each staff member (total and open work); unassigned cases.

## Status workflow
| From | Allowed ordinary transitions | Notes |
|---|---|---|
| Open | In Progress | needs an assigned agent |
| In Progress | Open, Resolved | |
| Resolved | Closed | back to Open **only** via *reopen* |
| Closed | none | final |

## Input rules
| Field | Rule |
|---|---|
| Client / staff name | 1-100 bytes after trimming; no control characters |
| Email | one `@`; local part 1-64 chars (letters, digits, common symbols, no leading/trailing/double dots); dotted domain of letters/digits/inner hyphens, TLD >= 2 chars; max 254. Unique, case-insensitive |
| Phone | digits plus optional leading `+`, spaces, `-`, `(`, `)`; 7-15 digits |
| Username | 3-32 chars: letters, digits, `.` `_` `-`; starts with a letter or digit; unique, case-insensitive |
| Password | 8-128 characters, not trimmed, no control characters |
| Case title | 1-150 characters |
| Case description | up to 2000 characters (may be empty) |
| Status note | up to 500 characters (optional); reopen reason required |

## Non-functional
- C++17, SQLite3, CMake, GoogleTest; standard library wherever possible.
- Layered architecture: CLI -> services -> repositories -> database.
- Prepared statements for every user-supplied value; foreign keys enforced; transactions for
  multi-step operations.
- Timestamps: UTC ISO 8601 (`YYYY-MM-DDTHH:MM:SSZ`).
- Meaningful error messages; invalid input never crashes the program.
- Data persists across restarts; database files are never committed.

## Out of scope
Web UI / REST API, cloud deployment, microservices, complex role hierarchies, AI features.
