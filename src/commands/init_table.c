#include "core/header.h"
#include "core/page.h"
#include "db_engine.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SchemaPage *create_schema_page(uint16_t col_count,
                                      const ColumnDef *cols) {
    SchemaPage *new_schema = (SchemaPage *)malloc(
        sizeof(SchemaPage) + sizeof(ColumnDef) * col_count);
    if (new_schema == NULL)
        return NULL;

    new_schema->page_type = PAGE_TYPE_SCHEMA;
    new_schema->col_count = col_count;

    memcpy(new_schema->col_data, cols, sizeof(ColumnDef) * col_count);
    return new_schema;
}

// A table is only usable if its schema fits in one page and at least one row
// fits in a data page (otherwise max_rows would be 0).
static int valid_columns(uint16_t col_count, const ColumnDef *cols,
                         uint16_t page_size) {
    if (col_count == 0 ||
        sizeof(SchemaPage) + sizeof(ColumnDef) * col_count > page_size)
        return 0;

    uint32_t row_size = 0;
    for (uint16_t i = 0; i < col_count; i++) {
        uint8_t size = cols[i].size;
        switch (cols[i].type) {
        case COL_TYPE_INT:
            if (size != 1 && size != 2 && size != 4 && size != 8)
                return 0;
            break;
        case COL_TYPE_FLOAT:
            if (size != 4 && size != 8)
                return 0;
            break;
        case COL_TYPE_CHAR:
            if (size == 0)
                return 0;
            break;
        default:
            return 0;
        }
        row_size += size;
    }
    return sizeof(DataPage) + row_size <= page_size;
}

int init_table(char *db_name, char table_name[10], uint16_t col_count,
               const ColumnDef *cols) {
    if (strlen(table_name) >= sizeof(((TableEntry *)0)->table_name))
        return -1;

    get_db_path(&db_name);
    DbHeader *db_header = read_db_header(db_name);
    if (db_header == NULL)
        return -1;

    if (!valid_columns(col_count, cols, db_header->page_size)) {
        free(db_header);
        return -1;
    }

    FILE *fp = fopen(db_name, "r+b");
    if (fp == NULL) {
        free(db_header);
        return -1;
    }

    TableEntry *new_table = (TableEntry *)malloc(sizeof(TableEntry));
    if (new_table == NULL) {
        fclose(fp);
        free(db_header);
        return -1;
    }
    strcpy(new_table->table_name, table_name);
    new_table->page_count = 0;

    new_table->schema_page_id = db_header->total_pages++;
    new_table->pages_page_id = db_header->total_pages++;

    pad_page(new_table->schema_page_id, db_header->page_size, fp);
    pad_page(new_table->pages_page_id, db_header->page_size, fp);

    SchemaPage *tables_schema = create_schema_page(col_count, cols);
    if (tables_schema == NULL) {
        fclose(fp);
        free(db_header);
        free(new_table);
        return -1;
    }
    fseek(fp, new_table->schema_page_id * db_header->page_size, SEEK_SET);
    fwrite(tables_schema,
           sizeof(SchemaPage) + sizeof(ColumnDef) * tables_schema->col_count, 1,
           fp);

    db_header->table_count += 1;
    DbHeader *tmp =
        realloc(db_header,
                sizeof(DbHeader) + sizeof(TableEntry) * db_header->table_count);
    if (tmp == NULL) {
        fclose(fp);
        free(db_header);
        free(new_table);
        free(tables_schema);
        return -1;
    }
    db_header = tmp;
    db_header->tables[db_header->table_count - 1] = *new_table;

    fseek(fp, 0, SEEK_SET);
    fwrite(db_header,
           sizeof(DbHeader) + sizeof(TableEntry) * db_header->table_count, 1,
           fp);

    fclose(fp);
    free(db_header);
    free(new_table);
    free(tables_schema);
    return 0;
}
