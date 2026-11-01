# Entity-Relationship Diagram

```mermaid
erDiagram
    Clients      ||--o{ SupportCases : "raises"
    Staff        |o--o{ SupportCases : "is assigned"
    SupportCases ||--o{ CaseHistory  : "has"
    Staff        ||--o{ CaseHistory  : "records changes"

    Clients {
        INTEGER id PK
        TEXT    name
        TEXT    email "UNIQUE, case-insensitive"
        TEXT    phone
        TEXT    registered_at "ISO 8601 UTC"
        INTEGER is_archived "0 or 1"
    }
    Staff {
        INTEGER id PK
        TEXT    username "UNIQUE, case-insensitive"
        TEXT    full_name
        TEXT    role "Administrator | SupportAgent"
        TEXT    password_hash "encoded hash, never plaintext"
        INTEGER is_active "0 or 1"
        TEXT    created_at
    }
    SupportCases {
        INTEGER id PK
        INTEGER client_id FK
        TEXT    title
        TEXT    description
        TEXT    priority "Low | Medium | High"
        TEXT    status "Open | InProgress | Resolved | Closed"
        INTEGER assigned_staff_id FK "nullable"
        TEXT    created_at
        TEXT    updated_at
    }
    CaseHistory {
        INTEGER id PK
        INTEGER case_id FK
        INTEGER changed_by_staff_id FK
        TEXT    previous_status "NULL on creation"
        TEXT    new_status
        TEXT    note
        TEXT    changed_at
    }
```

## Design rules enforced by the schema
- Every foreign key is `ON DELETE RESTRICT`: records referenced by cases or history cannot be
  deleted. Clients are archived and staff deactivated instead, preserving history.
- Enum-like columns carry `CHECK` constraints, so invalid values cannot be stored.
- Email and username are unique regardless of letter case.
- Indexes cover the filters the application uses: case client, status, priority, assignee,
  and history by case + time.
  and history by case + time.
- Timestamps are `YYYY-MM-DDTHH:MM:SSZ` text, which sorts chronologically.
