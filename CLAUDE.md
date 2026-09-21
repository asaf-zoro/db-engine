# CerberusDB (db-engine)

A from-scratch, dependency-free embedded database engine written in C11.
This is a **portfolio/exploration project**: the goal is to build something
real and showcase-worthy, but scope and design are still actively evolving.
The repo's history shows several full restarts of the storage design
("RESTART AGAIN", "Redone AGAIN basic db initialization", etc.) — treat the
current code as a snapshot, not a settled contract, and expect the on-disk
format to keep changing.

## Build

```sh
cmake -B cmake-build-debug -G Ninja
cmake --build cmake-build-debug
```

- CMake 3.28+, C11, no external dependencies (plain libc only — this is a
  hard preference, not just a default).
- `cmake-build-debug/`, `.idea/`, `build/`, and `*.crdb` files are gitignored;
  don't commit build artifacts or generated `.crdb` database files.
- Everything currently builds into a single executable target `db-engine`
  (see `CMakeLists.txt`) — there is no separate library target and no
  dedicated test binary yet.
- **The build currently fails at link time**: `insert` is declared in
  `db_engine.h` and `src/commands/insert.c` is in `CMakeLists.txt`, but the
  file has no function body, so `tests/main.c` hits an undefined reference
  to `insert`. This is expected until insert is written.

## Current architecture (as implemented in code, not the README)

- `include/db_engine.h` — the canonical struct/constant definitions:
  - `DbHeader` (page 0): `magic[4]`, `page_size`, `total_pages`,
    `table_count`, followed by a flexible array of `TableEntry`.
  - `TableEntry`: `table_name[10]`, `schema_page_id`, `page_count`,
    `pages_page_id`. Note this is **indirection-based** — a table's schema
    and its data-page list each live in their own page, referenced by id.
  - `ColumnDef`: `{ uint8_t type; uint8_t size; }` (2 bytes) — one per
    column, replacing the earlier untyped "byte width only" columns.
  - Column type tags `COL_TYPE_INT/FLOAT/CHAR` (0x01/0x02/0x03) and their
    allowed sizes: INT = 1/2/4/8 (signed), FLOAT = 4/8, CHAR = any size
    >= 1 (text, zero-padded, not null-terminated).
  - `SchemaPage`: `page_type`, `col_count`, flexible `ColumnDef col_data[]`
    (so the schema takes `4 + 2 * col_count` bytes).
  - `DataPage`: `page_type`, `table_index`, `row_count`, `max_rows`.
  - `Value`: **in-memory only**, never written to disk. A tagged union
    (`type` + `as.i` / `as.f` / `as.chars{data,len}`) that is the typed
    argument of `insert`.
  - Page type tags: `PAGE_TYPE_HEADER/SCHEMA/DATA` (0x01/0x02/0x03).
  - Public functions: `init_db(db_name)`,
    `init_table(db_name, table_name, col_count, const ColumnDef *cols)`,
    `insert(db_name, table_name, const Value *values)` (declared only).
  - Structs are written to disk raw, **not packed**, so alignment padding is
    part of the format: `DbHeader` fixed part is 16 bytes (2 bytes padding
    after `page_size`), `TableEntry` is 24 bytes (padding after
    `table_name` and after `page_count`). Reordering fields or changing
    types changes the file layout.
- `src/core/page.c` — `pad_page()`: zero-fills a page at a given page id.
- `src/core/header.c` — `get_db_path()` (appends `.crdb`, allocates the
  path string — caller does not currently free it), and `read_db_header()`
  (reads page 0 into a heap-allocated `DbHeader`, sized to `page_size`).
- `src/commands/init_db.c` — creates a new `.crdb` file with a zeroed page 0
  and writes the initial `DbHeader`.
- `src/commands/init_table.c` — validates the column schema
  (`valid_columns()`), allocates a schema page + a page-list page for a new
  table, writes the schema, and appends a `TableEntry` to page 0.
  `valid_columns()` rejects: `col_count == 0`, a schema that doesn't fit in
  a page, an unknown column type or a size not allowed for its type, and a
  row so wide that not even one row fits in a data page
  (`sizeof(DataPage) + row_size > page_size`).
- `src/commands/insert.c` — **empty stub** (only includes, no function
  body). `insert()` is declared in `db_engine.h` but not defined.
- `tests/main.c` — **not a real test suite**, but no longer just a smoke
  test: it's a single `main()` using `assert` (plain `assert.h`, no
  framework) that calls `init_db`, `init_table` with a 3-column
  `{INT4, INT1, FLOAT8}` schema, then inserts 315 rows and checks the
  overflow into a second data page (`max_rows == 314`, `page_count == 2`,
  `total_pages == 5`). It is **written ahead of the implementation and
  partly stale**: it still passes a raw `uint8_t row[13]` to `insert`
  instead of a `Value` array, so it doesn't compile cleanly against the
  current signature, and it can't link until `insert` exists.

### README.md and the code

`README.md` was rewritten to describe the current code (indirection-based
`TableEntry`, separate schema / page-list / data pages, `ColumnDef` typed
columns, `Value`), and should match `db_engine.h`. Both still describe a
snapshot, not a settled design — **when changing the storage format, update
both the README and this file together**, and check with the user before
making a format decision (e.g. how `insert` narrows a `Value` into a
column) that the code doesn't already fix.

## What's implemented vs. missing

Implemented: creating a database file (`init_db`), creating a table with a
typed fixed-width column schema (`init_table`, including column validation).

Declared but not implemented: `insert` (signature is
`insert(db_name, table_name, const Value *values)`; the body is empty).

Not implemented yet: row read/select, any query logic, page
overflow/growth when a data page fills up, and any real test suite.

## Known rough edges

Fixed (per user decision — see git history):
- `init_db.c`'s magic bytes are now `"CRDB"` (4 bytes, fits `char[4]`
  exactly with no null terminator stored — this is legal C).
- `init_table()` now rejects table names that don't fit in the 10-byte
  `table_name` buffer (checked via `strlen(table_name) >=
  sizeof(((TableEntry*)0)->table_name)`) instead of overflowing via `strcpy`.
- `fopen`/`malloc`/`realloc` results are now NULL-checked in
  `init_table.c` and `header.c`'s `read_db_header()`, with cleanup on each
  failure path (previously a missing db file, e.g. calling `init_table`
  before `init_db`, would dereference a NULL header and crash).

Still open (deliberately left as-is, or out of scope so far):
- `pages_page_id`/`page_count` on `TableEntry` are allocated/reserved but
  never actually written to or incremented — this is an intentional stub
  until row insert/data-page tracking is designed; don't "fix" it without
  discussing the design first.
- `get_db_path()`'s allocated path string is never freed by callers
  (`init_db`, `init_table` both leak it) — not yet addressed.
- `init_table`'s declaration in `db_engine.h` (`char *table_name`) doesn't
  match its definition's `char table_name[10]` — triggers a harmless
  `-Warray-parameter` warning, not yet aligned.

## Conventions

- C11, snake_case for functions/variables, PascalCase for typedef'd structs.
- Header guards follow `DB_ENGINE_CORE_<NAME>_H` for files under `src/core/`.
- Includes use paths relative to `include/` and `src/` (both added via
  `include_directories` in `CMakeLists.txt`), e.g. `#include "core/header.h"`.
- No external libraries/frameworks (build system, test framework, or
  otherwise) — keep everything on plain C stdlib.
