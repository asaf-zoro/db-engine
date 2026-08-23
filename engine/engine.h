#ifndef ENGINE_H
#define ENGINE_H
#define FILE_ENDING ".crdb"

#include <stdint.h>

//TODO FIX README TOMORROW
typedef struct {
    char *table_name;
    uint64_t table_start;
} TableData;

typedef struct {
    uint32_t table_count;
    TableData *tables;
} DbHeader;

//Initialize a new database.
int init_db(char *db_name);

//Initialize a new (fixed size for now) table, and adds it to the db.
int init_table(char *db_name, char *table_name);

#endif
