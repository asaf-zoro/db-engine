#include "db_engine.h"
#include "core/header.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Sizes {4,1,8} -> row_size 13, max_rows (4096-8)/13 = 314, so row 315 spills to a 2nd data page.
int main() {
    ColumnDef cols[3] = {{COL_TYPE_INT, 4}, {COL_TYPE_INT, 1}, {COL_TYPE_FLOAT, 8}};
    assert(init_db("MyDB") == 0);
    assert(init_table("MyDB", "MyTab", 3, cols) == 0);

    uint8_t row[13];
    for (int i = 0; i < 315; i++) {
        memset(row, 0, sizeof row);
        memcpy(row, &i, sizeof i);
        assert(insert("MyDB", "MyTab", row) == 0);
    }
    assert(insert("MyDB", "NoSuchTab", row) == -1);

    char *path = "MyDB";
    get_db_path(&path);
    DbHeader *hdr = read_db_header(path);
    assert(hdr != NULL);
    assert(hdr->tables[0].page_count == 2);
    assert(hdr->total_pages == 5); // header, schema, page list, 2 data pages

    FILE *fp = fopen(path, "rb");
    assert(fp != NULL);

    DataPage dp;
    uint8_t got[13];

    fseek(fp, 3L * hdr->page_size, SEEK_SET);
    assert(fread(&dp, sizeof dp, 1, fp) == 1);
    assert(dp.row_count == 314 && dp.max_rows == 314 && dp.table_index == 0);

    fseek(fp, 4L * hdr->page_size, SEEK_SET);
    assert(fread(&dp, sizeof dp, 1, fp) == 1);
    assert(dp.row_count == 1);
    assert(fread(got, sizeof got, 1, fp) == 1);
    int last = 314;
    memset(row, 0, sizeof row);
    memcpy(row, &last, sizeof last);
    assert(memcmp(got, row, sizeof row) == 0);

    fclose(fp);
    free(hdr);
    puts("ok");
    return 0;
}
