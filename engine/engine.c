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

static int pad_page(const uint32_t starting_byte, const uint16_t page_size, FILE *fp) {
    if (fseek(fp, starting_byte + page_size - 1, SEEK_SET) != 0) return -1;

    if (fputc(0, fp) != 0) return -1;

    return 0;
}

int init_db(char *db_name) {
    const DbHeader new_db = {
        .magic = "CDB",
        .page_size = DEFAULT_PAGE_SIZE,
        .total_pages = 1,
        .table_count = 0,
    };

    get_db_path(&db_name);
    FILE *fp = fopen(db_name, "wb");
    if (fp == NULL) return -1;

    if (fwrite(&new_db, sizeof(DbHeader), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    if (pad_page(0, new_db.page_size, fp) != 0) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}
