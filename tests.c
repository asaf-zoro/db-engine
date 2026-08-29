#include "engine/engine.h"


int main() {
    uint8_t sizes[3] = {4, 1, 8};
    init_table("MyDB", "MyTab", 3, sizes);

    return 0;
}
