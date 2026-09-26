#include "core/header.h"
#include "core/schema.h"
#include "core/table.h"
#include "db_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DB_NAME "DemoDB"

static Value int_val(int64_t i) { return (Value){COL_TYPE_INT, {.i = i}}; }
static Value float_val(double f) { return (Value){COL_TYPE_FLOAT, {.f = f}}; }
static Value char_val(const char *s) {
    return (Value){COL_TYPE_CHAR, {.chars = {s, strlen(s)}}};
}

static void print_column_type(ColumnDef col) {
    const char *name = col.type == COL_TYPE_INT     ? "INT"
                       : col.type == COL_TYPE_FLOAT ? "FLOAT"
                                                    : "CHAR";
    printf("%s(%u)", name, col.size);
}

static void print_value(const uint8_t *src, ColumnDef col) {
    switch (col.type) {
    case COL_TYPE_INT: {
        int64_t x;
        switch (col.size) {
        case 1: {
            int8_t n;
            memcpy(&n, src, 1);
            x = n;
            break;
        }
        case 2: {
            int16_t n;
            memcpy(&n, src, 2);
            x = n;
            break;
        }
        case 4: {
            int32_t n;
            memcpy(&n, src, 4);
            x = n;
            break;
        }
        default:
            memcpy(&x, src, 8);
            break;
        }
        printf("%lld", (long long)x);
        break;
    }
    case COL_TYPE_FLOAT:
        if (col.size == 4) {
            float f;
            memcpy(&f, src, 4);
            printf("%g", f);
        } else {
            double f;
            memcpy(&f, src, 8);
            printf("%g", f);
        }
        break;
    case COL_TYPE_CHAR: {
        int len = 0; // stop at the zero padding
        while (len < col.size && src[len] != '\0')
            len++;
        printf("\"%.*s\"", len, (const char *)src);
        break;
    }
    }
}

static void show_table(const char *db_path, DbHeader *hdr,
                       const char *table_name) {
    TableEntry *table = find_table(table_name, hdr);
    SchemaPage *schema =
        table ? get_schema(db_path, table, hdr->page_size) : NULL;
    uint8_t *page = malloc(hdr->page_size);
    FILE *fp = fopen(db_path, "rb");
    if (schema == NULL || page == NULL || fp == NULL ||
        fseek(fp, (long)table->pages_page_id * hdr->page_size, SEEK_SET) != 0 ||
        fread(page, hdr->page_size, 1, fp) != 1) {
        printf("  could not read table %s\n", table_name);
        goto done;
    }

    printf("\n  Table \"%s\"  (schema page %u, data page %u)\n  columns: ",
           table_name, table->schema_page_id, table->pages_page_id);
    uint32_t row_size = 0;
    for (uint16_t c = 0; c < schema->col_count; c++) {
        print_column_type(schema->col_data[c]);
        printf(c + 1 < schema->col_count ? " | " : "\n");
        row_size += schema->col_data[c].size;
    }

    DataPage dp;
    memcpy(&dp, page, sizeof dp);
    if (dp.page_type != PAGE_TYPE_DATA) {
        printf("  (no rows yet)\n");
        goto done;
    }
    printf("  rows: %u of %u\n", dp.row_count, dp.max_rows);

    for (uint16_t r = 0; r < dp.row_count; r++) {
        const uint8_t *row = page + sizeof(DataPage) + (size_t)r * row_size;
        printf("    [%u] ", r);
        for (uint16_t c = 0; c < schema->col_count; c++) {
            print_value(row, schema->col_data[c]);
            printf(c + 1 < schema->col_count ? " | " : "\n");
            row += schema->col_data[c].size;
        }
    }

done:
    if (fp != NULL)
        fclose(fp);
    free(page);
    free(schema);
}

int main(void) {
    printf("== 1. Create the database ==\n");
    printf("init_db(\"%s\") -> %d\n", DB_NAME, init_db(DB_NAME));

    printf("\n== 2. Create tables ==\n");
    ColumnDef users[3] = {
        {COL_TYPE_INT, 4}, {COL_TYPE_CHAR, 12}, {COL_TYPE_FLOAT, 8}};
    ColumnDef items[2] = {{COL_TYPE_CHAR, 8}, {COL_TYPE_INT, 2}};
    printf("init_table(\"Users\": INT(4), CHAR(12), FLOAT(8)) -> %d\n",
           init_table(DB_NAME, "Users", 3, users));
    printf("init_table(\"Items\": CHAR(8), INT(2))            -> %d\n",
           init_table(DB_NAME, "Items", 2, items));
    printf("init_table(\"Empty\": INT(8))                     -> %d\n",
           init_table(DB_NAME, "Empty", 1, (ColumnDef[]){{COL_TYPE_INT, 8}}));

    printf("\n== 3. Insert rows ==\n");
    Value u1[3] = {int_val(1), char_val("Ada"), float_val(99.5)};
    Value u2[3] = {int_val(2), char_val("Grace"), float_val(87.25)};
    Value u3[3] = {int_val(3), char_val("Linus"), float_val(-3.0)};
    Value i1[2] = {char_val("apple"), int_val(12)};
    Value i2[2] = {char_val("banana"), int_val(-300)};
    printf("insert Users (1, \"Ada\", 99.5)      -> %d\n",
           insert(DB_NAME, "Users", u1));
    printf("insert Users (2, \"Grace\", 87.25)   -> %d\n",
           insert(DB_NAME, "Users", u2));
    printf("insert Users (3, \"Linus\", -3.0)    -> %d\n",
           insert(DB_NAME, "Users", u3));
    printf("insert Items (\"apple\", 12)         -> %d\n",
           insert(DB_NAME, "Items", i1));
    printf("insert Items (\"banana\", -300)      -> %d\n",
           insert(DB_NAME, "Items", i2));

    printf("\n== 4. Inserts that are rejected (-1, nothing written) ==\n");
    Value too_big[2] = {char_val("cherry"), int_val(70000)};
    Value too_long[2] = {char_val("dragonfruit"), int_val(1)};
    Value wrong_type[2] = {int_val(5), int_val(1)};
    printf("insert Items (\"cherry\", 70000)     -> %d  (70000 doesn't fit "
           "INT(2))\n",
           insert(DB_NAME, "Items", too_big));
    printf("insert Items (\"dragonfruit\", 1)    -> %d  (11 chars don't fit "
           "CHAR(8))\n",
           insert(DB_NAME, "Items", too_long));
    printf("insert Items (5, 1)                -> %d  (INT given for a CHAR "
           "column)\n",
           insert(DB_NAME, "Items", wrong_type));
    printf("insert Nope (...)                  -> %d  (no such table)\n",
           insert(DB_NAME, "Nope", u1));

    printf("\n== 5. What's in the file ==\n");
    char *db_path = DB_NAME;
    get_db_path(&db_path);
    DbHeader *hdr = read_db_header(db_path);
    if (hdr == NULL) {
        printf("could not read %s\n", db_path);
        return 1;
    }
    printf("  %s: magic \"%.4s\", page size %u, %u pages, %u tables\n", db_path,
           hdr->magic, hdr->page_size, hdr->total_pages, hdr->table_count);
    for (uint32_t t = 0; t < hdr->table_count; t++)
        show_table(db_path, hdr, hdr->tables[t].table_name);

    free(hdr);
    free(db_path);
    return 0;
}
