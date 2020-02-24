#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAX_BUF 512

int readword(char* buf) {
    char data;
    int len = 0;
    while (read(0, &data, 1)) {
        if (data == ' ' || data == '\n') break;
        buf[len++] = data;
    }
    buf[len] = '\0';
    return len > 0;
}

void
main(int argc, char** argv) {
    int argn = 0;
    char* args[MAXARG];
    for (int i = 1; i < argc; i++) {
        args[argn] = argv[i];
        argn++;
    }
    char buf[MAX_BUF];
    while (readword(buf)) {
        int len = strlen(buf);
        char* p = (char*) malloc(len);
        memmove(p, buf, len);
        args[argn] = p;
        argn++;
    }
    // for (int i = 0; i < argn; i++) {
    //     printf("Line %d: %s, length: %d\n", i, args[i], strlen(args[i]));
    // }
    if (fork()) {
        wait(0);
        exit(0);
    } else {
        exec(argv[1], args);
        printf("exec failed!");
    }
    exit(0);
}