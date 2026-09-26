#include "core/schema.h"
#include <stdio.h>
#include <stdlib.h>

SchemaPage *get_schema(const char *db_path, const TableEntry *table,
                       uint16_t page_size) {
    FILE *fp = fopen(db_path, "rb");
    if (fp == NULL)
        return NULL;

    SchemaPage *schema = malloc(page_size);
    if (schema == NULL) {
        fclose(fp);
        return NULL;
    }

    if (fseek(fp, (long)table->schema_page_id * page_size, SEEK_SET) != 0 ||
        fread(schema, page_size, 1, fp) != 1) {
        fclose(fp);
        free(schema);
        return NULL;
    }

    fclose(fp);
    return schema;
}
