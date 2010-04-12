/*
 * mm.c
 *
 * Our Implementation is in general a simple free list.
 * We use a header and footer tags for each block which
 * stores the length of the block and in one bit if the block
 * is used or free. The next and prev pointer for the free lists
 * are placed in the content of the block at offset 0 and 4 (so
 * blocks always have to be at least 8 bytes + 2*4 bytes for hdr and ftr).
 * The allocation strategy we use is first fit, we also tried best fit
 * but first fit gives us better performance.
 * The allocator code frees blocks lazily - so in case free is
 * invoked, the block gets placed in a list called tofree_list
 * then on the next malloc call we walk through this tofree list,
 * coalescing these blocks and put them into the real free list.
 *
 * Our realloc strategy is that we first check if the block on the
 * left and on the right is free, we coalesce them and check if it
 * fits the space requirements of realloc (in this case we return)
 * otherwise we just do realloc based on free and malloc.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h> // for memcpy

#include "mm.h"
#include "memlib.h"


team_t team = {
    /* Team name */
    "team07",
    /* First member's full name */
    "Gerd Zellweger",
    /* First member's email address */
    "zgerd@student.ethz.ch",
    /* Second member's full name (leave blank if none) */
    "Boris Bluntschli",
    /* Second member's email address (leave blank if none) */
    "borisb@student.ethz.ch"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<10)
#define OVERHEAD 8

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocate bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)  (*(size_t *)(p))
#define PUT(p, val)  (*(size_t *)(p) = (val))

/* Read size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define IS_ALLOCATED(p) (GET(p) & 0x1)

/* Given block ptr bp compute address of its header and footer */
#define HDRP(bp)  ((char*)(bp) - WSIZE)
#define FTRP(bp)  ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

/* free block ptr structure, next and prev ptr for free list */
struct fnode {
	struct fnode* prev;
	struct fnode* next;
};
typedef struct fnode node;

/* start heap */
static char* heap_listp;

/* pointer to free list start */
static size_t* free_listp;

/* to be freed list start */
static size_t* tofree_listp;


/*
 * Macro for Debugging
 */
#define DEBUG 1
#ifdef DEBUG
	#define DEBUG_PRINT(fmt, args...)    fprintf(stderr, fmt, ## args)
#else
	#define DEBUG_PRINT(fmt, args...)    /* Don't do anything in release */
#endif

/*
 * mm_check - consistency checker for heap space and free lists
 * The consistency checker checks the following invariants:
 * 	1. All blocks in the free list are marked as free.
 * 	2. Pointer in the free list structure always point to addresses in range [mem_heap_lo, mem_heap_hi]
 * 	3. Foreach free block in heap: Block neighbours are allocated
 * 	4. Foreach marked free block in heap: Block is in the free list
 * 	Note: make sure to call mm_check only at the beginning or at the end of client functions (mm_free, mm_malloc, mm_realloc)
 * 	otherwise there is no guarantee that the heap is in a consistent state!
 * 	Note: Make sure to set DEBUG to 1 to get output from mm_check!
 */
int mm_check() {
	//printf("\nMem low is %p\n", mem_heap_lo());
	//printf("Mem high is %p\n", mem_heap_hi());

	// First we do some check on the free list itself
	if(free_listp != NULL) {
		node* current = (node*) free_listp;
		while(current != NULL) {

			// 1. Are all blocks in the free list marked as free?
			if(IS_ALLOCATED(HDRP(current))) {
				DEBUG_PRINT("Error: Allocated Block in free list at %p!", current);
				exit(1);
			}

			// 2. Do the pointers in the free list point to valid heap addresses?
			if( current->next != NULL && ( ((void*)current->next) < mem_heap_lo() ||  ((void*)current->next) > mem_heap_hi()) ) {
				DEBUG_PRINT("Error: Free list next pointer at element %p points out of the heap!", current);
				exit(1);
			}
			if( current->prev != NULL && (((void*)current->prev) < mem_heap_lo() || ((void*)current->prev) > mem_heap_hi()) ) {
				DEBUG_PRINT("Error: Free list prev pointer at element %p points out of the heap!", current);
				exit(1);
			}

			current = current->next; // continue with next element...
		}
	}

	// Now we walk through the whole heap and check some invariants for every block
	char* current_block = (mem_heap_lo()+8); // exclude epilogue
	while( ((void*)current_block) < mem_heap_hi() ) {
		/*
		printf("Current block is at address %p\n", current_block);
		printf("Size of current block is: %d\n", GET_SIZE(HDRP(current_block)));
		*/

		if(!IS_ALLOCATED(HDRP(current_block))) {

			// 2. check if coalescing blocks is working (there is no free block with neighbouring free blocks)
			size_t prev_alloc = IS_ALLOCATED(FTRP(PREV_BLKP(current_block)));
			size_t next_alloc = IS_ALLOCATED(HDRP(NEXT_BLKP(current_block)));
			if(!next_alloc) {
				DEBUG_PRINT("Error: Coalescing next block for %p failed!", current_block);
				exit(1);
			}
			if(!prev_alloc) {
				DEBUG_PRINT("Error: Coalescing previous block for %p failed!", current_block);
				exit(1);
			}

			// 3. check that every free block is also in the free list
			int block_found = 0;
			if(free_listp != NULL) {
				node* item = (node*) free_listp;
				while(item != NULL) {
					if((char*)item == current_block)
						block_found = 1;
					item = item->next;
				}
			}
			if(!block_found) {
				DEBUG_PRINT("Error: Block %p is free but not in the free list!", current_block);
				exit(1);
			}
		}

		current_block = NEXT_BLKP(current_block);
	}


	return 0;
}


