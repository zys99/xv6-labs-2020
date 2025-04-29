#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// 从路径中提取文件名，并将其格式化为固定长度（DIRSIZ）的字符串
char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  // p 指针从路径字符串的末尾向前搜索，直到找到最后一个斜杠 /，然后指向斜杠后的第一个字符（即文件名
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  // 将文件名复制到静态缓冲区 buf 中，并用空格填充剩余部分，使其长度达到 DIRSIZ。
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}

void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  // 如果打开路径失败，打印错误信息并返回。
  if((fd = open(path, 0)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }
  // 使用 fstat 获取文件或目录的状态信息。如果失败，打印错误信息，关闭文件描述符并返回。
  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  // 如果路径指向一个普通文件（T_FILE），则打印文件名、类型、inode 号和文件大小。
  case T_FILE:
    printf("%s %d %d %l\n", fmtname(path), st.type, st.ino, st.size);
    break;
  // 如果路径指向一个目录（T_DIR），首先检查路径长度是否超过缓冲区大小。如果超过，打印错误信息并退出。
  case T_DIR:
    // 首先检查路径长度是否超过缓冲区大小。如果超过，打印错误信息并退出。 
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("ls: path too long\n");
      break;
    }

    // 将路径复制到 buf 中，并在末尾添加斜杠 /，以便后续拼接目录条目
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    // 使用 read 读取目录条目
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      // 如果 inum（inode 号）为 0，表示该条目无效，跳过
      if(de.inum == 0)
        continue;
      // 将目录条目名称拼接到 buf 中，并调用 stat 获取该条目状态信息。
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("ls: cannot stat %s\n", buf);
        continue;
      }
      // 打印条目名称、类型、inode 号和大小
      printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i;
  // 没有提供命令行参数（argc < 2），则列出当前目录
  if(argc < 2){ 
    ls(".");
    exit(0);
  }
  // 依次列出每个命令行参数指定的路径
  for(i=1; i<argc; i++)
    ls(argv[i]);
  exit(0);
}
