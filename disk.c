#include "disk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static int disk_fd = -1;
static int total_blocks = 0;

int disk_init(const char *filename, int nblocks) {
    disk_fd = open(filename, O_RDWR | O_CREAT, 0666);
    if (disk_fd < 0) return -1;
    
    total_blocks = nblocks;
    
    // Set file size
    off_t size = (off_t)nblocks * BLOCK_SIZE;
    if (ftruncate(disk_fd, size) != 0) {
        close(disk_fd);
        return -1;
    }
    return 0;
}

int disk_read(int blocknum, char *data) {
    if (blocknum < 0 || blocknum >= total_blocks) return -1;
    if (pread(disk_fd, data, BLOCK_SIZE, blocknum * BLOCK_SIZE) != BLOCK_SIZE) return -1;
    return 0;
}

int disk_write(int blocknum, const char *data) {
    if (blocknum < 0 || blocknum >= total_blocks) return -1;
    if (pwrite(disk_fd, data, BLOCK_SIZE, blocknum * BLOCK_SIZE) != BLOCK_SIZE) return -1;
    return 0;
}

void disk_close() {
    if (disk_fd >= 0) {
        close(disk_fd);
        disk_fd = -1;
    }
}
