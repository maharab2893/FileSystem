#include "fs.h"
#include "disk.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define INODES_PER_BLOCK (BLOCK_SIZE / sizeof(Inode))
#define DENTRIES_PER_BLOCK (BLOCK_SIZE / sizeof(DirEntry))

static Superblock sb;
static pthread_rwlock_t fs_lock = PTHREAD_RWLOCK_INITIALIZER;

typedef struct {
    int inode_num;
    int offset;
    int is_open;
} FileDescriptor;

static FileDescriptor fd_table[MAX_OPEN_FILES];

// Helper: Bit operations
static void set_bit(char *bitmap, int bit) { bitmap[bit / 8] |= (1 << (bit % 8)); }
static void clear_bit(char *bitmap, int bit) { bitmap[bit / 8] &= ~(1 << (bit % 8)); }
static int get_bit(char *bitmap, int bit) { return (bitmap[bit / 8] & (1 << (bit % 8))) != 0; }

// Helper: Block Management
static int alloc_block() {
    char bitmap[BLOCK_SIZE];
    disk_read(2, bitmap); // Block 2 is Data Bitmap
    for (uint32_t i = 0; i < sb.total_blocks; i++) {
        if (!get_bit(bitmap, i)) {
            set_bit(bitmap, i);
            disk_write(2, bitmap);
            sb.free_blocks--;
            char buf[BLOCK_SIZE];
            disk_read(0, buf);
            memcpy(buf, &sb, sizeof(Superblock));
            disk_write(0, buf);
            
            char zero[BLOCK_SIZE] = {0};
            disk_write(i, zero);
            return i;
        }
    }
    return -1; // Disk Full
}

// Helper: Inode Management
static int alloc_inode() {
    char bitmap[BLOCK_SIZE];
    disk_read(1, bitmap); // Block 1 is Inode Bitmap
    for (uint32_t i = 1; i < sb.total_inodes; i++) { // Inode 0 is Root
        if (!get_bit(bitmap, i)) {
            set_bit(bitmap, i);
            disk_write(1, bitmap);
            sb.free_inodes--;
            char buf[BLOCK_SIZE];
            disk_read(0, buf);
            memcpy(buf, &sb, sizeof(Superblock));
            disk_write(0, buf);
            return i;
        }
    }
    return -1; // Inodes Full
}

static void read_inode(int inum, Inode *inode) {
    int block = 3 + (inum / INODES_PER_BLOCK); // Block 3 is start of Inode Table
    int offset = inum % INODES_PER_BLOCK;
    char buf[BLOCK_SIZE];
    disk_read(block, buf);
    memcpy(inode, buf + (offset * sizeof(Inode)), sizeof(Inode));
}

static void write_inode(int inum, Inode *inode) {
    int block = 3 + (inum / INODES_PER_BLOCK);
    int offset = inum % INODES_PER_BLOCK;
    char buf[BLOCK_SIZE];
    disk_read(block, buf);
    memcpy(buf + (offset * sizeof(Inode)), inode, sizeof(Inode));
    disk_write(block, buf);
}

// Logical to Physical Block Mapping (Direct & Indirect)
static int bmap(Inode *inode, int log_block, int allocate) {
    if (log_block < POINTERS_PER_INODE) {
        if (inode->direct[log_block] == 0) {
            if (!allocate) return -1;
            int b = alloc_block();
            if (b < 0) return -1;
            inode->direct[log_block] = b;
        }
        return inode->direct[log_block];
    } else {
        if (inode->indirect == 0) {
            if (!allocate) return -1;
            int b = alloc_block();
            if (b < 0) return -1;
            inode->indirect = b;
        }
        uint32_t ind_block[BLOCK_SIZE / 4];
        disk_read(inode->indirect, (char*)ind_block);
        int ind_idx = log_block - POINTERS_PER_INODE;
        if (ind_block[ind_idx] == 0) {
            if (!allocate) return -1;
            int b = alloc_block();
            if (b < 0) return -1;
            ind_block[ind_idx] = b;
            disk_write(inode->indirect, (char*)ind_block);
        }
        return ind_block[ind_idx];
    }
}

