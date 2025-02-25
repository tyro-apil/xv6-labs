#include<kernel/types.h>
#include<user/user.h>


int
main(int argc, char *argv[])
{
    int pipe1_fd[2];
    int pipe2_fd[2];

    pipe(pipe1_fd);
    pipe(pipe2_fd);


    if(fork()==0)
    {
        char buf[1];
        int child_pid=getpid();

        int n=read(pipe1_fd[0], buf, 1);
        if(n<0){
            fprintf(2, "read error\n");
            exit(1);
        }
        printf("%d: received ping\n", child_pid);

        close(pipe1_fd[0]);
        close(pipe1_fd[1]);

        write(pipe2_fd[1], "a", 1);
    }
    else
    {
        char buf[1];
        int parent_pid=getpid();

        write(pipe1_fd[1],"a",1);

        wait(0);

        int n=read(pipe2_fd[0], buf, 1);
        if(n<0){
            fprintf(2, "read error\n");
            exit(1);
        }
        printf("%d: received pong\n", parent_pid);

        close(pipe2_fd[0]);
        close(pipe2_fd[1]);
    }

    exit(0);
}