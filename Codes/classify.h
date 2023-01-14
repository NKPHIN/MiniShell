//
// Author  ph
// Company NKCS
// Created by mac on 2023/1/8.
//

#ifndef MINISHELL_CLASSIFY_H
#define MINISHELL_CLASSIFY_H

#include <string.h>

// 判断是否为环境变量赋值
bool check_env_assign(int argc, char** argv)
{
    // 不支持"a = 1"这种包含空格的形式
    if(argc != 1) return false;

    char* assign = argv[0];
    int len = strlen(assign);

    // a== 或 a= 命令认为是有效的，不过a分别是 = 和 null
    if(assign[0] == '=') return false;
    for(int i=1; i<len; i++)
        if(assign[i] == '=') return true;
    return false;
}

// 判断是否为管道命令
bool check_pipe(int argc, char** argv)
{
    // 管道符号 | 左右不能为空
    if(!strcmp(argv[0], "|") || !strcmp(argv[argc-1], "|"))
        return false;
    for(int i=1; i<argc-1; i++)
        if(!strcmp(argv[i], "|")) return true;
    return false;
}

// 判断是否为内建命令
bool check_build_in(int argc, char** argv)
{
    if(!strcmp(argv[0], "cd"))
        return true;
    else if(!strcmp(argv[0], "echo"))
        return true;
    else if(!strcmp(argv[0], "history"))
        return true;
    else if(!strcmp(argv[0], "exit"))
        return true;
    else if(!strcmp(argv[0], "bash"))
        return true;
    else return false;
}

// 判断是否引用后台进程
bool check_back_ref(char* arg)
{
    int len = strlen(arg);
    if(arg[0] != '%') return false;

    for(int i=1; i<len; i++)
        if(arg[i] < '0' || arg[i] > '9')
            return false;
    return true;
}

#endif //MINISHELL_CLASSIFY_H
