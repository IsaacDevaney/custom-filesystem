#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <sys/mman.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FSSIZE 10000000
#define BLOCK_SIZE 512
#define NAME_SIZE 255
#define MAX_DIR_SIZE 10
#define MAX_FILES 100
#define O_CREAT 0100
#define MAX_FILE_BLOCKS 64
#define MAX_FILE_SIZE   (MAX_FILE_BLOCKS * BLOCK_SIZE)

#define BLOCK_FREE -1
#define BLOCK_END  -2

struct super_block {
    int inodes;
    int blocks;
};

struct disk_block {
    int next;
    char data[BLOCK_SIZE];
};

struct inode {
    int first_block;
    int size;
    int is_directory;
    int parent_inode;
    char name[NAME_SIZE];
};

struct mydirent {
    int size;
    int fds[MAX_DIR_SIZE];
    char d_name[NAME_SIZE];
};

struct myopenfile {
    int fd;
    int pos;
};

typedef struct myDIR {
    int fd;
}myDIR;

void createroot();
void writebyte(int, int, char);
int allocate_file(int, const char*);
int find_empty_inode();
int find_empty_block();

void mapfs(int fd);
void unmapfs();
void formatfs();
void loadfs(const char*);
void lsfs(char* fname);
void addfilefs(char* fname, char* target_dir);
void removefilefs(char* fname);
void extractfilefs(char* fname);
void mysync(const char*);
void printfs_dsk(char*);
void printfd(int);
int myopen(const char *pathname, int flags);
int myclose(int myfd);
//File system I/O

int mycreatefile(const char *path, const char* name);
myDIR* myopendir(const char *name);
struct mydirent *myreaddir(myDIR* dirp);
int myclosedir(myDIR* dirp);
int mymkdir(const char *path, const char* name);


int block_alloc(void);
void block_free_chain(int start_block);
int block_read(int block_id, char *buffer);
int block_write(int block_id, const char *buffer);
int block_next(int block_id);
void block_set_next(int block_id, int next_block);
void printdir(const char*);

int block_read_byte(int block_id, int offset, char *out);
int block_write_byte(int block_id, int offset, char value);
void *block_data_ptr(int block_id);
#endif