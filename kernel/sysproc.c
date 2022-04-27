#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"

uint64 num_of_unused();
int num_of_free_memory();


uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_trace(void) {
  
  //mask 是一个32位的整数，10进制
  int mask;
  if(argint(0, &mask) < 0) 
    return -1;
  //将mask转换为 2进制，
  //将mask 记录在 pcb里面的 mask数组
  
  int idx = 0;
  
  while(mask / 2) {
    myproc()->masks[idx++] = mask % 2;
    mask /= 2;
  }
  myproc()->masks[idx++] = mask % 2;
  return 0;
}

//打印系统信息
uint64
sys_sysinfo(void) {

  //获取函数的参数
  uint64 p;
  struct sysinfo si;

  if(argaddr(0, &p) < 0) {
    return -1;
  }

  uint64 poccess_num = num_of_unused();
  uint64 free_byte_num = num_of_free_memory() * 4096;
  printf("%d---------\n", free_byte_num);
  struct proc *pcb = myproc();
  si.freemem = free_byte_num;
  si.nproc = poccess_num;
  if(copyout(pcb->pagetable, p, (char*)&si, sizeof(si)) < 0)
    return -1;
  
  return 0;
}


uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
