#ifndef ENGINE_H
#define ENGINE_H
#define MAX_ROWS 100

#include <stdint.h>

typedef struct {
    uint32_t RowId;
    char Value[255];
} Row;

typedef struct {
    uint32_t TableId;
    char TableName[255];
    Row Rows[MAX_ROWS];
} Table;

/*Insert a value into a table*/
int insert(Table *table, const char *value);

/*Displays the table's contents according to parameters (For now only show full table).*/
int select_table(const Table *table);

#endif
