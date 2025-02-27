#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#include "kernel/param.h"

int
main(int argc, char *argv[])
{

    char buf[512];

    while(read(0, buf, sizeof(char))==sizeof(char)){
        buf[1]=0;
        if(strcmp(buf,"\r") ==0){
            continue;
        }
        if(strcmp(buf,"\n") ==0){
            continue;
        }
        printf("%s\n", buf);
    }


    return 0;
}