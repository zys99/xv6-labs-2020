#include <kernel/types.h>
#include <kernel/stat.h>
#include <user/user.h>

// 质数筛选函数, 接收一个管道作为参数
void sieve(int pleft[2]) {

    // pleft  是父进程创建的管道，用于父进程与当前进程（右邻居）之间的通信
    // pright 是新创建的管道，子进程（即右邻居）使用它来与下一个进程通信
    
    // 从左邻居读取整数
    int p;                          
    read(pleft[0], &p, sizeof(p));
    if(p == -1) {               // 读到的数为-1,则为结束标志 退出进程
        exit(0);
    }
    printf("prime %d\n", p);    // 打印

    int pright[2];              // 创建新管道, 用于与下一个进程通信
    pipe(pright);

    if(fork() == 0) {           // 右邻居(子进程)
        close(pright[1]);       // 关闭右管道写端
        close(pleft[0]);        // 关闭左管道读端
        sieve(pright);          // 递归调用
    } else {                    // 左邻居(当前进程)
        close(pright[0]);       // 关闭右管道读端
        int buf;
        // 从左邻居接收数字
        while(read(pleft[0], &buf, sizeof(buf)) && buf != -1) {
            // 如果接收到的数字不是第一次接收到的数字的倍数
            if(buf % p != 0) {
                // 才往管道中给右邻居写入这个数字
                write(pright[1], &buf, sizeof(buf));
            }
        }
        // 此时接收到了左邻居传来的-1，要给右邻居也传-1，结束右邻居进程
        buf = -1;
        write(pright[1], &buf, sizeof(buf));
        wait(0);
        exit(0);
    }
}

int main(int argc, char** argv) {
    int input_pipe[2];              // 创建初始管道
    pipe(input_pipe);

    if(fork() == 0) {               // 子进程(右邻居)
        close(input_pipe[1]);       // 右邻居用不到这个管道的写端，关闭右邻居的管道写文件描述符
        sieve(input_pipe);          // 调用筛选函数
        exit(0);
    } else {                        // 父进程(左邻居)
        close(input_pipe[0]);       // 父进程只会向管道中给右邻居写数据，关闭父进程的管道读文件描述符
        int i;
        for(int i = 2; i <= 35; i++) {  // 向管道写入2~35的整数
            write(input_pipe[1], &i, sizeof(i));
        }
        i = -1;                     // 写入结束标志
        write(input_pipe[1], &i, sizeof(i));
    }
    wait(0);                        // 等待子进程退出
    exit(0);                        
}