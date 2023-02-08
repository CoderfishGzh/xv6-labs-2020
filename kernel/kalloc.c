// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.
struct run {
  struct run *next;
};

struct nkmem{
    struct spinlock lock;
    struct run *freelist;
};

// 定义 NCPU 个kmem
struct nkmem cpu_kmem[NCPU];

void
init_kmem_lock_and_cnt() {
    for(int i = 0; i < NCPU; i++) {
        char buffer[6];
        snprintf(buffer, 6, "kmem_%d", i);
        initlock(&cpu_kmem[i].lock, buffer);
    }
}

void
kinit()
{
    init_kmem_lock_and_cnt();
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}


// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // get cpu id
  push_off();
  int cpu_id = cpuid();

  // get cpu_id's kmem_lock
  acquire(&cpu_kmem[cpu_id].lock);
  r->next = cpu_kmem[cpu_id].freelist;
  cpu_kmem[cpu_id].freelist = r;
  release(&cpu_kmem[cpu_id].lock);
  pop_off();

//  acquire(&kmem.lock);
//  r->next = kmem.freelist;
//  kmem.freelist = r;
//  release(&kmem.lock);
}

// 在cpu id对应的freelist分配内存
void*
new_kalloc(void) {
    struct run *r;

    // get cpu id
    push_off();
    int cpu_id = cpuid();


    acquire(&cpu_kmem[cpu_id].lock);
    // 将 r 指向 freelist 头，分配空闲页
    r = cpu_kmem[cpu_id].freelist;

    if(r) {
        // 如果 r != 0, 代表有空闲页，将 freelist 头移动到下一个位置
        cpu_kmem[cpu_id].freelist = r->next;
    } else {
        // 如果 r == 0, 代表没有空闲页
        // 寻找别的cpu的 freelist 是否还有空闲页
        for(int target_cpu = 0; target_cpu < NCPU; target_cpu++) {
            if(target_cpu == cpu_id)
                continue;
            acquire(&cpu_kmem[target_cpu].lock);
            r = cpu_kmem[target_cpu].freelist;
            if(r) {
                r = cpu_kmem[target_cpu].freelist;
                cpu_kmem[target_cpu].freelist = r->next;
                release(&cpu_kmem[target_cpu].lock);
                break;
            }
            release(&cpu_kmem[target_cpu].lock);
        }

    }
    release(&cpu_kmem[cpu_id].lock);
    pop_off();

    if(r)
        memset((char*)r, 5, PGSIZE); // fill with junk
    return (void*)r;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
//  struct run *r;
//
//  acquire(&kmem.lock);
//  r = kmem.freelist;
//  if(r)
//    kmem.freelist = r->next;
//  release(&kmem.lock);
//
//  if(r)
//    memset((char*)r, 5, PGSIZE); // fill with junk
//  return (void*)r;
    return new_kalloc();
}
