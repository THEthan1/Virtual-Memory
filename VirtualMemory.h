#pragma once

#include "MemoryConstants.h"

/*
 * Initialize the virtual memory.
 */
void VMinitialize();

/* Reads a word from the given virtual address_arr
 * and puts its content in *value.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address_arr cannot be mapped to a physical
 * address_arr for any reason)
 */
int VMread(uint64_t virtualAddress, word_t* value);

/* Writes a word to the given virtual address_arr.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address_arr cannot be mapped to a physical
 * address_arr for any reason)
 */
int VMwrite(uint64_t virtualAddress, word_t value);
