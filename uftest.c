#include <assert.h>
#include <libgen.h>
#include <string.h>
#include "blocks.h"

#define FS_SIZE (1024 * 1024)

extern pinode inodes;
extern pblock blocks;
extern psb sblock; 

int main(){

    
    mymkfs(FS_SIZE);
    
    printf("root inode: %d\n", get_dir_by_path("root"));


    if(create_dir("root","os") < 0){
        printf("failed\n");
    }

    //Test myopen and 
    int fd = myopen("root/hi.txt", 0);

    myDirnet * r = malloc(sizeof(myDirnet));
    for(int i = 0; i < sizeof(myDirnet);i++){
        readbyte(get_dir_by_path("root"),i , ((char*)r) + i);
    }

    printf("root contains %ld files and dirs:\n", r->files_num);
    for(int i = 0 ; i < MAX_FILES_PER_DIR; i++){
        if(r->inodes[i] != -1){
            printf("%s\n" ,inodes[r->inodes[i]].name);
        }
    }
    assert(r->files_num == 2);

    //test myred and mywrite
    char word[] = "hello";
    mywrite(fd, word, strlen(word) + 1);
    word[0] = '\0';
    mylseek(fd, 0, SEEK_SET);
    myread(fd, word, 6);

    assert(strcmp(word, "hello") == 0);

    ufs_stats();
    
}