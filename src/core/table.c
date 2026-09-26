#include "core/table.h"
#include <string.h>

TableEntry *find_table(const char *table_name, DbHeader *db_header) {
    for (uint32_t i = 0; i < db_header->table_count; i++) {
        if (strncmp(table_name, db_header->tables[i].table_name,
                    sizeof(db_header->tables[i].table_name)) == 0)
            return &db_header->tables[i];
    }
    return NULL;
}
