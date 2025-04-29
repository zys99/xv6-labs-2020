#include <kernel/types.h>
#include <kernel/stat.h>
#include <user/user.h>
#include <kernel/fs.h>

void find(char* path, char* target) {

    char buf[512];       // buf存储当前路径.这个数组会逐步构建成一个完整的路径,每次读取到一个目录项的名字时,都会将该目录项的名字拼接到 buf中
    char* p;             // 指向buf末尾位置的指针,用于在路径字符串中逐步拼接新的目录或文件名
    struct dirent de;    // 目录项,用于表示目录中的一个条目,也就是目录中的每一个文件或子目录
    struct stat st;      // 文件状态, 用于存储文件或目录的状态信息,包括文件的类型 大小 权限等信息
    int fd;

    // 打开路径
    fd = open(path, 0);
    if(fd < 0) {
        fprintf(2, "can not open %s\n", path);
        return;
    }

    // 获取文件状态
    if(fstat(fd, &st) < 0) {
        fprintf(2, "can not stat %s\n", path);
        close(fd);
        return;
    }
    
    // 判断文件类型
    switch(st.type){
        // 文件
        case T_FILE:
            // 判断path中最后面target长度的内容是否和target一致
            if(strcmp(path + strlen(path) - strlen(target), target) == 0) {
                printf("%s\n", path);
            }
            break;

        // 路径
        case T_DIR:
            // 判断长度 path + / + 目录项大小 + \0 是否大于buf长度
            if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
                printf("find: path too long\n");
                break;
            }
            
            strcpy(buf, path);      // 将当前路径复制到 buf 缓冲区
            p = buf + strlen(buf);  // 设置指针 p 为 buf 字符串的末尾
            *p++ = '/';             // 在路径末尾加上斜杠 /

            // 读取目录中的每一项,直到读取完所有项.每次读取一个 dirent 结构体(即一个目录项)
            while(read(fd, &de, sizeof de) == sizeof de) {
                // 如果目录项的 inode 为 0，表示该项为空，跳过此项
                if(de.inum == 0)
                    continue;

                // 将目录项的文件名复制到缓冲区 buf 中
                memmove(p, de.name, DIRSIZ);    // 将 de.name 中的最多 DIRSIZ 字节的数据（即当前目录项的文件名）复制到 buf 中 p 指向的位置
                p[DIRSIZ] = 0;                  // 确保字符串以 \0 结尾
                
                // 获取当前目录项（即子目录或文件）的状态,如果获取失败,打印错误信息并跳过该项
                if(stat(buf, &st) < 0) {
                    printf("find: cannot stat %s\n", buf);
                    continue;
                }
                // 排除当前目录 . 和父目录 .. 不对这两个特殊目录进行递归查找
                if(strcmp(buf + strlen(buf) - 2, "/.") != 0 && strcmp(buf + strlen(buf) - 3, "/..") != 0) {
                    find(buf, target);
                }
            }
            break;
    }
    close(fd);
}

int main(int argc, char** argv) {

    if(argc != 3) {     // 参数个数不正确，打印用法
        printf("Usage: find path target\n");
        exit(0);
    }

    char target[512];   // 存放待查找文件名称的buf
    target[0] = '/';    // 在待查找文件开头添加/
    strcpy(target + 1, argv[2]);    // 将目标文件名存储在 target 中
    find(argv[1], target);      // 查找
    exit(0);
}