#include "core/header.h"
#include "core/schema.h"
#include "core/table.h"
#include "db_engine.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int encode_value(uint8_t *dst, ColumnDef col, const Value *v) {
    if (v->type != col.type)
        return -1;

    switch (col.type) {
    case COL_TYPE_INT: {
        int64_t x = v->as.i;
        if (col.size < 8) {
            int64_t limit = INT64_C(1) << (col.size * 8 - 1);
            if (x < -limit || x >= limit)
                return -1;
        }
        switch (col.size) {
        case 1: { int8_t n = (int8_t)x; memcpy(dst, &n, 1); break; }
        case 2: { int16_t n = (int16_t)x; memcpy(dst, &n, 2); break; }
        case 4: { int32_t n = (int32_t)x; memcpy(dst, &n, 4); break; }
        default: memcpy(dst, &x, 8); break;
        }
        return 0;
    }
    case COL_TYPE_FLOAT:
        if (col.size == 4) {
            float f = (float)v->as.f;
            memcpy(dst, &f, 4);
        } else {
            memcpy(dst, &v->as.f, 8);
        }
        return 0;
    case COL_TYPE_CHAR:
        if (v->as.chars.len > col.size)
            return -1;
        memset(dst, 0, col.size);
        if (v->as.chars.len > 0)
            memcpy(dst, v->as.chars.data, v->as.chars.len);
        return 0;
    default:
        return -1;
    }
}

// Rows go straight into the table's reserved page (pages_page_id), which is a
// DataPage. init_table leaves it zeroed, so the DataPage header is written on
// the first insert.
// ponytail: one data page per table, insert fails once it's full.
int insert(char *db_name, char *table_name, const Value *values) {
    int result = -1;
    DbHeader *db_header = NULL;
    SchemaPage *schema = NULL;
    uint8_t *row = NULL;
    FILE *fp = NULL;

    get_db_path(&db_name);
    db_header = read_db_header(db_name);
    if (db_header == NULL)
        goto done;

    TableEntry *table = find_table(table_name, db_header);
    if (table == NULL)
        goto done;

    schema = get_schema(db_name, table, db_header->page_size);
    if (schema == NULL)
        goto done;

    uint32_t row_size = 0;
    for (uint16_t i = 0; i < schema->col_count; i++)
        row_size += schema->col_data[i].size;

    row = malloc(row_size);
    if (row == NULL)
        goto done;

    uint32_t offset = 0;
    for (uint16_t i = 0; i < schema->col_count; i++) {
        if (encode_value(row + offset, schema->col_data[i], &values[i]) != 0)
            goto done;
        offset += schema->col_data[i].size;
    }

    fp = fopen(db_name, "r+b");
    if (fp == NULL)
        goto done;

    long page_start = (long)table->pages_page_id * db_header->page_size;
    DataPage data_page;
    if (fseek(fp, page_start, SEEK_SET) != 0 ||
        fread(&data_page, sizeof(DataPage), 1, fp) != 1)
        goto done;

    if (data_page.page_type != PAGE_TYPE_DATA) {
        data_page = (DataPage){
            .page_type = PAGE_TYPE_DATA,
            .table_index = (uint16_t)(table - db_header->tables),
            .row_count = 0,
            .max_rows = (db_header->page_size - sizeof(DataPage)) / row_size,
        };
    }
    if (data_page.row_count >= data_page.max_rows)
        goto done;

    // Row first, then the header: if the second write fails, the row just
    // isn't counted.
    long row_start = page_start + sizeof(DataPage) + (long)data_page.row_count * row_size;
    if (fseek(fp, row_start, SEEK_SET) != 0 ||
        fwrite(row, row_size, 1, fp) != 1)
        goto done;

    data_page.row_count++;
    if (fseek(fp, page_start, SEEK_SET) != 0 ||
        fwrite(&data_page, sizeof(DataPage), 1, fp) != 1)
        goto done;

    result = 0;

done:
    if (fp != NULL)
        fclose(fp);
    free(row);
    free(schema);
    free(db_header);
    return result;
}
