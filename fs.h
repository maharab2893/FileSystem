#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <pthread.h>

#define FS_MAGIC 0x1A2B3C4D
#define BLOCK_SIZE 4096
#define MAX_FILENAME 28
#define POINTERS_PER_INODE 10
#define MAX_OPEN_FILES 128

// Superblock (Block 0)
typedef struct {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t total_inodes;
    uint32_t free_blocks;
    uint32_t free_inodes;
    uint32_t inode_blocks;
} Superblock;

// Inode Structure (Supports Direct & Single Indirect)
typedef struct {
    uint32_t size;
    uint32_t is_dir;
    uint32_t in_use;
    uint32_t direct[POINTERS_PER_INODE];
    uint32_t indirect; // Points to a block of 1024 pointers
} Inode;

// Directory Entry
typedef struct {
    uint32_t inode_num;
    char name[MAX_FILENAME];
} DirEntry;

// File System API
int fs_format(int total_blocks);
int fs_mount();
int fs_create(const char *path);
int fs_mkdir(const char *path);
int fs_open(const char *path);
int fs_read(int fd, char *buf, int size);
int fs_write(int fd, const char *buf, int size);
int fs_close(int fd);
int fs_unlink(const char *path);

#endif
