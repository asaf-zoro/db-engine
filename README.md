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
| Create a table with a fixed-width column schema (`init_table`) | ✅ Implemented |
| Insert a row | ❌ Not implemented |
| Read / select rows | ❌ Not implemented |
| Data-page allocation & overflow when a table's page fills up | ❌ Not implemented |
| Query language / parser | ❌ Not implemented |

There is currently no query engine, no row storage, and no test suite
(`tests/main.c` is a manual smoke test, not an automated one). What exists
so far is the metadata layer: creating a `.crdb` file and registering a
table's schema inside it.

---

## Build

Requires CMake 3.28+ and a C11 compiler. No external dependencies.

```sh
cmake -B cmake-build-debug -G Ninja
cmake --build cmake-build-debug
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
|   [ page_type: 2B ][ col_count: 2B ][ col_data[]: col_count B ]  |
+-----------------------------------------------------------------+
| PAGE N+1: Page-list page for that table (reserved, currently     |
|   zero-filled — see "Known limitations" below)                  |
+-----------------------------------------------------------------+
```

Each table takes **two pages** at creation time: one for its schema, one
reserved for tracking its data pages. Actual row/data pages (a third kind,
described below) are not created yet since insert isn't implemented.

### Page 0: Global DB Header (`DbHeader`)

| Field | Size | Type | Description |
| :--- | :--- | :--- | :--- |
| `magic` | 4 bytes | `char[4]` | File signature, `"CRDB"` (not currently validated on read). |
| `page_size` | 2 bytes | `uint16_t` | Page size in bytes (currently always `4096`). |
| `total_pages` | 4 bytes | `uint32_t` | Total pages allocated in the file so far. |
| `table_count` | 4 bytes | `uint32_t` | Number of tables defined. |
| `tables` | variable | `TableEntry[]` | One entry per table, appended in creation order. |

### Table Descriptor (`TableEntry`)

| Field | Size | Type | Description |
| :--- | :--- | :--- | :--- |
| `table_name` | 10 bytes | `char[10]` | Table name (no length check on write — see limitations). |
| `schema_page_id` | 4 bytes | `uint32_t` | Page id holding this table's `SchemaPage`. |
| `page_count` | 2 bytes | `uint16_t` | Reserved for tracking how many data pages this table owns; not yet updated anywhere. |
| `pages_page_id` | 4 bytes | `uint32_t` | Page id reserved for this table's data-page list; currently allocated but never written to. |

### Schema Page (`SchemaPage`)

| Field | Size | Type | Description |
| :--- | :--- | :--- | :--- |
| `page_type` | 2 bytes | `uint16_t` | `PAGE_TYPE_SCHEMA` (`0x02`). |
| `col_count` | 2 bytes | `uint16_t` | Number of columns. |
| `col_data` | `col_count` bytes | `uint8_t[]` | Byte-width of each column, in order (columns are fixed-size, untyped — a column is just "N bytes wide"). |

### Data Page (`DataPage`) — defined, not yet produced by any code path

| Field | Size | Type | Description |
| :--- | :--- | :--- | :--- |
| `page_type` | 2 bytes | `uint16_t` | `PAGE_TYPE_DATA` (`0x03`). |
| `table_index` | 2 bytes | `uint16_t` | Index of the owning table. |
| `row_count` | 2 bytes | `uint16_t` | Rows currently stored on this page. |
| `max_rows` | 2 bytes | `uint16_t` | Capacity: `(page_size - 8) / row_len`. |

Rows, once insertion exists, are expected to be packed back-to-back
immediately after this 8-byte header.

---

## Known limitations

- **Data-page tracking is a stub**: each table reserves a `pages_page_id`
  page and a `page_count` field, but nothing ever writes to that page or
  increments the count — there's no way yet to actually allocate or find a
  table's data pages. This needs to be designed as part of implementing
  insert.
- **No automated tests**: `tests/main.c` just calls `init_table` once by
  hand; there's no `init_db` call before it (it relies on a `MyDB.crdb`
  already existing from a previous run) and nothing is asserted.
- `init_table`'s declaration in `db_engine.h` (`char *table_name`) doesn't
  match its definition's `char table_name[10]`, which triggers a compiler
  warning (`-Warray-parameter`) — harmless today, but worth aligning.

---

## Project layout

```
include/db_engine.h        Public types & function declarations
src/core/header.c, header.h    DB-path helpers, header read/write
src/core/page.c, page.h        Page-level helpers (zero-fill a page)
src/commands/init_db.c         Create a new .crdb file
src/commands/init_table.c      Register a new table's schema
tests/main.c                   Manual smoke test
```
