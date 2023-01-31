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

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct cow_ref {
    struct spinlock lock;
//    int page_cnt;
//    char* page_ref;
//    char* end_;

    int cnt[PHYSTOP / PGSIZE];  // 引用计数
} ref;


void
ref_init() {
    ref.page_cnt = PHYSTOP - (uint64) end;
    ref.page_ref = end;
    ref.end_ = ref.page_ref + ref.page_cnt;
    // set the ref = 0
    for(int i = 0; i < ref.page_cnt; i++) {
        ref.page_ref[i] = 1;
    }
}

int
pa2index(uint64 pa) {
    pa = PGROUNDDOWN(pa);
    int index = (pa - (uint64) ref.end_) / PGSIZE;
    if(index < 0 || index >= ref.page_cnt) {
        panic("pa2index: index illegal");
    }
    return index;
}

void
incr(uint64 pa) {
    int index = pa2index(pa);
    acquire(&ref.lock);
    ref.page_ref[index]++;
    release(&ref.lock);
}

void
desc(uint64 pa) {
    int index = pa2index(pa);
    acquire(&ref.lock);
    ref.page_ref[index]--;
    if(ref.page_ref[index] < 0) {
        panic("panic: desc page_ref < 0");
    }

    release(&ref.lock);
}

int
get_pa_ref(uint64 pa) {
    int index = pa2index(pa);
    return ref.page_ref[index];
}

int kaddrefcnt(void* pa) {
    if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
        return -1;
    acquire(&ref.lock);
    ++ref.cnt[(uint64)pa / PGSIZE];
    release(&ref.lock);
    return 0;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ref.lock, "ref");
//  ref_init();
//  freerange(ref.end_, (void*)PHYSTOP);
    freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
//  char *p;
//  p = (char*)PGROUNDUP((uint64)pa_start);
//  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
//    kfree(p);

    char *p;
    p = (char*)PGROUNDUP((uint64)pa_start);
    for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
        // 在kfree中将会对cnt[]减1，这里要先设为1，否则就会减成负数
        ref.cnt[(uint64)p / PGSIZE] = 1;
        kfree(p);
    }
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

//  desc((uint64) pa);
////  acquire(&ref.lock);
//  int page_ref = get_pa_ref((uint64) pa);
////  release(&ref.lock);
//
//  if(page_ref == 0) {
//      // Fill with junk to catch dangling refs.
//      printf("page ref == 0\n");
//      memset(pa, 1, PGSIZE);
//
//      r = (struct run*)pa;
//
//      acquire(&kmem.lock);
//      r->next = kmem.freelist;
//      kmem.freelist = r;
//      release(&kmem.lock);
//  } else {
//      printf("page ref not 0\n");
//  }

    // 只有当引用计数为0了才回收空间
    // 否则只是将引用计数减1
    acquire(&ref.lock);
    if(--ref.cnt[(uint64)pa / PGSIZE] == 0) {
        release(&ref.lock);

        r = (struct run*)pa;

        // Fill with junk to catch dangling refs.
        memset(pa, 1, PGSIZE);

        acquire(&kmem.lock);
        r->next = kmem.freelist;
        kmem.freelist = r;
        release(&kmem.lock);
    } else {
        release(&ref.lock);
    }

}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
      kmem.freelist = r->next;
//      acquire(&ref.lock);
//      int index = pa2index((uint64) r);
//      ref.page_ref[index] = 1;
//      release(&ref.lock);
      acquire(&ref.lock);
      ref.cnt[(uint64)r / PGSIZE] = 1;  // 将引用计数初始化为1
      release(&ref.lock);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}


