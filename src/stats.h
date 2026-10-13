#pragma once
#include <stdint.h>

typedef struct {
    uint64_t mem_total_kb;
    uint64_t mem_available_kb;
} mem_stats_t;

int read_mem(mem_stats_t *shared_mem);
mem_stats_t *stats_init();
void print_log(mem_stats_t *shared_mem);