// Path Resolution
static int resolve_path(const char *path, int *parent_inum, char *child_name) {
    if (path[0] != '/') return -1; // Only absolute paths
    
    int curr_inum = 0; // Root inode
    char path_copy[256];
    strncpy(path_copy, path, 255);
    
    char *token = strtok(path_copy, "/");
    char *next_token = strtok(NULL, "/");
    
    while (token != NULL) {
        if (next_token == NULL) {
            if (parent_inum) *parent_inum = curr_inum;
            if (child_name) strncpy(child_name, token, MAX_FILENAME-1);
            return curr_inum;
        }
        
        Inode curr;
        read_inode(curr_inum, &curr);
        if (!curr.is_dir) return -1;
        
        int found = 0;
        int blocks = (curr.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
        for (int i = 0; i < blocks && !found; i++) {
            int p_block = bmap(&curr, i, 0);
            if (p_block <= 0) continue;
            
            DirEntry entries[DENTRIES_PER_BLOCK];
            disk_read(p_block, (char*)entries);
            
            for (int j = 0; j < DENTRIES_PER_BLOCK; j++) {
                if (entries[j].inode_num != 0 && strcmp(entries[j].name, token) == 0) {
                    curr_inum = entries[j].inode_num;
                    found = 1;
                    break;
                }
            }
        }
        if (!found) return -1; // Path not found
        token = next_token;
        next_token = strtok(NULL, "/");
    }
    return curr_inum;
}

static int create_entity(const char *path, int is_dir) {
    pthread_rwlock_wrlock(&fs_lock);
    
    int parent_inum;
    char name[MAX_FILENAME] = {0};
    
    if (resolve_path(path, &parent_inum, name) < 0) {
        pthread_rwlock_unlock(&fs_lock);
        return -1; // Parent missing or invalid path
    }
    
    Inode parent;
    read_inode(parent_inum, &parent);
    
    int new_inum = alloc_inode();
    if (new_inum < 0) {
        pthread_rwlock_unlock(&fs_lock);
        return -1;
    }
    
    Inode new_inode;
    memset(&new_inode, 0, sizeof(Inode));
    new_inode.in_use = 1;
    new_inode.is_dir = is_dir;
    
    if (is_dir) {
        // Setup '.' and '..'
        int b = bmap(&new_inode, 0, 1);
        DirEntry entries[DENTRIES_PER_BLOCK];
        memset(entries, 0, BLOCK_SIZE);
        
        entries[0].inode_num = new_inum;
        strcpy(entries[0].name, ".");
        entries[1].inode_num = parent_inum;
        strcpy(entries[1].name, "..");
        
        disk_write(b, (char*)entries);
        new_inode.size = 2 * sizeof(DirEntry);
    }
    
    write_inode(new_inum, &new_inode);
    
    // Add to parent directory
    int p_blocks = (parent.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (p_blocks == 0) p_blocks = 1; // Empty dir
    
    int p_bmap = bmap(&parent, p_blocks - 1, 1);
    DirEntry p_entries[DENTRIES_PER_BLOCK];
    disk_read(p_bmap, (char*)p_entries);
    
    int added = 0;
    for (int i = 0; i < DENTRIES_PER_BLOCK; i++) {
        if (p_entries[i].inode_num == 0) {
            p_entries[i].inode_num = new_inum;
            strncpy(p_entries[i].name, name, MAX_FILENAME-1);
            parent.size += sizeof(DirEntry);
            disk_write(p_bmap, (char*)p_entries);
            write_inode(parent_inum, &parent);
            added = 1;
            break;
        }
    }
    
    pthread_rwlock_unlock(&fs_lock);
    return added ? 0 : -1;
}


int fs_format(int total_blocks) {
    if (total_blocks < 10) return -1;
    
    char zero[BLOCK_SIZE] = {0};
    for (int i = 0; i < total_blocks; i++) disk_write(i, zero);
    
    sb.magic = FS_MAGIC;
    sb.total_blocks = total_blocks;
    sb.total_inodes = total_blocks / 2; // Arbitrary ratio
    sb.inode_blocks = (sb.total_inodes * sizeof(Inode)) / BLOCK_SIZE + 1;
    sb.free_blocks = total_blocks - (3 + sb.inode_blocks); 
    sb.free_inodes = sb.total_inodes - 1; // root used
    
    char buf[BLOCK_SIZE] = {0};
    memcpy(buf, &sb, sizeof(Superblock));
    disk_write(0, buf);
    
    // Set bitmaps for reserved blocks
    char dbmap[BLOCK_SIZE] = {0};
    for (uint32_t i = 0; i < 3 + sb.inode_blocks; i++) set_bit(dbmap, i);
    disk_write(2, dbmap);
    
    char ibmap[BLOCK_SIZE] = {0};
    set_bit(ibmap, 0); // Root inode
    disk_write(1, ibmap);
    
    // Create Root Directory
    Inode root;
    memset(&root, 0, sizeof(Inode));
    root.in_use = 1;
    root.is_dir = 1;
    
    int b = alloc_block();
    root.direct[0] = b;
    DirEntry entries[DENTRIES_PER_BLOCK];
    memset(entries, 0, BLOCK_SIZE);
    
    entries[0].inode_num = 0;
    strcpy(entries[0].name, ".");
    entries[1].inode_num = 0;
    strcpy(entries[1].name, "..");
    
    disk_write(b, (char*)entries);
    root.size = 2 * sizeof(DirEntry);
    
    write_inode(0, &root);
    return 0;
}

int fs_mount() {
    char buf[BLOCK_SIZE];
    disk_read(0, buf);
    memcpy(&sb, buf, sizeof(Superblock));
    if (sb.magic != FS_MAGIC) return -1;
    memset(fd_table, 0, sizeof(fd_table));
    return 0;
}

int fs_create(const char *path) { return create_entity(path, 0); }
int fs_mkdir(const char *path) { return create_entity(path, 1); }

int fs_open(const char *path) {
    pthread_rwlock_rdlock(&fs_lock);
    
    int inum = -1;
    
    // Search directory tree
    if (path[0] == '/') {
        char name[MAX_FILENAME];
        int parent;
        resolve_path(path, &parent, name);
        
        Inode p_node;
        read_inode(parent, &p_node);
        int blocks = (p_node.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
        for (int i = 0; i < blocks; i++) {
            int p_block = bmap(&p_node, i, 0);
            if (p_block <= 0) continue;
            DirEntry entries[DENTRIES_PER_BLOCK];
            disk_read(p_block, (char*)entries);
            for (int j = 0; j < DENTRIES_PER_BLOCK; j++) {
                if (entries[j].inode_num != 0 && strcmp(entries[j].name, name) == 0) {
                    inum = entries[j].inode_num;
                    break;
                }
            }
        }
    }
    
    if (inum < 0) {
        pthread_rwlock_unlock(&fs_lock);
        return -1; // Not found
    }
    
    int fd = -1;
    for(int i=0; i<MAX_OPEN_FILES; i++) {
        if(!fd_table[i].is_open) {
            fd_table[i].is_open = 1;
            fd_table[i].offset = 0;
            fd_table[i].inode_num = inum;
            fd = i;
            break;
        }
    }
    pthread_rwlock_unlock(&fs_lock);
    return fd;
}

int fs_write(int fd, const char *buf, int size) {
    if(fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].is_open) return -1;
    
    pthread_rwlock_wrlock(&fs_lock); // Write lock for file modifications
    
    int inum = fd_table[fd].inode_num;
    Inode inode;
    read_inode(inum, &inode);
    
    int offset = fd_table[fd].offset;
    int bytes_written = 0;
    
    while (bytes_written < size) {
        int log_block = offset / BLOCK_SIZE;
        int block_offset = offset % BLOCK_SIZE;
        int phys_block = bmap(&inode, log_block, 1);
        
        if (phys_block < 0) break; // Disk Full
        
        char block_data[BLOCK_SIZE];
        if (block_offset > 0 || (size - bytes_written) < BLOCK_SIZE) {
            disk_read(phys_block, block_data);
        }
        
        int to_write = BLOCK_SIZE - block_offset;
        if (size - bytes_written < to_write) to_write = size - bytes_written;
        
        memcpy(block_data + block_offset, buf + bytes_written, to_write);
        disk_write(phys_block, block_data);
        
        offset += to_write;
        bytes_written += to_write;
    }
    
    if (offset > (int)inode.size) inode.size = offset;
    write_inode(inum, &inode);
    fd_table[fd].offset = offset;
    
    pthread_rwlock_unlock(&fs_lock);
    return bytes_written;
}

int fs_read(int fd, char *buf, int size) {
    if(fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].is_open) return -1;
    
    pthread_rwlock_rdlock(&fs_lock); // Concurrent reads allowed
    
    int inum = fd_table[fd].inode_num;
    Inode inode;
    read_inode(inum, &inode);
    
    int offset = fd_table[fd].offset;
    if (offset >= (int)inode.size) {
        pthread_rwlock_unlock(&fs_lock);
        return 0; // EOF
    }
    
    if (offset + size > (int)inode.size) size = inode.size - offset;
    int bytes_read = 0;
    
    while (bytes_read < size) {
        int log_block = offset / BLOCK_SIZE;
        int block_offset = offset % BLOCK_SIZE;
        int phys_block = bmap(&inode, log_block, 0);
        
        if (phys_block <= 0) break;
        
        char block_data[BLOCK_SIZE];
        disk_read(phys_block, block_data);
        
        int to_read = BLOCK_SIZE - block_offset;
        if (size - bytes_read < to_read) to_read = size - bytes_read;
        
        memcpy(buf + bytes_read, block_data + block_offset, to_read);
        
        offset += to_read;
        bytes_read += to_read;
    }
    
    fd_table[fd].offset = offset;
    pthread_rwlock_unlock(&fs_lock);
    return bytes_read;
}

int fs_close(int fd) {
    if(fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].is_open) return -1;
    pthread_rwlock_wrlock(&fs_lock);
    fd_table[fd].is_open = 0;
    pthread_rwlock_unlock(&fs_lock);
    return 0;
}
