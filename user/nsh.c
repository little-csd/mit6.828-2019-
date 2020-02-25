#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"
#include "kernel/fcntl.h"

#define MAX_CMD 100

char cmd[MAX_CMD];
char data[MAXARG][MAX_CMD];

void execute(char* cmd);

int
readcmd(char* buf) {
    int len = 0;
    char c;
    while (read(0, &c, 1)) {
        if (c == '\n') break;
        buf[len++] = c;
    }
    buf[len] = '\0';
    return len > 0;
}

void
trim(char* str) {
    int len = strlen(str);
    int pos = 0;
    int _len = 0;
    while (pos < len && str[pos] == ' ') pos++;
    while (pos < len) {
        str[_len++] = str[pos++];
    }
    while (str[_len-1] == ' ') _len--;
    str[_len] = '\0';
}

void
split (char* src, int* size, char* dst) {
    // printf("Before trim: %s\n", src);
    trim(src);
    // printf("After trim: %s\n", src);
    int siz = 0;
    int len = strlen(src);
    int l = 0;
    for (int i = 0; i < len; i++) {
        if (src[i] == ' ' && src[i-1] != ' ') {
            dst[siz*MAX_CMD+l] = '\0';
            siz++;
            l = 0;
        } else {
            dst[siz*MAX_CMD+l] = src[i];
            l++;
        }
    }
    siz++;
    *size = siz;
}

void
redirect(int d, int* fd) {
    close(d);
    dup(fd[d]);
    close(fd[0]);
    close(fd[1]);
}

void
exepipe(char* str1, char* str2) {
    if (fork() > 0) {
        wait(0);
        return;
    } else {
        int pd[2];
        pipe(pd);
        if (fork() > 0) {
            redirect(1, pd);
            execute(str1);
        } else {
            redirect(1, pd);
            execute(str2);
        }
    }
}

void execute(char* cmd) {
    int len = strlen(cmd);
    for (int i = 0; i < len; i++) {
        if (cmd[i] == '|') {
            cmd[i] = '\0';
            int pd[2];
            pipe(pd);
            if (fork() > 0) {
                redirect(0, pd);
                cmd = cmd + i + 1;
                wait(0);
            } else {
                redirect(1, pd);
            }
            // exepipe(str1, str2);
            break;
        }
    }
    int args;
    split(cmd, &args, (char*)data);
    // for (int i = 0; i < args; i++) {
    //     printf("Line %s, length: %d\n", data[i], strlen(data[i]));
    // }
    int count = 0;
    for (int i = 1; i < args; i++) {
        int len = strlen(data[i]);
        if (len == 1) {
            if (data[i][0] == '<') {
                char* file = data[i+1];
                int fd = open(file, O_RDONLY);
                close(0);
                dup(fd);
                count++;
                close(fd);
            } else if (data[i][0] == '>') {
                char* file = data[i+1];
                int fd = open(file, O_CREATE | O_WRONLY);
                close(1);
                dup(fd);
                count++;
                close(fd);
            }
        }
    }
    char *argv[MAX_CMD];
    int siz = args - count * 2;
    // printf("args: %d, siz: %d\n", args, siz);
    for (int i = 0; i < siz; i++) {
        argv[i] = data[i];
    }
    argv[siz] = 0;
    // for (int i = 0; argv[i]; i++) {
    //     printf("Line %s, length %d\n", argv[i], strlen(argv[i]));
    // }
    exec(data[0], argv);
}

void
main() {
    char cmd[MAX_CMD];
    printf("@");
    while(readcmd(cmd)) {
        // printf("Execute: %s, length %d\n", cmd, strlen(cmd));
        if (fork() > 0) {
            wait(0);
        } else {
            execute(cmd);
        }
        printf("@");
    }
    exit(0);
}