#include <libgen.h>
#include <string.h>
#include "blocks.h"
#include <threads.h>

//global variables - contain the data of the loaded ufs
pinode inodes;
pblock blocks;
psb sblock; 
myopenfile openfiles[MAX_OPEN_FILES];

thread_local int myErrno = 0;


char* myPerror (char* user_comment){
    switch (myErrno)
    {
    case 1:
        printf("%s: malloc failed, allocation error.\n", user_comment);
        break;
    
    case 2:
        printf("%s: Couldn't find free inode\n", user_comment);

    case 3:
        printf("%s: Couldn't find free block\n", user_comment);

    case 4:
        printf("%s: Next block isn't free\n", user_comment);

    case 5:
        printf("%s: First block doesn't have next block\n", user_comment);
        
    case 6:
        printf("%s: Couldn't open file with 'w' mode.\n", user_comment); 

    case 7:
        printf("%s: Couldn't open file with 'r' mode.\n", user_comment);

    case 8:
        printf("%s: Realloc failure, allocation error.\n", user_comment); 

    case 9:
        printf("%s: Not root directory.\n", user_comment);

    case 10:
        printf("%s: couldn't find existing directory.\n", user_comment); 

    case 11:
        printf("%s: myreaddir failed, folder does not exists.\n", user_comment);    

    case 12:
        printf("%s: couldn't get an absolute path of directory.\n", user_comment);    

    case 13:
        printf("%s: Folder is at max capacity.\n", user_comment);

    case 14:
        printf("%s: File isn't exist, O_CREATE flag is false.\n", user_comment);        

    default:
        printf("The error is not recognized by myErrno, try to use the default perror function.\n");
        break;
    }
}


void mymkfs(size_t size){

    int inum = ((size - sizeof(sb)) * 0.1) /sizeof(inode);
    int bnum = ((size - sizeof(sb)) *0.9) / BLOCK_SIZE;
    sblock = (psb) malloc(sizeof(sb));
    inodes = (pinode) malloc(sizeof(inode) * inum);
    blocks = (pblock) malloc(sizeof(block) * bnum);
    if(!sblock || !inodes || !blocks){
        myErrno = 1;
        perror("globals init");
        exit(1);
    }
    // 10% of the space is allocted to inodes, and 90% is saved for blocks
    sblock->ufs_size = size;
    sblock->inode_num = inum;
    sblock->block_num = bnum;
    sblock->used_blocks = 0;
    sblock->used_inodes = 0;

    //initalize the global variables

    for(int i = 0; i < sblock->inode_num; i++){
        inodes[i].isDir = 0;
        inodes[i].first_block = FREE_INODE_BLOCK_VAL;
        inodes[i].blocks_num = 0;
        inodes[i].size = 0;
    }
    
    for(int i = 0 ; i < sblock->block_num; i++){
        blocks[i].next = FREE_BLOCK;
    }

    for(int i = 0 ; i < MAX_OPEN_FILES; i++){
        openfiles[i].inode_id = -1;
        openfiles[i].pos = 0;

    }
    //create the root diretory
    int rootInode = create_file(0,"root");
    inodes[rootInode].isDir = 1;
    inodes[rootInode].size = sizeof(myDirnet);

    char * rootDir = (char*) malloc(sizeof(myDirnet));
    for(int i = 0 ; i < MAX_FILES_PER_DIR; i++){
        ((myDirnet*)rootDir)->inodes[i] = -1;
    }
    ((myDirnet*)rootDir)->files_num = 0;
    strcpy(((myDirnet*)rootDir)->d_name, "root");

    //wrtie the root dir to the ufs
    for(int i = 0; i < sizeof(myDirnet); i++){
        writebyte(rootInode, i, rootDir[i]);
    }
}

int create_file(size_t size, char * fname){

    //calculate how many blocks are needed for the file
    int needed_blks = (size / BLOCK_SIZE) + 1;
    
    int node = find_free_inode();
    int blk_id = find_free_block(); // get the first block

    if( node < 0){
        //myErrno = 2 - set in find_free_inode
        return -1;
    }

    if(blk_id < 0 ){
        //myErrno = 3-set in find_free_block
        return -1;
    }

    inodes[node].isDir = 0;
    inodes[node].first_block = blk_id;
    inodes[node].blocks_num = 1;
    strcpy(inodes[node].name,fname);

    //allocate the rest of the blocks (if there are more)
    for(int i = 1 ; i < needed_blks; i++){

        blocks[blk_id].next = find_free_block();
        if(blocks[blk_id].next < 0 ){
            myErrno = 4;
            return -1;
        } 
        inodes[node].blocks_num++;
        blk_id = blocks[blk_id].next;
    }

    return node;
}

