#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"


/* single word (4) or double word (8) alignment */
#define SIZE_T_SIZE (sizeof(size_t))
#define ALIGNMENT (2 * SIZE_T_SIZE)

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

// basic constants
#define WORD_SIZE   4
#define DOUBLE_SIZE 8
#define CHUNK_SIZE  (1 << 12)  // 4096 bytes
#define BUCKET_NUM  16

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

// pack a size and allocated bit
#define PACK(size, alloc) ((size) | (alloc))

// read or write a word at address p
#define GET(p)      (*(uint32_t *) (p))
#define PUT(p, val) (*(uint32_t *) (p) = (val))

// extract size and allocated bits from address p
#define BLOCK_SIZE(p) (GET(p) & ~0x7)
#define CURR_ALLOC(p) (GET(p) & 0x1)   // last bit
#define PREV_ALLOC(p) (GET(p) & 0x2)   // second-last bit
#define ALLOC_BITS(p) (GET(p) & 0x3)   // last two bits

// set allocated bits
#define SET_CURR_ALLOC(p) (GET(p) |= 0x1)
#define SET_PREV_ALLOC(p) (GET(p) |= 0x2)
#define SET_CURR_FREE(p)  (GET(p) &= ~0x1)
#define SET_PREV_FREE(p)  (GET(p) &= ~0x2)

// used for pointer arithmetic
#define BLOCK_PTR(bp) ((char *)(bp))

// compute addresses of header and footer
#define HEADER(bp) (BLOCK_PTR(bp) - WORD_SIZE)
#define FOOTER(bp) (BLOCK_PTR(bp) + BLOCK_SIZE(HEADER(bp)) - DOUBLE_SIZE)

// compute addresses of next or previous block
#define NEXT_BLOCK(bp) (BLOCK_PTR(bp) + BLOCK_SIZE(HEADER(bp)))
#define PREV_BLOCK(bp) (BLOCK_PTR(bp) - BLOCK_SIZE((BLOCK_PTR(bp) - DOUBLE_SIZE)))

// doubly linked list
#define PREV_NODE(bp)  ((void *) (*(size_t *) ((char *)(bp))))
#define NEXT_NODE(bp)  ((void *) (*(size_t *) ((char *)(bp) + SIZE_T_SIZE)))
#define SET_PREV_NODE(bp, val)   (*(size_t *) ((char *)(bp)) = (size_t)(val))
#define SET_NEXT_NODE(bp, val)   (*(size_t *) ((char *)(bp) + SIZE_T_SIZE) = (size_t)(val))

// global variable, always points to the prologue block
static char* heap_list = 0;

// segregated free lists
static void* free_lists[BUCKET_NUM];

// function prototypes
static void *extend_heap(size_t bytes);
static void *coalesce(void *bp);
static void *find_fit(size_t align_size);
static void place(void *ptr, size_t align_size);
static inline void insert_node(void *bp, size_t size);
static inline void remove_node(void *bp);

// heap checker for debugging
static void check_heap();
static void check_freelist();

/**
 * Initialize the malloc package.
 * @return 0 if okay, -1 if there was a problem in performing the initialization.
 */
int mm_init(void) {
    // initialize segregated free lists
    if ((heap_list = mem_sbrk(BUCKET_NUM * SIZE_T_SIZE)) == (void *) -1) {
        return -1;
    }
    for (int i = 0; i < BUCKET_NUM; i++) {
        free_lists[i] = 0;
    }

    // create initial heap with empty free list
    if ((heap_list = mem_sbrk(4 * WORD_SIZE)) == (void *) -1) {
        return -1;
    }
    PUT(heap_list, 0);
    PUT(heap_list + (1 * WORD_SIZE), PACK(DOUBLE_SIZE, 1));  // header
    PUT(heap_list + (2 * WORD_SIZE), PACK(DOUBLE_SIZE, 1));  // footer
    PUT(heap_list + (3 * WORD_SIZE), PACK(0, 3));            // epilogue
    heap_list += DOUBLE_SIZE;

    // extend heap size
    if (extend_heap(CHUNK_SIZE) == NULL)
        return -1;
    return 0;
}

