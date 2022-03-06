#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

#define MAX 512

int
main(int argc, char *argv[])
{
  char *args[MAXARG];
  char buf[MAX];
  int args_idx = 0;

  for (int i = 1; i < argc; ++i){
    args[args_idx++] = argv[i];
  }

  int n;
  // when meet the ctlr + d, namely n < 0, exit
  while ((n = read(0, buf, MAX)) > 0){
    if (fork() == 0){
      // 申请一块内存来存放各个参数，用二维数组也可以
      // each parameter in left part of '|', separate by ' '
      char *para = (char*)malloc(sizeof(buf));
      int idx = 0;
      for (int i = 0; i < n; i++){
        if (buf[i] == ' ' || buf[i] == '\n'){
          para[idx] = 0;	// 字符串结束符
          args[args_idx++] = para;	// 将当前参数保存到 args 中
          idx = 0;
          para = (char*)malloc(sizeof(buf));
        }else{
          para[idx++] = buf[i];
        }
      }
      args[args_idx] = 0;
      exec(args[0], args);
    }else{  // 父进程
      wait(0);
    }
  }
  exit(0);
}