#ifndef DB_ENGINE_CORE_PAGE_H
#define DB_ENGINE_CORE_PAGE_H

#include <stdint.h>
#include <stdio.h>

int pad_page(uint32_t page_id, uint16_t page_size, FILE *fp);

#endif
