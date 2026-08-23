#include "engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void get_db_path(char **db_name) {
    char *path = malloc((strlen(*db_name) + strlen(FILE_ENDING) + 1) * sizeof(char));
    if (path == NULL) return;
    snprintf(path, 255, "./%s%s", *db_name, FILE_ENDING);
    *db_name = path;
}

int init_db(char *db_name) {
    const DbHeader new_db = {
        .table_count = 0
    };
    get_db_path(&db_name);
    printf("File name: %s", db_name);

    FILE *fptr = fopen(db_name, "wb");
    fwrite(&new_db, 1, sizeof(DbHeader), fptr);
    fclose(fptr);

    return 0;
}
