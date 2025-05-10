#include <stdio.h>
#include <string.h>
#include "vsfsck.h"

void check_superblock(Superblock *sb, int fix) {
    printf("Checking superblock...\n");
    if (sb->magic != MAGIC_NUMBER) {
        printf("Error: Invalid magic number (0x%04x, expected 0x%04x)\n", sb->magic, MAGIC_NUMBER);
        errors_found++;
        if (fix) {
            sb->magic = MAGIC_NUMBER;
            printf("Fixed: Set magic number to 0x%04x\n", MAGIC_NUMBER);
            fixes_applied++;
        }
    }
    if (sb->block_size != BLOCK_SIZE) {
        printf("Error: Invalid block size (%u, expected %u)\n", sb->block_size, BLOCK_SIZE);
        errors_found++;
        if (fix) {
            sb->block_size = BLOCK_SIZE;
            printf("Fixed: Set block size to %u\n", BLOCK_SIZE);
            fixes_applied++;
        }
    }
    if (sb->total_blocks != TOTAL_BLOCKS) {
        printf("Error: Invalid total blocks (%u, expected %u)\n", sb->total_blocks, TOTAL_BLOCKS);
        errors_found++;
        if (fix) {
            sb->total_blocks = TOTAL_BLOCKS;
            printf("Fixed: Set total blocks to %u\n", TOTAL_BLOCKS);
            fixes_applied++;
        }
    }
    if (sb->inode_bitmap_block != 1) {
        printf("Error: Invalid inode bitmap block (%u, expected 1)\n", sb->inode_bitmap_block);
        errors_found++;
        if (fix) {
            sb->inode_bitmap_block = 1;
            printf("Fixed: Set inode bitmap block to 1\n");
            fixes_applied++;
        }
    }
    if (sb->data_bitmap_block != 2) {
        printf("Error: Invalid data bitmap block (%u, expected 2)\n", sb->data_bitmap_block);
        errors_found++;
        if (fix) {
            sb->data_bitmap_block = 2;
            printf("Fixed: Set data bitmap block to 2\n");
            fixes_applied++;
        }
    }
    if (sb->inode_table_start != INODE_TABLE_START) {
        printf("Error: Invalid inode table start (%u, expected %u)\n", sb->inode_table_start, INODE_TABLE_START);
        errors_found++;
        if (fix) {
            sb->inode_table_start = INODE_TABLE_START;
            printf("Fixed: Set inode table start to %u\n", INODE_TABLE_START);
            fixes_applied++;
        }
    }
    if (sb->first_data_block != DATA_BLOCK_START) {
        printf("Error: Invalid first data block (%u, expected %u)\n", sb->first_data_block, DATA_BLOCK_START);
        errors_found++;
        if (fix) {
            sb->first_data_block = DATA_BLOCK_START;
            printf("Fixed: Set first data block to %u\n", DATA_BLOCK_START);
            fixes_applied++;
        }
    }
    if (sb->inode_size != INODE_SIZE) {
        printf("Error: Invalid inode size (%u, expected %u)\n", sb->inode_size, INODE_SIZE);
        errors_found++;
        if (fix) {
            sb->inode_size = INODE_SIZE;
            printf("Fixed: Set inode size to %u\n", INODE_SIZE);
            fixes_applied++;
        }
    }
    if (sb->inode_count != INODE_COUNT) {
        printf("Error: Invalid inode count (%u, expected %u)\n", sb->inode_count, INODE_COUNT);
        errors_found++;
        if (fix) {
            sb->inode_count = INODE_COUNT;
            printf("Fixed: Set inode count to %u\n", INODE_COUNT);
            fixes_applied++;
        }
    }
}

void check_inode_bitmap(uint8_t *inode_bitmap, Inode *inode_table, int fix) {
    printf("Checking inode bitmap...\n");
    for (uint32_t i = 0; i < INODE_COUNT; i++) {
        Inode *inode = &inode_table[i];
        int bitmap_bit = get_bit(inode_bitmap, i);
        int is_valid = (inode->links > 0 && inode->dtime == 0);
        if (bitmap_bit && !is_valid) {
            printf("Error: Inode %u marked used in bitmap but invalid (links=%u, dtime=%u)\n",
                   i, inode->links, inode->dtime);
            errors_found++;
            if (fix) {
                set_bit(inode_bitmap, i, 0);
                printf("Fixed: Cleared inode %u in bitmap\n", i);
                fixes_applied++;
            }
        } else if (!bitmap_bit && is_valid) {
            printf("Error: Inode %u valid but not marked in bitmap\n", i);
            errors_found++;
            if (fix) {
                set_bit(inode_bitmap, i, 1);
                printf("Fixed: Set inode %u in bitmap\n", i);
                fixes_applied++;
            }
        }
    }
}

