# Advanced Virtual File System (VFS) in C

**Course:** CSE 325 - Operating Systems  
**Developer:** Maharab Hossain  

## 📌 Project Overview
This project is a fully functional, multi-threaded Virtual File System implemented in C. It simulates a physical hard drive using a local binary file (`vdisk.bin`) and provides a comprehensive set of POSIX-like system calls to manage files, hierarchical directories, and raw data bytes. The system is designed to be highly concurrent, thread-safe, and capable of handling large files using indirect block pointers.

---

## 🏗️ Architecture & Disk Layout
The virtual disk is divided into fixed-size blocks (4096 Bytes / 4KB per block). The layout is structured as follows:

| Block Number | Component | Description |
| :--- | :--- | :--- |
| **Block 0** | Superblock | Stores critical metadata: magic number, total blocks, total inodes, and free block/inode counts. |
| **Block 1** | Inode Bitmap | Tracks free and used Inodes using bit-level operations for O(1) free space lookup. |
| **Block 2** | Data Bitmap | Tracks free and used Data Blocks across the disk. |
| **Block 3 to N** | Inode Table | Pre-allocated blocks storing the `Inode` structures. |
| **Block N+1 to End**| Data Blocks | Stores actual file contents and directory entries (dentry). |

---

## 🚀 Key Features Implemented (Based on Rubric)

### 1. Inode & Metadata Management
Each file and directory is represented by an Inode containing:
* **Size:** Exact byte count of the file.
* **Type:** Flag distinguishing between standard files and directories.
* **Direct Pointers (10):** Points directly to the first 10 data blocks (up to 40 KB).
* **Single-Indirect Pointer (1):** Points to a block containing 1024 additional block pointers, allowing the file to grow up to ~4 MB.

### 2. Hierarchical Directories & Path Resolution
* **Directories:** Implemented as special files containing an array of `DirEntry` structures (Inode Number + Filename).
* **Self & Parent Linking:** Every newly created directory automatically gets `.` (current) and `..` (parent) entries.
* **Path Traversing:** The `resolve_path()` function parses multi-level absolute paths (e.g., `/dir1/dir2/file.txt`) by recursively reading directory data blocks.

### 3. Core System Calls API
The following file system operations are implemented from scratch:
* `fs_format(blocks)`: Initializes the virtual disk, bitmaps, and root directory.
* `fs_mount()`: Loads the Superblock into memory.
* `fs_mkdir(path)`: Creates a new directory in the specified path.
* `fs_create(path)`: Creates a new empty file.
* `fs_open(path)`: Assigns a File Descriptor (FD) and initializes the byte offset.
* `fs_write(fd, buf, size)`: Writes bytes, handling partial block offsets and spanning multiple blocks.
* `fs_read(fd, buf, size)`: Reads bytes directly from mapped physical blocks into the buffer.
* `fs_close(fd)`: Frees the File Descriptor.

### 4. Concurrency & Thread Safety
* Utilizes **POSIX Reader-Writer Locks (`pthread_rwlock_t`)**.
* Multiple threads can concurrently read from the file system (`fs_read`).
* Exclusive write-locks are acquired during block allocations, directory mutations, and file modifications (`fs_write`, `fs_create`) to prevent race conditions and data corruption.

---

## 📂 Project Structure
* **`disk.h` / `disk.c`:** The physical layer. Manages the host-level `vdisk.bin` file and provides raw block `disk_read()` and `disk_write()` capabilities.
* **`fs.h` / `fs.c`:** The logical layer. Contains the Superblock management, Bitmaps, Inode mapping, Path resolution, and the System Call API.
* **`main.c`:** The test suite. Formats the disk, creates directories, and launches a multi-threaded stress test.
* **`Makefile`:** Build automation script.

---

## ⚙️ Compilation & Execution

**1. Compile the Project:**
```bash
make
```

**2. Run the Stress Test:**
```bash
./fs_project
```

**3. Run with Memory Leak Check (Valgrind):**
```bash
valgrind --leak-check=full ./fs_project
```

**4. Clean build files:**
```bash
make clean
```

---

## 🧪 About the Stress Test (`main.c`)
When you run the project, the `main.c` file executes a heavy workload to prove system stability:
1. Creates a deep directory tree: `/dir1/dir2/deep_file.txt`.
2. Spawns **4 concurrent threads**.
3. Each thread creates a unique file and writes **45 KB of data**.
4. *Why 45 KB?* Because 10 Direct pointers can only hold 40 KB (10 x 4096 bytes). Writing 45 KB forces the file system to successfully allocate and navigate the **Indirect Pointer** block.
5. All threads then read the data back to validate zero data corruption.