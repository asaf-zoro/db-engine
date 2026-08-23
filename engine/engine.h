#ifndef ENGINE_H
#define ENGINE_H
#define FILE_ENDING ".crdb"

#include <stdint.h>

typedef struct {
    uint32_t table_count;
    uint32_t tables_start[];
} DbHeader;

//Initialize a new database.
int init_db(char *db_name);

//Initialize a new (fixed size for now) table, and adds it to the db.
int init_table(char *db_name, char *table_name);

#endif
