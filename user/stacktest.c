#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/defs.h"

int main()
{
    int status = 0;
    if (fork() == 0) {
        char* sp = (char*) r_sp();
        sp -= PGSIZE;
        printf("Point at: %p\n", sp);
        printf("Read stack %d\n", *sp);
        exit(1);
    }
    int pid = wait((uint64)&status);
    if (status == -1) {
        printf("OK\n");
        printf("%d\n", pid);
    } else {
        printf("Failed\n");
    }
    return 0;
}