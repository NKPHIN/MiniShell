//
// Author  ph
// Company NKCS
// Created by mac on 2023/1/8.
//

#include "shell.h"
#include "classify.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>

Shell::Shell()
{
    user = (char*)malloc(sizeof(char)*16);
    host = (char*)malloc(sizeof(char)*64);
    login_path = (char*)malloc(sizeof(char)*128);

    getlogin_r(user, 16);
    gethostname(host, 64);
    getcwd(login_path, 128);

    // 去除.local后缀
    host = rm_local_label(host);

    // 申请共享内存
    init_shm();

    num = 0;
    dict = new VarTable[255];
}

void Shell::init_shm()
{
    count = 0;
    arr = new BackTable[255];

    // 考虑到bash生成的子shell进程同样需要共享内存，而一个key只能申请一次
    // 因此循环key直到申请成功
    key_t key = 431322;
    while(true){
        shm_id = shmget(key, 1024*8, IPC_CREAT | IPC_EXCL | 0666);
        if(shm_id != -1) break;
        key++;
    }

    addr = (BackTable*)shmat(shm_id, NULL, 0);
    memcpy(addr, arr, sizeof(BackTable)*255);
}

Shell::~Shell()
{
    free(user);
    free(host);
    free(login_path);
    delete[] arr;
}

char* Shell::rm_local_label(char *host)
{
    int len = strlen(host);
    if(len < 6) return host;

    // 去除.local后缀，将'.'置换为'\0'
    char* postfix = (char*)".local";
    if(!strcmp(host + len - 6, postfix))
        host[len-6] = '\0';
    return host;
}

void Shell::print_prompt()
{
    char* cwd = (char*)malloc(sizeof(char)*128);
    getcwd(cwd, 128);

    int pivot = 0;
    for(int i=0; i<strlen(cwd); i++)
    {
        if(cwd[i] == '/')
            pivot = i+1;
    }

    // mac zsh 风格的prompt
    printf("[ph-2013122]%s@%s %s %% ", user, host, cwd+pivot);
    free(cwd);
}

void Shell::parse_line(char *line)
{
    // 原始输入中可能包含以';'分隔的多条指令
    // 本函数通过处理得到单个指令 token
    // 将token传给parse_token函数进一步处理

    // 去除行末的换行
    line[strlen(line)-1] = '\0';
    const char sep[2] = ";";

    // 读取以';'分隔的多条指令
    char* token = strtok(line, sep);
    while(token != NULL){
        parse_token(token);
        token = strtok(NULL, sep);
    }
}

bool Shell::is_gap(char ch)
{
    if(isspace(ch) || ch == '\0')
        return true;
    else return false;
}

int Shell::parse_num(char *arg)
{
    int ans = 0;
    int len = strlen(arg);

    for(int i=1; i<len; i++)
    {
        ans *= 10;
        ans += arg[i] - '0';
    }
    return ans;
}

void Shell::parse_token(char *token)
{
    // token中可能包含空格、tab键等无效信息
    // 本函数通过处理识别出命令个数 argc和参数列表 argv
    // 将两者传给Classify函数进行命令类型的识别

    int argc=0;
    char** argv = (char**)malloc(sizeof(char*)*32);
    // 这里不能用静态定义，因为栈区的定义在本函数结束后就被释放了
    // 如果存在后台进程输出的话就会找不到argv的位置

    int len = strlen(token);
    for(int i=0; i<len; i++)
    {
        if(isspace(token[i])) token[i] = '\0';
        else if(i == 0 || is_gap(token[i-1]))
            argv[argc++] = token + i;
    }

    // 将参数中引用环境变量的部分还原
    argc = translate(argc-1, argv+1) + 1;
    //print_command(argc, argv);
    classify(argc, argv);
}

void Shell::print_command(int argc, char **argv)
{
    // 打印完整的命令
    for(int i=0; i<argc; i++)
        printf("%s ", argv[i]);
    printf("\n");
}

void Shell::add_history(char *line)
{
    // 原始输入在下一次输入后就被修改了，因此保存副本
    char* copy = (char*)malloc(sizeof(char)*(strlen(line)+1));
    strcpy(copy, line);

    if(history.size() < MAX)
        history.push(copy);
    else{
        history.pop();
        history.push(copy);
    }
}

void Shell::show_history()
{
    int size = history.size();
    for(int i=0; i<size; i++)
    {
        char* cur = history.front();
        // 当前history查询命令不显示
        if(strcmp(cur, "history\n") || i < size-1)
            printf("%s", cur);
        history.pop();
        history.push(cur);
    }
}

