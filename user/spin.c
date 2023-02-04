// spin.c
// Created by gzh on 23-2-4.
//

#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[]) {
    int pid;
    char c;

    pid = fork();
    if(pid == 0) {
        c = '/';
    } else {
        printf("parent pid is %d, child is %d\n", getpid(), pid);
    }

    for(int i = 0; ; i++) {
        if((i % 10000000) == 0) {
            write(2, &c, 1);
        }
    }
    exit(0);
}
