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
  char* cow_page_ref;
  uint64 page_cnt;
  char* new_end;
} kmem;

// 初始化空闲列表
// 保存从end 到 PHYSTOP 之间的每一页，
void
kinit()
{
  initlock(&kmem.lock, "kmem");

  // 将内核结束地址往上抬，留出空间作为记录pa引用计数
  kmem.page_cnt = (PHYSTOP - (uint64)end) / PGSIZE;
  kmem.new_end = (char*)((uint64) end + kmem.page_cnt);
  printf("page cnt: %d, new end: %p, end: %p\n", kmem.page_cnt, (uint64)kmem.new_end, (uint64) end);
  kmem.cow_page_ref = end;
  
  // 初始化 引用计数
  for(int i = 0; i < kmem.page_cnt; i++) {
    kmem.cow_page_ref[i] = 1;
  }
  printf("page cnt: %d\n", kmem.page_cnt);
  freerange(kmem.new_end, (void*)PHYSTOP);
}

uint64
get_index(uint64 pa) {
  pa = PGROUNDDOWN(pa);

  uint64 index = (pa - (uint64) kmem.new_end) / PGSIZE;
  if(index < 0 || index >= kmem.page_cnt) {
    printf("index: %d\n", index);
    printf("pa: %p, kmem new_end: %p\n", pa, kmem.new_end);
    panic("ref index illegl");
  }

  return index;
}

// 增加引用计数
void
insr(uint64 pa) {
  pa = PGROUNDDOWN(pa);
  // get index 
  uint64 index = get_index(pa);
  acquire(&kmem.lock);
  kmem.cow_page_ref[index]++;
  release(&kmem.lock);
}

// 减少引用计数
void 
desc(uint64 pa) {
  pa = PGROUNDDOWN(pa);
  // get index 
  uint64 index = get_index(pa);
  acquire(&kmem.lock);
  kmem.cow_page_ref[index]--;
  release(&kmem.lock);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}
//释放 v 指向的物理内存页，
//通常应该由 a 返回
//调用 kalloc()。 （例外是当
//初始化分配器；参见上面的 kinit。）
void
kfree(void *pa)
{
  struct run *r;

  
  // get pa ref 
  // 如果引用 > 0, 则不进行释放
  // get pa index
  uint64 index = get_index((uint64) pa);
  // 对引用计数-1
  desc((uint64) pa);
  // get ref 
  uint64 ref_cnt = kmem.cow_page_ref[index]; 
  if(ref_cnt != 0) {
    return;
  }

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

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
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    // r 是 pa
    insr((uint64) r);
  }
    

  

  return (void*)r;
}
