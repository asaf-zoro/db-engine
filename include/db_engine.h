#ifndef DB_ENGINE_H
#define DB_ENGINE_H

#include <stddef.h>
#include <stdint.h>

#define FILE_ENDING ".crdb"
#define DEFAULT_PAGE_SIZE 4096

#define PAGE_TYPE_HEADER 0x01
#define PAGE_TYPE_SCHEMA 0x02
#define PAGE_TYPE_DATA   0x03

typedef struct {
    uint16_t page_type;
    uint16_t table_index;
    uint16_t row_count;
    uint16_t max_rows;
} DataPage;

#define COL_TYPE_INT   0x01 // signed, size 1/2/4/8
#define COL_TYPE_FLOAT 0x02 // size 4/8
#define COL_TYPE_CHAR  0x03 // text, zero-padded to size

typedef struct {
    uint8_t type;
    uint8_t size; // width in bytes
} ColumnDef;

// A typed value handed to insert; `type` is a COL_TYPE_* and selects the member.
typedef struct {
    uint8_t type;
    union {
        int64_t i;     // COL_TYPE_INT
        double f;      // COL_TYPE_FLOAT
        struct {       // COL_TYPE_CHAR (not null-terminated)
            const char *data;
            size_t len;
        } chars;
    } as;
} Value;

typedef struct {
    uint16_t page_type;
    uint16_t col_count;
    ColumnDef col_data[];
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

int init_db(char *db_name);

int init_table(char *db_name, char *table_name, uint16_t col_count, const ColumnDef *cols);

int insert(char *db_name, char *table_name, const Value *values);

#endif