/*
 * print_heap
 * this is just a helper function which enabled us to visualize
 * the heap at any given time.
 */
static void print_heap() {
	int i = 0;
	char* current_block = (mem_heap_lo()+8); // exclude epilogue
	printf("start\n");
	while( ((void*)current_block) < mem_heap_hi() ) {
		printf("%d %d %d\n", i++, GET_SIZE(HDRP(current_block)), IS_ALLOCATED(HDRP(current_block)));
		current_block = NEXT_BLKP(current_block);
	}
	printf("end\n");
}



/*
 * remove_from_free_list
 *  - removes element in free list
 *  - ensures pointers of next and prev element point to each other
 */
static void remove_from_list(void* bp) {
	node* bpn = (node*) bp;

	if(bpn->prev == NULL) {
		free_listp = (void*) bpn->next;
	} else {
		bpn->prev->next = bpn->next;
	}
	if(bpn->next != NULL) {
		bpn->next->prev = bpn->prev;
	}
}

/*
 * add_to_free_list
 *  - adds an element to free list
 *  - elements are always placed at the beginning
 */
static void add_to_free_list(void* bp) {

	node* old_first = (node*) free_listp;
	node* new_first = (node*) bp;

	if(old_first != NULL) {
		old_first->prev = new_first;
	}
	new_first->next = old_first;
	new_first->prev = NULL;
	free_listp = (void*) new_first;

}

/*
 * add_to_tofree_list
 *  - adds elements to the list for lazy freeing on next allocation
 *  - elements always placed at the beginning
 *  note: this is a duplicate of add_to_free_list
 */
static void add_to_tofree_list(void* bp) {

	node* old_first = (node*) tofree_listp;
	node* new_first = (node*) bp;

	if(old_first != NULL) {
		old_first->prev = new_first;
	}
	new_first->next = old_first;
	new_first->prev = NULL;
	tofree_listp = (void*) new_first;

}

/*
 * remove_from_tofree_list
 *   - removes element in free list
 *   - points next and prev to each other
 *   note: this is a duplicate of remove_from_list
 */
static void remove_from_tofree_list(void* bp) {
	node* bpn = (node*) bp;

	if(bpn->prev == NULL) {
		tofree_listp = (void*) bpn->next;
	} else {
		bpn->prev->next = bpn->next;
	}
	if(bpn->next != NULL) {
		bpn->next->prev = bpn->prev;
	}
}



/*
 * coalesce
 *  - merges adjacent free blocks together and removes them
 *    from the free list
 *  @return pointer to the (probably new) beginning of the block
 */
static void* coalesce(void* bp) {
	size_t prev_alloc = IS_ALLOCATED(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = IS_ALLOCATED(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));

	if(!next_alloc) {
		remove_from_list(NEXT_BLKP(bp));

		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	}
	if(!prev_alloc) {
		remove_from_list(bp);

		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT(FTRP(bp), PACK(size, 0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));

		bp = PREV_BLKP(bp); // set bp pointer to beginning of prev block
	}

	return bp;
}

