#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char *argv[])
{
    if(argc < 2) {
        fprintf(2, "Usage: xargs command [args...]\n");
        exit(1);
    }
    
    char *base_args[MAXARG];
    int base_count = 0;
    
    for(int i=1; i<argc; i++) {
        base_args[base_count] = argv[i];
        base_count++;
    }
    
    char line[512];
    char c;
    int j = 0;
    
    // Process each line of input separately
    while(read(0, &c, sizeof(char)) == sizeof(char)) {
        if(c == '\n') {
            line[j] = '\0';
            
            char *exec_args[MAXARG];
            int arg_count = 0;
            
            for(int i=0; i<base_count; i++) {
                exec_args[arg_count++] = base_args[i];
            }
            

            // Add this line as an argument
            exec_args[arg_count] = (char *)malloc(j+1);
            strcpy(exec_args[arg_count], line);
            arg_count++;
            
            exec_args[arg_count] = 0;

            
            // Create the command path
            char cmd_path[MAXARG];
            cmd_path[0] = '/';
            strcpy(cmd_path+1, exec_args[0]);
            


            int pid = fork();
            if(pid < 0) {
                fprintf(2, "Fork error\n");
                exit(1);
            }
            else if(pid == 0) {
                exec(cmd_path, exec_args);
                fprintf(2, "exec failed\n");
                exit(1);
            }
            else {
                wait(0);
                
                // Free the allocated memory
                if(j > 0) {
                    free(exec_args[base_count]);
                }
            }
            
            // Reset for the next line
            j = 0;
        }
        else {
            line[j++] = c;
        }
    }
    
    exit(0);
}