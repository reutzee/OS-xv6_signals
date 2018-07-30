#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

extern void* begin_sigret;
extern void* end_sigret;
struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}


//#define SIG_NUM 32
// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}


int 
allocpid(void) 
{
  int pid;
   do {
    pid = nextpid;
  } while (!cas(&nextpid, pid, pid + 1));

return pid;
}

// Sending SIGSTOP to a blocked process will be ignored
void handle_sig_stop()
{

  while(!(myproc()->pending_signals & (1<<SIGCONT)) && !(myproc()->pending_signals & (1<<SIGKILL))){    
        yield();
  }

  if(myproc()->killed){
      return;
  }
  int rest_signals;
  do{
      rest_signals = myproc()->pending_signals; 
  }while(!cas(&myproc()->pending_signals, rest_signals, rest_signals^(1<<SIGCONT)));
}

void all_signal_handlers(struct trapframe* tf)
{
  struct proc* p=myproc();
  if(p==0 || p->executing_signal)
  {
      return;
  }
   int temp_pend_signals;
   do{
        temp_pend_signals = p->pending_signals; 
    }while(!cas(&p->pending_signals, temp_pend_signals, temp_pend_signals));
    temp_pend_signals &= ~(p->signal_mask);

    int i;
    for(i=0; i<SIG_NUM; i++)
    {
      if((1<<i) & (temp_pend_signals))
      {

        //shutdown signal-bit
        int rest_signals;
        do{
        rest_signals = p->pending_signals;
        }while(!cas(&p->pending_signals, rest_signals, rest_signals^(1<<i)));

        sighandler_t handler = p->signal_handler[i];
        p->executing_signal = 1;
        if((int)handler == SIG_IGN)
        {
          p->executing_signal=0;
         continue; //IGNORE
        }
        else if((int)handler == SIGSTOP || ((int)handler == SIG_DFL && i == SIGSTOP))
        {
          handle_sig_stop();
        }
        else if((int)handler == SIGCONT || ((int)handler == SIG_DFL && i == SIGCONT))
        {
        // handle it on stop() already
        }

        else if((int)handler == SIGKILL || (int)handler == SIG_DFL)
        {
          p->killed = 1;
          p->executing_signal=0;
          return;
        }
        else
        {
          memmove(&p->user_backup, p->tf, sizeof(struct trapframe));//backing up trap frame
          p->tf->esp -= ((uint)&end_sigret - (uint)&begin_sigret); //clear space for sigret invoke func
          memmove(((void*)(p->tf->esp)), &begin_sigret, (uint)&end_sigret - (uint)&begin_sigret);//copy sigret invoke func
          p->tf->esp -=4;
          *((int*)(p->tf->esp)) = i;
          p->tf->esp -=4;
          *((int*)(p->tf->esp)) = p->tf->esp + 8;//return func of the handler is sigret
          p->tf->eip = (uint)p->signal_handler[i];
          return;
        }
        p->executing_signal = 0;

      }
    }
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

pushcli();
  do {
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
      if(p->state == UNUSED)
        break;
    if (p == &ptable.proc[NPROC]) {
      popcli();
      return 0; // ptable is full
    }
  } while (!cas(&p->state, UNUSED, EMBRYO));
popcli();

  p->pid = allocpid();


  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;
  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;
  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  memset(p->signal_handler, SIG_DFL, SIG_NUM);           // The default handler, for all signals, should be SIG_DFL
  p->signal_handler[SIG_IGN]=(sighandler_t)SIG_IGN;

  p->pending_signals=0;
  p->signal_mask=0;
  p->executing_signal=0;
  p->frozen=0;
  p->context->eip = (uint)forkret;
  return p;
}



uint sigprocmask(uint sigmask)
{
  struct proc *p = myproc();
  uint old_mask = p->signal_mask;
  p->signal_mask = sigmask;
  return old_mask;
}
 
sighandler_t signal(int signum, sighandler_t handler)
{

  if(signum<0||signum>SIG_NUM)
  {
    return (sighandler_t)-2;
  }
  struct proc * p= myproc();
  sighandler_t tmp=(sighandler_t)(p->signal_handler[signum]);
  p->signal_handler[signum]=handler;

  return tmp;
}

void sigret(void) {
  struct proc * p=myproc();
  if(p==0)
    return;
  memmove(p->tf, & (p->user_backup), sizeof(struct trapframe));
  p->signal_mask =p->backup_mask; // enable handling next pending signal
  p->executing_signal = 0;
}


