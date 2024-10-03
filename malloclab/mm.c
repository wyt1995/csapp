#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"


/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

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

// global variable, always points to the prologue block
static char* heap_list = 0;

// function prototypes
static inline void *extend_heap(size_t bytes);
static void *coalesce(void *bp);
static void *find_fit(size_t align_size);
static void place(void *ptr, size_t align_size);

// heap checker for debugging
static void check_heap();
static void check_freelist();

/**
 * Initialize the malloc package.
 * @return 0 if okay, -1 if there was a problem in performing the initialization.
 */
int mm_init(void) {
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
    if (size <= DOUBLE_SIZE) {
        align_size = 2 * DOUBLE_SIZE;
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
    return NULL;
}


/**
 * Extend the size of heap memory when initialized OR malloc is unable to find a fit.
 * @param bytes the number of bytes to grow; will be 8-byte aligned.
 * @return a pointer to the new free memory block; null pointer on error.
 */
static inline void *extend_heap(size_t bytes) {
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


static void *coalesce(void *bp) {
    int prev_alloc = PREV_ALLOC(HEADER(bp));
    int next_alloc = CURR_ALLOC(HEADER(NEXT_BLOCK(bp)));
    int prev_prev;
    size_t curr_size = BLOCK_SIZE(HEADER(bp));

    if (prev_alloc && next_alloc) {
        SET_PREV_FREE(HEADER(NEXT_BLOCK(bp)));
    } else if (prev_alloc && !next_alloc) {
        curr_size += BLOCK_SIZE(HEADER(NEXT_BLOCK(bp)));
        PUT(HEADER(bp), PACK(curr_size, 2));
        PUT(FOOTER(bp), PACK(curr_size, 2));
    } else if (!prev_alloc && next_alloc) {
        curr_size += BLOCK_SIZE(HEADER(PREV_BLOCK(bp)));
        prev_prev = PREV_ALLOC(HEADER(PREV_BLOCK(bp)));
        SET_PREV_FREE(HEADER(NEXT_BLOCK(bp)));
        PUT(FOOTER(bp), PACK(curr_size, prev_prev));
        PUT(HEADER(PREV_BLOCK(bp)), PACK(curr_size, prev_prev));
        bp = PREV_BLOCK(bp);
    } else {
        curr_size += BLOCK_SIZE(HEADER(PREV_BLOCK(bp))) + BLOCK_SIZE(HEADER(NEXT_BLOCK(bp)));
        prev_prev = PREV_ALLOC(HEADER(PREV_BLOCK(bp)));
        PUT(HEADER(PREV_BLOCK(bp)), PACK(curr_size, prev_prev));
        PUT(FOOTER(NEXT_BLOCK(bp)), PACK(curr_size, prev_prev));
        bp = PREV_BLOCK(bp);
    }
    return bp;
}


static void *find_fit(size_t align_size) {
    void *bp;
    for (bp = heap_list; BLOCK_SIZE(HEADER(bp)) > 0; bp = NEXT_BLOCK(bp)) {
        if ((CURR_ALLOC(HEADER(bp)) == 0) && (align_size <= BLOCK_SIZE(HEADER(bp)))) {
            return bp;
        }
    }
    return NULL;
}


static void place(void *ptr, size_t align_size) {
    size_t free_size = BLOCK_SIZE(HEADER(ptr));
    size_t remainder = free_size - align_size;

    if (remainder < 2 * DOUBLE_SIZE) {
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
    }
}


static void check_freelist() {
    void *bp;
    for (bp = heap_list; BLOCK_SIZE(HEADER(bp)) > 0; bp = NEXT_BLOCK(bp)) {
        printf("address %p, size %d, %s\n", bp, BLOCK_SIZE(HEADER(bp)),
               CURR_ALLOC(HEADER(bp)) ? "allocated" : "free");
    }
    printf("\n");
}
