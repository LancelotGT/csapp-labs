/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Ning Wang",
    /* First member's email address */
    "glningwang@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

#define WSIZE 4 /* word size in bytes */
#define DSIZE 8 /* double word size */
#define CHUNKSIZE (1<<12) /* extend heap by this amount in bytes */

#define MAX(x, y) ((x > y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* read and write a word at address p */
#define GET(p) (*(unsigned int*)(p)) 
#define PUT(p, val) (*(unsigned int*)(p) = (unsigned int)(val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* given a block, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* given a block ptr bp, compute address of its previous and next blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET(((char *)(bp) - DSIZE)))

/* given a free block ptr bp, compute address of its previous and next blocks in free list */ 
#define NEXT_FREE_BLKP(bp) ((char *)(bp) + GET((char *)(bp)))
#define PREV_FREE_BLKP(bp) ((char *)(bp) - GET(((char *)(bp) + WSIZE))) 

/* give a free block prt bp, change its next or prev word */
#define PUT_NEXT_BLKP(bp, val) PUT((char*) (bp), val)
#define PUT_PREV_BLKP(bp, val) PUT((char*) (bp) + WSIZE, val) 

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (DSIZE - 1)) & ~0x7)

/* global variable */
static char* heap_listp;
static char* free_listp;

/* private function prototypes */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void mm_checkheap(int verbose); 
static void printblock(void *bp); 
static void checkblock(void *bp);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *) -1)
         return -1;

    PUT(heap_listp, 0); /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1)); /* Epilogue header */
    heap_listp += (2 * WSIZE);
    
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if ((free_listp = extend_heap(CHUNKSIZE/WSIZE)) == NULL)
        return -1;
    
    /* initialize free list */
    PUT_NEXT_BLKP(free_listp, (void*) 0);
    PUT_PREV_BLKP(free_listp, (void*) 0); 
    heap_listp = free_listp;
    mm_checkheap(1); 
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    /* Ignore spurious request */
    if (size == 0)
         return NULL;

    /* Adjust block size to include overhead and alignment request. */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) 
    {
        place(bp, asize); /* insert the new block and split up if necessary */
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free
 */
void mm_free(void *bp)
{   // use LIFO to insert freed block
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    /* update the free list and join free blocks if possible */
    char* old_free = free_listp;
    free_listp = coalesce(bp);
    PUT_NEXT_BLKP(free_listp, old_free);
    PUT_PREV_BLKP(free_listp, (void*) 0); 
    PUT_PREV_BLKP(old_free, free_listp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    return ptr;
}


static void* extend_heap(size_t words) 
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0)); /* Free block header */
    PUT(FTRP(bp), PACK(size, 0)); /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

static void* find_fit(size_t asize)
{   /* First-fit search through the free list */ 
    char* bp;
    for (bp = free_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_FREE_BLKP(bp))
    {
        if (asize <= GET_SIZE(HDRP(bp)))
            return bp;
    }
    return NULL; /* No fit */
}

static void place(void* bp, size_t asize) 
{
    size_t csize = GET_SIZE(HDRP(bp));   

    /* if more than double blocks left, split the left blocks */
    /* new free blocks are inserted to the head of free list */
    if ((csize - asize) >= (2 * DSIZE)) 
    { 
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));

        /* update the free list */
        char* old_free = free_listp;
        free_listp = bp;
        PUT_NEXT_BLKP(free_listp, old_free);
        PUT_PREV_BLKP(free_listp, (void*) 0); 
        PUT_PREV_BLKP(old_free, free_listp); 
    }
    else 
    { 
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
} 

static void* coalesce(void* bp)
{   /* join the block bp with adjacent blocks */

    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {            /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) {      /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
    }

    else if (!prev_alloc && next_alloc) {      /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else {                                     /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
            GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/*
 * mm_checkheap - Implemented simply in terms of mm_malloc and mm_free
 */ 
void mm_checkheap(int verbose)
{
    char *bp = heap_listp;

    if (verbose)
        printf("\nHeap (%p):\n", heap_listp);

    if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp)))
        printf("Bad prologue header\n");
    checkblock(heap_listp);

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (verbose) 
            printblock(bp);
        checkblock(bp);
    }

    if (verbose)
        printblock(bp);
    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
        printf("Bad epilogue header\n");
}
 
static void checkblock(void *bp) 
{
    if ((size_t)bp % 8)
        printf("Error: %p is not doubleword aligned\n", bp);
    if (GET(HDRP(bp)) != GET(FTRP(bp)))
        printf("Error: header does not match footer\n");
}

static void printblock(void *bp) 
{
    size_t hsize, halloc, fsize, falloc;

    mm_checkheap(0);
    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));  
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));  

    if (hsize == 0) {
        printf("%p: EOL\n", bp);
        return;
    }

    printf("%p: header: [%ld:%c] footer: [%ld:%c]\n", bp, 
        hsize, (halloc ? 'a' : 'f'), 
        fsize, (falloc ? 'a' : 'f')); 
}