int find_free_inode(){

    for(int i = 0 ; i < sblock->inode_num; i++){
        if(inodes[i].first_block == FREE_INODE_BLOCK_VAL){
            inodes[i].first_block = TAKEN_INODE_BLOCK_VAL;
            return i;
        }
    }
    myErrno = 2;
    return -1;
}

int find_free_block(){

    for(int i =0; i < sblock->block_num; i++){
        if(blocks[i].next == FREE_BLOCK){
            blocks[i].next = LAST_BLOCK;
            return i;
        }
    }
    myErrno = 3;
    return -1;
}

void ufs_stats(){
    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
    printf("Ufs size: %ld\nTotal blocks: %ld  used: %ld\n",sblock->ufs_size, sblock->block_num, sblock->used_blocks);
    printf("Total inodes: %ld  in use: %ld\n", sblock->inode_num, sblock->used_inodes);
    printf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");

}

int readbyte(int inode_id, int pos, char * buf){

    //traverse to the correct block
    int blk_no = pos / BLOCK_SIZE;
    int curr_blk = inodes[inode_id].first_block;
    
    for(int i = 1 ; i< blk_no ; i++){
        if(curr_blk < 0){
            myErrno = 5;
            return -1;
        }
        curr_blk = blocks[curr_blk].next;
    }

    *buf = blocks[curr_blk].data[pos % BLOCK_SIZE];
    return 0;
}

int writebyte(int inode_id, int pos, char toWrite){
    
    int blk_no = pos / BLOCK_SIZE;
    int curr_blk = inodes[inode_id].first_block;
    
    if(curr_blk < 0){//first block should always exist
        myErrno = 5;
        return -1;
    }

    for(int i = 1; i < blk_no; i++){

        if(curr_blk == LAST_BLOCK){// file is to small, we add new blocks
            for(; i <blk_no; i++){
                int new_block = find_free_block();
                if(new_block < 0){
                    //myErrno = 3 -set in find_free_block
                    return -1;
                }
                blocks[curr_blk].next = new_block;
                inodes[inode_id].blocks_num++;
                curr_blk = new_block;
            }
            break;
        }

        curr_blk = blocks[curr_blk].next;
    }

    blocks[curr_blk].data[pos % BLOCK_SIZE] = toWrite;
    return 0;
}

int mymount(const char *source, const char * target, const char *filesystemtype, unsigned long mountflags, const void* data){

    if(target){

        FILE * fp = fopen(target,"w");
        if(!fp){
            myErrno = 6;
            perror("mount");
            exit(1);
        }
        //write the loaded ufs globals to the file
        fwrite(sblock, sizeof(sb), 1,  fp);
        fwrite(inodes, sizeof(inode)  ,sblock->inode_num ,fp);
        fwrite(blocks, sizeof(block) , sblock->block_num ,fp);
        fclose(fp);
    }

    if(source){

        FILE * fp = fopen(source,"r");
        if(!fp){
            myErrno = 7;
            perror("mount");
            exit(1);
        }
        //read the ufs from the file to the program
        void *ptr;
        ptr = realloc(sblock,sizeof(sb));
        if(!ptr){
            myErrno = 8;
            perror("Mount-realloc");
            exit(1);
        }
        fread(sblock, sizeof(sb), 1,  fp);

        ptr = realloc(inodes,sizeof(inode) * sblock->inode_num);
        if(!ptr){
            myErrno = 8;
            perror("Mount-realloc");
            exit(1);
        }
        ptr = realloc(inodes,sizeof(block) * sblock->block_num);
        if(!ptr){
            myErrno = 8;
            perror("Mount-realloc");
            exit(1);
        }

        fread(inodes, sizeof(inode)  ,sblock->inode_num ,fp);
        fread(blocks, sizeof(block) , sblock->block_num ,fp);
        fclose(fp);
    }

    return 0;
}


