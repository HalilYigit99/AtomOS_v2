#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*

    @return Pointer to the allocated memory block.
    @note This function allocates a single memory block of size PAGE_SIZE ( 4096 ).

*/
void* pmm_alloc_block();

/*

    @param count Number of blocks to allocate.
    @return Pointer to the allocated first memory block.
    @note This function allocates multiple memory blocks, each of size PAGE_SIZE ( 4096 ).
    @note The total size allocated is count * PAGE_SIZE.

*/
void* pmm_alloc_blocks(size_t count);

/*

    @param block Pointer to the memory block to free.
    @note This function frees a single memory block that was previously allocated by pmm_alloc_block().

*/
void pmm_free_block(void* block);

/*

    @param blocks Pointer to the first memory block to free.
    @param count Number of blocks to free.
    @note This function frees multiple memory blocks that were previously allocated by pmm_alloc_blocks().

*/
void pmm_free_blocks(void* blocks, size_t count);

#ifdef __cplusplus
}
#endif