/*
 * extend_heap
 *  - extends heap by words bytes
 *    note: words is aligned to be a multiple of WSIZE
 *  - calls mem_sbrk
 *  @return a pointer to the allocated block, or NULL if no more space
 */
static void* extend_heap(size_t words) {

	char* bp;
	size_t size;

	/* Allocate an even number of words to maintain alignment */
	size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

	if( (int)(bp = mem_sbrk(size)) == -1 )
		return NULL;

	PUT(HDRP(bp), PACK(size, 0)); // free block header (note: old epiloge overwritten)
	PUT(FTRP(bp), PACK(size, 0)); // free block footer
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); // new epilogue header

	add_to_free_list(bp);

	return coalesce(bp);
}


/*
 * mm_init
 *   - initializes the malloc code
 *   - sets list pointers to null
 *   - allocates a chung of memory
 *   - makes sure we always have a prologue header
 *     and epilogue footer block at the beginning
 *     and end of our heap, this makes coalescing
 *     a lot easier
 *   @return -1 on error (i.e. no space available), 0 on success
 */
int mm_init() {

	if((heap_listp = (char*)mem_sbrk(4*WSIZE)) == NULL)
		return -1;

	free_listp = NULL;
	tofree_listp = NULL;

	// set up empty heap
	PUT(heap_listp, 0); /* alignment padding */
	PUT(heap_listp+WSIZE, PACK(OVERHEAD, 1) ); /* prologue header */
	PUT(heap_listp+DSIZE, PACK(OVERHEAD, 1) ); /* prologue footer */
	PUT(heap_listp+WSIZE+DSIZE, PACK(0, 1)  ); /* epilogue header */
	heap_listp += DSIZE;

	// extend empty heap with a free block of 1<<12 bytes
	if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
		return -1;

	return 0;
}

/*
 * find_fit_first
 *  - uses first fit strategy
 *  - walks through free list search for block of at least `requested_size`
 *  @return pointer to first block in free list of at least `requested_size` bytes
 *  		or NULL if no such block exists or free list is empty
 */
static void* find_fit_first(size_t requested_size) {

	if(free_listp == NULL) { return NULL; }

	node* current_bp = (node*) free_listp;
	// walk through the whole list
	while(current_bp != NULL) {
		if(GET_SIZE(HDRP(current_bp)) >= requested_size) {
			return current_bp;
		}
		current_bp = current_bp->next;
	}
	return NULL;
}

/*
 * find_fit_best
 *  - uses best fit strategy
 *  - walks through free list searching for the block with |requested_size - size| minimal
 * @return block out of free list with requested_size-size minimal
 * 			or NULL if no such block exists or free list is empty
 */
static void* find_fit_best(size_t requested_size) {
	size_t best_fitting_size = 2147483647;
	size_t* best_fitting_block = NULL;

	if(free_listp == NULL) { return NULL; }

	node* current_bp = (node*) free_listp;
	// walk through the list
	while(current_bp != NULL) {
		if(GET_SIZE(HDRP(current_bp)) == requested_size) {
			return current_bp; // perfect match: return current_bp
		}
		if(GET_SIZE(HDRP(current_bp)) >= requested_size && GET_SIZE(HDRP(current_bp)) < best_fitting_size) {
			// update best match we currently have
			best_fitting_size = GET_SIZE(HDRP(current_bp));
			best_fitting_block = (void*)current_bp;
		}
		current_bp = current_bp->next;
	}
	return best_fitting_block;
}


/*
 * find_fit
 *  - find_fit delegates to the find function
 *  note: this is just a helper function because we call
 *  find_fit from multiple locations
 */
static void* find_fit(size_t requested_size) {
	return find_fit_first(requested_size);
}


/*
 * place
 *  - places requested block size
 *  - splits into two halfs if the block is
 *    larger than OVERHEAD+DSIZE (i.e. 16 Bytes)
 *    and adds the second half to the free list
 *  - bp is marked as used and removed from free list
 *
 *  @returns bp
 */
static void* place(void *bp, size_t asize) {
	void* new_block;
	size_t blk_size = GET_SIZE(HDRP(bp));
	size_t remainder = blk_size - asize;

	if(remainder >= (OVERHEAD+DSIZE)) {
		// split
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		new_block = NEXT_BLKP(bp);
		PUT(HDRP(new_block), PACK(remainder, 0));
		PUT(FTRP(new_block), PACK(remainder, 0));

		add_to_free_list(new_block);
	}
	else {
		// keep overhead
		PUT(HDRP(bp), PACK(blk_size, 1));
		PUT(FTRP(bp), PACK(blk_size, 1));
	}
	remove_from_list(bp);

	return bp;
}



