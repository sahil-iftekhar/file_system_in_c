#include "vsfsck.h"

int get_bit(uint8_t *bitmap, uint32_t index) {
    return (bitmap[index / 8] >> (index % 8)) & 1;
}

void set_bit(uint8_t *bitmap, uint32_t index, int value) {
    if (value)
        bitmap[index / 8] |= (1 << (index % 8));
    else
        bitmap[index / 8] &= ~(1 << (index % 8));
}