#ifndef PAGING_H
#define PAGING_H
#include <stdint.h>
void paging_init(void);
uint32_t* paging_create_page_dir(void);
#endif
