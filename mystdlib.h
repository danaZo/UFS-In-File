#pragma once

typedef struct myFILE{

    char *data;
    char *rptr, *wptr;
    char open_mode[3];
    int size, fd;

}myFILE; 

//open fd, and buffer the file content inside the myFILE structure that returned
myFILE *myfopen(const char * pathname, const char * mode);

int myfclose(myFILE * fp);

// read nmemb items to prt. return the number of items read
size_t myfread(void * ptr, size_t size, size_t nmemb, myFILE * stream);

// write nmemb items to prt. return the number of items written
size_t myfwrite(const void * ptr, size_t size, size_t nmemb, myFILE * stream);

int myfseek(myFILE *stream, long offset, int whence);


int myfscanf(myFILE * stream, const char * format, ...);
