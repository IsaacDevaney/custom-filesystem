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
//extern unsigned char* myfsys;

struct super_block {
    int inodes;
    int blocks;
};

struct disk_block {
    int next;
    char data[BLOCK_SIZE];
};

struct inode {
    int next;
    char name[NAME_SIZE];
    int dir; // 0 for file 1 for dir
    int size;
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
void addfilefs(char* fname);
void removefilefs(char* fname);
void extractfilefs(char* fname);
void mysync(const char*);
void printfs_dsk(char*);
void printfd(int);
int myopen(const char *pathname, int flags);
int myclose(int myfd);
//File system I/O
//
int mycreatefile(const char *path, const char* name);
myDIR* myopendir(const char *name);
struct mydirent *myreaddir(myDIR* dirp);
int myclosedir(myDIR* dirp);
int mymkdir(const char *path, const char* name);
// print a single file from our fs with a given fd
void printdir(const char*);
#endif