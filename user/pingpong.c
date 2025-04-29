#include <kernel/types.h>
#include <kernel/stat.h>
#include <user/user.h>


int main(int argc, char** argv) {
    int p_p2c[2];       // parent to child
    int p_c2p[2];       // chile to parent
    int pid;
    if(argc != 1) {
        printf("usage: pingpong\n");
    }
    pipe(p_p2c);        // 创建管道
    pipe(p_c2p);

    pid = fork();       // 创建子进程
    if(pid != 0) {      // parent
        write(p_p2c[1], ".", 1);    // 父进程向子进程发送一个字符
        close(p_p2c[1]);            // 传递完关闭管道出端

        char buf;               
        read(p_c2p[0], &buf, 1);    // 父进程从子进程读取一个字符
        printf("%d: received pong\n", getpid());
        wait(0);                    // 等待子进程结束
    } else {            // child
        char buf;
        read(p_p2c[0], &buf, 1);    // 子进程从父进程读取一个字符
        printf("%d: received ping\n", getpid());

        write(p_c2p[1], &buf, 1);   // 子进程向父进程发送一个字符
        close(p_c2p[1]);
        exit(0);
    }
    // 关闭管道的读端
    close(p_c2p[0]);
    close(p_p2c[0]);
    exit(0);
}