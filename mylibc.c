#include <string.h>
#include <stdarg.h>
#include "blocks.h"
#include "mystdlib.h"
#include <threads.h>

#define MIN(a,b) ((a < b) ? (a) : (b))
thread_local int myErrno = 0;
extern pinode inodes;
extern pblock blocks;
extern psb sblock;
extern myopenfile openfiles[MAX_OPEN_FILES];

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

    case 15:
        printf("%s: myfopen - no flag exists (r,r+,w,a).\n", user_comment);            

    case 16:
        printf("%s: Couldn't open file with given flag.\n", user_comment); 

    case 17:
        printf("%s: Unffited open mode.\n", user_comment); 
    
    case 18:
        printf("%s: The offset is outside it's bounds.\n", user_comment); 

    case 19:
        printf("%s: Unknown flag.\n", user_comment);     
    
    default:
        printf("The error is not recognized by myErrno, try to use the default perror function.\n");
        break;
    }
}

myFILE * myfopen(const char * pathname, const char * mode){

    //check that the flag exists
    if(strcmp(mode,"r") != 0 && strcmp(mode,"r+") != 0 && strcmp(mode, "w") != 0 && strcmp(mode, "a") != 0){
        myErrno = 15;
        return NULL;
    }
    int flag = 0;
    if(!strcmp(mode,"w")){
        flag = O_CREATE;
    }
    //we open the file in the ufs and buffer its data in an instance of myFILE struct
    int fd = myopen(pathname, flag);
    if(fd < 0){
        myErrno = 16;
        return NULL;
    }
    myFILE * fp = (myFILE *) malloc(sizeof(myFILE));
    if(!fp){
        myErrno = 1;
        return NULL;
    }
    fp->size = inodes[openfiles[fd].inode_id].size;
    fp->data = (char*) malloc(fp->size);
    myread(fd, fp->data, fp->size);

    fp->fd = fd;
    strcpy(fp->open_mode, mode);
    
    //init the read and write pointers
    if(strcmp(mode, "r") == 0){
        fp->rptr = fp->data;
    }
    else if (strcmp(mode, "r+") == 0)
    {
        fp->rptr = fp->data;
        fp->wptr = fp->data;
    }
    else if (strcmp(mode, "w") == 0)
    {
        fp->wptr = fp->data;
        bzero(fp->data, fp->size);
    }
    else if(strcmp(mode, "a")){
        fp->wptr = fp->data + fp->size;
    }
    
    return fp;
}

int myfclose(myFILE * fp){

    mywrite(fp->fd, fp->data, fp->size);
    myclose(fp->fd);
    free(fp->data);
    free(fp);

    return 0;
}

size_t myfread(void * ptr, size_t size, size_t nmemb, myFILE * stream){
    size_t bytestoread = size * nmemb;

    //check that the file is open in reading mode
    if(stream->open_mode[0] != 'r'){
        //unffited open mode
        myErrno = 17;
        return 0;
    }

    size_t bytes_left_to_read = stream->size - (stream->rptr - stream->data);
    size_t itemsread = MIN(bytes_left_to_read / size, bytestoread / size);

    memcpy(ptr,stream->rptr, bytes_left_to_read);
    
    stream->rptr += bytes_left_to_read;
    return itemsread;
}


size_t myfwrite(const void * ptr, size_t size, size_t nmemb, myFILE * stream){

    size_t bytestowrite = size * nmemb;

    if(stream->open_mode[1] == '\0'){ // the only illegal opening mode is r\0
        //unffited open mode
        myErrno = 17;
        return 0;
    }    

    stream->data = realloc(stream->data, stream->size + bytestowrite);
    if(!stream->data){
        myErrno = 8;
        return 0;
    }
    memcpy(stream->wptr, ptr, bytestowrite);
    
    stream->size += bytestowrite;
    stream->wptr += bytestowrite;
    
    //currently the changes are only present in the myFILE buffer, the will be written when 
    //we call myfclose
    return nmemb;
}

int myfseek(myFILE *stream, long offset, int whence){

    switch (whence)
    {
    case SEEK_SET:
        
        if(offset > stream->size || offset < 0){
            myErrno = 18;
            return -1;
        }
        stream->rptr = (stream->data + offset);
        stream->wptr = (stream->data + offset);
        return offset;
        
    case SEEK_CUR:;
    
        int curr_pos = (stream->rptr - stream->data);
        
        if(curr_pos + offset < 0 || curr_pos + offset > stream->size ){
            myErrno = 18;
            return -1;
        }

        stream->rptr = (stream->data + (curr_pos +offset));
        return curr_pos + offset;
    case SEEK_END:
        if( offset > 0 || offset < -1 * stream->size){
            myErrno = 18;
            return -1;
        }

        stream->rptr = stream->data + (stream->size + offset);
        return stream->size + offset;
    }

    myErrno = 19;
    return -1; //unknown flag
}

int myfscanf(myFILE * stream, const char * format, ...){

    va_list list;
    va_start(list, format);

    char * temp_rptr = stream->rptr;
    int items_read = 0;
    while (*format && *(format+1))
    {

        if(*format == '%'){

            format++;

            switch (*format)
            {
            case 'd': // we read int
                
                *(va_arg(list, int*)) = *((int*)stream->rptr);
                stream->rptr += sizeof(int);
                items_read++;    
                break;

            case 'f': // we read int
                
                *(va_arg(list, float*)) = *((float*)stream->rptr);
                stream->rptr += sizeof(float);    
                items_read++;
                break;            

            case 'c': // we read int
                
                *(va_arg(list, char*)) = *(stream->rptr);
                stream->rptr += sizeof(char);  
                items_read++;  
                break;   
            default:
                break;
            }

        }
        format++;

    }
    va_end(list);
    //we return the read ptr to its original location, since fscanf should not change it
    stream->rptr = temp_rptr;
    return items_read;
}


int myfprintf(myFILE * stream, const char * format, ...){


    va_list list;
    va_start(list,format );
    int items_printed = 0;

    char* buffer = (char*) malloc(sizeof(strlen(format) * 4)); //taking extra space for the expansions, result string is at most X4
    if(!buffer){
        myErrno = 1;
        return -1;
    }
    
    bzero(buffer, strlen(format) * 10);

    while(*format){

        if(*format == '%' && *(format +1)){

            switch (*(format+1))
            {
            case 'c':;
                
                char c = va_arg(list, int);
                *buffer = c;
                buffer++;
                format += 2;
                items_printed++;
                break;
            
            case 'd':
                *buffer =  va_arg(list, int);
                buffer+= sizeof(int);
                items_printed++;
                format += 2;

            case 'f':
                *buffer = va_arg(list, float);
                buffer+= sizeof(float);
                format+=2;
                items_printed++;
                break;

            default:
                break;
            }
        }
        else{
            *(buffer++) = *(format++); 
        }
    }

    myfwrite(buffer, strlen(buffer), 1, stream);
    free(buffer);

    return items_printed;
}



