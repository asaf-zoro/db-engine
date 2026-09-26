#ifndef DB_ENGINE_CORE_SCHEMA_H
#define DB_ENGINE_CORE_SCHEMA_H

#include "db_engine.h"

// Reads the table's schema page. Caller frees the result.
SchemaPage *get_schema(const char *db_path, const TableEntry *table, uint16_t page_size);

#endif
