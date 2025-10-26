#include "memory_manager.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>

//Simple thread-safe allocator using a pre-allocated memory pool.

#define ALIGNMENT 8
#define ALIGN(sz) (((sz) + (ALIGNMENT - 1)) & ~((ALIGNMENT - 1)))

typedef struct Block {
    size_t size;        
    int is_free;        // 1 = free, 0 = used
    struct Block *next; 
} Block;

#define HEADER_SIZE ALIGN(sizeof(Block))

static void *mem_pool_start = NULL;
static size_t mem_pool_total_size = 0;
static Block *free_list_head = NULL;
static pthread_mutex_t mem_mutex = PTHREAD_MUTEX_INITIALIZER;

#define USER_PTR_FROM_BLOCK(b) ((void *)((char *)(b) + HEADER_SIZE))
#define BLOCK_FROM_USER_PTR(p) ((Block *)((char *)(p) - HEADER_SIZE))

static inline Block *next_physical_block(Block *b) {
    return (Block *)((char *)b + b->size);
}

//Initialize the memory pool
void mem_init(size_t size)
{
    pthread_mutex_lock(&mem_mutex);

    if (mem_pool_start) {
        free(mem_pool_start);
        mem_pool_start = NULL;
        mem_pool_total_size = 0;
        free_list_head = NULL;
    }

    size_t total_size = ALIGN(size + 4 * HEADER_SIZE);
    mem_pool_start = malloc(total_size);
    if (!mem_pool_start) {
        fprintf(stderr, "ERROR: mem_init failed\n");
        pthread_mutex_unlock(&mem_mutex);
        return;
    }

    mem_pool_total_size = total_size;
    free_list_head = (Block *)mem_pool_start;
    free_list_head->size = total_size;
    free_list_head->is_free = 1;
    free_list_head->next = NULL;

    pthread_mutex_unlock(&mem_mutex);
}

//Split a block if it's large enough
static Block *split_block(Block *curr, Block *prev, size_t required_size)
{
    size_t leftover = curr->size - required_size;

    if (leftover >= HEADER_SIZE + ALIGNMENT) {
        Block *remainder = (Block *)((char *)curr + required_size);
        remainder->size = leftover;
        remainder->is_free = 1;
        remainder->next = curr->next;

        curr->size = required_size;
        curr->is_free = 0;

        if (prev)
            prev->next = remainder;
        else
            free_list_head = remainder;

        return curr;
    }

    curr->is_free = 0;
    if (prev)
        prev->next = curr->next;
    else
        free_list_head = curr->next;

    return curr;
}

//Allocate memory (first-fit)
void *mem_alloc(size_t size)
{
    if (size == 0) size = 1;

    pthread_mutex_lock(&mem_mutex);

    size_t aligned = ALIGN(size);
    size_t required = aligned + HEADER_SIZE;

    Block *curr = free_list_head;
    Block *prev = NULL;

    while (curr) {
        if (curr->is_free && curr->size >= required) {
            Block *allocated = split_block(curr, prev, required);
            void *ptr = USER_PTR_FROM_BLOCK(allocated);
            pthread_mutex_unlock(&mem_mutex);
            return ptr;
        }
        prev = curr;
        curr = curr->next;
    }

    pthread_mutex_unlock(&mem_mutex);
    return NULL;
}

//Merge adjacent free blocks and rebuild free list
static void rebuild_free_list_and_coalesce(void)
{
    Block *curr = (Block *)mem_pool_start;
    Block *prev_free = NULL;
    free_list_head = NULL;

    while ((char *)curr < (char *)mem_pool_start + mem_pool_total_size) {
        if ((char *)curr + curr->size >
            (char *)mem_pool_start + mem_pool_total_size) {
            fprintf(stderr, "ERROR: pool corruption\n");
            break;
        }

        if (curr->is_free) {
            if (prev_free && 
                (char *)prev_free + prev_free->size == (char *)curr) {
                prev_free->size += curr->size;
            } else {
                if (prev_free)
                    prev_free->next = curr;
                else
                    free_list_head = curr;
                prev_free = curr;
                prev_free->next = NULL;
            }
        }
        curr = next_physical_block(curr);
    }
}

//Free a block
void mem_free(void *ptr)
{
    if (!ptr) return;

    pthread_mutex_lock(&mem_mutex);

    Block *hdr = BLOCK_FROM_USER_PTR(ptr);
    if (hdr->is_free) {
        fprintf(stderr, "WARNING: double free ignored (%p)\n", ptr);
        pthread_mutex_unlock(&mem_mutex);
        return;
    }

    hdr->is_free = 1;
    rebuild_free_list_and_coalesce();

    pthread_mutex_unlock(&mem_mutex);
}

//Resize a block
void *mem_resize(void *block, size_t size)
{
    if (!block) return mem_alloc(size);
    if (size == 0) {
        mem_free(block);
        return NULL;
    }

    pthread_mutex_lock(&mem_mutex);

    Block *old_hdr = BLOCK_FROM_USER_PTR(block);
    size_t old_total = old_hdr->size;
    size_t old_user = (old_total >= HEADER_SIZE)
                    ? old_total - HEADER_SIZE : 0;

    size_t new_required = ALIGN(size) + HEADER_SIZE;

    if (new_required <= old_total) {
        pthread_mutex_unlock(&mem_mutex);
        return block;
    }

    pthread_mutex_unlock(&mem_mutex);

    void *new_block = mem_alloc(size);
    if (!new_block) return NULL;

    size_t copy_size = (old_user < size) ? old_user : size;
    memcpy(new_block, block, copy_size);

    mem_free(block);
    return new_block;
}

// Release memory pool
void mem_deinit(void)
{
    pthread_mutex_lock(&mem_mutex);

    if (mem_pool_start) {
        free(mem_pool_start);
        mem_pool_start = NULL;
        mem_pool_total_size = 0;
        free_list_head = NULL;
    }

    pthread_mutex_unlock(&mem_mutex);
    pthread_mutex_destroy(&mem_mutex);
}
