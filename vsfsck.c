#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "vsfsck.h"

uint8_t *fs_image;
size_t fs_size;
int errors_found = 0;
int fixes_applied = 0;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <vsfs.img>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDWR);
    if (fd < 0) 
    {
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

    printf("Checking and fixing filesystem...\n");
    check_filesystem(1);
    printf("Complete: %d errors found, %d fixes applied\n", errors_found, fixes_applied);

    if (msync(fs_image, fs_size, MS_SYNC) < 0) {
        perror("Failed to sync filesystem image");
    }

    printf("\nVerifying filesystem...\n");
    check_filesystem(0);
    if (errors_found == 0) {
        printf("Complete: Filesystem is error-free\n");
    } else {
        printf("Complete: %d errors remain\n", errors_found);
    }

    munmap(fs_image, fs_size);
    close(fd);
    return errors_found ? 1 : 0;
}