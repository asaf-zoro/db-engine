#ifndef DB_ENGINE_CORE_HEADER_H
#define DB_ENGINE_CORE_HEADER_H

#include "db_engine.h"

void get_db_path(char **db_name);

DbHeader *read_db_header(const char *db_path);

#endif