/**
 * Allocate a block of memory of at least size bytes.
 * Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    if (heap_list == 0) {
        mm_init();
    }

    // adjust size to include overhead, round up to be multiples of 8 bytes
    char *bp;
    size_t align_size;
    if (size <= ALIGNMENT) {
        align_size = 2 * ALIGNMENT;
    } else {
        align_size = ALIGN(size + WORD_SIZE);
    }

    if ((bp = find_fit(align_size)) != NULL) {
        place(bp, align_size);
        return bp;
    }

    // no fit found; extend heap memory
    size_t extend_size = MAX(align_size, CHUNK_SIZE);
    if ((bp = extend_heap(extend_size)) == NULL)
        return NULL;
    place(bp, align_size);
    return bp;
}

/**
 * Free the memory block pointed to by ptr; return nothing.
 */
void mm_free(void *ptr) {
    if (ptr == NULL || heap_list == 0) {
        return;
    }
    size_t size = BLOCK_SIZE(HEADER(ptr));
    int prev_alloc = PREV_ALLOC(HEADER(ptr));
    PUT(HEADER(ptr), PACK(size, prev_alloc));
    PUT(FOOTER(ptr), PACK(size, prev_alloc));
    coalesce(ptr);
}

/**
 * Return a pointer to an allocated region of at least size bytes.
 * If ptr is NULL, the call is equivalent to mm_malloc.
 * If size is equal to zero, the call is equivalent to mm_free.
 * Otherwise, it changes the size of the memory block pointed to by ptr to size bytes
 * and returns the address of the new block.
 */
void *mm_realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return mm_malloc(size);
    }
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    size_t old_size = BLOCK_SIZE(HEADER(ptr));
    size_t new_size;
    if (size <= ALIGNMENT) {
        new_size = 2 * ALIGNMENT;
    } else {
        new_size = ALIGN(size + WORD_SIZE);
    }

    if (new_size <= old_size) {
        if (old_size - new_size >= 2 * ALIGNMENT) {
            SET_PREV_FREE(HEADER(NEXT_BLOCK(ptr)));
            PUT(HEADER(ptr), PACK(new_size, ALLOC_BITS(HEADER(ptr))));
            PUT(HEADER(NEXT_BLOCK(ptr)), PACK(old_size - new_size, 2));
            PUT(FOOTER(NEXT_BLOCK(ptr)), PACK(old_size - new_size, 2));
            insert_node(NEXT_BLOCK(ptr), old_size - new_size);
        }
        return ptr;
    }

    void *new_ptr;
    size_t allowed_size = old_size;
    if (CURR_ALLOC(HEADER(NEXT_BLOCK(ptr))) == 0) {
        allowed_size += BLOCK_SIZE(HEADER(NEXT_BLOCK(ptr)));
    }
    if (new_size <= allowed_size) {
        new_ptr = ptr;
        remove_node(NEXT_BLOCK(ptr));
        if (allowed_size - new_size >= 2 * ALIGNMENT) {
            PUT(HEADER(ptr), PACK(new_size, ALLOC_BITS(HEADER(ptr))));
            ptr = NEXT_BLOCK(ptr);
            PUT(HEADER(ptr), PACK(allowed_size - new_size, 2));
            PUT(FOOTER(ptr), PACK(allowed_size - new_size, 2));
            insert_node(ptr, allowed_size - new_size);
        } else {
            PUT(HEADER(ptr), PACK(allowed_size, ALLOC_BITS(HEADER(ptr))));
            ptr = NEXT_BLOCK(ptr);
            SET_PREV_ALLOC(HEADER(ptr));
        }
    } else {
        new_ptr = mm_malloc(size);
        if (!new_ptr)
            return NULL;
        memcpy(new_ptr, ptr, old_size);
        mm_free(ptr);
    }
    return new_ptr;
}


