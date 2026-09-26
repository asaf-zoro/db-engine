#include "core/header.h"
#include "core/schema.h"
#include "core/table.h"
#include "db_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DB_NAME "TestDB"

// Path of the test database, built by get_db_path() in main().
static char *db_path;

// Not assert(): this must still fail in a build with NDEBUG defined.
#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
            remove(db_path);                                                   \
            exit(1);                                                           \
        }                                                                      \
    } while (0)

static const ColumnDef people[3] = {
    {COL_TYPE_INT, 4}, {COL_TYPE_CHAR, 20}, {COL_TYPE_FLOAT, 8}};
static const ColumnDef one_int[1] = {{COL_TYPE_INT, 8}};

// Every test starts from a new, empty database (init_db overwrites the file).
static void fresh_db(void) { CHECK(init_db(DB_NAME) == 0); }

// Reads a whole page back from disk. Caller frees.
static uint8_t *read_page(uint32_t page_id) {
    FILE *fp = fopen(db_path, "rb");
    CHECK(fp != NULL);
    uint8_t *page = malloc(DEFAULT_PAGE_SIZE);
    CHECK(page != NULL);
    CHECK(fseek(fp, (long)page_id * DEFAULT_PAGE_SIZE, SEEK_SET) == 0);
    CHECK(fread(page, DEFAULT_PAGE_SIZE, 1, fp) == 1);
    fclose(fp);
    return page;
}

static Value int_val(int64_t i) { return (Value){COL_TYPE_INT, {.i = i}}; }
static Value float_val(double f) { return (Value){COL_TYPE_FLOAT, {.f = f}}; }
static Value char_val(const char *s) {
    return (Value){COL_TYPE_CHAR, {.chars = {s, strlen(s)}}};
}

// ---- init_db ----

static void test_init_db(void) {
    fresh_db();
    DbHeader *hdr = read_db_header(db_path);
    CHECK(hdr != NULL);
    CHECK(memcmp(hdr->magic, "CRDB", 4) == 0);
    CHECK(hdr->page_size == DEFAULT_PAGE_SIZE);
    CHECK(hdr->total_pages == 1);
    CHECK(hdr->table_count == 0);
    free(hdr);
}

// ---- init_table ----

static void test_init_table(void) {
    fresh_db();

    // First table takes pages 1 (schema) and 2 (data).
    CHECK(init_table(DB_NAME, "People", 3, people) == 0);
    DbHeader *hdr = read_db_header(db_path);
    CHECK(hdr != NULL);
    CHECK(hdr->table_count == 1);
    CHECK(hdr->total_pages == 3);
    CHECK(strcmp(hdr->tables[0].table_name, "People") == 0);
    CHECK(hdr->tables[0].schema_page_id == 1);
    CHECK(hdr->tables[0].pages_page_id == 2);
    CHECK(hdr->tables[0].page_count == 0);
    free(hdr);

    // A second table is appended after the first, nothing is overwritten.
    CHECK(init_table(DB_NAME, "Second", 1, one_int) == 0);
    hdr = read_db_header(db_path);
    CHECK(hdr != NULL);
    CHECK(hdr->table_count == 2);
    CHECK(hdr->total_pages == 5);
    CHECK(strcmp(hdr->tables[0].table_name, "People") == 0);
    CHECK(strcmp(hdr->tables[1].table_name, "Second") == 0);
    CHECK(hdr->tables[1].schema_page_id == 3);
    CHECK(hdr->tables[1].pages_page_id == 4);
    free(hdr);
}

static void test_init_table_rejects(void) {
    fresh_db();
    CHECK(init_table(DB_NAME, "People", 3, people) == 0);

    ColumnDef bad_int[1] = {{COL_TYPE_INT, 3}};
    ColumnDef bad_float[1] = {{COL_TYPE_FLOAT, 2}};
    ColumnDef bad_char[1] = {{COL_TYPE_CHAR, 0}};
    ColumnDef bad_type[1] = {{0x7F, 4}};
    ColumnDef too_wide[17]; // 17 * 255 bytes: not even one row fits in a page
    for (int i = 0; i < 17; i++)
        too_wide[i] = (ColumnDef){COL_TYPE_CHAR, 255};

    CHECK(init_table(DB_NAME, "TenChars!!", 1, one_int) == -1); // no room for '\0'
    CHECK(init_table(DB_NAME, "NoCols", 0, one_int) == -1);
    CHECK(init_table(DB_NAME, "BadInt", 1, bad_int) == -1);
    CHECK(init_table(DB_NAME, "BadFloat", 1, bad_float) == -1);
    CHECK(init_table(DB_NAME, "BadChar", 1, bad_char) == -1);
    CHECK(init_table(DB_NAME, "BadType", 1, bad_type) == -1);
    CHECK(init_table(DB_NAME, "TooWide", 17, too_wide) == -1);
    CHECK(init_table("NoSuchDB", "Orphan", 1, one_int) == -1);

    // ...and none of them left anything behind.
    DbHeader *hdr = read_db_header(db_path);
    CHECK(hdr != NULL);
    CHECK(hdr->table_count == 1);
    CHECK(hdr->total_pages == 3);
    free(hdr);
}

