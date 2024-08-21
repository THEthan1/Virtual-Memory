//
// Created by ethan.glick on 6/12/23.
//

#include "VirtualMemory.h"
#include "PhysicalMemory.h"

#define ABS(x) (x < 0 ? -x : x)
#define MIN(abs) (NUM_PAGES - abs < abs ? NUM_PAGES - abs : abs)

void createPageAtIndex(uint64_t index) {
    for (uint64_t i = 0; i < PAGE_SIZE; i++)
        PMwrite(index * PAGE_SIZE + i, 0);
}

/*
 * Initialize the virtual memory.
 */
void VMinitialize() {
    createPageAtIndex(0);
}

uint64_t removeOffsetBits(uint64_t *virtualAddress, uint64_t offset_width) {
    uint64_t mask = (1LL << offset_width) - 1; // get rightmost bits
    uint64_t offset = *virtualAddress & mask;
    *virtualAddress = *virtualAddress >> offset_width;
    return offset;
}

void buildArrayFromInt(uint64_t value, uint64_t* arr, uint64_t arr_size) {
    double offset_base_val = (VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH) / (double) TABLES_DEPTH;
    uint64_t table_offset_width = CEIL(offset_base_val);
    uint64_t mask = (1LL << table_offset_width) - 1; // get rightmost bits
    for (uint64_t i = arr_size; i > 0; i--) { // note i-1 in array index []
        arr[i-1] = value & mask;
        value >>= table_offset_width;
    }
}

bool frameIsEmpty(uint64_t physical_frame_address){
    word_t value;
    for (uint64_t i = 0; i < PAGE_SIZE; i++){
        PMread(physical_frame_address + i, &value);
        if (value != 0)
            return false;
    }

    return true;
}

struct Victim {
    uint64_t address;
    uint64_t frame_index;
    uint64_t parent_frame_index;
    uint64_t offset_in_parent;
};

uint64_t searchTableForFrame(uint64_t main_address, uint64_t safe_index, uint64_t cur_address,
                             uint64_t cur_frame_index, uint64_t prev_frame_index, uint64_t *max_dist,
                             Victim *victim, uint64_t depth, uint64_t *max_frame_index) {
    double offset_base_val = (VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH) / (double) TABLES_DEPTH;
    uint64_t table_offset_width = CEIL(offset_base_val);
    if (cur_frame_index > *max_frame_index)
        *max_frame_index = cur_frame_index;

    if(depth == TABLES_DEPTH + 1) {
        uint64_t val = main_address - cur_address;
        uint64_t abs = ABS(val);
        uint64_t distance = MIN(abs);

        if(distance > *max_dist) {
            *max_dist = distance;
            victim->address = cur_address;
            victim->frame_index = cur_frame_index;
            victim->parent_frame_index = prev_frame_index;
            victim->offset_in_parent = cur_address & ((1LL << table_offset_width) - 1);
        }

        return 0;
    }

    word_t next_frame_index = 0;
    uint64_t next_address;

    for (uint64_t i = 0; i < PAGE_SIZE; i++){
        PMread(cur_frame_index * PAGE_SIZE + i, &next_frame_index);
        if(next_frame_index == 0)
            continue;

        next_address = (cur_address << table_offset_width) | i;
        if((uint64_t) next_frame_index != safe_index && depth < TABLES_DEPTH
                            && frameIsEmpty(next_frame_index * PAGE_SIZE)){
            //successfully recovered an empty frame_index
            PMwrite(cur_frame_index * PAGE_SIZE + i, 0);
            //detach from the current frame_index - its old parent
            return next_frame_index;
        }

        uint64_t empty_index = searchTableForFrame(main_address, safe_index, next_address, next_frame_index,
                                                   cur_frame_index, max_dist, victim, depth + 1, max_frame_index);

        if(empty_index != 0)
            return empty_index;
    }

    return 0;

}

/**
 * return a index for a frame_index that was already detached from its parent stored in disk and ready to be used
 * @param page_swapped_in_virtual_address
 * @return
 */
uint64_t getEmptyFrameIndex(uint64_t address, uint64_t safe_index){
    uint64_t max_frame_index = 0, max_dist = 0;
    Victim victim = {0,0,0,0};
    uint64_t empty_index = searchTableForFrame(address, safe_index, 0, 0, 0,
                                                 &max_dist, &victim, 1, &max_frame_index);

    if(empty_index != 0 && empty_index != safe_index)
        return empty_index;

    if (max_frame_index + 1 < NUM_FRAMES)
        return max_frame_index + 1;

    PMwrite(victim.parent_frame_index * PAGE_SIZE + victim.offset_in_parent, 0);
    PMevict(victim.frame_index, victim.address);

    return victim.frame_index;
}

void connectNewTableNode(uint64_t parent_frame_index, uint64_t frame_offset,
                         uint64_t new_empty_frame_index) {
    PMwrite(parent_frame_index * PAGE_SIZE + frame_offset,
            (word_t)new_empty_frame_index);//connect the new frame_index to its new father.

    createPageAtIndex(new_empty_frame_index);
}

void connectPageToTable(uint64_t parent_frame_index, uint64_t frame_offset,
                         uint64_t new_empty_frame_index, uint64_t page_virtual_address) {

    PMwrite(parent_frame_index * PAGE_SIZE + frame_offset,
            (word_t)new_empty_frame_index);//connect the new frame_index to its new father.

    PMrestore(new_empty_frame_index, page_virtual_address);
}

uint64_t getPhysicalAddress(uint64_t virtualAddress) {
    uint64_t address_arr[TABLES_DEPTH];
    uint64_t offset = removeOffsetBits(&virtualAddress, OFFSET_WIDTH);
    buildArrayFromInt(virtualAddress, address_arr, TABLES_DEPTH);

    double offset_base_val = (VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH) / (double) TABLES_DEPTH;
    uint64_t table_offset_width = CEIL(offset_base_val);

    word_t value = 0;
    uint64_t cur_address = 0;
    for (int i = 0; i < TABLES_DEPTH; i++) {
        cur_address <<= table_offset_width;
        cur_address |= address_arr[i];

        uint64_t parent_frame_index = value;
        PMread(parent_frame_index * PAGE_SIZE + address_arr[i], &value);

        if (!value) {
            uint64_t new_frame_index = getEmptyFrameIndex(virtualAddress, parent_frame_index);
            if (i == TABLES_DEPTH-1) {
                connectPageToTable(parent_frame_index, address_arr[i], new_frame_index, cur_address);
            }
            else {
                connectNewTableNode(parent_frame_index, address_arr[i], new_frame_index);
            }

            value = (word_t) new_frame_index;
        }
    }

    return value * PAGE_SIZE + offset;
}

/* Reads a word from the given virtual address
 * and puts its content in *value.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMread(uint64_t virtualAddress, word_t* value) {
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) return 0;
    uint64_t address = getPhysicalAddress(virtualAddress);

    PMread(address, value);
    return 1;
}

/* Writes a word to the given virtual address.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMwrite(uint64_t virtualAddress, word_t value) {
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) return 0;
    uint64_t address = getPhysicalAddress(virtualAddress);

    PMwrite(address, value);
    return 1;
}
