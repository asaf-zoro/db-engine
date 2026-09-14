#include "core/header.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void get_db_path(char **db_name) {
    char *path = malloc((strlen(*db_name) + strlen(FILE_ENDING) + 1) * sizeof(char));
    if (path == NULL) return;
    snprintf(path, 255, "./%s%s", *db_name, FILE_ENDING);
    *db_name = path;
}

DbHeader *read_db_header(const char *db_path) {
    FILE *fp = fopen(db_path, "rb");
    if (fp == NULL) return NULL;

    DbHeader base_info;
    fread(&base_info, sizeof(DbHeader), 1, fp);

    DbHeader *header = (DbHeader*)malloc(base_info.page_size);
    fseek(fp, 0, SEEK_SET);
    fread(header, base_info.page_size, 1, fp);

    fclose(fp);
    return header;
}
