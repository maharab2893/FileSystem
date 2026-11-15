#include "fs.h"
#include "disk.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define INODES_PER_BLOCK (BLOCK_SIZE / sizeof(Inode))

static Superblock sb;
static pthread_rwlock_t fs_lock = PTHREAD_RWLOCK_INITIALIZER;

// Simple in-memory Open File Table
typedef struct {
    int inode_num;
    int offset;
    int is_open;
} FileDescriptor;

static FileDescriptor fd_table[MAX_OPEN_FILES];

// Helper: Read Superblock
static void read_sb() {
    char buf[BLOCK_SIZE];
    disk_read(0, buf);
    memcpy(&sb, buf, sizeof(Superblock));
}

// Helper: Write Superblock
static void write_sb() {
    char buf[BLOCK_SIZE] = {0};
    memcpy(buf, &sb, sizeof(Superblock));
    disk_write(0, buf);
}

int fs_format(int total_blocks) {
    if (total_blocks < 10) return -1;
    
    char empty_block[BLOCK_SIZE] = {0};
    for (int i = 0; i < total_blocks; i++) {
        disk_write(i, empty_block);
    }
    
    sb.magic = FS_MAGIC;
    sb.total_blocks = total_blocks;
    sb.total_inodes = total_blocks; // 1 inode per block roughly
    sb.free_blocks = total_blocks - 3; // 0:SB, 1:InodeBitmap, 2:DataBitmap
    sb.free_inodes = sb.total_inodes;
    
    write_sb();
    return 0;
}

int fs_mount() {
    read_sb();
    if (sb.magic != FS_MAGIC) return -1;
    memset(fd_table, 0, sizeof(fd_table));
    return 0;
}

int fs_create(const char *name) {
    pthread_rwlock_wrlock(&fs_lock);
    // Simplified: Find free inode, allocate, set name in root dir
    // (A complete implementation would parse directory entries here)
    printf("Created file: %s\n", name);
    pthread_rwlock_unlock(&fs_lock);
    return 0;
}

int fs_mkdir(const char *name) {
    pthread_rwlock_wrlock(&fs_lock);
    printf("Created directory: %s\n", name);
    pthread_rwlock_unlock(&fs_lock);
    return 0;
}

int fs_open(const char *name) {
    (void)name;  // Warning সরানোর জন্য
    pthread_rwlock_rdlock(&fs_lock);
    int fd = -1;
    for(int i=0; i<MAX_OPEN_FILES; i++){
        if(!fd_table[i].is_open){
            fd_table[i].is_open = 1;
            fd_table[i].offset = 0;
            fd_table[i].inode_num = 1; 
            fd = i;
            break;
        }
    }
    pthread_rwlock_unlock(&fs_lock);
    return fd;
}

int fs_write(int fd, const char *buf, int size) {
    (void)buf;  // Warning সরানোর জন্য
    if(fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].is_open) return -1;
    pthread_rwlock_wrlock(&fs_lock);
    
    fd_table[fd].offset += size;
    printf("Thread %lu wrote %d bytes to FD %d\n", pthread_self(), size, fd);
    
    pthread_rwlock_unlock(&fs_lock);
    return size;
}

int fs_read(int fd, char *buf, int size) {
    (void)buf;  // Warning সরানোর জন্য
    if(fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].is_open) return -1;
    pthread_rwlock_rdlock(&fs_lock);
    
    printf("Thread %lu read from FD %d\n", pthread_self(), fd);
    
    pthread_rwlock_unlock(&fs_lock);
    return size;
}

int fs_close(int fd) {
    if(fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].is_open) return -1;
    pthread_rwlock_wrlock(&fs_lock);
    fd_table[fd].is_open = 0;
    pthread_rwlock_unlock(&fs_lock);
    return 0;
}
