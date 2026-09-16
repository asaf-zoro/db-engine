#include "db_engine.h"
#include "core/header.h"
#include "core/page.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SchemaPage* create_schema_page(uint16_t col_count, uint8_t col_data[]) {
    SchemaPage *new_schema = (SchemaPage *)malloc(sizeof(SchemaPage) + sizeof(uint8_t) * col_count);
    if (new_schema == NULL) return NULL;

    new_schema->page_type = PAGE_TYPE_SCHEMA;
    new_schema->col_count = col_count;

    memcpy(new_schema->col_data, col_data, sizeof(uint8_t) * col_count);
    return new_schema;
}

int init_table(char *db_name, char table_name[10], uint16_t col_count, uint8_t *col_sizes) {
    if (strlen(table_name) >= sizeof(((TableEntry *)0)->table_name)) return -1;

    get_db_path(&db_name);
    DbHeader *db_header = read_db_header(db_name);
    if (db_header == NULL) return -1;

    FILE *fp = fopen(db_name, "r+b");
    if (fp == NULL) {
        free(db_header);
        return -1;
    }

    TableEntry *new_table = (TableEntry*)malloc(sizeof(TableEntry));
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

    SchemaPage *tables_schema = create_schema_page(col_count, col_sizes);
    if (tables_schema == NULL) {
        fclose(fp);
        free(db_header);
        free(new_table);
        return -1;
    }
    fseek(fp, new_table->schema_page_id * db_header->page_size, SEEK_SET);
    fwrite(tables_schema, sizeof(SchemaPage) + sizeof(uint8_t) * tables_schema->col_count, 1, fp);

    db_header->table_count += 1;
    DbHeader *tmp = realloc(db_header, sizeof(DbHeader) + sizeof(TableEntry) * db_header->table_count);
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
    fwrite(db_header, sizeof(DbHeader) + sizeof(TableEntry) * db_header->table_count, 1, fp);

    fclose(fp);
    free(db_header);
    free(new_table);
    free(tables_schema);
    return 0;
}
