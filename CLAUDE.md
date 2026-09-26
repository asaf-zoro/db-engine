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
- `CMakeLists.txt` lists the engine sources once in `ENGINE_SOURCES` and
  builds two executables from them (no library target):
  - `db-engine-test` — the test runner, `main()` in `tests/tests.c`.
  - `db-demo` — `demo/demo.c`: creates `DemoDB` with three tables, inserts
    rows, shows rejected inserts, then reads the file back and prints the
    header, each table's schema and its decoded rows. It leaves
    `./DemoDB.crdb` behind for inspection. The demo decodes rows itself
    (there is no select function yet).
- New source files must be added to `ENGINE_SOURCES`.
- Tests and the demo build `.crdb` paths with `get_db_path()`, never a
  hardcoded path or `DB_PATH` macro (user preference); the tests keep the
  result in a static `db_path` set in `main()`.
- Run the tests with `./cmake-build-debug/db-engine-test`
  from a scratch directory: it prints `ok` on success, prints
  `FAIL <file>:<line>: <cond>` and exits 1 on failure, and creates
  `./TestDB.crdb` in the current directory. The user commented out the final
  `remove(db_path)` so the file stays after a successful run, for
  inspection; it holds only the **last** test's database
  (`test_insert_page_full`), since every test starts with `fresh_db()`.
- For memory bugs, build once with `-fsanitize=address,undefined`. The only
  expected report is the known `get_db_path()` path-string leak (run with
  `ASAN_OPTIONS=detect_leaks=0` to skip it).

## Current architecture (as implemented in code, not the README)

- `include/db_engine.h` — the canonical struct/constant definitions:
  - `DbHeader` (page 0): `magic[4]`, `page_size`, `total_pages`,
    `table_count`, followed by a flexible array of `TableEntry`.
  - `TableEntry`: `table_name[10]`, `schema_page_id`, `page_count`,
    `pages_page_id`. Note this is **indirection-based** — a table's schema
    and its data page each live in their own page, referenced by id.
    `pages_page_id` is the table's (single) **data page** — rows go directly
    into it (user decision). `page_count` is unused (always 0).
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
    `insert(db_name, table_name, const Value *values)`.
  - Structs are written to disk raw, **not packed**, so alignment padding is
    part of the format: `DbHeader` fixed part is 16 bytes (2 bytes padding
    after `page_size`), `TableEntry` is 24 bytes (padding after
    `table_name` and after `page_count`). Reordering fields or changing
    types changes the file layout.
- `src/core/page.c` — `pad_page()`: zero-fills a page at a given page id.
- `src/core/header.c` — `get_db_path()` (builds `./<name>.crdb`, allocates
  the path string — caller does not currently free it), and `read_db_header()`
  (reads page 0 into a heap-allocated `DbHeader`, sized to `page_size`).
