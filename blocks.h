#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#define BLOCK_SIZE 4096
#define MAX_FILES_PER_DIR 10
#define MAX_FILENAME 256
#define MAX_OPEN_FILES 10000
#define FREE_INODE_BLOCK_VAL -1
#define TAKEN_INODE_BLOCK_VAL -2 // used untill the first block is assigned to the inode
#define FREE_BLOCK -2
#define LAST_BLOCK -1
#define O_CREATE 10

/*
Here we defined the basic building blocks and functions of the file system
*/
typedef struct superblock{

    size_t inode_num, block_num;
    size_t used_blocks, used_inodes;
    size_t ufs_size;
}sb, *psb; 


typedef struct block{

    int next; //the next block of the file.
    char data[BLOCK_SIZE];

}*pblock,block;

typedef struct inode{

    char name[MAX_FILENAME];
    int first_block; 
    int isDir;
    int blocks_num;
    int size;

}inode, *pinode;

typedef struct myDirnet{

    size_t files_num;
    int inodes[MAX_FILES_PER_DIR];
    char d_name[MAX_FILENAME];
}myDirnet;

typedef struct myDir{
    int fd;
}myDir;

typedef struct myopenfile{
    int pos, inode_id; 
}myopenfile;



// functions

/*
This function is used to created a new ufs in file.
size - the required size for the file system 
*/
void mymkfs(size_t size);



//////// Ufs functions /////////


/*
this function is used to load and export file systems
target - if target is supplied, the loaded ufs will be exported into file with the target name
source - if source is supplied, we will load the ufs presented in the source file
The rest of the flags are igonred in this implementation
*/
int mymount(const char *source, const char * target, const char *filesystemtype, unsigned long mountflags, const void* data);

int myopen(const char * path, int flags);

//close files that was opend before by myopen
int myclose(int fd);

//This function receives an absolute path to directory, and return it's myDir structure
myDir * myopendir(const char * path);

//close myDir handles that was opened by myopendir
int myclosedir(myDir *p);

//read 'count' bytes from the file represented by fd to buf
ssize_t myread(int fd, void *buf, size_t count);

//write 'count' bytes from buf to file associated with fd
ssize_t mywrite(int fd, const void *buf, size_t count);
off_t mylseek(int fd, off_t offset, int whence);

//////// Helper functions ////////

/*
create a file with a given name and size, return file's inode;
*/
int create_file(size_t size, char * fname);

// return the first free inode , returns -1 on failure
int find_free_inode();

//return the first free block , returns -1 on failure

int find_free_block();

//prints ufs stats from the super block
void ufs_stats();

//reads a single byte from inode's data, using pos to calculate the offest 
int readbyte(int inode_id, int pos, char * buff);

/*
write a single byte into inode's data, using pos to calculate the correct block and offset
if the new byte position is beyoned the scope of the current file, we will enlagrge it
*/
int writebyte(int inode_id, int pos, char ch);

//get an absolute path of directory, and return its inode
int get_dir_by_path(const char * path);

int create_dir(const char *path, const char* name);

int get_free_fd(int inode);

//if the user would like to know what error occured according to our errno values
//then the user can use this function to print the description string of the error.
char* myPerror (char* user_comment);