void Shell::classify(int argc, char *argv[])
{
    if(argc == 0) return;

    // 后台符号&
    else if(!strcmp(argv[argc-1], "&"))
        exec_back(argc, argv);

    // 杀死后台进程
    else if(!strcmp(argv[0], "kill"))
        exec_kill(argc, argv);

    // 查看后台进程
    else if(!strcmp(argv[0], "jobs"))
        exec_jobs(argc);

    // 管道 pipe
    else if(check_pipe(argc, argv))
        exec_pipe(argc, argv);

    // 变量赋值
    else if(check_env_assign(argc, argv))
        exec_env_assign(argc, argv);

    // 删除变量
    else if(!strcmp(argv[0], "unset"))
        exec_env_unset(argc, argv);

    // 声明为环境变量
    else if(!strcmp(argv[0], "export"))
        exec_env_export(argc, argv);

    // 内建、外部命令
    else{
        if(check_build_in(argc, argv))
            build_in(argc, argv);
        else extern_out(argc, argv);
    }
}

void Shell::build_in(int argc, char **argv)
{
    if(!strcmp(argv[0], "cd")){
        // cd后不带路径则返回登录路径
        if(argc == 1)
            chdir(login_path);
        else if(argc == 2){
            int res = chdir(argv[1]);
            if(res == -1) printf("cd: 该路径不存在!\n");
        }
        else printf("cd: 参数个数错误!\n");
    }
    else if(!strcmp(argv[0], "echo")){
        // 单独的echo命令会打印回车
        if(argc == 1) printf("\n");
        for(int i=1; i<argc; i++){
            printf("%s ", argv[i]);
            if(i == argc-1) printf("\n");
        }
    }
    else if(!strcmp(argv[0], "history"))
        show_history();
    else if(!strcmp(argv[0], "exit")){
        // 解除共享内存地址映射,释放共享内存
        shmdt(addr);
        shmctl(shm_id, IPC_RMID, 0);
        exit(0);
    }
    else if(!strcmp(argv[0], "bash")){
        if(argc != 1){
            printf("bash: 参数个数错误!\n");
            return;
        }
        pid_t pid = fork();
        if(pid == -1) perror("fork");
        else if(pid == 0){
            shmdt(addr);    // 解除继承自父进程的共享内存映射
            delete[] arr;
            init_shm();     // 申请新的共享内存
            clear_local();  // 继承父进程的环境变量
        }
        else waitpid(pid, NULL, WUNTRACED);
    }
}

