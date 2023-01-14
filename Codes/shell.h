//
// Author  ph
// Company NKCS
// Created by mac on 2023/1/8.
//

#ifndef MINISHELL_SHELL_H
#define MINISHELL_SHELL_H

#include "define.h"
#include <stdio.h>
#include <queue>
#include <stdlib.h>
#include <string.h>
using namespace std;

/***********************************
 * Mini Shell
 *
 * 实现了后台符号&
 * 支持 &创建、jobs查看、kill删除后台进程
 * 同时支持 % 引用后台进程
 *
 * 实现了以;分隔多条指令，并串行执行
 *
 * 实现了多级管道
 *
 * 实现了环境变量
 * 支持全局/环境变量赋值、export导出环境变量
 * 同时支持 $ 引用变量
 *
 * 实现的内建命令有:
 * cd
 * echo
 * history
 * bash
 * exit
 * 其中bash命令进入shell子进程用于检验
 * 环境变量的正确性
 * *********************************/
class Shell{
private:
    char* user;
    char* host;
    char* login_path;      // 登录路径

    int shm_id;            // 共享内存id号，用于同步后台进程与shell进程之间的信息
    int count;             // 后台进程作业号在[0, count]之间
    BackTable* arr;
    BackTable* addr;       // 后台进程表，存储后台进程信息

    int num;               // 环境变量数
    VarTable* dict;        // 环境变量表，存储环境变量信息

    queue<char*> history;  // 保存历史输入

private:
    char* rm_local_label(char* host);
    void print_prompt();
    void init_shm();

    bool is_gap(char ch);
    int parse_num(char* arg);
    void parse_line(char* line);
    void parse_token(char* token);
    void print_command(int argc, char* argv[]);

    void add_history(char* line);
    void show_history();

    void classify(int argc, char* argv[]);

    void build_in(int argc, char* argv[]);
    void extern_out(int argc, char* argv[]);

    bool set_global(char* var);
    void clear_local();
    char* get_value(char* var);
    int translate(int argc, char** argv);

    int insert(int argc, char* argv[]);
    void exec_back(int argc, char* argv[]);
    void exec_jobs(int argc);
    void exec_kill(int argc, char* argv[]);
    void exec_pipe(int argc, char* argv[]);
    void exec_rec_pipe(int argc, char* argv[]);
    void exec_env_assign(int argc, char* argv[], int mode=0);
    void exec_env_unset(int argc, char* argv[]);
    void exec_env_export(int argc, char* argv[]);

public:
    Shell();
    ~Shell();

    void run();
    int get_shm_id() {return shm_id;}
    BackTable* get_shm_addr() {return addr;}
};


#endif //MINISHELL_SHELL_H
