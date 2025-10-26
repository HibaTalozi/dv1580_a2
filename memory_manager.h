#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <stddef.h> // For size_t

// Helps C++ compilers to handle C header files
#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * Initializes the memory manager with a specified size of memory pool.
     * The memory pool could be any data structure, for instance, a large array
     * or a similar contiguous block of memory.
     *
     * @param size The size of the memory pool to initialize.
     */
    void mem_init(size_t size);

    /**
     * Allocates a block of memory of the specified size. This function finds a
     * suitable block in the pool, marks it as allocated, and returns a pointer
     * to the start of the allocated block.
     *
     * @param size The size of the memory block to allocate.
     * @return A pointer to the allocated memory block, or NULL if allocation fails.
     */
    void *mem_alloc(size_t size);

    /**
     * Frees the specified block of memory. This function marks the block as free
     * within the memory manager's data structure.
     *
     * @param block A pointer to the memory block to free.
     */
    void mem_free(void *block);

    /**
     * Changes the size of an existing memory block, possibly moving it to accommodate
     * the new size. It may also shrink the block if the new size is smaller than the current size.
     *
     * @param block A pointer to the memory block to resize.
     * @param size The new size of the memory block.
     * @return A pointer to the resized memory block, or NULL if the resizing fails.
     */
    void *mem_resize(void *block, size_t size);

    /**
     * Frees up the entire memory pool that was initially allocated by mem_init.
     * This function should be called to clean up the memory manager resources before
     * the program terminates or when the memory manager is no longer needed.
     */
    void mem_deinit();

#ifdef __cplusplus
}
#endif

#endif // MEMORY_MANAGER_H