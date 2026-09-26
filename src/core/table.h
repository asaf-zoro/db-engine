#ifndef DB_ENGINE_CORE_TABLE_H
#define DB_ENGINE_CORE_TABLE_H

#include "db_engine.h"

TableEntry *find_table(const char *table_name, DbHeader *db_header);

#endif
