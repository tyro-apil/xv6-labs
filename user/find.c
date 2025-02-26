#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

void recursive_find(char *dir, char *file){
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(dir, O_RDONLY)) < 0){
    fprintf(2, "find: cannot open %s\n", dir);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", dir);
    close(fd);
    return;
  }

  switch (st.type)
  {
  case T_DIR:
    
    if(strlen(dir) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("find: path too long\n");
      break;
    }
    
    strcpy(buf, dir);
    p = buf+strlen(buf);
    *p++ = '/';

    
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;

      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;


      // For infinite recursion, remove . and ..
      if (strcmp(de.name,".")==0)
      {
        continue;
      }
      if (strcmp(de.name,"..")==0)
      {
        continue;
      }
      
      // If name matches, print path
      if (strcmp(de.name,file)==0)
      {
          printf("%s\n",buf);
      }

      if (stat(buf, &st) <0)
      {
        printf("find: cannot stat %s\n", buf);
        continue;
      }

      switch (st.type)
      {
      case T_DIR:
        recursive_find(buf, file);
        break;
      
      default:
        break;
      }

    }

    return;
    break;
  
  default:
    fprintf(2, "Provide a valid directory\n");
    break;
  }
}


int
main(int argc, char *argv[])
{

  if(argc < 3){
    printf(" invalid: follow this-> find [path] [expression]");
    printf("\n");
    exit(0);
  }
    recursive_find(argv[1],argv[2]);
  exit(0);
}
