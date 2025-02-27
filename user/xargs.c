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
    // buf[count] = (char *)malloc(j+1);
    buf[count] = 0;
    // printf("arg count: %d\n", count);

    // Print all arguments as debug purpose
    // for(int i=0; i<count; ++i){
    //     printf("%s\n",buf[i]);
    // }

    // All commands are in root path
    char *cmd_path=(char *)malloc(strlen(buf[0])+2);
    *cmd_path='/';      // Be aware of ""->char* and ''->char
    strcpy(cmd_path+1, buf[0]);

    // See command full path
    // printf("%s\n", cmd_path);


    int pid=fork();

    if(pid<0){
        fprintf(2, "Fork error\n");
        exit(1);
    }
    else if(pid>0){
        wait(0);
    }
    else{
        exec(cmd_path, buf);
    }


    return 0;
}