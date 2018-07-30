#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_sigret(void)
{
  sigret();
  return 0;
}

int 
sys_signal(void)
{
  int* newsignal;
  int signum;
  if(argint(0, &signum)<0)
  {
    return -1;
  }
  if (argint(1,(int*)(&newsignal))<0)
    return -1;

  return (int)signal(signum,(sighandler_t)newsignal);
}


int
sys_sigprocmask (void)
{
  int sigmask;
  argint(0, &sigmask);
  

  return sigprocmask((uint)sigmask);
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;
  int signum;

  if(argint(0, &pid) < 0)
    return -1;
  if(argint(1,&signum)<0)
    return -1;
  return kill(pid,signum);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
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

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed|| ( (myproc()->pending_signals & (1<< SIGKILL))!=0  && (myproc()->signal_mask &  (1<< SIGKILL))==0)  ){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
