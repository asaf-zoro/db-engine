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

## Current architecture (as implemented in code, not the README)

- `include/db_engine.h` — the canonical struct/constant definitions:
  - `DbHeader` (page 0): `magic[4]`, `page_size`, `total_pages`,
    `table_count`, followed by a flexible array of `TableEntry`.
  - `TableEntry`: `table_name[10]`, `schema_page_id`, `page_count`,
    `pages_page_id`. Note this is **indirection-based** — a table's schema
    and its data-page list each live in their own page, referenced by id —
    unlike the README's design (see below).
  - `SchemaPage`: `page_type`, `col_count`, flexible `col_data[]`.
  - `DataPage`: `page_type`, `table_index`, `row_count`, `max_rows`.
  - Page type tags: `PAGE_TYPE_HEADER/SCHEMA/DATA` (0x01/0x02/0x03).
- `src/core/page.c` — `pad_page()`: zero-fills a page at a given page id.
- `src/core/header.c` — `get_db_path()` (appends `.crdb`, allocates the
  path string — caller does not currently free it), and `read_db_header()`
  (reads page 0 into a heap-allocated `DbHeader`, sized to `page_size`).
- `src/commands/init_db.c` — creates a new `.crdb` file with a zeroed page 0
  and writes the initial `DbHeader`.
- `src/commands/init_table.c` — allocates a schema page + a page-list page
  for a new table, writes the schema, and appends a `TableEntry` to page 0.
- `tests/main.c` — **not a real test suite**: it's a manual smoke-test
  `main()` that calls `init_table("MyDB", "MyTab", ...)` directly (it does
  not call `init_db` first, so it depends on a `MyDB.crdb` already existing
  from a prior run). There is no assertion framework in use.

### ⚠️ README.md is not in sync with the code

`README.md` documents an **older/different** on-disk layout: it describes a
`TableEntry` with an inline `page_ids[]` array and a single `page_type`
tag for data pages (0x0001), with no concept of a separate schema page.
The actual code in `db_engine.h` instead splits each table's schema and
page-list into their own pages (`schema_page_id` / `pages_page_id`) and has
three page types. **Neither the README nor the code has been confirmed as
the final intended design** — when working on storage-format changes,
don't treat either as ground truth; ask or check with the user before
assuming which direction to reconcile them in.

## What's implemented vs. missing

Implemented: creating a database file (`init_db`), creating a table with a
fixed-size-column schema (`init_table`).

Not implemented yet: row insert, row read/select, any query logic, page
overflow/growth when a data page fills up (the README's "Insertion
Mechanics" section describes intended behavior for this, but no code
implements it), and any real test suite.

## Known rough edges (found during review, not yet fixed)

- `init_db.c`: `.magic = "CDB"` is a 3-character string literal assigned to
  a `char[4]` — it does *not* match the README's documented 4-byte magic
  `"CDB "` (with a trailing space); the 4th byte ends up as `'\0'`.
- Missing `NULL`/error checks in several places: `fopen` in `init_table.c`
  is not checked; `malloc` results in `init_table.c` (`new_table`) and
  `header.c` (`read_db_header`'s `base_info`/`header`) aren't checked either.
- `init_table()` does `strcpy(new_table->table_name, table_name)` into a
  10-byte buffer with no length validation — a longer name overflows it.
- `get_db_path()`'s allocated path string is never freed by callers
  (`init_db`, `init_table` both leak it).
- `init_table.c` leaks `db_header`/`new_table`/`tables_schema` on its early
  `return -1` path (after the `realloc` failure check).

## Conventions

- C11, snake_case for functions/variables, PascalCase for typedef'd structs.
- Header guards follow `DB_ENGINE_CORE_<NAME>_H` for files under `src/core/`.
- Includes use paths relative to `include/` and `src/` (both added via
  `include_directories` in `CMakeLists.txt`), e.g. `#include "core/header.h"`.
- No external libraries/frameworks (build system, test framework, or
  otherwise) — keep everything on plain C stdlib.
