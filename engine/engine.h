#ifndef ENGINE_H
#define ENGINE_H

#define FILE_ENDING ".crdb"
#define DEFAULT_PAGE_SIZE 4096

#define PAGE_TYPE_HEADER 0x01
#define PAGE_TYPE_SCHEMA 0x02
#define PAGE_TYPE_DATA   0x03

#include <stdint.h>

typedef struct {
    uint16_t page_type;
    uint16_t table_index;
    uint16_t row_count;
    uint16_t max_rows;
} DataPage;

typedef struct {
    uint16_t page_type;
    uint16_t col_count;
    uint8_t col_data[];
} SchemaPage;

typedef struct {
    char table_name[10];
    uint32_t schema_page_id;
    uint16_t page_count;
    uint32_t pages_page_id;
} TableEntry;

typedef struct {
    char magic[4];
    uint16_t page_size;
    uint32_t total_pages;
    uint32_t table_count;
    TableEntry tables[];
} DbHeader;

//Initialize a new database.
int init_db(char *db_name);

//Initialize a new (fixed size for now) table, and adds it to the db.
int init_table(char *db_name, char *table_name, uint16_t col_count, uint8_t *col_sizes);

#endif