static void test_init_table_boundary(void) {
    fresh_db();

    // A row of exactly page_size - sizeof(DataPage) bytes fits.
    ColumnDef exact[17];
    for (int i = 0; i < 16; i++)
        exact[i] = (ColumnDef){COL_TYPE_CHAR, 255};
    exact[16] = (ColumnDef){COL_TYPE_CHAR, 8}; // 16*255 + 8 = 4088 = 4096 - 8
    CHECK(init_table(DB_NAME, "Exact", 17, exact) == 0);
    exact[16].size = 9; // one byte over
    CHECK(init_table(DB_NAME, "OneOver", 17, exact) == -1);

    DbHeader *hdr = read_db_header(db_path);
    CHECK(hdr != NULL);
    CHECK(hdr->table_count == 1);
    CHECK(hdr->total_pages == 3);
    free(hdr);
}

// ---- find_table / get_schema ----

static void test_find_table_and_get_schema(void) {
    fresh_db();
    CHECK(init_table(DB_NAME, "People", 3, people) == 0);
    CHECK(init_table(DB_NAME, "Second", 1, one_int) == 0);

    DbHeader *hdr = read_db_header(db_path);
    CHECK(hdr != NULL);
    CHECK(find_table("People", hdr) == &hdr->tables[0]);
    CHECK(find_table("Second", hdr) == &hdr->tables[1]);
    CHECK(find_table("Nope", hdr) == NULL);
    CHECK(find_table("Peop", hdr) == NULL); // prefix is not a match

    SchemaPage *sp = get_schema(db_path, &hdr->tables[0], hdr->page_size);
    CHECK(sp != NULL);
    CHECK(sp->page_type == PAGE_TYPE_SCHEMA);
    CHECK(sp->col_count == 3);
    CHECK(memcmp(sp->col_data, people, sizeof people) == 0);
    free(sp);

    sp = get_schema(db_path, &hdr->tables[1], hdr->page_size);
    CHECK(sp != NULL);
    CHECK(sp->col_count == 1);
    CHECK(memcmp(sp->col_data, one_int, sizeof one_int) == 0);
    free(sp);

    char *missing = "NoSuchDB";
    get_db_path(&missing);
    CHECK(get_schema(missing, &hdr->tables[0], hdr->page_size) == NULL);
    free(missing);
    free(hdr);
}

// ---- insert ----

static void test_insert(void) {
    fresh_db();
    CHECK(init_table(DB_NAME, "People", 3, people) == 0);

    Value row1[3] = {int_val(42), char_val("Ada"), float_val(1.5)};
    Value row2[3] = {int_val(-7), char_val("Grace"), float_val(-2.25)};
    CHECK(insert(DB_NAME, "People", row1) == 0);
    CHECK(insert(DB_NAME, "People", row2) == 0);

    // Rows go into the reserved page 2, after the DataPage header.
    uint8_t *page = read_page(2);
    DataPage dp;
    memcpy(&dp, page, sizeof dp);
    CHECK(dp.page_type == PAGE_TYPE_DATA);
    CHECK(dp.table_index == 0);
    CHECK(dp.row_count == 2);
    CHECK(dp.max_rows == (DEFAULT_PAGE_SIZE - sizeof(DataPage)) / 32); // row = 4+20+8

    int32_t i;
    double f;
    char name[20] = {0};
    uint8_t *r = page + sizeof(DataPage);
    memcpy(&i, r, 4);
    memcpy(&f, r + 24, 8);
    CHECK(i == 42 && f == 1.5);
    memcpy(name, "Ada", 3);
    CHECK(memcmp(r + 4, name, 20) == 0); // zero-padded

    r += 32;
    memcpy(&i, r, 4);
    memcpy(&f, r + 24, 8);
    CHECK(i == -7 && f == -2.25);
    memset(name, 0, sizeof name);
    memcpy(name, "Grace", 5);
    CHECK(memcmp(r + 4, name, 20) == 0);
    free(page);

    // Insert doesn't allocate pages or touch the header.
    DbHeader *hdr = read_db_header(db_path);
    CHECK(hdr != NULL);
    CHECK(hdr->total_pages == 3);
    CHECK(hdr->table_count == 1);
    free(hdr);
}

