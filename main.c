#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>


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

uint8_t *fs_image;
size_t fs_size;
int errors_found = 0;
int fixes_applied = 0;

int get_bit(uint8_t *bitmap, uint32_t index) {
    return (bitmap[index / 8] >> (index % 8)) & 1;
}

void set_bit(uint8_t *bitmap, uint32_t index, int value) {
    if (value)
        bitmap[index / 8] |= (1 << (index % 8));
    else
        bitmap[index / 8] &= ~(1 << (index % 8));
}

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

    for (uint32_t i = 0; i < DATA_BLOCK_START; i++) {
        set_bit(computed_bitmap, i, 1);
        block_counts[i]++;
    }

    for (uint32_t i = 0; i < INODE_COUNT; i++) {
        Inode *inode = &inode_table[i];
        if (inode->links == 0 || inode->dtime != 0) continue;
        if (inode->direct[0] >= DATA_BLOCK_START && inode->direct[0] < TOTAL_BLOCKS) {
            block_counts[inode->direct[0]]++;
            set_bit(computed_bitmap, inode->direct[0], 1);
        }
    }

    for (uint32_t i = DATA_BLOCK_START; i < TOTAL_BLOCKS; i++) {
        if (block_counts[i] > 1) {
            printf("Error: Data block %u referenced %u times\n", i, block_counts[i]);
            errors_found++;
            if (fix) {
                printf("Fix not implemented: Duplicate block %u\n", i);
                fixes_applied++;
            }
        }
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
                printf("Fixed: Cleared invalid block reference in inode %u\n", i);
                fixes_applied++;
            }
        }
    }
}

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

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <vsfs.img>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        perror("Failed to open filesystem image");
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("Failed to stat filesystem image");
        close(fd);
        return 1;
    }
    fs_size = st.st_size;
    if (fs_size < TOTAL_BLOCKS * BLOCK_SIZE) {
        fprintf(stderr, "Filesystem image too small\n");
        close(fd);
        return 1;
    }

    fs_image = mmap(NULL, fs_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fs_image == MAP_FAILED) {
        perror("Failed to mmap filesystem image");
        close(fd);
        return 1;
    }

    printf("First pass: Checking and fixing filesystem...\n");
    check_filesystem(1);
    printf("First pass complete: %d errors found, %d fixes applied\n", errors_found, fixes_applied);

    if (msync(fs_image, fs_size, MS_SYNC) < 0) {
        perror("Failed to sync filesystem image");
    }

    printf("\nSecond pass: Verifying filesystem...\n");
    check_filesystem(0);
    if (errors_found == 0) {
        printf("Second pass complete: Filesystem is error-free\n");
    } else {
        printf("Second pass complete: %d errors remain\n", errors_found);
    }

    munmap(fs_image, fs_size);
    close(fd);
    return errors_found ? 1 : 0;
}