- `src/core/table.c` — `find_table(table_name, db_header)`: linear scan of
  `db_header->tables[]` comparing names with `strncmp` bounded by
  `sizeof(table_name)` (a 10-char name has no terminator). Returns a
  pointer **into the header buffer** (so edits like `page_count++` land in
  page 0's in-memory copy, to be written back), or `NULL` if not found.
  Caller must not free it and must not use it after freeing the header.
- `src/core/schema.c` — `get_schema(db_path, table, page_size)`: opens
  `db_path` (an already-resolved path, as from `get_db_path`), mallocs a
  whole `page_size` buffer, reads page `table->schema_page_id` into it,
  returns it as a `SchemaPage *` (caller frees), or `NULL` on any failure.
  Reading the full page (not just `4 + 2 * col_count` bytes) is deliberate:
  one read, no need to know `col_count` first.
- `src/commands/init_db.c` — creates a new `.crdb` file with a zeroed page 0
  and writes the initial `DbHeader`.
- `src/commands/init_table.c` — validates the column schema
  (`valid_columns()`), allocates a schema page + a data page (zero-filled,
  no `DataPage` header yet) for a new table, writes the schema, and appends
  a `TableEntry` to page 0.
  `valid_columns()` rejects: `col_count == 0`, a schema that doesn't fit in
  a page, an unknown column type or a size not allowed for its type, and a
  row so wide that not even one row fits in a data page
  (`sizeof(DataPage) + row_size > page_size`).
- `src/commands/insert.c` — `insert()`: resolves the path, reads the
  header, `find_table`, `get_schema`, encodes every value into a row buffer
  (`encode_value()`), then writes the row into the table's data page at
  `pages_page_id * page_size + sizeof(DataPage) + row_count * row_size`.
  If the page has no `DataPage` header yet (`page_type != PAGE_TYPE_DATA`),
  it creates one (`table_index` = entry's index in `tables[]`,
  `max_rows = (page_size - sizeof(DataPage)) / row_size`). The row is
  written before `row_count++` is written. Returns `-1` and writes nothing
  when the page is full (`row_count >= max_rows`) or a value is rejected.
  Narrowing rules (user decision: **reject**, don't truncate): type
  mismatch, INT outside the signed range of its size, CHAR longer than the
  column → rejected; shorter CHAR is zero-padded; FLOAT into a 4-byte
  column becomes `float`. Values are stored native-endian. The caller must
  pass exactly `col_count` values (not checked). Uses a single `goto done`
  cleanup path. It doesn't touch page 0.
- `tests/tests.c` — one test function per area, each starting from a fresh
  database (`fresh_db()`), all run from `main()`. A tiny `CHECK()` macro
  instead of `assert` (so it still fails under `NDEBUG`), no framework.
  Covers: the `init_db` header; `init_table` page ids / counts, its
  rejection cases (file unchanged) and the exact-fit boundary; `find_table`
  (hit, miss, prefix is not a match) and `get_schema` round-trip; `insert`
  row bytes and `DataPage` header, second table's `table_index`, INT/FLOAT/
  CHAR size edges, every rejection case (page stays blank), and the
  page-full case. Add new tests here as a new `test_*` function called
  from `main()`.

### README.md and the code

`README.md` was rewritten to describe the current code (indirection-based
`TableEntry`, separate schema / data pages, `ColumnDef` typed
columns, `Value`), and should match `db_engine.h`. Both still describe a
snapshot, not a settled design — **when changing the storage format, update
both the README and this file together**, and check with the user before
making a format decision (e.g. how `insert` narrows a `Value` into a
column) that the code doesn't already fix.

## What's implemented vs. missing

Implemented: creating a database file (`init_db`), creating a table with a
typed fixed-width column schema (`init_table`, including column validation),
two lookup helpers (`find_table`, `get_schema`), and inserting a typed row
into a table's single data page (`insert`).

Not implemented yet: row read/select, any query logic, page
overflow/growth when a data page fills up (insert returns `-1`), and a real
test framework / separate test target (the tests are `tests/tests.c`).

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
- `get_db_path()` allocated 2 bytes too few (it forgot the `"./"`) and
  passed `255` instead of the buffer size to `snprintf` — a heap overflow.
  It now sizes the buffer as `"./" + name + ".crdb" + '\0'` and passes that
  size.
- `init_table` now `calloc`s the new `TableEntry` (was `malloc`), so the
  bytes after the name's `'\0'` and the struct padding are written to disk
  as zeros instead of leftover heap garbage.
- `schema.h`'s header guard was `DB_ENGINE_CORE_PAGE_H` (clashing with
  `page.h`); now `DB_ENGINE_CORE_SCHEMA_H`.

Still open (deliberately left as-is, or out of scope so far):
- One data page per table: no growth, `page_count` unused. Adding more
  pages means deciding how a table finds its other pages (e.g. a
  `next_page_id`, or a page-id list) — ask the user first.
- `get_db_path()`'s allocated path string is never freed by callers
  (`init_db`, `init_table`, `insert` all leak it) — not yet addressed.
  Freeing it naively is unsafe: on malloc failure `get_db_path` leaves the
  caller's own string in place.
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
