#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

#include "kernel/param.h"

int
main(int argc, char *argv[])
{   
    char *buf[MAXARG];

    int count=0;

    for(int i=1; i<argc; ++i){
        buf[i-1]=argv[i];
        count++;
    }

    char line[512], c;
    int j=0;

    while(read(0, &c, sizeof(char))==sizeof(char)){
        
        if(c == '\n'){
            // printf("new line\n");
            line[j]='\0';

            // allocate memory 
            buf[count] = (char *)malloc(j+1);
            
            strcpy(buf[count], line);
            count++;
            j=0;
        }
        else{

            line[j++]=c;
        }
    }

    for(int i=0; i<count; ++i){
        printf("%s\n",buf[i]);
    }


    return 0;
}