#include "types.h"
#include "fcntl.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "defs.h"

static struct vma vmas[MAX_VMA];
static struct vma* head = 0;
struct vma*
allocvma() {
    if (!head) {
        for (int i = 0; i < MAX_VMA; i++) {
            memset(vmas+i, 0, sizeof(struct vma));
            if (i+1 < MAX_VMA) vmas[i].next = &vmas[i+1];
        }
        head = &vmas[0];
    }
    struct vma* r = head;
    head = head->next;
    if (!r) panic("allocvma: no vma!\n");
    r->next = 0;
    return r;
}

void
freevma(struct vma* r) {
    if (!r) panic("freevma: vma null!");
    memset(r, 0, sizeof(struct vma));
    r->next = head;
    head = r;
}

struct vma*
copyvma(struct vma* r) {
    if (!r) return 0;
    struct vma* nr = allocvma();
    nr->addr = r->addr;
    nr->f = r->f;
    nr->flags = r->flags;
    nr->len = r->len;
    nr->prot = r->prot;
    nr->next = 0;
    filedup(nr->f);
    return nr;
}

static
pte_t*
walk(pagetable_t pagetable, uint64 va) {
    for (int level = 2; level > 0; level--) {
        pte_t* pte = &pagetable[PX(level, va)];
        if (*pte & PTE_V) {
            pagetable = (pagetable_t)PTE2PA(*pte);
        } else {
            return 0;
        }
    }
    return &pagetable[PX(0, va)];
}

#define MAX_PAGE (1<<22)
void*
findregion(pagetable_t pagetable, int len, struct vma* r) {
    int pages = 0;
    int page_num = (len-1) / PGSIZE + 1;
    int i = 0;
    int start[10];
    int end[10];
    int c = 0;
    while (r) {
        start[c] = r->addr / PGSIZE;
        end[c] = (r->addr + r->len - 1) / PGSIZE;
        r = r->next;
        c++;
    }
    while (i < page_num && pages <= MAX_PAGE) {
        int ok = 1;
        for (int k = 0; k < c; k++) {
            if (pages >= start[k] && pages <= end[k]) {
                pages = end[k] + 1;
                i = 0;
                ok = 0;
                break;
            }
        }
        if (!ok) continue;
        if (!walk(pagetable, pages * PGSIZE)) {
            i++;
        } else {
            i = 0;
        }
        pages++;
    }
    uint64 res = (pages-page_num) * PGSIZE;
    return (void*)res;
}

static
int
check_perm(int prot, int flags, struct file* f) {
    // printf("check perm: %d\n", prot);
    if ((prot & PROT_READ) && !f->readable) {
        printf("check_perm read\n");
        return -1;
    }
    if ((prot & PROT_WRITE) && !f->writable && (flags & MAP_SHARED)) {
        printf("check perm write\n");
        return -1;
    }
    int res = PTE_U|PTE_X;
    if (prot & PROT_READ) res |= PTE_R;
    if (prot & PROT_WRITE) res |= PTE_W;
    return res;
}

static
void
writeback(struct proc* p, struct file* f, uint64 addr, int len) {
    uint64 base = PGROUNDDOWN(addr);
    int n = PGROUNDUP(addr + len - base) / PGSIZE;
    for (int i = 0; i < n; i++) {
        pte_t* pte = walk(p->pagetable, base);
        if (pte == 0) {
            printf("write back error\n");
            exit(0);
        }
        if (*pte & PTE_D) {
            filewrite(f, base, PGSIZE);
        }
        base += PGSIZE;
    }
}

void printvms(struct vma* r) {
    printf("\nPrintvma:\n");
    while (r) {
        printf("addr=%p,len=%p,next=%p\n", r->addr, r->len, r->next);
        r = r->next;
    }
}

#define MAP_FAILED ((void*)-1)
// offset and addr always 0
void* mmap(void* addr, int len, int prot, int flags, int fd, int offset) {
    // printf("mmap\n");
    if (len <= 0) panic("mmap: len <= 0");
    struct proc* p = myproc();
    if ((prot = check_perm(prot, flags, p->ofile[fd])) == -1) {
        return MAP_FAILED;
    }
    struct vma* r = allocvma();
    addr = findregion(p->pagetable, len, p->vmhead);
    // printf("After find region，addr = %p, len = %d\n", addr, len);
    r->addr = (uint64)addr;
    r->len = len;
    r->prot = prot;
    r->flags = flags;
    r->f = p->ofile[fd];
    r->next = p->vmhead;
    p->vmhead = r;
    filedup(r->f);
    return addr;
}

int munmap(void* addr, int len) {
    struct proc* p = myproc();
    // printf("Unmap %p, len = %d, head = %p\n", addr, len, p->vmhead);
    struct vma* r = p->vmhead;
    // printf("addr: %p, len: %p\n", r->addr, r->len);
    // printvms(p->vmhead);
    if (len <= 0) return -1;
    
    // printf("Before unmap\n");
    // printvms(p->vmhead);
    // struct vma* r = p->vmhead;
    struct vma* last = 0;
    uint64 vir = (uint64)addr;
    while (r) {
        if (vir >= r->addr && vir <= r->addr + r->len) break;
        last = r;
        r = r->next;
    }
    if (!r) return -1;
    if (vir + len > r->addr + r->len) return -1;
    if (r->flags & MAP_SHARED) {
        writeback(p, r->f, vir, len);
    }
    uvmunmap(p->pagetable, vir, len, 1);
    if (vir == r->addr) {
        r->addr += len;
    }
    r->len -= len;
    if (!r->len) {
        fileclose(r->f);
        if (last) {
            last->next = r->next;
            freevma(r);
        } else {
            p->vmhead = r->next;
            freevma(r);
        }
    }
    // printf("After unmap\n");
    // printvms(p->vmhead);
    return 0;
}
