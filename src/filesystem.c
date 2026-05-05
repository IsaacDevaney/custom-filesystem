#include "filesystem.h"
#include <string.h>
#include <libgen.h>

struct super_block super_block;
struct inode *inodes;
struct disk_block *disk_blocks;
struct myopenfile openfiles[MAX_FILES];
unsigned char* fs;

void mapfs(int fd){
  if ((fs = mmap(NULL, FSSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == NULL){
      perror("mmap failed");
      exit(EXIT_FAILURE);
  }
}


void unmapfs(){
  munmap(fs, FSSIZE);
}


void formatfs(){
   printf("Formatting file\n");
   int size_without_superblock = FSSIZE - sizeof(struct super_block);
   super_block.inodes = (size_without_superblock/10)/(sizeof(struct inode));
   super_block.blocks = (size_without_superblock-size_without_superblock/10)/(sizeof(struct disk_block));

   inodes = malloc(super_block.inodes*sizeof(struct inode));
   disk_blocks = malloc(super_block.blocks*sizeof(struct disk_block));

    for (size_t i = 0; i < super_block.inodes; i++)
    {
        strcpy(inodes[i].name, "");
        inodes[i].first_block = BLOCK_FREE;
        inodes[i].size = 0;
        inodes[i].is_directory = 0;
        inodes[i].parent_inode = -1;
    }

    for (size_t i = 0; i < super_block.blocks; i++)
    {
        strcpy(disk_blocks[i].data, "");
        disk_blocks[i].next = BLOCK_FREE;
    }

    createroot();
}

void mysync(const char* target) {
    /**
     * @brief Will load the UFS that is currently on the memory into a file named target.
     *  The file could be loaded in the future using resync.
     */
    FILE *file;
    file = fopen(target, "w+");
    fwrite(&super_block, sizeof(struct super_block), 1, file);
    fwrite(inodes, super_block.inodes*sizeof(struct inode), 1, file);
    fwrite(disk_blocks, super_block.blocks*sizeof(struct disk_block), 1, file);
    fclose(file);
//    myfsys = target;
}

void createroot() {

    int zerofd = allocate_file(sizeof(struct mydirent), "root");
    if (zerofd != 0 ) {
        errno = 131;
	printf("Error formatting file. Error: %d\n", errno);
        return;// -1;
    }
    inodes[zerofd].is_directory = 1;
    struct mydirent* rootdir = malloc(sizeof(struct mydirent));
    for (size_t i = 0; i < MAX_DIR_SIZE; i++)
    {
        rootdir->fds[i] = -1;
    }
    strcpy(rootdir->d_name, "root");
    rootdir->size = 0;
    char* rootdiraschar = (char*)rootdir;
    for (size_t i = 0; i < sizeof(struct mydirent); i++)
    {
        writebyte(zerofd, i, rootdiraschar[i]);
    }
    free(rootdir);
    inodes[zerofd].is_directory = 1;
    inodes[zerofd].parent_inode = -1;
}

int myopen(const char *pathname, int flags)
{
    char str[80];
    strcpy(str, pathname);

    char *token;
    const char s[2] = "/";
    token = strtok(str, s);

    char currpath[NAME_SIZE] = "";
    char lastpath[NAME_SIZE] = "";

    while (token != NULL) {
        strcpy(lastpath, currpath);
        strcpy(currpath, token);
        token = strtok(NULL, s);
    }

    for (size_t i = 0; i < super_block.inodes; i++) {
        if (!strcmp(inodes[i].name, currpath)) {
            if (inodes[i].is_directory != 0) {
                errno = 21;
                return -1;
            }

            openfiles[i].fd = (int)i;
            openfiles[i].pos = 0;
            return (int)i;
        }
    }

    if (flags != O_CREAT) {
        errno = 2;
        return -1;
    }

    int new_fd = mycreatefile(lastpath, currpath);
    if (new_fd == -1) {
        return -1;
    }

    openfiles[new_fd].fd = new_fd;
    openfiles[new_fd].pos = 0;
    return new_fd;
}

int myclose(int myfd) {
    if (myfd < 0 || myfd >= MAX_FILES) {
        return -1;
    }

    openfiles[myfd].fd = -1;
    openfiles[myfd].pos = -1;
    return 0;
}

void writebyte(int fd, int opos, char data) {
    if (fd < 0 || fd >= super_block.inodes) {
        printf("Invalid file descriptor\n");
        return;
    }

    int pos = opos;
    int rb = inodes[fd].first_block;

   if (rb == BLOCK_FREE)  {
        rb = block_alloc();
        if (rb == BLOCK_FREE) {
            printf("Error allocating first block\n");
            return;
        }

        inodes[fd].first_block = rb;
    }

    while (pos >= BLOCK_SIZE) {
        pos -= BLOCK_SIZE;

        int next = block_next(rb);

        if (next == BLOCK_FREE) {
            printf("Error writing to freed block\n");
            return;
        }

        if (next == BLOCK_END) {
            int new_block = block_alloc();
            if (new_block == BLOCK_FREE) {
                printf("Error allocating new block\n");
                return;
            }

            block_set_next(rb, new_block);
            rb = new_block;
        } else {
            rb = next;
        }
    }

    if (block_write_byte(rb, pos, data) == -1) {
        printf("Error writing byte to block\n");
        return;
    }

    if (opos >= inodes[fd].size) {
        inodes[fd].size = opos + 1;
    }
}

int allocate_file(int size, const char* name) {
    /**
     * @brief This function will allocate new inode and enough blocks for a new file.
     * (One inode is allocated, the amount of needed blocks is calculated)
     *
     */
    if (strlen(name) >= NAME_SIZE) {
        errno = 36;
        return -1;
    }
    int inode = find_empty_inode();
    if (inode == -1) {
        errno = 28;
        return -1;
    }
    int curr_block = block_alloc();
    if (curr_block == BLOCK_FREE) {
        return -1;
    }
    inodes[inode].size = size;
    inodes[inode].first_block = curr_block;
    inodes[inode].is_directory = 0;
    inodes[inode].parent_inode = 0;
    block_set_next(curr_block, BLOCK_END);
    strcpy(inodes[inode].name, name);
    if (size>BLOCK_SIZE) {  // REQUIRES TESTS
        int allocated_size = -(3*BLOCK_SIZE)/4;
        //int bn = size/BLOCK_SIZE;
        int next_block;
        while (allocated_size<size)
        {
            next_block = block_alloc();
            if (next_block == BLOCK_FREE) {
                errno = 28;
                return -1;
            }
            block_set_next(curr_block, next_block);
            curr_block = next_block;
            allocated_size+=BLOCK_SIZE;
        }
    }
    block_set_next(curr_block, BLOCK_END);

    return inode;
}
int block_alloc(void) {
    int block = find_empty_block();

    if (block == BLOCK_FREE) {
        errno = 28;
        return -1;
    }

    disk_blocks[block].next = BLOCK_END;
    memset(disk_blocks[block].data, 0, BLOCK_SIZE);

    return block;
}

void block_free_chain(int start_block) {
    int block = start_block;

    while (block != BLOCK_FREE && block != BLOCK_END) {
        int next = disk_blocks[block].next;

        disk_blocks[block].next = BLOCK_FREE;
        memset(disk_blocks[block].data, 0, BLOCK_SIZE);

        block = next;
    }
}

int block_read(int block_id, char *buffer) {
    if (block_id < 0 || block_id >= super_block.blocks || buffer == NULL) {
        return -1;
    }

    memcpy(buffer, disk_blocks[block_id].data, BLOCK_SIZE);
    return 0;
}

int block_write(int block_id, const char *buffer) {
    if (block_id < 0 || block_id >= super_block.blocks || buffer == NULL) {
        return -1;
    }

    memcpy(disk_blocks[block_id].data, buffer, BLOCK_SIZE);
    return 0;
}

int block_next(int block_id) {
    if (block_id < 0 || block_id >= super_block.blocks) {
        return -1;
    }

    return disk_blocks[block_id].next;
}

void block_set_next(int block_id, int next_block) {
    if (block_id < 0 || block_id >= super_block.blocks) {
        return;
    }

    disk_blocks[block_id].next = next_block;
}

/*
 * Deterministic first-fit allocation:
 * scans from the beginning of the block table and returns
 * the lowest-numbered free block.
 */
int find_empty_block(void) {
    for (int i = 0; i < super_block.blocks; i++) {
        if (disk_blocks[i].next == BLOCK_FREE) {
            return i;
        }
    }

    return BLOCK_FREE;
}
int find_empty_inode() {
    for (size_t i = 0; i < super_block.inodes; i++)
    {
        if (inodes[i].first_block == -1) {
            return i;
        }

    }

    return -1;
}

void loadfs(const char* target){
 FILE *file;
 file = fopen(target, "r");
 if (!file) {
      printf("Error loading file system.\n");
      return;// -1;
 }
 fread(&super_block, sizeof(super_block), 1, file);
 inodes = malloc(super_block.inodes*sizeof(struct inode));
 disk_blocks = malloc(super_block.blocks*sizeof(struct disk_block));
 fread(inodes, super_block.inodes*sizeof(struct inode), 1, file);
 fread(disk_blocks, super_block.blocks*sizeof(struct disk_block), 1, file);
 fclose(file);
 printf("File system successfully loaded.\n");
}

void printfs_dsk(char* target) {
    /**
     * @brief Function used for debugging, print information about the UFS from a file on the disk.
     */
    FILE *file;
    file = fopen(target, "r");
    struct super_block temp_super_block;
    fread(&temp_super_block, sizeof(super_block), 1, file);
    struct inode *temp_inodes = malloc(temp_super_block.inodes*sizeof(struct inode));
    struct disk_block *temp_disk_blocks = malloc(temp_super_block.blocks*sizeof(struct disk_block));
    fread(temp_inodes, temp_super_block.inodes*sizeof(struct inode), 1, file);
    fread(temp_disk_blocks, temp_super_block.blocks*sizeof(struct disk_block), 1, file);
    printf("SUPERBLOCK\n");
    printf("\t inodes amount: %d\n\t blocks amount: %d\n", temp_super_block.inodes, temp_super_block.blocks);
    printf("\nINODES\n");
    for (size_t i = 0; i < temp_super_block.inodes; i++)
    {
        printf("%ld.\t name: %s | isdir: %d | next: %d\n",i , temp_inodes[i].name, temp_inodes[i].is_directory ,temp_inodes[i].first_block);
    }

    printf("\nBLOCKS\n");
    for (size_t i = 0; i < temp_super_block.blocks; i++)
    {
        printf("%ld.\t next: %d\n",i , temp_disk_blocks[i].next);
    }


    fclose(file);
    free(temp_disk_blocks);
    free(temp_inodes);
}

void lsfs(char* target){
    printdir("/root/");
}

void printdir(const char* pathname) {
    myDIR* dirp = myopendir(pathname);

    if (dirp == NULL) {
        printf("Error opening directory %s\n", pathname);
        return;
    }
   // int parent_fd = dirp->fd;

    int fd = dirp->fd;

    if (fd < 0 || fd >= super_block.inodes || inodes[fd].is_directory == 0) {
        errno = 20;
        printf("Error attempting to list contents of file system.\n");
        myclosedir(dirp);
        return;
    }

    printf("NAME OF DIRECTORY: %s\n", inodes[fd].name);

    struct mydirent* currdir = myreaddir(dirp);
    if (currdir == NULL) {
        printf("Error reading directory %s\n", pathname);
        myclosedir(dirp);
        return;
    }

    for (size_t i = 0; i < currdir->size; i++) {
        int child_fd = currdir->fds[i];

        if (child_fd < 0 || child_fd >= super_block.inodes) {
            continue;
        }

        if (inodes[child_fd].is_directory == 0) {
            printf("\tfile number %ld: %s \n", i, inodes[child_fd].name);
        } else {
            printf("NAME OF DIRECTORY: %s\n", inodes[child_fd].name);
        }
    }

    myclosedir(dirp);
    printf("\nDONE\n");
}

int mycreatefile(const char *path, const char* name)
{
    char fin[256];

    if (strcmp(path, ".") == 0 || strcmp(path, "/") == 0 || strlen(path) == 0) {
        strcpy(fin, "/root/");
    } else {
        snprintf(fin, sizeof(fin), "/root/%s", path);
    }

    printf("Path is %s\n", fin);

    myDIR* dirp = myopendir(fin);
    if (dirp == NULL) {
        printf("Error opening directory %s\n", fin);
        return -1;
    }

    int parent_fd = dirp->fd;

    struct mydirent *currdir = myreaddir(dirp);
    if (currdir == NULL) {
        printf("Error reading directory %s\n", fin);
        myclosedir(dirp);
        return -1;
    }

    if (currdir->size >= MAX_DIR_SIZE) {
        printf("Directory full\n");
        myclosedir(dirp);
        return -1;
    }

    int newfd = allocate_file(1, name);
    if (newfd == -1) {
        printf("Error creating file %s\n", name);
        myclosedir(dirp);
        return -1;
    }

    inodes[newfd].parent_inode = parent_fd;

    currdir->fds[currdir->size++] = newfd;

    myclosedir(dirp);
    return newfd;
}

void printfd(int fd) {
    if (fd < 0 || fd >= super_block.inodes) {
        printf("Invalid file descriptor\n");
        return;
    }

    int rb = inodes[fd].first_block;
    int bytes_remaining = inodes[fd].size;

    printf("NAME: %s\n", inodes[fd].name);

    while (rb != BLOCK_END && bytes_remaining > 0) {
        if (rb == BLOCK_FREE) {
            printf("Error retrieving file\n");
            return;
        }

        char *data = block_data_ptr(rb);
        if (data == NULL) {
            printf("Error reading block data\n");
            return;
        }

        int bytes_to_print = bytes_remaining < BLOCK_SIZE ? bytes_remaining : BLOCK_SIZE;

        for (int i = 0; i < bytes_to_print; i++) {
            putchar(data[i]);
        }

        bytes_remaining -= bytes_to_print;
        rb = block_next(rb);
    }

    printf("\nDONE %s\n", inodes[fd].name);
}

myDIR* myopendir(const char *pathname) {

    if (pathname == NULL || strcmp(pathname, "/") == 0 || strcmp(pathname, ".") == 0 || strlen(pathname) == 0) {
        myDIR* root = malloc(sizeof(myDIR));
        if (root == NULL) {
            perror("malloc");
            return NULL;
        }

        root->fd = 0;
        return root;
    }

    char str[80];
    strcpy(str, pathname);
    char *token;
    const char s[2] = "/";
    token = strtok(str, s);
    char currpath[NAME_SIZE] = "";
    char lastpath[NAME_SIZE] = "";
    while(token != NULL ) {
        if (token != NULL) {
            strcpy(lastpath, currpath);
            strcpy(currpath, token);
        }
        token = strtok(NULL, s);
    }
    for (size_t i = 0; i < super_block.inodes; i++)
    {
        if (!strcmp(inodes[i].name, currpath)) {
            if (inodes[i].is_directory!=1) {
                errno = 20;
		printf("Error attempting to open directory.\n");
                return NULL;//-1;
            }
            myDIR* newdir = (myDIR*)malloc(sizeof(myDIR));
            newdir->fd = i;
            return newdir;
        }
    }
    myDIR* newdir = (myDIR*)malloc(sizeof(myDIR));
    newdir->fd = mymkdir(lastpath, currpath);
    return newdir;
}

struct mydirent *myreaddir(myDIR* dirp) {
    if (dirp == NULL) {
        return NULL;
    }

    int fd = dirp->fd;

    if (fd < 0 || fd >= super_block.inodes) {
        return NULL;
    }

    if (inodes[fd].is_directory != 1) {
        errno = 20;
        printf("Error attempting to read directory\n");
        return NULL;
    }

    int block = inodes[fd].first_block;

    if (block < 0 || block >= super_block.blocks) {
        return NULL;
    }

    void *data = block_data_ptr(block);
    if (data == NULL) {
        return NULL;
    }

    return (struct mydirent*)data;
}

int myclosedir(myDIR* dirp) {
    if (dirp == NULL) {
        return -1;
    }

    free(dirp);
    return 0;
}
int mymkdir(const char *path, const char* name) {
    /**
     * @brief This function goes through the path and finds the FD of the last directory in the path.
     * Then, it creates a new directory inside the FD that was found.
     */
    myDIR* dirp = myopendir(path);
    if (dirp == NULL) {
        return -1;
    }
    int fd = dirp->fd;
    if (fd==-1) {
        errno = 2;
        return -1;
    }
    if (inodes[fd].is_directory!=1) {
        errno = 20;
        return -1;
    }
    int dirblock = inodes[fd].first_block;
    struct mydirent* currdir = (struct mydirent*)disk_blocks[dirblock].data;
    if (currdir->size>=MAX_DIR_SIZE) {
        errno = 31;
        return -1;
    }
    int newdirfd = allocate_file(sizeof(struct mydirent), name);
    currdir->fds[currdir->size++] = newdirfd;
    inodes[newdirfd].is_directory = 1;
    struct mydirent* newdir = malloc(sizeof(struct mydirent));
    newdir->size = 0;
    for (size_t i = 0; i < MAX_DIR_SIZE; i++)
    {
        newdir->fds[i] = -1;
    }
    strcpy(newdir->d_name, name);
    char *newdiraschar = (char*)newdir;

    for (size_t i = 0; i < sizeof(struct mydirent); i++)
    {
        writebyte(newdirfd, i, newdiraschar[i]);
    }
    myclosedir(dirp);
    return newdirfd;
}

void addfilefs(char* fname) {
    char *dirc = strdup(fname);
    char *basec = strdup(fname);

    if (dirc == NULL || basec == NULL) {
        perror("strdup");
        free(dirc);
        free(basec);
        return;
    }

    char *bname = basename(basec);
    char *dname = dirname(dirc);

    printf("Adding directory: %s, file: %s\n", dname, bname);

    int fd = mycreatefile(dname, bname);
    if (fd == -1) {
        free(dirc);
        free(basec);
        return;
    }

    FILE *src = fopen(fname, "rb");
    if (src == NULL) {
        perror("fopen");
        free(dirc);
        free(basec);
        return;
    }

    int pos = 0;
    int ch;
    while ((ch = fgetc(src)) != EOF) {
        writebyte(fd, pos++, (char)ch);
    }

    fclose(src);
    free(dirc);
    free(basec);
}

void removefilefs(char* fname) {
    printf("Removing file %s\n", fname);

    int file_fd = -1;

    for (size_t i = 0; i < super_block.inodes; i++) {
        if (strcmp(inodes[i].name, fname) == 0) {
            file_fd = (int)i;
            break;
        }
    }

    if (file_fd == -1) {
        printf("File not found: %s\n", fname);
        return;
    }

    myDIR* dirp = myopendir("/root/");
    if (dirp == NULL) {
        printf("Error opening root directory\n");
        return;
    }

    struct mydirent* currdir = myreaddir(dirp);
    if (currdir == NULL) {
        printf("Error reading root directory\n");
        myclosedir(dirp);
        return;
    }

    int found_in_dir = 0;

    for (int i = 0; i < currdir->size; i++) {
        if (currdir->fds[i] == file_fd) {
            found_in_dir = 1;

            for (int j = i; j < currdir->size - 1; j++) {
                currdir->fds[j] = currdir->fds[j + 1];
            }

            currdir->fds[currdir->size - 1] = -1;
            currdir->size--;
            break;
        }
    }

    if (!found_in_dir) {
        printf("File inode found, but not listed in root directory\n");
        myclosedir(dirp);
        return;
    }

    block_free_chain(inodes[file_fd].first_block);
    inodes[file_fd].first_block = -1;
    inodes[file_fd].size = 0;
    inodes[file_fd].is_directory = 0;
    strcpy(inodes[file_fd].name, "");

    myclosedir(dirp);

    printf("Removed file %s\n", fname);
}
void extractfilefs(char* fname){
    int fd = myopen(fname, 0);

    if (fd == -1) {
        printf("Error retrieving file %s\n", fname);
        return;
    }

    printfd(fd);
    myclose(fd);
}

int block_read_byte(int block_id, int offset, char *out) {
    if (block_id < 0 || block_id >= super_block.blocks) {
        return -1;
    }

    if (offset < 0 || offset >= BLOCK_SIZE) {
        return -1;
    }

    if (out == NULL) {
        return -1;
    }

    *out = disk_blocks[block_id].data[offset];
    return 0;
}

int block_write_byte(int block_id, int offset, char value) {
    if (block_id < 0 || block_id >= super_block.blocks) {
        return -1;
    }

    if (offset < 0 || offset >= BLOCK_SIZE) {
        return -1;
    }

    disk_blocks[block_id].data[offset] = value;
    return 0;
}

void *block_data_ptr(int block_id) {
    if (block_id < 0 || block_id >= super_block.blocks) {
        return NULL;
    }

    return disk_blocks[block_id].data;
}