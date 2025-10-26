#include "memory_manager.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>

// Simple thread-safe memory allocator using a fixed-size pool

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

// Initialize memory pool
void mem_init(size_t size)
{
    if (size == 0) size = 1;
    pthread_mutex_lock(&mem_mutex);
    if (mem_pool_start) {
        free(mem_pool_start);
        mem_pool_start = NULL;
        mem_pool_total_size = 0;
        free_list_head = NULL;
    }

    mem_pool_total_size = ALIGN(size);
    mem_pool_start = malloc(mem_pool_total_size);

    if (!mem_pool_start) {
        fprintf(stderr, "ERROR: mem_init failed\n");
        pthread_mutex_unlock(&mem_mutex);
        return;
    }

    // Create first free block
    free_list_head = (Block *)mem_pool_start;
    free_list_head->size = mem_pool_total_size;
    free_list_head->is_free = 1;
    free_list_head->next = NULL;

    pthread_mutex_unlock(&mem_mutex);
}


// Split a free block if there is leftover space
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

// Allocate memory (first-fit)
void *mem_alloc(size_t size)
{
    if (size == 0) size = 1;

    pthread_mutex_lock(&mem_mutex);

    size_t aligned = ALIGN(size);
    size_t required = ALIGN(aligned + HEADER_SIZE);

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

// Rebuild free list and merge adjacent free blocks
static void rebuild_free_list_and_coalesce(void)
{
    if (!mem_pool_start) return;

    Block *curr = (Block *)mem_pool_start;
    Block *prev_free = NULL;
    free_list_head = NULL;

    char *pool_start = (char *)mem_pool_start;
    char *pool_end = pool_start + mem_pool_total_size;

    while ((char *)curr + HEADER_SIZE <= pool_end) {
        if (curr->size == 0 || (char *)curr + curr->size > pool_end) {
            fprintf(stderr, "ERROR: pool corruption at %p (size=%zu)\n",
                    (void *)curr, curr->size);
            break;
        }

        if (curr->is_free) {
            if (prev_free && (char *)prev_free + prev_free->size == (char *)curr) {
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

        Block *next = next_physical_block(curr);
        if ((char *)next <= (char *)curr || (char *)next > pool_end)
            break;

        curr = next;
    }
}

// Free a previously allocated block
void mem_free(void *ptr)
{
    if (!ptr) return;

    pthread_mutex_lock(&mem_mutex);

    if ((char *)ptr < (char *)mem_pool_start ||
        (char *)ptr >= (char *)mem_pool_start + mem_pool_total_size) {
        fprintf(stderr, "WARNING: mem_free() called on pointer outside pool (%p)\n", ptr);
        pthread_mutex_unlock(&mem_mutex);
        return;
    }
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



// Resize (like realloc)
void *mem_resize(void *block, size_t size)
{
    if (!block) return mem_alloc(size);
    if (size == 0) {
        mem_free(block);
        return NULL;
    }

    pthread_mutex_lock(&mem_mutex);

    Block *old_hdr = BLOCK_FROM_USER_PTR(block);
    if (old_hdr->is_free) {
        fprintf(stderr, "WARNING: resize() called on freed block (%p)\n", block);
        pthread_mutex_unlock(&mem_mutex);
        return NULL;
    }

    size_t old_total = old_hdr->size;
    size_t old_user = (old_total >= HEADER_SIZE) ? old_total - HEADER_SIZE : 0;
    size_t new_required = ALIGN(size) + HEADER_SIZE;

    if (new_required <= old_total) {
        pthread_mutex_unlock(&mem_mutex);
        return block;
    }

    pthread_mutex_unlock(&mem_mutex);

    void *new_block = mem_alloc(size);
    if (!new_block) return NULL;

    memcpy(new_block, block, old_user < size ? old_user : size);
    mem_free(block);
    return new_block;
}

// Deinitialize and release memory pool
void mem_deinit(void)
{
    pthread_mutex_lock(&mem_mutex);

    if (mem_pool_start) {
        free(mem_pool_start);
        mem_pool_start = NULL;
    }

    mem_pool_total_size = 0;
    free_list_head = NULL;

    pthread_mutex_unlock(&mem_mutex);
}
