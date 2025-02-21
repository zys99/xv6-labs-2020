#include <kernel/types.h>
#include <kernel/stat.h>
#include <user/user.h>
#include <kernel/fs.h>

void run(char* prog, char** args) {
    if(fork() == 0) {
        exec(prog, args);
        exit(0);
    }
    return;
}

int main(int argc, char** argv) {
    char buf[2048];
    char* p = buf;                      // p 指向当前字符的位置
    char* last_p = buf;                 // last_p 记录当前参数的开始位置 也就是遇到换行或者空格后的第一个位置

    char* argsbuf[128];                 // argsbuf中的每个元素都是指向一个字符串的起始位置(即命令的开头)
    char** args = argsbuf;              // args 是指向 argsbuf 开始位置的指针，它在一开始指向 argsbuf 的第一个位置

    for(int i = 1; i < argc; i++) {     // 先将xargs的参数复制到argsbuf中
        *args = argv[i];
        args++;
    }

    char** pa = args;                   // pa指向最新的存放命令的位置 每当一个参数被处理完并添加到 argsbuf 时，pa 会向前移动

    while(read(0, p, 1) != 0) {         // 接下来, 从标准输入中读取额外参数
        // 遇到空格或者换行 说明读取到了一个完整的参数
        if(*p == ' ' || *p == '\n') {   
            if(*p == ' ') {
                *p = '\0';              // 如果是空格，则替换成结束符
            }
            
            
            *pa = last_p;               // 新的参数指向 当前读到的参数的起始位置
            pa++;
            last_p = p + 1;             // 更新last_p指向下一个位置

            // 每当遇到换行符 \n 时，表示一组参数读取完毕，调用 run 函数执行程序，传递参数
            if(*p == '\n') {            
                *p = '\0';              // 确保换行符被替换为字符串结束符
                *pa = 0;                // 在 argsbuf 中添加一个 NULL 作为参数列表的结束标志
                run(argv[1], argsbuf);
                pa = args;              // 将 pa 重置为 args，准备处理下一个参数列表
            }
        }
        p++;
    }

    // 确保在输入的最后，如果没有遇到换行符（例如文件结束或最后一行没有换行），仍然会处理并传递最后一个参数
    if(pa != args) {
        *p = '\0';
        *pa = last_p;
        pa++;
        *pa = 0;
        run(argv[1], argsbuf);
    }
    while(wait(0) != -1);               // 等待所有进程结束
    exit(0);
}
