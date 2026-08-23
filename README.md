# ___CerberusDB___

**CerberusDB** is a simple, lightweight dbms to use freely. Completely open sourced!

The database file is structured as such:

| Section / Field    | Field Size | Data Type | Description |
|:-------------------| :--- | :--- | :--- |
| **`table_count`**  | `4 bytes` | `uint32` | Total number of tables defined in the file |
| **`tables_start`** | `N × 4 bytes` | `uint32[]` | List of absolute byte offsets pointing to table starting positions |

### Table Offset Table Details
- **`table1_start`**: `<byte_offset_1> - uint32_t` — Byte location where Table 1 definition/data begins.
- **`table2_start`**: `<byte_offset_2> - uint32_t` — Byte location where Table 2 definition/data begins.
- **`...`**

---

## 2. Table Header Layout

| Field Name | Size | Data Type | Description |
| :--- | :--- | :--- | :--- |
| **`table_name`** | `10 bytes` | `char[10]` | Null-terminated table name string (up to 9 characters + `\0`) |
| **`row_count`** | `4 bytes` | `uint32` | Total number of rows stored in this table |
| **`row_len`** | `2 bytes` | `uint16` | Fixed length of each row payload in bytes |

---

## 3. Data Region Layout

```
+-------------------------------------------------------------+
|                        HEADER AREA                          |
|  [ table_count ] [ table1_start ] [ table2_start ] ...      |
+-------------------------------------------------------------+
|                       TABLE DATA AREA                       |
|  +-------------------------------------------------------+  |
|  | Table 1 Header: [ row_count ] [ row_len ]             |  |
|  |-------------------------------------------------------|  |
|  | Payload: [ Row 0 ] [ Row 1 ] ... [ Row N ]            |  |
|  +-------------------------------------------------------+  |
|  +-------------------------------------------------------+  |
|  | Table 2 Header: [ row_count ] [ row_len ]             |  |
|  |-------------------------------------------------------|  |
|  | Payload: [ Row 0 ] [ Row 1 ] ... [ Row N ]            |  |
|  +-------------------------------------------------------+  |
+-------------------------------------------------------------+
```