#include "core/header.h"
#include "core/page.h"
#include "db_engine.h"

#include <stdio.h>

int init_db(char *db_name) {
    const DbHeader new_db = {
        .magic = "CRDB",
        .page_size = DEFAULT_PAGE_SIZE,
        .total_pages = 1,
        .table_count = 0,
    };

    get_db_path(&db_name);
    FILE *fp = fopen(db_name, "wb");
    if (fp == NULL)
        return -1;

    if (pad_page(0, new_db.page_size, fp) != 0) {
        fclose(fp);
        return -1;
    }

    fseek(fp, 0, SEEK_SET);
    if (fwrite(&new_db, sizeof(DbHeader), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}
