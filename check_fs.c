#include <stdio.h>
#include "vsfsck.h"

void check_filesystem(int fix) {
    errors_found = 0;
    fixes_applied = 0;

    Superblock *sb = (Superblock *)fs_image;
    uint8_t *inode_bitmap = fs_image + BLOCK_SIZE;
    uint8_t *data_bitmap = fs_image + 2 * BLOCK_SIZE;
    Inode *inode_table = (Inode *)(fs_image + INODE_TABLE_START * BLOCK_SIZE);

    check_superblock(sb, fix);
    check_inode_bitmap(inode_bitmap, inode_table, fix);
    check_data_bitmap(data_bitmap, inode_table, fix);
}