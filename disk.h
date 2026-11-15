#ifndef DISK_H
#define DISK_H

#include <stdint.h>

#define BLOCK_SIZE 4096

// Initializes the simulated disk (a file on the host OS)
int disk_init(const char *filename, int nblocks);

// Reads a block from the disk
int disk_read(int blocknum, char *data);

// Writes a block to the disk
int disk_write(int blocknum, const char *data);

// Closes the disk
void disk_close();

#endif