static void test_insert_second_table(void) {
    fresh_db();
    CHECK(init_table(DB_NAME, "People", 3, people) == 0);
    CHECK(init_table(DB_NAME, "Second", 1, one_int) == 0);

    Value v[1] = {int_val(INT64_MIN)};
    CHECK(insert(DB_NAME, "Second", v) == 0);

    uint8_t *page = read_page(4);
    DataPage dp;
    memcpy(&dp, page, sizeof dp);
    CHECK(dp.table_index == 1 && dp.row_count == 1);
    int64_t got;
    memcpy(&got, page + sizeof(DataPage), 8);
    CHECK(got == INT64_MIN);
    free(page);

    // People's page is untouched.
    page = read_page(2);
    memcpy(&dp, page, sizeof dp);
    CHECK(dp.page_type == 0 && dp.row_count == 0);
    free(page);
}

static void test_insert_sizes(void) {
    fresh_db();
    ColumnDef cols[4] = {
        {COL_TYPE_INT, 1}, {COL_TYPE_INT, 2}, {COL_TYPE_FLOAT, 4}, {COL_TYPE_CHAR, 3}};
    CHECK(init_table(DB_NAME, "Sizes", 4, cols) == 0);

    // Edges of each range are accepted; a CHAR exactly the column width is too.
    Value lo[4] = {int_val(-128), int_val(-32768), float_val(0.5), char_val("abc")};
    Value hi[4] = {int_val(127), int_val(32767), float_val(3.0), char_val("")};
    CHECK(insert(DB_NAME, "Sizes", lo) == 0);
    CHECK(insert(DB_NAME, "Sizes", hi) == 0);

    uint8_t *page = read_page(2);
    uint8_t *r = page + sizeof(DataPage);
    int8_t a;
    int16_t b;
    float c;
    memcpy(&a, r, 1);
    memcpy(&b, r + 1, 2);
    memcpy(&c, r + 3, 4);
    CHECK(a == -128 && b == -32768 && c == 0.5f);
    CHECK(memcmp(r + 7, "abc", 3) == 0);
    r += 10;
    memcpy(&a, r, 1);
    memcpy(&b, r + 1, 2);
    memcpy(&c, r + 3, 4);
    CHECK(a == 127 && b == 32767 && c == 3.0f);
    CHECK(memcmp(r + 7, "\0\0\0", 3) == 0);
    free(page);
}

static void test_insert_rejects(void) {
    fresh_db();
    ColumnDef cols[3] = {{COL_TYPE_INT, 1}, {COL_TYPE_CHAR, 4}, {COL_TYPE_FLOAT, 8}};
    CHECK(init_table(DB_NAME, "T", 3, cols) == 0);

    Value ok[3] = {int_val(1), char_val("abcd"), float_val(1.0)};
    Value too_big[3] = {int_val(128), char_val("a"), float_val(1.0)};
    Value too_small[3] = {int_val(-129), char_val("a"), float_val(1.0)};
    Value too_long[3] = {int_val(1), char_val("abcde"), float_val(1.0)};
    Value wrong_type[3] = {int_val(1), char_val("a"), int_val(1)};

    CHECK(insert(DB_NAME, "T", too_big) == -1);
    CHECK(insert(DB_NAME, "T", too_small) == -1);
    CHECK(insert(DB_NAME, "T", too_long) == -1);
    CHECK(insert(DB_NAME, "T", wrong_type) == -1);
    CHECK(insert(DB_NAME, "Nope", ok) == -1);
    CHECK(insert("NoSuchDB", "T", ok) == -1);

    // Nothing was written: the data page is still blank.
    uint8_t *page = read_page(2);
    DataPage dp;
    memcpy(&dp, page, sizeof dp);
    CHECK(dp.page_type == 0 && dp.row_count == 0);
    free(page);

    CHECK(insert(DB_NAME, "T", ok) == 0);
}

static void test_insert_page_full(void) {
    fresh_db();

    // 16 * 255 = 4080 bytes per row: exactly one row fits in a data page.
    ColumnDef wide[16];
    Value row[16];
    for (int i = 0; i < 16; i++) {
        wide[i] = (ColumnDef){COL_TYPE_CHAR, 255};
        row[i] = char_val("x");
    }
    CHECK(init_table(DB_NAME, "Wide", 16, wide) == 0);
    CHECK(insert(DB_NAME, "Wide", row) == 0);
    CHECK(insert(DB_NAME, "Wide", row) == -1);

    uint8_t *page = read_page(2);
    DataPage dp;
    memcpy(&dp, page, sizeof dp);
    CHECK(dp.max_rows == 1 && dp.row_count == 1);
    free(page);
}

int main(void) {
    db_path = DB_NAME;
    get_db_path(&db_path);

    test_init_db();
    test_init_table();
    test_init_table_rejects();
    test_init_table_boundary();
    test_find_table_and_get_schema();
    test_insert();
    test_insert_second_table();
    test_insert_sizes();
    test_insert_rejects();
    test_insert_page_full();

//   remove(db_path);
    free(db_path);
    puts("ok");
    return 0;
}
