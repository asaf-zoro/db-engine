# CerberusDB

**CerberusDB** is a lightweight, open-source DBMS utilizing a **Paged Architecture** (4096-byte pages) for robust raw binary file storage.

---

## 1. Database Architecture Overview

The database file is divided into uniform **4KB (4096 bytes)** pages. Space is allocated dynamically in page units, preventing data corruption and eliminating the need to shift file bytes when tables grow.

```
+-----------------------------------------------------------------+
| PAGE 0: GLOBAL DB HEADER (4096 Bytes)                           |
|   [ magic_bytes: 4B ] [ page_size: 2B ] [ total_pages: 4B ]     |
|   [ table_count: 4B ]                                           |
|   Table Definitions:                                            |
|     - Table 1 (Name: "users", Pages: [ 1, 3 ])                  |
|     - Table 2 (Name: "orders", Pages: [ 2 ])                    |
+-----------------------------------------------------------------+
| PAGE 1: Table 1 Data Page (4096 Bytes)                          |
|   [ Page Header ] [ Row 0 ] [ Row 1 ] ... [ Row N ]             |
+-----------------------------------------------------------------+
| PAGE 2: Table 2 Data Page (4096 Bytes)                          |
|   [ Page Header ] [ Row 0 ] [ Row 1 ] ... [ Row M ]             |
+-----------------------------------------------------------------+
| PAGE 3: Table 1 Data Page (Overflow / Dynamic Allocation)       |
|   [ Page Header ] [ Row N+1 ] [ Row N+2 ] ...                   |
+-----------------------------------------------------------------+
```

---

## 2. Page 0: Global DB Header Layout

Page `0` is located at byte offset `0x0000` and contains global database metadata and table allocation maps.

### Global Header Structure

| Field | Size | Data Type | Description |
| :--- | :--- | :--- | :--- |
| **`magic`** | `4 bytes` | `char[4]` | File signature identifier (`"CDB "`). |
| **`page_size`** | `2 bytes` | `uint16_t` | Fixed size of each page in bytes (Default: `4096`). |
| **`total_pages`** | `4 bytes` | `uint32_t` | Total number of allocated pages in the database file. |
| **`table_count`** | `4 bytes` | `uint32_t` | Number of defined tables in the database. |
| **`table_entries`**| Variable | `TableEntry[]` | Array of table definition entries. |

### TableEntry Descriptor Structure

Each table defined in Page 0 uses a descriptor to track its schema and page chain:

| Field | Size | Data Type | Description |
| :--- | :--- | :--- | :--- |
| **`table_name`** | `10 bytes` | `char[10]` | Null-terminated table name string. |
| **`row_len`** | `2 bytes` | `uint16_t` | Fixed payload length of an individual row in bytes. |
| **`page_count`** | `2 bytes` | `uint16_t` | Total number of pages allocated to this table. |
| **`page_ids`** | `page_count 脳 4B` | `uint32_t[]` | Array of absolute Page IDs assigned to this table. |

---

## 3. Data Page Structure (Pages 1+)

All pages after Page 0 store row data for assigned tables.

### Data Page Header Layout (8 bytes total)

Every data page starts with an 8-byte header tracking its state:

| Field | Size | Data Type | Description |
| :--- | :--- | :--- | :--- |
| **`page_type`** | `2 bytes` | `uint16_t` | Flag identifying page category (`0x0001` = Data Page). |
| **`table_id`** | `2 bytes` | `uint16_t` | Index of the table that owns this page. |
| **`row_count`** | `2 bytes` | `uint16_t` | Number of rows currently stored in this specific page. |
| **`max_rows`** | `2 bytes` | `uint16_t` | Maximum rows this page can hold ($(	ext{page\_size} - 8) / 	ext{row\_len}$). |

### Data Payload Layout

Rows are packed sequentially immediately after the 8-byte data page header:

$$	ext{Data Region Offset} = (	ext{Page ID} 	imes 4096) + 8$$

---

## 4. Insertion Mechanics

When inserting a row into a target table:

1. Locate the active page ID from the table's `page_ids` list in Page 0.
2. Check if `row_count < max_rows` for that page.
3. **If space is available:** Append the row data at offset $(	ext{Data Region Offset} + (	ext{row\_count} 	imes 	ext{row\_len}))$ and increment `row_count`.
4. **If full:** Allocate a new page at file offset $(	ext{total\_pages} 	imes 4096)$, append the new Page ID to the table's `page_ids` list in Page 0, update `total_pages`, and write the row to the new page.