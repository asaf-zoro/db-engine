#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *buffer;
    size_t size;
    ssize_t chars_read;
} InputLine;

typedef struct {
    size_t id;
    char data[255];
} Row;

typedef struct {
    size_t id;
    size_t row_count;
    Row rows[100];
} Table;

static InputLine* init_input_line() {
    InputLine* line = (InputLine*)malloc(sizeof(InputLine));
    line->buffer = NULL;
    line->size = 0;
    line->chars_read = 0;

    return line;
}

static Table* init_table(const size_t id) {
    Table* table = (Table*)malloc(sizeof(Table));
    table->row_count = 0;
    table->id = id;

    return table;
}

static void readLine(InputLine* input) {
    input->chars_read = getline(&input->buffer, &input->size, stdin);

    if (input->chars_read == -1) {
        printf("Couldn't read from stdin\n");
        exit(EXIT_FAILURE);
    } else {
        input->chars_read -= 1;
        input->buffer[input->chars_read] = '\0';
    }
}

static void parse_input(const InputLine *input, int* running) {
    if (strncmp(input->buffer, ".exit", 5) == 0) {
        printf("Exiting...\n");
        *running = 0;
    } else if (strncmp(input->buffer, "insert", 6) == 0) {
        printf("Insert here!");
    } else if (strncmp(input->buffer, "select", 6) == 0) {
        printf("Select here!");
    } else {
        printf("Unknown command: %s\n", input->buffer);
    }
}

int main() {
    InputLine *input = init_input_line();
    int *running = (int*)malloc(sizeof(int));
    *running = 1;

    Table *t1 = init_table(0);

    while (*running) {
        readLine(input);
        parse_input(input, running);
    }

    free(input->buffer);
    free(input);
    free(t1);
    free(running);
    return 0;
}