//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  pushcli();

  if (!(cas(&p->state, EMBRYO, RUNNABLE)))
  {
    panic("cas failed at user init");
  }
  popcli();
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));
  np->signal_mask=curproc->signal_mask;
  
  np->pending_signals=0;
  np->executing_signal=0;
  int tmp;
  for(tmp=0;tmp<SIG_NUM;tmp++)
  {
    np->signal_handler[tmp] = curproc->signal_handler[tmp];
  }
  pid = np->pid;

  pushcli();
  if(!(cas(&np->state, EMBRYO, RUNNABLE)))
  {
    panic("cas failed at fork FROM EMBRYO to RUNNABLE");
  }

  popcli();

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  //acquire(&ptable.lock);
  pushcli();

  // Parent might be sleeping in wait().
  //wakeup1(curproc->parent);

  if(!cas(&curproc->state,RUNNING, -ZOMBIE))
  {
    panic("cas failed at exit");
  }

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  //curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  pushcli();
  for(;;){
    curproc->chan = (void*)curproc;
    // Scan through table looking for exited children.
    if (!cas(&(curproc->state), RUNNING,-SLEEPING)){ 
          panic("wait: failed doing cas");
      }  

    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      while(p->state == -ZOMBIE);
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->chan=0;
        curproc->chan = 0;
        if (!(cas(&p->state,ZOMBIE,UNUSED)))
        {
          panic("cas failed at wait CHILD MOVE ZOMBIE to UNUSED");
        }

        if (!(cas(&curproc->state,-SLEEPING, RUNNING)))
        {
          panic("cas failed at wait PARENT MOVE NEG SLEEPING to RUNNING");
        }

        popcli();
        return pid;
      }
    }
    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      curproc->chan = 0;
      while(!(cas(&curproc->state, -SLEEPING, RUNNING)));
      popcli();
      return -1;
    }
    // Wait for children to exit.  (See wakeup1 call in proc_exit.)

    sched();
    curproc->chan = 0;
    //sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    pushcli();
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){

      if(!(cas(&p->state,RUNNABLE,RUNNING)))
      {
        continue;
      }
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);

      swtch(&(c->scheduler), p->context);
      switchkvm();

      if(cas(&p->state,-ZOMBIE, ZOMBIE))
      {
        wakeup1(p->parent);
      }

      if(cas(&p->state, -SLEEPING, SLEEPING))
      {
        if(p->killed==1)
        {
          cas(&p->state, SLEEPING,RUNNABLE);
        }
      }
      cas(&p->state, -RUNNABLE, RUNNABLE);
   
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }

    popcli();
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  //if(!holding(&ptable.lock))
  //  panic("sched ptable.lock");
  if(mycpu()->ncli != 1){
    panic("sched locks");
  }
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  pushcli();
  if (!cas(&myproc()->state,RUNNING,-RUNNABLE)){
      panic("yield: failed doing cas");
  }
  sched();
  popcli();
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.

  popcli();

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  
  pushcli();

  // Go to sleep.
  p->chan = chan;
  //p->state = SLEEPING;
  if(!(cas(&p->state, RUNNING,-SLEEPING)))
  {
    panic("cas failed at sleep");
  }


  if(lk != &ptable.lock){  //DOC: sleeplock0
    release(lk);
  }

  sched();

  // Tidy up.
  p->chan = 0;

  popcli();

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if( (p->state == SLEEPING || p->state == -SLEEPING)  && p->chan == chan){
      while(p->state== -SLEEPING);
      cas(&p->state, SLEEPING, RUNNABLE);
    }
  }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  pushcli();
  wakeup1(chan);
  popcli();
}




// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid,int signum)
{

  if(signum<0||signum>SIG_NUM)
  {
    return -1;
  }
  struct proc *p;

  pushcli();
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      if(p->state==ZOMBIE)
      {
        popcli();
        return -1;
      }
      if(p->state==EMBRYO)
      {
        popcli();
        return -1;
      }
      if(p->state==UNUSED)
      {
        popcli();
        return -1;
      }
      if(p->killed==1)
      {
        popcli();
        return -1;
      }


      if(signum==SIGSTOP)
      {
        if(p->state==SLEEPING)//ignore sigstop for blocked proccesses
        {
          popcli();
          return 0;
        }
      }

      int ps=0;
      int x=1<<signum;
      do{
        ps=p->pending_signals;
      }while( !(cas(&p->pending_signals,ps,ps|x)));

      popcli();
      return 0;
    }
  }
  popcli();
  return -1;
}
  



//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie",
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else {  
      state = "???";
    if(p->state==-SLEEPING)
          state = "NEG_SLEP";
      if(p->state==-RUNNABLE)
        state="NEG_RUNBLE";
      if(p->state==-ZOMBIE)
        state="NEG_ZOMBI";
    }
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
