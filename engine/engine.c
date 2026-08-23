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

static DbHeader *get_db_header(const char *db_name) {
    FILE *fptr = fopen(db_name, "rb");

    DbHeader *new_db = (DbHeader *) malloc(sizeof(DbHeader));
    fread(new_db, sizeof(DbHeader), 1, fptr);

    if (new_db == NULL) return NULL;
    new_db->tables = (TableData *)malloc(sizeof(TableData) * new_db->table_count);
    fread(new_db->tables, sizeof(TableData) * new_db->table_count, 1, fptr);
    fclose(fptr);

    return new_db;
};

int init_db(char *db_name) {
    const DbHeader new_db = {
        .table_count = 0
    };
    get_db_path(&db_name);

    FILE *fptr = fopen(db_name, "wb");
    fwrite(&new_db, 1, sizeof(DbHeader), fptr);
    fclose(fptr);

    return 0;
}

int init_table(char *db_name, char *table_name) {
    get_db_path(&db_name);

    DbHeader *my_db = get_db_header(db_name);
    FILE *fptr = fopen(db_name, "wb");

    fseek(fptr, 0, SEEK_END);
    uint64_t new_table_pos = ftell(fptr);

    const TableData new_table = {
        .table_name = table_name,
        .table_start = new_table_pos,
    };

    my_db->table_count += 1;
    TableData *tmp = (TableData *) realloc(my_db->tables, my_db->table_count * sizeof(TableData));
    if (tmp == NULL) {
        free(my_db->tables);
        free(my_db);
        return -1;
    }

    my_db->tables = tmp;
    my_db->tables[my_db->table_count - 1] = new_table;

    fwrite(my_db, (sizeof(DbHeader) + sizeof(TableData) * my_db->table_count), 1, fptr);
    fclose(fptr);
    return 0;
}
