#include "kernel/types.h"
#include "user/user.h"

void
redirect(int p, int* fd) {
    close(p);
    dup(fd[p]);
    close(fd[0]);
    close(fd[1]);
}

void
input() {
    int data[11] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31};
    write(1, data, 11 * sizeof(int));
    close(1);
}

void
dfs() {
    int data;
    int res = read(0, &data, sizeof(int));
    if (!res) return;
    printf("prime %d\n", data);
    int pd[2];
    pipe(pd);
    if (fork() > 0) {
        redirect(0, pd);
        wait(0);
        dfs();
    } else {
        redirect(1, pd);
        while (read(0, &data, sizeof(int))) {
            write(1, &data, sizeof(int));
        }
    }
}

int
main() {
    int pd[2];
    pipe(pd);
    if (fork() > 0) {
        redirect(0, pd);
        wait(0);
        dfs();
    } else {
        redirect(1, pd);
        input();
    }
    exit(0);
}