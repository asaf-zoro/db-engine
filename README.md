# CerberusDB

**CerberusDB** is a lightweight, dependency-free embedded database engine
written in C11, using a fixed-size **paged file format** (4096-byte pages)
for raw binary storage. It's a from-scratch systems-programming project and
is still early / actively evolving — the format below reflects what the
code actually does today, not a finished spec.

---

## Status

| Capability | Status |
| :--- | :--- |
| Create a database file (`init_db`) | ✅ Implemented |
| Create a table with a typed, fixed-width column schema (`init_table`) | ✅ Implemented |
| Insert a row (`insert`) | ✅ Implemented (one data page per table) |
| Read / select rows | ❌ Not implemented |
| Data-page allocation & overflow when a table's page fills up | ❌ Not implemented (insert fails when the page is full) |
| Query language / parser | ❌ Not implemented |

There is currently no query engine. What exists so far is creating a `.crdb`
file, registering a table's typed schema inside it, and inserting rows into
the table's single data page. `tests/tests.c` tests `init_db`, `init_table`,
`find_table`, `get_schema` and `insert` by reading the `.crdb` file back; run
the built `db-engine-test` binary and it prints `ok` on success.

---

## Build

Requires CMake 3.28+ and a C11 compiler. No external dependencies.

```sh
cmake -B cmake-build-debug -G Ninja
cmake --build cmake-build-debug

./cmake-build-debug/db-engine-test   # run the tests (prints "ok")
./cmake-build-debug/db-demo     # create DemoDB.crdb, insert rows, print its contents
```

---

## On-Disk Format

A database is a single `<name>.crdb` file, divided into fixed **4096-byte
pages**, numbered from 0.

```
+-----------------------------------------------------------------+
| PAGE 0: Global DB Header                                         |
|   [ magic: 4B ][ page_size: 2B ][ total_pages: 4B ]              |
|   [ table_count: 4B ][ TableEntry[] ... ]                        |
+-----------------------------------------------------------------+
| PAGE N: Schema Page for a table                                  |
|   [ page_type: 2B ][ col_count: 2B ]                             |
|   [ ColumnDef[]: col_count x (type: 1B, size: 1B) ]              |
+-----------------------------------------------------------------+
| PAGE N+1: Data page for that table (pages_page_id)               |
|   [ DataPage header: 8B ][ row 0 ][ row 1 ] ...                  |
+-----------------------------------------------------------------+
```

Each table takes **two pages** at creation time: one for its schema, and one
data page that `insert` writes rows into. `init_table` zero-fills the data
page; the first `insert` writes its `DataPage` header.

> **Structs are written to disk as-is (not packed).** The compiler's
> alignment padding is part of the file format, so the offsets below include
> it. Changing a struct's field order or types changes the on-disk layout.

### Page 0: Global DB Header (`DbHeader`)

Fixed part is **16 bytes**, followed by the `TableEntry` array.

| Field | Offset | Size | Type | Description |
| :--- | :--- | :--- | :--- | :--- |
| `magic` | 0 | 4 bytes | `char[4]` | File signature, `"CRDB"` (not currently validated on read). |
| `page_size` | 4 | 2 bytes | `uint16_t` | Page size in bytes (currently always `4096`). |
| *(padding)* | 6 | 2 bytes | | Alignment padding before `total_pages`. |
| `total_pages` | 8 | 4 bytes | `uint32_t` | Total pages allocated in the file so far. |
| `table_count` | 12 | 4 bytes | `uint32_t` | Number of tables defined. |
| `tables` | 16 | variable | `TableEntry[]` | One entry per table, appended in creation order. |

### Table Descriptor (`TableEntry`)

**24 bytes** per entry.

| Field | Offset | Size | Type | Description |
| :--- | :--- | :--- | :--- | :--- |
| `table_name` | 0 | 10 bytes | `char[10]` | Table name. Must be at most 9 characters (plus the terminator); longer names are rejected by `init_table`. |
| *(padding)* | 10 | 2 bytes | | Alignment padding before `schema_page_id`. |
| `schema_page_id` | 12 | 4 bytes | `uint32_t` | Page id holding this table's `SchemaPage`. |
| `page_count` | 16 | 2 bytes | `uint16_t` | Reserved; not updated by anything yet (always 0). |
| *(padding)* | 18 | 2 bytes | | Alignment padding before `pages_page_id`. |
| `pages_page_id` | 20 | 4 bytes | `uint32_t` | Page id of this table's data page, where `insert` writes rows. |

### Schema Page (`SchemaPage`)

The fixed part is **4 bytes**; the whole schema is `4 + 2 * col_count` bytes
and must fit in one page.

