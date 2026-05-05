# Custom File System 

## Overview

This project implements a custom file system in user space written in C. It models core operating system concepts including inodes, block-based storage, directory hierarchy, and persistent storage using a disk image.

The system supports file and directory operations while maintaining a structured separation between metadata (inodes) and data (blocks). The design favors predictable behavior and clarity while also maintaining extensibility for future upgrades.

---
## What This Project Demonstrates

- Low-level systems programming in C
- Memory-mapped file I/O using `mmap`
- Filesystem design (inodes, block allocation, directory hierarchy)
- Tradeoff analysis between allocation strategies (linked vs contiguous vs indexed)
- Debugging and managing persistent state
## How It Works (Execution Flow)

1. Filesystem is memory-mapped from disk (`mmap`)
2. If new, `formatfs()` initializes:
    - superblock
    - inode table
    - root directory
3. Operations (`-a`, `-l`, `-e`, `-r`, `-d`, `-D`) modify in-memory structures
4. `mysync()` writes all structures back to disk image
5. On next run, `loadfs()` reconstructs state from disk
---
## Features
### File Operations
- Add and extract files from host system
- Remove files
- Open and close files

### Directory Operations
- Create directories
- Nested directory support
- Add files to specific directories via command line interface(CLI) 
- Per-directory listing

### Storage and Persistence 
- Persistent filesystem stored in a disk image named `myfs.img`
- Memory-mapped I/O (mmap) for efficient access
- Explicit sync to disk via `mysync()` method

---

## System Design
### Key Design Decisions

- **Linked allocation** was chosen for simplicity and flexibility in file growth, at the cost of random access performance.
- **First-fit allocation** was used to maintain deterministic behavior and reduce metadata complexity.
- **Explicit sync model (`mysync`)** was implemented instead of automatic persistence to simplify debugging and state control.
- **Parent inode tracking** was added to support bidirectional directory traversal and maintain structural consistency.

### Block-Based Storage Abstraction

The filesystem uses fixed-size blocks (512 bytes) to store file data.

A block abstraction layer encapsulates:
- Allocation (`block_alloc`)
- Deallocation (`block_free_chain`)
- Traversal (`block_next`)
- Read/write operations

This separates low-level storage from filesystem logic.

---

### Inode-Based Metadata

Each file/directory is represented by an inode:

```c
struct inode {
    int first_block;
    int size;
    int is_directory;
    int parent_inode;
    char name[NAME_SIZE];
};
```
### Directory Structure

Directories store references to child inodes, enabling proper directory nesting and bidirectional traversal.

Each directory maintains a list of inode indices (`fds[]`) representing its contents, improving structural consistency and simplifying validation.

This mirrors real filesystem designs where directory entries map names to inode identifiers, while inodes maintain structural metadata.
```c
struct mydirent {
    int size;
    int fds[MAX_DIR_SIZE];
};
```

### File Allocation Strategy
This filesystem uses **linked allocation** to store data. In this approach, each block contains a pointer to the next block in the file, forming a linked chain of blocks that represents the file’s contents.
### Allocation Method 
Blocks are allocated using a **deterministic first-fit strategy**, which scans the block table from index 0 upward and selects the first available free block. I will briefly discuss some advantages and disadvantages of this strategy below.
#### Advantages
- **Simple implementation** with minimal metadata overhead
- **No requirement for contiguous space**, avoiding external fragmentation constraints by eliminating the need for contiguous space
- **Flexible file growth**, as additional blocks can be appended dynamically

#### Disadvantages
- **Fragmentation over time**, leading to reduced spatial locality
- **Inefficient random access**, as blocks must be traversed sequentially
- **Allocation cost is O(n)** due to linear scanning of the block table

Overall, this design prioritizes simplicity and flexibility over access performance, making it well-suited for a teaching-oriented or prototype filesystem.

In future iterations, a bitmap-based allocation strategy could replace the current first-fit scan to improve allocation efficiency and scalability by avoiding O(n) block searches. Additionally, adopting an indexed allocation model would improve random access performance by eliminating the need to traverse block chains.

### Maximum File Size Enforcement

To prevent unbounded growth, the filesystem enforces a maximum file size based on a fixed number of blocks:

```c
#define MAX_FILE_BLOCKS ...
#define MAX_FILE_SIZE (MAX_FILE_BLOCKS * BLOCK_SIZE)
```
This constraint simplifies block management by placing an upper bound on the number of blocks a single file can consume. Advantages and disadvantages of this constrains are as follows:
#### Advantages
- Greatly lowers the probability of runaway allocation or uncontrolled resource usage
- Simplifies allocation logic and testing 
- Provides predictable upper bounds for storage behavior
#### Disadvantages
- Reduces scalability by limiting file size
- Reduces user experience

### Persistence Model

The filesystem state is persisted to a disk image (`myfs.img`), enabling recovery across program executions.

The `mysync()` function serializes the in-memory filesystem structures to disk, including:
- Superblock
- Inode table
- Data blocks

The `loadfs()` function reconstructs these structures from the disk image at startup.

### Behavior

- The filesystem operates primarily in memory
- Changes are only written to disk when `mysync()` is explicitly invoked
- The disk image serves as a complete snapshot of the filesystem state

#### Advantages

- Enables full recovery between runs
- Provides a simple and deterministic persistence model
- Easy to reason about and debug

#### Disadvantages 

- No automatic persistence — data may be lost if the program exits before syncing
- Not crash-safe due to the lack of recovery mechanisms 
- Entire filesystem is rewritten on each sync, which may be inefficient for large datasets

## Usage
Basic command-line interface for interacting with the filesystem:
#### Initialize Filesystem
```bash
./filefs -f myfs.img
```
#### Add file to root
```bash
./filefs -a file.txt -f myfs.img
```
#### Add File to Directory
```bash
./filefs -a file.txt -d docs -f myfs.img
```
#### List Files
```bash
./filefs -l -f myfs.img
````
#### Extract Files
```bash
./filefs -e file.txt -f myfs.img
```
#### Remove File
```bash
./filefs -r file.txt -f myfs.img
```

#### Example Structure

```
/root (directory)
├── hello.txt (file)
└── docs (directory)
    └── notes.txt (file)
```
---
## Filesystem Limitations 
- No concurrent access handling
- No crash recovery
- Fixed block size
- Linear search for inodes and blocks (O(n))
- No caching layer
## Filesystem Future Improvements
- Bitmap based allocation
- File permissions/timestamps 
- Caching layer
## What I Learned

- Tradeoffs between filesystem allocation strategies
- Importance of separating metadata from data storage
- Challenges of debugging memory-mapped systems
- Designing deterministic and testable low-level systems