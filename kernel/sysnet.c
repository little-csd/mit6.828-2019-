//
// network system calls.
//

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"
#define min(a,b) ((a)<(b)?(a):(b))
static const int HEADROOM = sizeof(struct udp) + sizeof(struct ip) + sizeof(struct eth);

struct sock {
  struct sock *next; // the next socket in the list
  uint32 raddr;      // the remote IPv4 address
  uint16 lport;      // the local UDP port number
  uint16 rport;      // the remote UDP port number
  struct spinlock lock; // protects the rxq
  struct mbufq rxq;  // a queue of packets waiting to be received
};

static struct spinlock lock;
static struct sock *sockets;

void
sockinit(void)
{
  initlock(&lock, "socktbl");
}

int
sockalloc(struct file **f, uint32 raddr, uint16 lport, uint16 rport)
{
  struct sock *si, *pos;

  si = 0;
  *f = 0;
  if ((*f = filealloc()) == 0)
    goto bad;
  if ((si = (struct sock*)kalloc()) == 0)
    goto bad;

  // initialize objects
  si->raddr = raddr;
  si->lport = lport;
  si->rport = rport;
  initlock(&si->lock, "sock");
  mbufq_init(&si->rxq);
  (*f)->type = FD_SOCK;
  (*f)->readable = 1;
  (*f)->writable = 1;
  (*f)->sock = si;

  // add to list of sockets
  acquire(&lock);
  pos = sockets;
  while (pos) {
    if (pos->raddr == raddr &&
        pos->lport == lport &&
	pos->rport == rport) {
      release(&lock);
      goto bad;
    }
    pos = pos->next;
  }
  si->next = sockets;
  sockets = si;
  release(&lock);
  return 0;

bad:
  if (si)
    kfree((char*)si);
  if (*f)
    fileclose(*f);
  return -1;
}

//
// Your code here.
//
// Add and wire in methods to handle closing, reading,
// and writing for network sockets.
//

// called by protocol handler layer to deliver UDP packets
void
sockrecvudp(struct mbuf *m, uint32 raddr, uint16 lport, uint16 rport)
{
  //
  // Your code here.
  //
  // Find the socket that handles this mbuf and deliver it, waking
  // any sleeping reader. Free the mbuf if there are no sockets
  // registered to handle it.
  //
  // printf("UDP received\n");
  acquire(&lock);
  struct sock* pos = sockets;
  while (pos) {
    if (pos->lport == lport &&
    pos->rport == rport &&
    pos->raddr == raddr) {
      break;
    }
    pos = pos->next;
  }
  if (!pos) {
    goto sockrecv_end;
  }
  release(&lock);

  acquire(&pos->lock);
  struct mbufq* q = &pos->rxq;
  mbufq_pushtail(q, m);
  release(&pos->lock);
  wakeup((void*)pos);
  return;

sockrecv_end:
  printf("Socket not found\n");
  release(&lock);
  mbuffree(m);
}

void
sockclose(struct sock* s) {
  // printf("Closing\n");
  if (!s) {
    printf("sockclose: null\n");
    return;
  }
  acquire(&s->lock);
  acquire(&lock);
  struct sock* pos = sockets;
  struct sock* last = 0;
  while (pos) {
    if (pos->raddr == s->raddr &&
    pos->lport == s->lport &&
    pos->rport == s->rport) {
      break;
    }
    last = pos;
    pos = pos->next;
  }
  if (!pos) {
    release(&lock);
    panic("Socket not found!\n");
  }
  if (!last) {
    sockets = s->next;
  } else {
    last->next = s->next;
  }
  release(&lock);

  struct mbufq* q = &s->rxq;
  struct mbuf* buf = mbufq_pophead(q);
  while (buf) {
    mbuffree(buf);
    buf = mbufq_pophead(q);
  }
  release(&s->lock);
  kfree((char*)s);
}

int
sockread(struct sock* s, uint64 addr, int n) {
  // printf("sockread\n");
  acquire(&s->lock);
  struct mbufq* q = &s->rxq;
  while (mbufq_empty(q)) sleep((void*)s, &s->lock);
  // printf("sockread wakeup\n");
  struct proc* p = myproc();
  struct mbuf* buf = mbufq_pophead(q);
  if (!buf) {
    panic("sockread: buf is empty!\n");
  }
  int cnt = min(n, buf->len);
  if (copyout(p->pagetable, addr, buf->head, cnt)) {
    panic("sockread: copyout error!\n");
  }
  if (cnt == buf->len) {
    buf->len -= cnt;
    buf->head += cnt;
    buf->next = q->head;
    q->head = buf;
  } else {
    mbuffree(buf);
  }
  release(&s->lock);
  return cnt;
}

int
sockwrite(struct sock* s, uint64 addr, int n) {
  if (n > MBUF_SIZE) {
    printf("Error!\n");
    return 0;
  }
  acquire(&s->lock);
  struct mbuf* buf = mbufalloc(HEADROOM);
  if (!buf) {
    printf("sockwrite error!\n");
    return 0;
  }
  struct proc* p = myproc();
  if (copyin(p->pagetable, buf->head, addr, n)) {
    panic("sockwrite error!\n");
  }
  mbufput(buf, n);
  // printf("Transmit to lower layer\n");
  net_tx_udp(buf, s->raddr, s->lport, s->rport);
  release(&s->lock);
  return n;
}