| Field | Offset | Size | Type | Description |
| :--- | :--- | :--- | :--- | :--- |
| `page_type` | 0 | 2 bytes | `uint16_t` | `PAGE_TYPE_SCHEMA` (`0x02`). |
| `col_count` | 2 | 2 bytes | `uint16_t` | Number of columns (at least 1). |
| `col_data` | 4 | `2 * col_count` bytes | `ColumnDef[]` | One `ColumnDef` per column, in order. |

### Column types (`ColumnDef`, `COL_TYPE_*`)

Each column has a type and a fixed width in bytes. A `ColumnDef` is 2 bytes:

| Field | Size | Type | Description |
| :--- | :--- | :--- | :--- |
| `type` | 1 byte | `uint8_t` | One of the `COL_TYPE_*` tags below. |
| `size` | 1 byte | `uint8_t` | Column width in bytes; the allowed values depend on `type`. |

| Type | Tag | Allowed `size` | Meaning |
| :--- | :--- | :--- | :--- |
| `COL_TYPE_INT` | `0x01` | 1, 2, 4 or 8 | Signed integer. |
| `COL_TYPE_FLOAT` | `0x02` | 4 or 8 | Floating point (`float` / `double`). |
| `COL_TYPE_CHAR` | `0x03` | 1 or more | Text, zero-padded to `size`. Not null-terminated. |

`init_table` rejects a table (returns `-1`) if any of these fail:

- `col_count` is `0`, or the schema doesn't fit in one page.
- Any column has an unknown `type` or a `size` outside the table above.
- Not even one row fits in a data page (`sizeof(DataPage) + row_size > page_size`,
  where `row_size` is the sum of the column sizes).

### Row values (`Value`) — in-memory only

`Value` is the typed argument that `insert` takes; it is **not** stored on
disk. Its `type` (a `COL_TYPE_*` tag) selects which union member is valid:

| `type` | Member | C type |
| :--- | :--- | :--- |
| `COL_TYPE_INT` | `as.i` | `int64_t` |
| `COL_TYPE_FLOAT` | `as.f` | `double` |
| `COL_TYPE_CHAR` | `as.chars` | `{ const char *data; size_t len; }` (not null-terminated) |

`insert(db_name, table_name, const Value *values)` takes one `Value` per
column, in schema order (the caller must pass exactly `col_count` values).
It returns `0` on success and `-1` without writing anything if a value
doesn't fit its column:

- `Value.type` differs from the column type → rejected.
- INT outside the signed range of the column size (e.g. 128 into a 1-byte
  column) → rejected.
- CHAR longer than the column → rejected. Shorter is zero-padded.
- FLOAT into a 4-byte column is converted to `float` (loses precision).

Values are copied in native byte order, like the structs.

### Data Page (`DataPage`)

| Field | Size | Type | Description |
| :--- | :--- | :--- | :--- |
| `page_type` | 2 bytes | `uint16_t` | `PAGE_TYPE_DATA` (`0x03`). |
| `table_index` | 2 bytes | `uint16_t` | Index of the owning table. |
| `row_count` | 2 bytes | `uint16_t` | Rows currently stored on this page. |
| `max_rows` | 2 bytes | `uint16_t` | Capacity: `(page_size - 8) / row_len`, where `row_len` is the sum of the column sizes. |

Rows are packed back-to-back immediately after this 8-byte header, with no
padding between columns: row `n` starts at byte `8 + n * row_len` of the
page.

---

## Known limitations

- **One data page per table**: once `row_count == max_rows`, `insert`
  returns `-1`. There is no page growth yet, and `page_count` is unused.
- **Minimal tests**: `tests/tests.c` is a set of test functions run from one
  `main()` with a small `CHECK` macro, not a test framework, and it lives in
  the same executable as the rest of the code.
- **Path string leak**: callers of `get_db_path()` never free the path it
  allocates.
- `init_table`'s declaration in `db_engine.h` (`char *table_name`) doesn't
  match its definition's `char table_name[10]`, which triggers a compiler
  warning (`-Warray-parameter`) — harmless today, but worth aligning.

---

## Project layout

```
include/db_engine.h        Public types (ColumnDef, Value, page structs) & declarations
src/core/header.c, header.h    DB-path helpers, header read
src/core/page.c, page.h        Page-level helpers (zero-fill a page)
src/commands/init_db.c         Create a new .crdb file
src/commands/init_table.c      Validate a column schema and register a new table
src/core/table.c, table.h      Find a table's entry in the header
src/core/schema.c, schema.h    Read a table's schema page
src/commands/insert.c          Insert a row into a table's data page
tests/tests.c                  Tests for everything above
demo/demo.c                    Demo: create a db and tables, insert, print the file
```
