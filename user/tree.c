#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

void tree(char *dir){
  char buf[512], *p;
  char buf2[512], *p2;
  char file_name[512],*p3 ;
  int fd, i, count;
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
    p3 = file_name;
    
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
      
   
          
        strcpy(buf2,buf);
        p2 = buf2 + strlen(buf2);
         i = strlen(buf2);
         count = 0;
        
        while (i>0)
        {
            if(*p2 == '/'){
                count++;
                
            }
            
            p2--;
            i--;
        }
        *p3 = 0;
        if(stat(buf, &st) < 0){
            printf("ls: cannot stat %s\n", buf);
            continue;
          }
          while (count>1)
          {   
              printf("   ");
              count--;
            };
            printf("|__");
        printf("%s",de.name);

        if(st.type == T_DIR)
        {
            printf("/");
        }
        printf("\n");
        
        



      if (stat(buf, &st) <0)
      {
        printf("find: cannot stat %s\n", buf);
        continue;
      }

      switch (st.type)
      {
      case T_DIR:
        tree(buf);
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

  if(argc < 2){
    printf(" invalid: follow this-> tree [path]");
    printf("\n");
    exit(0);
  }
    tree(argv[1]);
  exit(0);
}
