#include "cachelab.h"
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#define ADDRESS 64

static int verbose = 0;
static unsigned set_bits;
static unsigned lines;
static unsigned block_bits;

unsigned int hit = 0, miss = 0, eviction = 0;
unsigned long timestamp = 0;


typedef struct cache_block {
    unsigned tag;
    unsigned last_use;
    bool valid;
} block;

typedef block* set;
typedef set* cache;


void print_help() {
    printf("Usage: ./csim-ref [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n"
           "\t-h: Help message\n"
           "\t-v: Optional verbose flag that displays trace info\n"
           "\t-s <s>: Number of set index bits\n"
           "\t-E <E>: Associativity (number of lines per set)\n"
           "\t-b <b>: Number of block bits\n"
           "\t-t <trace-file>: Name of the valgrind trace to replay\n"
    );
}

cache init_cache() {
    unsigned int num_sets = 1 << set_bits;
    cache cache_memory = (cache) calloc(num_sets, sizeof(set));
    for (int i = 0; i < num_sets; i++) {
        cache_memory[i] = (set) calloc(lines, sizeof(block));
        for (int j = 0; j < lines; j++) {
            cache_memory[i][j].tag = 0;
            cache_memory[i][j].last_use = 0;
            cache_memory[i][j].valid = false;
        }
    }
    return cache_memory;
}

void clear_cache(cache cache_memory) {
    unsigned size = 1 << set_bits;
    for (int i = 0; i < size; i++) {
        free(cache_memory[i]);
    }
    free(cache_memory);
}

void cache_simulator(cache memory, size_t address, int is_modify) {
    size_t tag = address >> (set_bits + block_bits);
    size_t set_index = (address >> block_bits) & ((1 << set_bits) - 1);

    set curr_set = memory[set_index];
    int lru_index = 0;
    int lru_time = curr_set[lru_index].last_use;

    for (int i = 0; i < lines; i++) {
        block* curr_line = &curr_set[i];
        if (curr_line->valid && curr_line->tag == tag) {
            hit += 1;
            hit += is_modify;
            curr_line->last_use = timestamp;
            if (verbose) {
                printf("hit\n");
            }
            return;
        }
        if (curr_line->last_use < lru_time) {
            lru_time = curr_line->last_use;
            lru_index = i;
        }
    }

    miss += 1;
    if (lru_time != 0) {
        eviction += 1;
    }
    hit += is_modify;
    curr_set[lru_index].tag = tag;
    curr_set[lru_index].last_use = timestamp;
    curr_set[lru_index].valid = true;
    if (verbose) {
        if (lru_time == 0) {
            if (is_modify) {
                printf("miss hit\n");
            } else {
                printf("miss\n");
            }
        } else {
            if (is_modify) {
                printf("miss eviction hit\n");
            } else {
                printf("miss eviction\n");
            }
        }
    }
}

void read_from_file(cache memory, FILE* trace) {
    char operation;
    size_t address;
    unsigned data_size;

    while (fscanf(trace, " %c %lx,%u\n", &operation, &address, &data_size) != EOF) {
        timestamp += 1;
        if (verbose && (operation != 'I')) {
            printf("%c %lx,%u ", operation, address, data_size);
        }
        switch (operation) {
            case 'I':
                continue;
            case 'L':
            case 'S':
                cache_simulator(memory, address, 0);
                break;
            case 'M':
                cache_simulator(memory, address, 1);
                break;
        }
    }
    fclose(trace);
}

int main(int argc, char* argv[]) {
    int option;
    FILE* trace_file;
    while ((option = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (option) {
            case 'h':
                print_help();
                return 0;
            case 'v':
                verbose = 1;
                break;
            case 's':
                set_bits = atoi(optarg);
                break;
            case 'E':
                lines = atoi(optarg);
                break;
            case 'b':
                block_bits = atoi(optarg);
                break;
            case 't':
                trace_file = fopen(optarg, "r");
                break;
            default:
                print_help();
                return 0;
        }
    }

    cache cache_memory = init_cache();
    read_from_file(cache_memory, trace_file);
    printSummary(hit, miss, eviction);
    clear_cache(cache_memory);
    return 0;
}