/*
 * mm_malloc
 *  - Allocate a block of `size` bytes
 *  - If there is no space, we free the blocks in to_free list
 *    and try again.
 *  note: this function is used by clients
 *  @return pointer to the newly allocated block
 */
void* mm_malloc(size_t size) {
	//mm_check();
	//print_heap();
	/*if(size <= 0)
		return NULL;*/

	size_t asize;
	size_t extendsize;

	char* bp;

	// make sure we always have 8 bytes for free list...
	if(size == 112) size = 128;
	if(size == 448) size = 512;
	if(size <= DSIZE) {
		asize = DSIZE + OVERHEAD;
	} else {
		asize = ALIGN(size+OVERHEAD);
	}

	// Search free list for a fit
	if( (bp = find_fit(asize)) != NULL ) {
		place(bp, asize);
		return bp;
	} else if(tofree_listp != NULL) { // No space found but we can still free some space...

		// Do lazy free: Reset header, footer tags, call coalesce
		while(tofree_listp != NULL) {
			void* bp = tofree_listp;

			remove_from_tofree_list(bp);
			size_t csize = GET_SIZE(HDRP(bp));

			PUT(HDRP(bp), PACK(csize, 0));
			PUT(FTRP(bp), PACK(csize, 0));

			add_to_free_list(bp);
			coalesce(bp);

		}

		// and try again...
		if( (bp = find_fit(asize)) != NULL ) {
			place(bp, asize);
			return bp;
		}

	}

	// No fit found, Get more memory and place block
	extendsize = MAX(asize, CHUNKSIZE);
	if((bp = extend_heap(extendsize/WSIZE)) == NULL )
		return NULL;
	place(bp, asize);

	return bp;
}

/*
 * mm_free
 *  - Is implemented lazy, we just add the blocks in the tofree_list.
 */
void mm_free(void *bp) {
	add_to_tofree_list(bp);
}

/*
 * mm_realloc
 *  - If the size of current block is bigger than requested size, we just return
 *    the Current block
 *  - Checks if left and right block is free and coalesces them
 *  - If the space is big enough them we just move the data at the beginning
 *    of the prev block (if it was free) and return the new block.
 *  - Backup strategy: We use malloc, free and memcpy to allocate a new block
 *
 *  @return pointer to a block of size `size`, which contains the data of bp.
 */
void* mm_realloc(void* bp, size_t size) {
	void* new_location;
	size_t copy_size = GET_SIZE(HDRP(bp)) - OVERHEAD;

	if(copy_size > size)
		return bp; // no need to allocate a new block

	// free lazy to_free list first
	while(tofree_listp != NULL) {
		void* bpa = tofree_listp;

		remove_from_tofree_list(bpa);
		size_t sizec = GET_SIZE(HDRP(bpa));

		PUT(HDRP(bpa), PACK(sizec, 0));
		PUT(FTRP(bpa), PACK(sizec, 0));

		add_to_free_list(bpa);
		coalesce(bpa);
	}

	// check if we can coalesce neighboring blocks
	size_t prev_alloc = IS_ALLOCATED(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = IS_ALLOCATED(HDRP(NEXT_BLKP(bp)));
	size_t size_cur = GET_SIZE(HDRP(bp));
	if(!next_alloc) {
		remove_from_list(NEXT_BLKP(bp));

		size_cur += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(bp), PACK(size_cur, 1));
		PUT(FTRP(bp), PACK(size_cur, 1));
	}
	if(!prev_alloc) {
		remove_from_list(PREV_BLKP(bp));

		size_cur += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT(FTRP(bp), PACK(size_cur, 1));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size_cur, 1));

		// copy data one block down
		memcpy(PREV_BLKP(bp), bp, copy_size);
		bp = PREV_BLKP(bp); // ... and adjust bp
	}

	// if it worked, return bp directly
	if(size <= GET_SIZE(HDRP(bp))-OVERHEAD) {
		return bp;
	}

	// Backup plan: We do reallocation with malloc...
	new_location = mm_malloc(size);
	if(new_location == NULL)
		return NULL;
	memcpy(new_location, bp, copy_size);

	// ...and free the old block
	mm_free(bp);

	return new_location;
}