int get_dir_by_path(const char * path){

    char ppath[strlen(path)]; // so the -Wall flag will leave me alone
    strcpy(ppath,path);

    char * tok =  strtok(ppath, "/"); //should be "root"
    if(strcmp(tok , "root") != 0){
        myErrno = 9;
        return -1;
    }
    int dir_inode = 0; //since the first one is the root

    myDirnet * currdir = (myDirnet*)(blocks[0].data);
    strtok(NULL, "/");
    while(!tok){
        
        int found = 0;
        //check if the next token is a valid directory inside the current dir
        for(int i = 0 ; i < currdir->files_num; i++){
            int curr_inode = currdir->inodes[i];
            if(strcmp(tok, inodes[curr_inode].name)){
                dir_inode = curr_inode;
                currdir = (myDirnet*)(blocks[inodes[curr_inode].first_block].data);
                found = 1;
                break;
            }
        }
        if(!found){ //this function can only handle existing dirs
            myErrno = 10;
            return -1;
        }

    strtok(NULL, "/");
    }
    return dir_inode;
}

int create_dir(const char *path, const char* name){

    int lowest_dir_inode = get_dir_by_path(path);
    if( lowest_dir_inode < 0){
        //myErrno = 10 or 9
        printf("Parent file does not exists\n");
        return -1;
    }
    //init the new dir's dirnet and inode
    myDirnet *newdir = (myDirnet*) malloc(sizeof(myDirnet));
    for(int i = 0 ; i < MAX_FILES_PER_DIR; i++){
        newdir->inodes[i] = -1;
    }
    strcpy(newdir->d_name, name);
    newdir->files_num = 0;

    int new_inode = find_free_inode();
    int new_block = find_free_block();
    if(new_block < -1 ){
        //myErrno = 3 -set in find_free_block
        return -1;
    }

    if ( new_inode < -1)
    {
        //myErrno = 2 -set in find_free_inode
        return -1;
    }
    

    inodes[new_inode].blocks_num = 1;
    inodes[new_inode].first_block = new_block;
    inodes[new_inode].isDir = 1;
    strcpy(inodes[new_inode].name, name);
    
    for(int i = 0 ; i < sizeof(myDirnet); i++){
        writebyte(new_inode, i, ((char*)newdir)[i]);
    }

    //read the parent dir
    myDirnet * parent = (myDirnet*) malloc(sizeof(myDirnet));
    for(int i = 0 ; i < sizeof(myDirnet); i++){
        readbyte(lowest_dir_inode, i, ((char*)parent) + i);
    }

    
    // add the new dir to its parent dirnet struct
    for(int i = 0; i <MAX_FILES_PER_DIR; i++){
        if(parent->inodes[i] == -1){
            parent->inodes[i] = new_inode;
            break;
        }
    }
    parent->files_num++;
    //write it back to memory
    for(int i = 0 ; i < sizeof(myDirnet); i++){
        writebyte(lowest_dir_inode, i, ((char*)parent)[i]);
    }

    free(parent);
    free(newdir);

    return 0;
}

int get_free_fd(int inode){

    for(int i = 0 ; i < MAX_OPEN_FILES; i++){
        if(openfiles[i].inode_id == -1){
            openfiles[i].inode_id = inode;
            return i;
        }
    }

    myErrno = 12;
    return -1;
}

myDir * myopendir(const char * path){
    int dir_inode = get_dir_by_path(path);
    int dir_fd = get_free_fd(dir_inode);
    
    if(dir_inode < 0 || dir_fd){
        //myErrno = 12 - dir_fd failed
        //myErrno = 9 or 10 - dir_inode failed
        printf("failed at myopendir\n");
        return NULL;
    }

    myDir * d = (myDir*) malloc(sizeof(myDir));
    d->fd = dir_fd;
    return d;
}

int myclosedir(myDir *p){
    openfiles[p->fd].inode_id = -1;
    openfiles[p->fd].pos = 0;
    free(p);

    return 0;
}


