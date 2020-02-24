#include "kernel/types.h"
#include "user.h"

int
main() {
    int fd_ping[2];
    int fd_pong[2];
    int res_ping = pipe(fd_ping);
    int res_pong = pipe(fd_pong);
    if (res_ping == -1 || res_pong == -1) {
        exit(0);
    }
    int pid = fork();
    if (pid == -1) {
        exit(0);
    }
    if (pid == 0) {
        char bytes[1];
        read(fd_ping[0], bytes, 1);
        printf("%d: received ping\n", getpid());
        write(fd_pong[1], "4", 1);
    } else {
        write(fd_ping[1], "3", 1);
        char bytes[1];
        read(fd_pong[0], bytes, 1);
        printf("%d: received pong\n", getpid());
        wait(0);
    }
    exit(0);
}