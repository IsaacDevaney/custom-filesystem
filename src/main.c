#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string.h>

#include <errno.h>

#include <sys/mman.h>

#include "filesystem.h"

int zerosize(int fd);
void exitusage(char* pname);

/* types */
#define FS_MAGIC 0xf0f03410

int main(int argc, char** argv){

  int opt;
  int create = 0;
  int list = 0;
  int add = 0;
  int remove = 0;
  int extract = 0;
  int mydebug = 0;
  char* toadd = NULL;
  char* toremove = NULL;
  char* toextract = NULL;
  char* fsname = NULL;
  char* target_dir = NULL;
  int fd = -1;
  int newfs = 0;
  int filefsname = 0;


  while ((opt = getopt(argc, argv, "lDa:r:e:f:d:")) != -1) {
    switch (opt) {
    case 'l':
      list = 1;
      break;
    case 'a':
      add = 1;
      toadd = strdup(optarg);
      break;
    case 'r':
      remove = 1;
      toremove = strdup(optarg);
      break;
    case 'D':
      mydebug = 1;
      break;
    case 'f':
      filefsname = 1;
      fsname = strdup(optarg);
      break;
    case 'e':
      extract = 1;
      toextract = strdup(optarg);
      break;
    case 'd':
      target_dir = optarg;
      break;
    default:
      exitusage(argv[0]);
    }
  }

  if (!filefsname){
    exitusage(argv[0]);
  }

  if ((fd = open(fsname, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR)) == -1){
    perror("open failed");
    exit(EXIT_FAILURE);
  }
  else{
    if (zerosize(fd)){
      newfs = 1;
    }

    if (newfs)
      if (lseek(fd, FSSIZE-1, SEEK_SET) == -1){
	perror("seek failed");
	exit(EXIT_FAILURE);
      }
      else{
	if(write(fd, "\0", 1) == -1){
	  perror("write failed");
	  exit(EXIT_FAILURE);
	}
      }
  }


  mapfs(fd);
  //printf("Value of newfs is %d\n", newfs);
  if (newfs){
    //printf("About to call formatfs()\n");
    formatfs();
    mysync(fsname);
  }

  loadfs(fsname);

  if (mydebug) {
    printfs_dsk(fsname);
  }

  if (add){
    addfilefs(toadd,target_dir);
    mysync(fsname);
  }

  if (remove){
    removefilefs(toremove);
    mysync(fsname);
  }

  if (extract){
    extractfilefs(toextract);
  }

  if(list){
    lsfs(fsname);
  }

  unmapfs();

  return 0;
}


int zerosize(int fd){
  struct stat stats;
  fstat(fd, &stats);
  if(stats.st_size == 0)
    return 1;
  return 0;
}

void exitusage(char* pname){
  fprintf(stderr, "Usage %s [-l] [-a path] [-e path] [-r path] [-d directory] [-D] -f name\n", pname);
  exit(EXIT_FAILURE);
}