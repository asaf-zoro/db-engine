#include "core/page.h"

#include <stdlib.h>

int pad_page(const uint32_t page_id, const uint16_t page_size, FILE *fp) {
    if (fseek(fp, page_id * page_size, SEEK_SET) != 0) return -1;

    uint8_t *padding = calloc(1, page_size);
    if (padding == NULL) return -1;
    size_t written = fwrite(padding, 1, page_size, fp);

    free(padding);
    return (written == page_size) ? 0 : -1;
}
