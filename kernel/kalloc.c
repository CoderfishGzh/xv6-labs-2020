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

extern char end[]; //内核之后的第一个地址。
                   //由 kernel.ld 定义。

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  char* end_;
  char* page_ref;
  int page_cnt;
} cow_ref;

// 初始化空闲列表
// 保存从end 到 PHYSTOP 之间的每一页，
void
kinit()
{
  initlock(&kmem.lock, "kmem");
  // initlock(&cow_ref.lock, "cow_ref");
  // 初始化 cow_ref 需要记录的变量
  cow_ref.page_cnt = (PHYSTOP - (uint64) end) / PGSIZE;
  printf("page cnt: %d\n", cow_ref.page_cnt);
  cow_ref.page_ref = end;
  cow_ref.end_ = cow_ref.page_ref + cow_ref.page_cnt;

  for(int i = 0; i < cow_ref.page_cnt; i++) {
    cow_ref.page_ref[i] = 1;
  }
  freerange(cow_ref.end_, (void*)PHYSTOP);
}

int
get_index(uint64 pa) {
  pa = PGROUNDDOWN(pa);

  int index = (pa - (uint64) cow_ref.end_) / PGSIZE;
  if(index < 0 || index >= cow_ref.page_cnt) {
    printf("index: %d\n", index);
    printf("pa: %p, cow_ref end_: %p\n", pa, cow_ref.end_);
    panic("ref index illegl");
  }

  return index;
}

// 增加引用计数
void
insr(uint64 pa) {
  pa = PGROUNDDOWN(pa);
  // get index 
  int index = get_index(pa);
  acquire(&cow_ref.lock);
  cow_ref.page_ref[index]++;
  release(&cow_ref.lock);
}

// 减少引用计数
void 
desc(uint64 pa) {
  pa = PGROUNDDOWN(pa);
  // get index 
  int index = get_index(pa);
  acquire(&cow_ref.lock);
  cow_ref.page_ref[index]--;
  release(&cow_ref.lock);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    kfree(p);
  }
}
//释放 v 指向的物理内存页，
//通常应该由 a 返回
//调用 kalloc()。 （例外是当
//初始化分配器；参见上面的 kinit。）
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  acquire(&cow_ref.lock);
  desc((uint64) pa);
  uint64 index = get_index((uint64) pa);
  int ref_cnt = cow_ref.page_ref[index];
  if(ref_cnt != 0) {
    release(&cow_ref.lock);
    return;
  } else {
    release(&cow_ref.lock);
  }

  //填充垃圾以捕获悬挂的引用。
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

//分配一个 4096 字节的物理内存页。
//返回一个内核可以使用的指针。
//如果无法分配内存，则返回 0。
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    insr((uint64) r); 
  }
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
    
  return (void*)r;
}