/**
 * Extend the size of heap memory when initialized OR malloc is unable to find a fit.
 * @param bytes the number of bytes to grow; will be 8-byte aligned.
 * @return a pointer to the new free memory block; null pointer on error.
 */
static void *extend_heap(size_t bytes) {
    size_t size = ALIGN(bytes);
    char *block_ptr = mem_sbrk(size);

    // error handling
    if ((size_t) block_ptr == -1) {
        return NULL;
    }

    // initialize free block header/footer
    int prev_alloc = PREV_ALLOC(HEADER(block_ptr));
    PUT(HEADER(block_ptr), PACK(size, prev_alloc));
    PUT(FOOTER(block_ptr), PACK(size, prev_alloc));

    // the heap always ends with a special epilogue of size 0, consists only a header
    PUT(HEADER(NEXT_BLOCK(block_ptr)), PACK(0, 1));

    return coalesce(block_ptr);
}

/**
 * Merge adjacent free blocks.
 * @param bp a pointer to the newly freed block.
 * @return a pointer to beginning of the merged block.
 */
static void *coalesce(void *bp) {
    int prev_alloc = PREV_ALLOC(HEADER(bp));
    int next_alloc = CURR_ALLOC(HEADER(NEXT_BLOCK(bp)));
    int prev_prev;
    size_t curr_size = BLOCK_SIZE(HEADER(bp));

    if (prev_alloc && next_alloc) {
        SET_PREV_FREE(HEADER(NEXT_BLOCK(bp)));
    } else if (prev_alloc && !next_alloc) {
        remove_node(NEXT_BLOCK(bp));
        curr_size += BLOCK_SIZE(HEADER(NEXT_BLOCK(bp)));
        PUT(HEADER(bp), PACK(curr_size, 2));
        PUT(FOOTER(bp), PACK(curr_size, 2));
    } else if (!prev_alloc && next_alloc) {
        remove_node(PREV_BLOCK(bp));
        curr_size += BLOCK_SIZE(HEADER(PREV_BLOCK(bp)));
        prev_prev = PREV_ALLOC(HEADER(PREV_BLOCK(bp)));
        SET_PREV_FREE(HEADER(NEXT_BLOCK(bp)));
        PUT(FOOTER(bp), PACK(curr_size, prev_prev));
        PUT(HEADER(PREV_BLOCK(bp)), PACK(curr_size, prev_prev));
        bp = PREV_BLOCK(bp);
    } else {
        remove_node(PREV_BLOCK(bp));
        remove_node(NEXT_BLOCK(bp));
        curr_size = curr_size + BLOCK_SIZE(HEADER(PREV_BLOCK(bp)))
                              + BLOCK_SIZE(HEADER(NEXT_BLOCK(bp)));
        prev_prev = PREV_ALLOC(HEADER(PREV_BLOCK(bp)));
        PUT(HEADER(PREV_BLOCK(bp)), PACK(curr_size, prev_prev));
        PUT(FOOTER(NEXT_BLOCK(bp)), PACK(curr_size, prev_prev));
        bp = PREV_BLOCK(bp);
    }
    insert_node(bp, curr_size);
    return bp;
}

static int find_group(size_t size) {
    int offset = 30 - __builtin_clz(size);
    return (offset >= BUCKET_NUM) ? (BUCKET_NUM - 1) : offset;
}

static void *find_fit(size_t align_size) {
    void *bp;
    int n = find_group(align_size);
    for (; n < BUCKET_NUM; n++) {
        for (bp = free_lists[n]; bp != 0; bp = NEXT_NODE(bp)) {
            if (align_size <= BLOCK_SIZE(HEADER(bp))) {
                return bp;
            }
        }
    }
    return NULL;
}

