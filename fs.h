#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <pthread.h>

#define FS_MAGIC 0x1A2B3C4D
#define BLOCK_SIZE 4096
#define MAX_FILENAME 28
#define POINTERS_PER_INODE 12
#define MAX_OPEN_FILES 64

// Superblock structure
typedef struct {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t total_inodes;
    uint32_t free_blocks;
    uint32_t free_inodes;
} Superblock;

// Inode structure
typedef struct {
    uint32_t size;
    uint32_t is_dir;
    uint32_t direct[POINTERS_PER_INODE];
    uint32_t indirect;
} Inode;

// Directory Entry
typedef struct {
    uint32_t inode_num;
    char name[MAX_FILENAME];
} DirEntry;

// File System API
int fs_format(int total_blocks);
int fs_mount();
int fs_create(const char *name);
int fs_mkdir(const char *name);
int fs_open(const char *name);
int fs_read(int fd, char *buf, int size);
int fs_write(int fd, const char *buf, int size);
int fs_close(int fd);

#endif