void check_data_bitmap(uint8_t *data_bitmap, Inode *inode_table, int fix) {
    printf("Checking data bitmap...\n");
    uint8_t computed_bitmap[BITMAP_SIZE] = {0};
    uint32_t block_counts[TOTAL_BLOCKS] = {0};
    uint32_t first_inode_for_block[TOTAL_BLOCKS] = {0}; 
    
    for (uint32_t i = 0; i < DATA_BLOCK_START; i++) {
        set_bit(computed_bitmap, i, 1);
        block_counts[i]++;
    }

    for (uint32_t i = 0; i < INODE_COUNT; i++) {
        Inode *inode = &inode_table[i];
        if (inode->links == 0 || inode->dtime != 0) continue;
        if (inode->direct[0] >= DATA_BLOCK_START && inode->direct[0] < TOTAL_BLOCKS) {
            block_counts[inode->direct[0]]++;
            if (block_counts[inode->direct[0]] == 1) {
                first_inode_for_block[inode->direct[0]] = i; 
            }
        }
    }


    for (uint32_t i = DATA_BLOCK_START; i < TOTAL_BLOCKS; i++) {
        if (block_counts[i] > 1) {
            printf("Error: Data block %u referenced %u times\n", i, block_counts[i]);
            errors_found++;
            if (fix) {
                for (uint32_t j = 0; j < INODE_COUNT; j++) {
                    Inode *inode = &inode_table[j];
                    if (inode->links == 0 || inode->dtime != 0) continue;
                    if (inode->direct[0] == i && j != first_inode_for_block[i]) {
                        inode->direct[0] = 0;
                        inode->blocks = 0;
                        inode->size = 0; 
                        printf("Fixed: Cleared duplicate reference to block %u in inode %u\n", i, j);
                        fixes_applied++;
                    }
                }
                block_counts[i] = 1; 
            }
        }
    }


    memset(computed_bitmap, 0, BITMAP_SIZE);
    for (uint32_t i = 0; i < DATA_BLOCK_START; i++) {
        set_bit(computed_bitmap, i, 1);
    }
    for (uint32_t i = 0; i < INODE_COUNT; i++) {
        Inode *inode = &inode_table[i];
        if (inode->links == 0 || inode->dtime != 0) continue;
        if (inode->direct[0] >= DATA_BLOCK_START && inode->direct[0] < TOTAL_BLOCKS) {
            set_bit(computed_bitmap, inode->direct[0], 1);
        }
    }

    for (uint32_t i = DATA_BLOCK_START; i < TOTAL_BLOCKS; i++) {
        int bitmap_bit = get_bit(data_bitmap, i);
        int computed_bit = get_bit(computed_bitmap, i);
        if (bitmap_bit && !computed_bit) {
            printf("Error: Data block %u marked used but not referenced\n", i);
            errors_found++;
            if (fix) {
                set_bit(data_bitmap, i, 0);
                printf("Fixed: Cleared data block %u in bitmap\n", i);
                fixes_applied++;
            }
        } else if (!bitmap_bit && computed_bit) {
            printf("Error: Data block %u referenced but not marked in bitmap\n", i);
            errors_found++;
            if (fix) {
                set_bit(data_bitmap, i, 1);
                printf("Fixed: Set data block %u in bitmap\n", i);
                fixes_applied++;
            }
        }
    }

    for (uint32_t i = 0; i < INODE_COUNT; i++) {
        Inode *inode = &inode_table[i];
        if (inode->links == 0 || inode->dtime != 0) continue;
        if (inode->direct[0] >= TOTAL_BLOCKS) {
            printf("Error: Inode %u references invalid block %u\n", i, inode->direct[0]);
            errors_found++;
            if (fix) {
                inode->direct[0] = 0;
                inode->blocks = 0;
                inode->size = 0; 
                printf("Fixed: Cleared invalid block reference in inode %u\n", i);
                fixes_applied++;
            }
        }
    }
}
