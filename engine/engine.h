#ifndef ENGINE_H
#define ENGINE_H

#define FILE_ENDING ".crdb"
#define DEFAULT_PAGE_SIZE 4096

#include <stdint.h>

typedef struct {
    uint16_t page_type;
    uint16_t table_index;
    uint16_t row_count;
    uint16_t max_rows;
} DataPage;

typedef struct {
    char table_name[10];
    uint16_t row_len;
    uint16_t page_count;
    uint32_t page_ids[];
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
int init_table(char *db_name, char *table_name);

#endif
