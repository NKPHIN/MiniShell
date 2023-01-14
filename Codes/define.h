//
// Author  ph
// Company NKCS
// Created by mac on 2023/1/8.
//

#ifndef MINISHELL_DEFINE_H
#define MINISHELL_DEFINE_H

#include <unistd.h>

const int MAX = 12;

// 一些状态的定义
enum status{
    Unused = 0,
    Running = 1,
    Stopped = 2,
    local = 0,
    global = 1
};

// 定义一个表用来存储所有后台进程信息
struct BackTable{
    pid_t pid;
    int status;
    int argc;
    char** argv;
};

// 定义一个表用来存储所有全局/环境变量
struct VarTable{
    char* var;
    char* value;
    int mode;     // 0表示全局、1表示环境
};

#endif //MINISHELL_DEFINE_H
