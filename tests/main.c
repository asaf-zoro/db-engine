#include "core/header.h"
#include "db_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DB_NAME "TestDB"
#define DB_PATH "./" DB_NAME FILE_ENDING

// Not assert(): this must still fail in a build with NDEBUG defined.
#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
            remove(DB_PATH);                                                   \
            exit(1);                                                           \
        }                                                                      \
    } while (0)

// Reads the whole schema page back from disk into a page_size buffer.
static SchemaPage *read_schema_page(uint32_t page_id, uint16_t page_size) {
    FILE *fp = fopen(DB_PATH, "rb");
    CHECK(fp != NULL);
    SchemaPage *sp = malloc(page_size);
    CHECK(sp != NULL);
    CHECK(fseek(fp, (long)page_id * page_size, SEEK_SET) == 0);
    CHECK(fread(sp, page_size, 1, fp) == 1);
    fclose(fp);
    return sp;
}

int main(void) {
    ColumnDef cols[3] = {
        {COL_TYPE_INT, 4}, {COL_TYPE_CHAR, 20}, {COL_TYPE_FLOAT, 8}};

    // --- init_db: page 0 holds a valid, empty header ---
    CHECK(init_db(DB_NAME) == 0);
    DbHeader *hdr = read_db_header(DB_PATH);
    CHECK(hdr != NULL);
    CHECK(memcmp(hdr->magic, "CRDB", 4) == 0);
    CHECK(hdr->page_size == DEFAULT_PAGE_SIZE);
    CHECK(hdr->total_pages == 1);
    CHECK(hdr->table_count == 0);
    free(hdr);

    // --- init_table: first table takes pages 1 (schema) and 2 (page list) ---
    CHECK(init_table(DB_NAME, "People", 3, cols) == 0);
    hdr = read_db_header(DB_PATH);
    CHECK(hdr != NULL);
    CHECK(hdr->table_count == 1);
    CHECK(hdr->total_pages == 3);
    CHECK(strcmp(hdr->tables[0].table_name, "People") == 0);
    CHECK(hdr->tables[0].schema_page_id == 1);
    CHECK(hdr->tables[0].pages_page_id == 2);
    CHECK(hdr->tables[0].page_count == 0);

    // The schema on disk round-trips exactly.
    SchemaPage *sp = read_schema_page(hdr->tables[0].schema_page_id, hdr->page_size);
    CHECK(sp->page_type == PAGE_TYPE_SCHEMA);
    CHECK(sp->col_count == 3);
    CHECK(memcmp(sp->col_data, cols, sizeof cols) == 0);
    free(sp);
    free(hdr);

    // --- a second table is appended after the first, nothing is overwritten ---
    ColumnDef one[1] = {{COL_TYPE_INT, 8}};
    CHECK(init_table(DB_NAME, "Second", 1, one) == 0);
    hdr = read_db_header(DB_PATH);
    CHECK(hdr != NULL);
    CHECK(hdr->table_count == 2);
    CHECK(hdr->total_pages == 5);
    CHECK(strcmp(hdr->tables[0].table_name, "People") == 0);
    CHECK(strcmp(hdr->tables[1].table_name, "Second") == 0);
    CHECK(hdr->tables[1].schema_page_id == 3);
    CHECK(hdr->tables[1].pages_page_id == 4);
    sp = read_schema_page(hdr->tables[1].schema_page_id, hdr->page_size);
    CHECK(sp->page_type == PAGE_TYPE_SCHEMA && sp->col_count == 1);
    CHECK(memcmp(sp->col_data, one, sizeof one) == 0);
    free(sp);
    free(hdr);

    // --- rejected tables: each returns -1 ---
    ColumnDef bad_int[1] = {{COL_TYPE_INT, 3}};
    ColumnDef bad_float[1] = {{COL_TYPE_FLOAT, 2}};
    ColumnDef bad_char[1] = {{COL_TYPE_CHAR, 0}};
    ColumnDef bad_type[1] = {{0x7F, 4}};
    ColumnDef too_wide[17]; // 17 * 255 bytes: not even one row fits in a page
    for (int i = 0; i < 17; i++)
        too_wide[i] = (ColumnDef){COL_TYPE_CHAR, 255};

    CHECK(init_table(DB_NAME, "TenChars!!", 1, one) == -1); // 10 chars, no room for '\0'
    CHECK(init_table(DB_NAME, "NoCols", 0, one) == -1);
    CHECK(init_table(DB_NAME, "BadInt", 1, bad_int) == -1);
    CHECK(init_table(DB_NAME, "BadFloat", 1, bad_float) == -1);
    CHECK(init_table(DB_NAME, "BadChar", 1, bad_char) == -1);
    CHECK(init_table(DB_NAME, "BadType", 1, bad_type) == -1);
    CHECK(init_table(DB_NAME, "TooWide", 17, too_wide) == -1);
    CHECK(init_table("NoSuchDB", "Orphan", 1, one) == -1);

    // ...and none of them left anything behind.
    hdr = read_db_header(DB_PATH);
    CHECK(hdr != NULL);
    CHECK(hdr->table_count == 2);
    CHECK(hdr->total_pages == 5);
    free(hdr);

    // --- boundary: a row of exactly page_size - sizeof(DataPage) bytes fits ---
    ColumnDef exact[17];
    for (int i = 0; i < 16; i++)
        exact[i] = (ColumnDef){COL_TYPE_CHAR, 255};
    exact[16] = (ColumnDef){COL_TYPE_CHAR, 8}; // 16*255 + 8 = 4088 = 4096 - 8
    CHECK(init_table(DB_NAME, "Exact", 17, exact) == 0);
    exact[16].size = 9; // one byte over
    CHECK(init_table(DB_NAME, "OneOver", 17, exact) == -1);

    hdr = read_db_header(DB_PATH);
    CHECK(hdr != NULL);
    CHECK(hdr->table_count == 3);
    CHECK(hdr->total_pages == 7);
    free(hdr);

    puts("ok");
    return 0;
}