myDirnet* myreaddir(myDir * dp){

    int inode = openfiles[dp->fd].inode_id;
    myDirnet * dir = (myDirnet*) malloc(sizeof(myDirnet));
    if(!dir || !dp){
        myErrno = 1;
        return NULL;
    }
    for(int i = 0 ; i < sizeof(myDirnet); i++){
        readbyte(inode, i, ((char*)dir) + i);
    }

    return dir;
}
int myopen(const char * pathname, int flags){

    char * fullpath = (char*)malloc(strlen(pathname));
    strcpy(fullpath, pathname);    
    //split the path ans the file name
    char * fname = (char*)malloc(strlen(pathname));
    fname = basename(fullpath);
    *(fname - 1) = '\0';
    
    
    //find the file in the parent dir
    myDir * dir = myopendir(pathname);
    myDirnet * parent = myreaddir(dir);
    if(!parent){
        myErrno = 11;
        printf("Parent folder does not exists\n");
        return -1;
    }
    if(parent->files_num == MAX_FILES_PER_DIR){
        myErrno = 13;
        printf("parent folder is at max capacity\n");
        return -1;
    }
    for(int i = 0; i < MAX_FILES_PER_DIR; i++){
        if(strcmp(fname, inodes[parent->inodes[i]].name) == 0){
            int fd = get_free_fd(parent->inodes[i]);
            if(fd < 0){
                // myErrno = 12 - set at get_free_fd 
                printf("no fd left\n");
                return -1;
            }
            parent->files_num++;
            //write the new parent dirnet back to the memory 
            mylseek(dir->fd, 0, SEEK_SET);  
            mywrite(dir->fd, parent, sizeof(myDirnet));
            return fd;
        }
    }

    // If we got here the file does not exists, so we will create it
    if( flags != O_CREATE){
        myErrno = 14;
        return -1;
    }
    int new_inode_id = create_file(1, fname);
    int fd = get_free_fd(new_inode_id);
    if(fd < 0 || new_inode_id < 0){
        // myErrno = 12 - set on get_free_fd
        // myErrno = 2 or 3 or 4 - set on create_file 
        return -1;
    }
    openfiles[fd].inode_id = new_inode_id;
    openfiles[fd].pos = 0;
    // add the new file to it's parent folder
    for(int i = 0 ; i < MAX_FILES_PER_DIR; i++){
        if(parent->inodes[i] == -1){
            parent->inodes[i] = new_inode_id;
            break;
        }
    }
    parent->files_num++;
    //write the new parent dirnet back to the memory
    mylseek(dir->fd, 0, SEEK_SET);  
    mywrite(dir->fd, parent, sizeof(myDirnet));
    return fd;
}

int myclose(int fd){
    openfiles[fd].inode_id = -1;
    openfiles[fd].pos = 0;

    return 0;
}

ssize_t myread(int fd, void *buf, size_t count){

    myopenfile ofile = openfiles[fd];
    int bytesread = 0;
    for(int i = ofile.pos; i < ofile.pos + count; i++){
        if((readbyte(ofile.inode_id,i, ((char*)buf +(i -ofile.pos))) < 0)){
            openfiles[fd].pos += bytesread;
            return bytesread;
        };
        bytesread++;
    }
    openfiles[fd].pos += bytesread;
    return bytesread;
}

ssize_t mywrite(int fd, const void *buf, size_t count){

    myopenfile ofile = openfiles[fd];
    int byteswritten = 0, orig_size = inodes[ofile.inode_id].size;
    for(int i = ofile.pos; i < ofile.pos + count; i++){

        if(writebyte(ofile.inode_id, i,((char*)buf)[i]) < 0 ){
            openfiles[fd].pos += byteswritten;
            return byteswritten;
        }

    byteswritten++;
    if(i >= orig_size){
        inodes[ofile.inode_id].size++;
        }
    }
    openfiles[fd].pos += byteswritten;
    return byteswritten;
}

off_t mylseek(int fd, off_t offset, int whence){

    myopenfile * ofile = &openfiles[fd];
    off_t oldpos = ofile->pos;
    switch(whence){

        case SEEK_SET:
            if(offset < 0 || offset > inodes[ofile->inode_id].blocks_num * BLOCK_SIZE){
                return (off_t) -1;
            }
            ofile->pos = offset;
            return offset;

        case SEEK_CUR:

            if((oldpos + offset) < 0 || (oldpos + offset) > inodes[ofile->inode_id].blocks_num * BLOCK_SIZE){
                return (off_t) -1;
            }       

            ofile->pos += offset;
            return ofile->pos;
            

        case SEEK_END:

            if( offset > 0 || (offset * -1) > inodes[ofile->inode_id].blocks_num * BLOCK_SIZE){
                return (off_t) -1;
            }       

            ofile->pos = (inodes[ofile->inode_id].blocks_num * BLOCK_SIZE) + offset;
            return ofile->pos;
        
        default:
            break;
    }
            return (off_t) -1;
}