static void place(void *ptr, size_t align_size) {
    size_t free_size = BLOCK_SIZE(HEADER(ptr));
    size_t remainder = free_size - align_size;
    remove_node(ptr);

    if (remainder < 2 * ALIGNMENT) {
        SET_CURR_ALLOC(HEADER(ptr));
        SET_PREV_ALLOC(HEADER(NEXT_BLOCK(ptr)));
        if (CURR_ALLOC(HEADER(NEXT_BLOCK(ptr))) == 0) {
            SET_PREV_ALLOC(FOOTER(NEXT_BLOCK(ptr)));
        }
    } else {
        PUT(HEADER(ptr), PACK(align_size, PREV_ALLOC(HEADER(ptr)) + 1));
        ptr = NEXT_BLOCK(ptr);
        PUT(HEADER(ptr), PACK(remainder, 2));
        PUT(FOOTER(ptr), PACK(remainder, 2));
        insert_node(ptr, remainder);
    }
}

static inline void insert_node(void *bp, size_t size) {
    int n = find_group(size);
    void *prev = 0;
    void *current = free_lists[n];

    while (current != 0 && current < bp) {
        prev = current;
        current = NEXT_NODE(current);
    }
    SET_PREV_NODE(bp, prev);
    SET_NEXT_NODE(bp, current);

    if (prev == 0) {
        free_lists[n] = bp;
    } else {
        SET_NEXT_NODE(prev, bp);
    }
    if (current) {
        SET_PREV_NODE(current, bp);
    }
}

static inline void remove_node(void *bp) {
    void *prev = PREV_NODE(bp);
    void *next = NEXT_NODE(bp);
    size_t size = BLOCK_SIZE(HEADER(bp));
    int num = find_group(size);
    if (prev == 0) {
        free_lists[num] = next;
    } else {
        SET_NEXT_NODE(prev, next);
    }
    if (next != 0) {
        SET_PREV_NODE(next, prev);
    }
}


static void check_heap() {
    void *bp = mem_heap_lo() + BUCKET_NUM * DOUBLE_SIZE;

    // prologue
    if (GET(bp) != 0) {
        printf("prologue error\n");
    }
    if (GET(bp + WORD_SIZE) != PACK(DOUBLE_SIZE, 1)) {
        printf("prologue error: header incorrect at %p\n", bp);
    }
    if (GET(bp + DOUBLE_SIZE) != PACK(DOUBLE_SIZE, 1)) {
        printf("prologue error: footer incorrect at %p\n", bp);
    }
    bp += DOUBLE_SIZE;

    // heap
    int prev_alloc = 1;
    int prev_free = 0;
    while (bp < mem_heap_hi()) {
        if (BLOCK_SIZE(HEADER(bp)) == 0) {
            printf("invalid block size at %p\n", bp);
        }
        if (prev_alloc != 1) {
            if (PREV_ALLOC(HEADER(bp)) != prev_alloc) {
                printf("block header error: prev alloc bit incorrect at %p\n", bp);
            }
        }
        prev_alloc = PREV_ALLOC(HEADER(bp));

        if (CURR_ALLOC(HEADER(bp)) == 0) {
            if (GET(HEADER(bp)) != GET(FOOTER(bp))) {
                printf("header and footer do not match for free block at %p\n", bp);
            }
            if (prev_free) {
                printf("consecutive free blocks at %p\n", bp);
            }
            prev_free = 1;
        } else {
            prev_free = 0;
        }
        bp = NEXT_BLOCK(bp);
    }

    // epilogue
    if (BLOCK_SIZE(HEADER(bp)) != 0) {
        printf("epilogue size error at %p\n", bp);
    }
    if (CURR_ALLOC(HEADER(bp)) != 1) {
        printf("epilogue alloc bit error at %p\n", bp);
    }
    if (PREV_ALLOC(HEADER(bp)) != prev_alloc) {
        printf("epilogue prev alloc bit error at %p\n", bp);
    }
    if (bp > mem_heap_hi()) {
        printf("block exceeds heap break at %p\n", bp);
    }
}

static void check_freelist() {
    void *bp;
    for (int i = 0; i < BUCKET_NUM; i++) {
        printf("Group %d: ", i);
        for (bp = free_lists[i]; bp; bp = NEXT_NODE(bp)) {
            printf("%d, ", BLOCK_SIZE(HEADER(bp)));
        }
        printf("\n");
    }
}
