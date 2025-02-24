#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if (argc != 2){
        printf("1 integer argument must be provided\n");
        exit(1);
    }
    int secs=atoi(argv[1]);
    sleep(secs);
    exit(0);
}