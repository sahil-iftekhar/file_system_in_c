#ifndef VSFSCK_H
#define VSFSCK_H

#include <stdint.h>
#include <stddef.h>

#define BLOCK_SIZE 4096
#define TOTAL_BLOCKS 64
#define INODE_SIZE 256
#define INODE_COUNT (5 * BLOCK_SIZE / INODE_SIZE) 
#define MAGIC_NUMBER 0xD34D
#define SUPERBLOCK_SIZE 4096
#define BITMAP_SIZE (BLOCK_SIZE / 8) 
#define DATA_BLOCK_START 8
#define INODE_TABLE_START 3
#define INODE_TABLE_BLOCKS 5

typedef struct {
    uint16_t magic;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t inode_bitmap_block;
    uint32_t data_bitmap_block;
    uint32_t inode_table_start;
    uint32_t first_data_block;
    uint32_t inode_size;
    uint32_t inode_count;
    uint8_t reserved[4058];
} Superblock;

typedef struct {
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t size;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint32_t links;
    uint32_t blocks;
    uint32_t direct[1]; 
    uint32_t single_indirect;
    uint32_t double_indirect;
    uint32_t triple_indirect;
    uint8_t reserved[156];
} Inode;

extern uint8_t *fs_image;
extern size_t fs_size;
extern int errors_found;
extern int fixes_applied;

int get_bit(uint8_t *bitmap, uint32_t index);
void set_bit(uint8_t *bitmap, uint32_t index, int value);
void check_superblock(Superblock *sb, int fix);
void check_inode_bitmap(uint8_t *inode_bitmap, Inode *inode_table, int fix);
void check_data_bitmap(uint8_t *data_bitmap, Inode *inode_table, int fix);
void check_filesystem(int fix);

#endif