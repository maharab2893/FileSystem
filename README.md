# Advanced Virtual File System
A fully functioning C-based Virtual File System implementing hierarchical directories, indirect pointers, block bitmaps, and thread-safety.

## Implemented Rubric Features:
1. **Disk Layout & Superblock**: Block 0 is Superblock, Block 1 is Inode Bitmap, Block 2 is Data Block Bitmap.
2. **Inode & Metadata**: Inodes map to physical blocks using `alloc_block()`. Fully supports `direct` (first 10 blocks) and `single-indirect` pointers for large files.
3. **Directory Path Resolution**: `fs_mkdir` creates `.` and `..` dentry records. `resolve_path()` supports parsing multi-level absolute paths like `/dir1/dir2/file.txt`.
4. **Core System Calls**: Complete working implementation of `create()`, `mkdir()`, `open()`, `read()`, `write()`, and `close()`.
5. **Concurrency**: Implemented using `pthread_rwlock_t`. Safely handles simultaneous reads and exclusively locks on file mutations and block allocations. Gracefully returns `-1` if the disk bitmaps are full.
6. **Testing**: `main.c` executes a 4-thread stress test where each thread writes 45KB of data to trigger the allocation of indirect pointers across the simulated disk.

## Run Instructions
```bash
make
./fs_project
valgrind ./fs_project
```
