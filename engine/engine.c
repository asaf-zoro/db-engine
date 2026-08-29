#include "engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void get_db_path(char **db_name) {
    char *path = malloc((strlen(*db_name) + strlen(FILE_ENDING) + 1) * sizeof(char));
    if (path == NULL) return;
    snprintf(path, 255, "./%s%s", *db_name, FILE_ENDING);
    *db_name = path;
}

static int pad_page(const uint32_t page_id, const uint16_t page_size, FILE *fp) {
    if (fseek(fp, page_id * page_size, SEEK_SET) != 0) return -1;

    uint8_t *padding = calloc(1, page_size);
    if (padding == NULL) return -1;
    size_t written = fwrite(padding, 1, page_size, fp);

    free(padding);
    return (written == page_size) ? 0 : -1;
}

int init_db(char *db_name) {
    const DbHeader new_db = {
        .magic = "CDB",
        .page_size = DEFAULT_PAGE_SIZE,
        .total_pages = 1,
        .table_count = 0,
    };

    get_db_path(&db_name);
    FILE *fp = fopen(db_name, "wb");
    if (fp == NULL) return -1;


    if (pad_page(0, new_db.page_size, fp) != 0) {
        fclose(fp);
        return -1;
    }

    fseek(fp, 0, SEEK_SET);
    if (fwrite(&new_db, sizeof(DbHeader), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

static DbHeader* read_db_header(const char *db_path) {
    FILE *fp = fopen(db_path, "rb");
    if (fp == NULL) return NULL;

    DbHeader base_info;
    fread(&base_info, sizeof(DbHeader), 1, fp);

    DbHeader *header = (DbHeader*)malloc(base_info.page_size);
    fseek(fp, 0, SEEK_SET);
    fread(header, base_info.page_size, 1, fp);

    fclose(fp);
    return header;
}

static SchemaPage* create_schema_page(uint16_t col_count, uint8_t col_data[]) {
    SchemaPage *new_schema = (SchemaPage *)malloc(sizeof(SchemaPage) + sizeof(uint8_t) * col_count);

    new_schema->page_type = PAGE_TYPE_SCHEMA;
    new_schema->col_count = col_count;

    memcpy(new_schema->col_data, col_data, sizeof(uint8_t) * col_count);
    return new_schema;
}

int init_table(char *db_name, char table_name[10], uint16_t col_count, uint8_t *col_sizes) {
    get_db_path(&db_name);
    DbHeader *db_header = read_db_header(db_name);

    FILE *fp = fopen(db_name, "r+b");

    TableEntry *new_table = (TableEntry*)malloc(sizeof(TableEntry));
    strcpy(new_table->table_name, table_name);
    new_table->page_count = 0;

    //Set the schema and page list page ids.
    new_table->schema_page_id = db_header->total_pages++;
    new_table->pages_page_id = db_header->total_pages++;

    //Pad the pages.
    pad_page(new_table->schema_page_id, db_header->page_size, fp);
    pad_page(new_table->pages_page_id, db_header->page_size, fp);

    //Creating and writing schema to file.
    SchemaPage *tables_schema = create_schema_page(col_count, col_sizes);
    fseek(fp, new_table->schema_page_id * db_header->page_size, SEEK_SET);
    fwrite(tables_schema, sizeof(SchemaPage) + sizeof(uint8_t) * tables_schema->col_count, 1, fp);

    //Add table to database header.
    db_header->table_count += 1;
    DbHeader *tmp = realloc(db_header, sizeof(DbHeader) + sizeof(TableEntry) * db_header->table_count);
    if (tmp == NULL) return -1;
    db_header = tmp;
    db_header->tables[db_header->table_count - 1] = *new_table;

    //Write database header to file.
    fseek(fp, 0, SEEK_SET);
    fwrite(db_header, sizeof(DbHeader) + sizeof(TableEntry) * db_header->table_count, 1, fp);

    fclose(fp);
    free(db_header);
    free(new_table);
    free(tables_schema);
    return 0;
}