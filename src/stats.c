#include "stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>

int read_mem(mem_stats_t *shared_mem) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return -1;

    char line[128];
    int found = 0;
    while (fgets(line, sizeof(line), f) && found < 2) {
        if (sscanf(line, "MemTotal: %lu kB", &shared_mem->mem_total_kb) == 1) found++;
        else if (sscanf(line, "MemAvailable: %lu kB", &shared_mem->mem_available_kb) == 1) found++;
    }
    fclose(f);
    return found == 2 ? 0 : -1;
}

// Выделение общей памяти - MAP_SHARED
// MAP_ANONYMOUS - память не связана ни с каким файлом. Ядро выдает страницы, заполненные нулями
// Поэтому дескриптор не нужен, передается как -1
mem_stats_t *stats_init() {
    mem_stats_t *shared = mmap(NULL, sizeof(mem_stats_t),
                               PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    return shared;
}

void print_log(mem_stats_t *shared_mem) {
    printf("Total:      %lu MB\n", shared_mem->mem_total_kb / 1024);
    printf("Available:  %lu MB\n", shared_mem->mem_available_kb / 1024);
    printf("Used:       %lu MB\n", (shared_mem->mem_total_kb - shared_mem->mem_available_kb) / 1024);
}