void Shell::extern_out(int argc, char **argv)
{
    pid_t pid = fork();
    if(pid == 0){
        int res = execvp(argv[0], argv);
        if(res == -1){
            perror(argv[0]);
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }
    // 父进程等待子进程返回结果
    // 注意不能使用wait，否则会和正在运行的后台进程产生混淆
    waitpid(pid, NULL, WUNTRACED);
}

int Shell::insert(int argc, char** argv)
{
    // 为后台进程分配作业号 pivot
    int pivot = count + 1;
    for(int i=0; i<=count; i++) {
        if (addr[i].status == Unused) {
            pivot = i;
            break;
        }
    }
    if(pivot > count) count++;

    addr[pivot].status = Running;
    addr[pivot].argc = argc;
    addr[pivot].argv = argv;
    return pivot;
}

void Shell::exec_back(int argc, char **argv)
{
    // 忽略后台进程符 &
    argc--;
    argv[argc] = NULL;
    if(argc == 0) return;

    int cur = insert(argc, argv);

    int pid = fork();
    if(pid == -1){
        perror("fork");
        addr[cur].status = Unused;
    }
    else if(pid == 0){
        // 因为execvp命令会替代当前进程
        // 因此如果将extern_out函数直接替换成execvp命令
        // 后续的指令将不会执行，所以通过extern_out再创建
        // 一个孙进程执行具体指令，而子进程等待执行结果
        // setpgrp函数将子进程和孙进程绑定到一个进程组，方便kill
        setpgrp();
        if(check_build_in(argc, argv))
            build_in(argc, argv);
        else
            extern_out(argc, argv);

        addr[cur].status = Unused;

        printf("[%d] +\t\t", cur+1);
        print_command(argc, argv);

        exit(EXIT_SUCCESS);
    }
    else{
        addr[cur].pid = pid;
        printf("[%d]\t\t%d\n", cur+1, pid);
    }
}

void Shell::exec_jobs(int argc)
{
    if(argc != 1){
        printf("jobs: 参数个数错误!\n");
        return;
    }

    for(int i=0; i<=count; i++)
    {
        if(addr[i].status == Running){
            printf("[%d] +\tRunning\t\t", i+1);
            print_command(addr[i].argc, addr[i].argv);
        }
    }
}

void Shell::exec_kill(int argc, char **argv)
{
    if(argc != 2){
        printf("kill: 参数个数错误!\n");
        return;
    }
    else if(!check_back_ref(argv[1])){
        printf("kill: 参数格式错误!\n");
        return;
    }

    int id = parse_num(argv[1]) - 1;

    if(addr[id].status == Unused){
        printf("kill: 该作业号不存在!\n");
        return;
    }
    else if(addr[id].status == Running){
        pid_t pid = addr[id].pid;
        kill(-pid, SIGKILL);        // 若pid为负，将kill掉pid代表的整个进程组
        // 等待回收子进程资源
        waitpid(pid, NULL, 0);

        addr[id].status = Unused;
        printf("[%d] +\tterminated\t", id+1);
        print_command(addr[id].argc, addr[id].argv);
    }
}

void Shell::exec_pipe(int argc, char **argv)
{
    // 因为父子进程都要被execvp替代，因此不能直接在exec_pipe中执行
    // 通过exec_rec_pipe进行递归执行
    pid_t pid = fork();
    if(pid == -1) perror("fork");
    else if(pid == 0)
        exec_rec_pipe(argc, argv);
    else waitpid(pid, NULL, 0);
}

void Shell::exec_rec_pipe(int argc, char **argv)
{
    int pivot = argc;
    for(int i=0; i<argc; i++)
    {
        if(!strcmp(argv[i], "|"))
        {
            pivot = i;
            break;
        }
    }
    argv[pivot] = NULL;

    int pd[2];
    pipe(pd);

    // 若当前指令不含"|",直接执行返回
    if(pivot == argc){
        execvp(argv[0], argv);
        exit(EXIT_SUCCESS);
    }

    pid_t pid = fork();
    if(pid == -1) perror("fork");
    else if(pid == 0){
        close(pd[0]);
        dup2(pd[1], STDOUT_FILENO);
        close(pd[1]);
        execvp(argv[0], argv);
        exit(EXIT_SUCCESS);
    }
    else{
        // 子进程执行前一条指令，父进程等待输入
        waitpid(pid, NULL, WUNTRACED);

        close(pd[1]);
        dup2(pd[0], STDIN_FILENO);
        close(pd[0]);
        exec_rec_pipe(argc-pivot-1, argv+pivot+1);
    }
}

void Shell::exec_env_assign(int argc, char **argv, int mode)
{
    char* value = NULL;
    char* var = argv[0];
    int len = strlen(var);

    //  =1 命令非法
    for(int i=1; i<len; i++)
    {
        if(var[i] == '=') var[i] = '\0';
        if(var[i-1] == '\0') value = var + i;
    }

    // 变量已存在
    for(int i=0; i<num; i++)
        if(!strcmp(dict[i].var, var))
        {
            dict[i].value = value;
            if(mode == 1) dict[i].mode = mode;
            return;
        }

    dict[num].var = var;
    dict[num].value = value;
    dict[num++].mode = mode;
}

void Shell::exec_env_unset(int argc, char **argv)
{
    if(argc != 2){
        printf("unset: 参数个数错误!\n");
        return;
    }
    int pivot = -1;
    for(int i=0; i<num; i++)
        if(!strcmp(dict[i].var, argv[1]))
        {
            pivot = i;
            break;
        }

    if(pivot == -1){
        printf("unset: 变量不存在!\n");
        return;
    }
    else{
        for(int i=pivot; i<num-1; i++)
            dict[i] = dict[i+1];
        num--;
    }
}

bool Shell::set_global(char* var)
{
    for(int i=0; i<num; i++)
    {
        if(!strcmp(dict[i].var, var))
        {
            dict[i].mode = global;
            return true;
        }
    }
    return false;
}

void Shell::exec_env_export(int argc, char **argv)
{
    if(argc != 2){
        printf("export: 参数个数错误!\n");
        return;
    }

    // 判断是否为 export a=1 的形式
    if(check_env_assign(1, argv+1))
        exec_env_assign(1, argv+1, global);
    else{
        // export a 的形式
        if(!set_global(argv[1]))
        {
            dict[num].var = argv[1];
            dict[num++].mode = global;
        }
    }
}

void Shell::clear_local()
{
    // 子shell进程只继承环境变量
    int pivot = 0;
    for(int i=0; i<num; i++)
    {
        if(dict[i].mode == global)
            dict[pivot++] = dict[i];
    }
    num = pivot;
}

char* Shell::get_value(char *var)
{
    for(int i=0; i<num; i++)
        if(!strcmp(dict[i].var, var))
            return dict[i].value;
    return NULL;
}

int Shell::translate(int argc, char **argv)
{
    int pivot = argc;
    for(int i=0; i<argc; i++)
    {
        if(argv[i][0] == '$')
        {
            argv[i] = get_value(argv[i]+1);
            if(argv[i] == NULL) pivot--;
        }
    }
    return pivot;
}

void Shell::run()
{
    print_prompt();

    size_t len = 16;
    char* line = (char*)malloc(sizeof(char)*16);

    // 超过16个字符的命令会被重新分配内存
    while(getline(&line, &len, stdin)){
        // 保留副本，否则下一次输入后原有的指令就会被修改
        int l = strlen(line);
        char* copy = (char*)malloc(sizeof(char)*(l+1));
        strcpy(copy, line);

        add_history(line);

        parse_line(copy);
        print_prompt();
    }
    printf("Bye!\n");
    free(line);
}