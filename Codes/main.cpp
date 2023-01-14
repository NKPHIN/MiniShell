//
// Author  ph
// Company NKCS
// Created by mac on 2023/1/8.
//

#include "shell.h"
#include <csignal>
#include <sys/shm.h>

// 共享内存id号
int shm_id;
BackTable* addr;

// 捕获中断信号、释放共享内存
void signalHandler(int signum)
{
    shmdt(addr);
    shmctl(shm_id, 0, NULL);

    exit(signum);
}

int main()
{
    signal(SIGINT | SIGTERM, signalHandler);
    Shell mini_shell = Shell();

    shm_id = mini_shell.get_shm_id();
    addr = mini_shell.get_shm_addr();

    mini_shell.run();
    return 0;
}