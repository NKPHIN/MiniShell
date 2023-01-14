MiniShell

.
├── classify.h    定义了一些函数对指令进行分类
├── define.h      定义了环境变量表等数据结构
├── main          
├── main.cpp      程序入口
├── makefile
├── shell.cpp     主要功能实现
└── shell.h       主要功能定义


参考了mac zsh的风格，实现功能包括:

-- 内建命令 cd  echo  history  bash  exit

-- 后台任务的创建(&)、查看(jobs)、引用(%)和删除(kill)

-- 串行执行以';'分隔的多条指令

-- 多级管道

-- 全局/环境变量的赋值(=)、引用($)、删除(unset)和导出(export)

-- 外部命令的执行


