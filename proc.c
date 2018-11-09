#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "uproc.h"

static char *states[] = {
[UNUSED]    "unused",
[EMBRYO]    "embryo",
[SLEEPING]  "sleep ",
[RUNNABLE]  "runble",
[RUNNING]   "run   ",
[ZOMBIE]    "zombie"
};

// Packages the head and tail pointers so we can put them in an array
#ifdef CS333_P3
struct ptrs {
  struct proc* head;
  struct proc* tail;
};
#endif // CS333_P3

// Process table struct
static struct {
  struct spinlock lock;
  struct proc proc[NPROC];
  #ifdef CS333_P3
  struct ptrs list[statecount];
  #endif // CS333_P3
  #ifdef CS333_P4
  struct ptrs ready[MAXPRIO+1];
  uint PromoteAtTime;
  #endif // CS333_P4
} ptable;

static struct proc *initproc;
uint nextpid = 1;
extern void forkret(void);
extern void trapret(void);
static void wakeup1(void* chan);
#ifdef CS333_P4
static void procdumpP4(struct proc* p, char* state);
static void assertPriority(struct proc* p, int priority);
static void promote(void);
/*
static void transition(struct proc** oldhead, struct proc** oldtail,
  struct proc** newhead, struct proc** newtail,
  enum procstate oldstate, enum procstate newstate,
  struct proc* p);
*/
#endif // CS333_P4
#ifdef CS333_P3
// List management function prototypes
static void initProcessLists(void);
static void initFreeList(void);
static void stateListAdd(struct ptrs* list, struct proc* p);
static int stateListRemove(struct ptrs* list, struct proc* p);
// Assert state function prototype
static void assertState(struct proc* p, enum procstate state);
// Helper functions
static void printcmd(struct proc* p);
#endif // CS333_P3
#ifdef CS333_P2
extern void procdumpP2(struct proc* p, char* state);
#endif // CS 333_P2
#ifdef CS333_P1
extern void padmilliseconds(int milliseconds);
extern void procdumpP1(struct proc* p, char* state);
#endif // CS 333_P1

#ifdef CS333_P3
// List management helper functions
static void
stateListAdd(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL){
    (*list).head = p;
    (*list).tail = p;
    p->next = NULL;
  } else{
    ((*list).tail)->next = p;
    (*list).tail = ((*list).tail)->next;
    ((*list).tail)->next = NULL;
  }
}

static int
stateListRemove(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL || (*list).tail == NULL || p == NULL){
    return -1;
  }

  struct proc* current = (*list).head;
  struct proc* previous = 0;

  if(current == p){
    (*list).head = ((*list).head)->next;
    // prevent tail remaining assigned when we've removed the only item
    // on the list
    if((*list).tail == p){
      (*list).tail = NULL;
    }
    return 0;
  }

  while(current){
    if(current == p){
      break;
    }

    previous = current;
    current = current->next;
  }

  // Process not found. return error
  if(current == NULL){
    return -1;
  }

  // Process found.
  if(current == (*list).tail){
    (*list).tail = previous;
    ((*list).tail)->next = NULL;
  } else{
    previous->next = current->next;
  }

  // Make sure p->next doesn't point into the list.
  p->next = NULL;

  return 0;
}

static void
initProcessLists()
{
  int i;

  for (i = UNUSED; i <= ZOMBIE; i++) {
    ptable.list[i].head = NULL;
    ptable.list[i].tail = NULL;
  }
#ifdef CS333_P4
  for (i = 0; i <= MAXPRIO; i++) {
    ptable.ready[i].head = NULL;
    ptable.ready[i].tail = NULL;
  }
#endif
}

static void
initFreeList(void)
{
  struct proc* p;

  for(p = ptable.proc; p < ptable.proc + NPROC; ++p){
    p->state = UNUSED;
    stateListAdd(&ptable.list[UNUSED], p);
  }
}
#endif // CS333_P3

#ifdef CS333_P3
// Invokes kernel panic if state check fails; check should be performed
// after a process is removed from a list, and before the process is added to another list
static void
assertState(struct proc* p, enum procstate state)
{
  if(p->state != state) {
    cprintf("Failure: p->state is %s and p->state should be %s", states[p->state], states[state]);
    panic("Kernel panic\n");
  }
}
#endif // CS333_P3

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
    if (cpus[i].apicid == apicid) {
      return &cpus[i];
    }
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

#ifdef CS333_P3
//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  /* modified version of allocproc() */
  struct proc *p;
  char *sp;
  int found = 0;

  acquire(&ptable.lock);

  if(ptable.list[UNUSED].head) {
    p = ptable.list[UNUSED].head;
    found = 1;
  }

  if(found) {
    if(stateListRemove(&ptable.list[UNUSED], p) < 0) {
      panic("Error: failed to remove process from UNUSED list in allocproc()\n");
    } else {
      assertState(p, UNUSED);
      p->state = EMBRYO;
      stateListAdd(&ptable.list[EMBRYO], p);
      p->pid = nextpid++;
      release(&ptable.lock);
    }
  }
  else if(!found) {
    release(&ptable.lock);
    return 0;
  }

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0) {
    acquire(&ptable.lock);
    // rc is the return code that indicates success or failure
    int rc = stateListRemove(&ptable.list[EMBRYO], p);
    if(rc < 0) {
      panic("Error: failed to remove process from EMBRYO list in allocproc()\n");
    }
    assertState(p, EMBRYO);
    p->state = UNUSED;
    stateListAdd(&ptable.list[UNUSED], p);
    release(&ptable.lock);
    //return 0;
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
  p->context->eip = (uint)forkret;

  #ifdef CS333_P1
  p->start_ticks = ticks;
  #endif // CS333_P1
  #ifdef CS333_P2
  p->cpu_ticks_total = 0;
  p->cpu_ticks_in = 0;
  #endif // CS333_P2
  #ifdef CS333_P4
  p->priority = MAXPRIO;
  p->budget = BUDGET;
  #endif // CS333_P4
  return p;
}

#else
//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  /* original version of allocproc() */
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  int found = 0;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED) {
      found = 1;
      break;
    }
  if (!found) {
    release(&ptable.lock);
    return 0;
  }
  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);

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
  p->context->eip = (uint)forkret;

  #ifdef CS333_P1
  p->start_ticks = ticks;
  #endif // CS333_P1
  #ifdef CS333_P2
  p->cpu_ticks_total = 0;
  p->cpu_ticks_in = 0;
  #endif // CS333_P2
  return p;
}
#endif

//PAGEBREAK: 32
// Set up first user process.
void userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  #ifdef CS333_P3
  // Initialize lists; since the new lists are part of the oncurrent data structure ptable,
  // proper concurrency control must be used (ptable lock acquired and released correctly)
  acquire(&ptable.lock);
  initProcessLists();
  initFreeList();
  release(&ptable.lock);
  #endif // CS333_P3
  #ifdef CS333_P4
  acquire(&ptable.lock);
  ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
  release(&ptable.lock);
  #endif // CS333_P4

  // Routine calls allocproc() which is where a process will be removed from the free list and allocated
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

  #ifdef CS333_P2
  p->uid = DEFUID;
  p->uid = DEFGID;
  #endif // CS333_P2

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  #ifdef CS333_P3
  // EMBRYO to RUNNABLE state transition
  acquire(&ptable.lock);
  int rc = stateListRemove(&ptable.list[EMBRYO], p);
  if(rc < 0) {
    panic("Error: failed to remove process from EMBRYO list in userinit()\n");
  }
  assertState(p, EMBRYO);
  p->state = RUNNABLE;
  stateListAdd(&ptable.ready[MAXPRIO], p);
  release(&ptable.lock);
  #else
  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  p->state = RUNNABLE;
  #endif
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
  int i;
  uint pid;
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
    #ifdef CS333_P3
    acquire(&ptable.lock);
    int rc = stateListRemove(&ptable.list[EMBRYO], np);
    if(rc < 0) {
      panic("Error: failed to remove process from EMBRYO list in fork()\n");
    }
    assertState(np, EMBRYO);
    np->state = UNUSED;
    stateListAdd(&ptable.list[UNUSED], np);
    release(&ptable.lock);
    #else
    np->state = UNUSED;
    #endif
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;
  #ifdef CS333_P2
  np->uid = curproc->uid;
  np->gid = curproc->gid;
  #endif // CS333_P2

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);
  #ifdef CS333_P3
  int rc = stateListRemove(&ptable.list[EMBRYO], np);
  if(rc < 0) {
    panic("Error: failed to remove process from EMBRYO state list in fork()\n");
  }
  assertState(np, EMBRYO);
  np->state = RUNNABLE;
  stateListAdd(&ptable.ready[MAXPRIO], np);
  #else
  np->state = RUNNABLE;
  #endif
  release(&ptable.lock);

  return pid;
}

#ifdef CS333_P3
// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  /* modified version of exit() */
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
  iput(curproc->cwd); end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);
  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);
  // Search runnable list for abandoned children
  #ifdef CS333_P4
  for(int i = 0; i <= MAXPRIO; ++i) {
    p = ptable.ready[i].head;
    while(p) {
      if(p->parent == curproc)
        p->parent = initproc;
      p = p->next;
    }
  }
  #endif // CS333_P4
  // Search running list for abandoned children
  p = ptable.list[RUNNING].head;
  while(p) {
    if(p->parent == curproc)
      p->parent = initproc;
    p = p->next;
  }
  // Search embryo list for abandoned children
  p = ptable.list[EMBRYO].head;
  while(p) {
    if(p->parent == curproc)
      p->parent = initproc;
    p = p->next;
  }
  // Search sleeping list for abandoned children
  p = ptable.list[SLEEPING].head;
  while(p) {
    if(p->parent == curproc)
      p->parent = initproc;
    p = p->next;
  }
  // Search zombie list for abandoned children
  p = ptable.list[ZOMBIE].head;
  while(p) {
    if(p->parent == curproc) {
      p->parent = initproc;
      wakeup1(initproc);
    }
    p = p->next;
  }
  // Jump into the scheduler, never to return.
  // RUNNING -> ZOMBIE state transition
  int rc = stateListRemove(&ptable.list[RUNNING], curproc);
  if(rc < 0) {
    panic("Error: failed to remove process from RUNNING state list in exit()\n");
  }
  assertState(curproc, RUNNING);
  curproc->state = ZOMBIE;
  stateListAdd(&ptable.list[ZOMBIE], curproc);
  assertState(curproc, ZOMBIE);

  sched();
  panic("zombie exit");
}

#else
// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  /* original version of exit() */
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

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}
#endif

#ifdef CS333_P3
// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  /* modified version of wait() */
  struct proc *p;
  int havekids = 0;
  uint pid = 0;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    p = ptable.list[ZOMBIE].head;
    while(p && !havekids) {
      if(p->parent == curproc) {
        havekids = 1;
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        int rc = stateListRemove(&ptable.list[ZOMBIE], p);
        if(rc < 0) {
          panic("Error: failed to remove process from ZOMBIE list in wait()\n");
        }
        assertState(p, ZOMBIE);
        p->state = UNUSED;
        stateListAdd(&ptable.list[UNUSED], p);
        assertState(p, UNUSED);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
        }
      p = p->next;
    }

    p = ptable.list[RUNNING].head;
    while(p && !havekids) {
      if(p->parent == curproc) {
        havekids = 1;
      }
      p = p->next;
    }

    p = ptable.list[SLEEPING].head;
    while(p && !havekids) {
      if(p->parent == curproc) {
        havekids = 1;
      }
      p = p->next;
    }

    for(int i = 0; i <= MAXPRIO; ++i) {
      p = ptable.ready[i].head;
      while(p) {
        if(p->parent == curproc) {
          havekids = 1;
        }
        p = p->next;
      }
    }

    p = ptable.list[EMBRYO].head;
    while(p && !havekids) {
      if(p->parent == curproc) {
        havekids = 1;
      }
      p = p->next;
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
  return 0;
}

#else
// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  /* original version of wait() */
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
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
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
#endif

#ifdef CS333_P3
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
  /* modified version of scheduler() */
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    #ifdef CS333_P4
    if(ticks >= ptable.PromoteAtTime) {
      promote();
      ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
    }
    for(int i = 0; i <= MAXPRIO; ++i) {
      p = ptable.ready[i].head;
      if(p) {
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
#ifdef PDX_XV6
      idle = 0;  // not idle this timeslice
#endif // PDX_XV6
      c->proc = p;
      switchuvm(p);
      p = ptable.ready[i].head;
      int rc = stateListRemove(&ptable.ready[i], p);
      if(rc < 0) {
        panic("Error: failed to remove process from RUNNABLE state list in scheduler()\n");
      }
      assertState(p, RUNNABLE);
      assertPriority(p, p->priority);
      p->state = RUNNING;
      stateListAdd(&ptable.list[RUNNING], p);
      assertState(p, RUNNING);
#endif // CS333_P4
#ifdef CS333_P2
      p->cpu_ticks_in = ticks;
#endif // CS333_P2
      swtch(&(c->scheduler), p->context);
      switchkvm();
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      }
    }
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}

#else
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
  /* original version of scheduler() */
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
#ifdef PDX_XV6
      idle = 0;  // not idle this timeslice
#endif // PDX_XV6
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
#ifdef CS333_P2
      p->cpu_ticks_in = ticks;
#endif // CS333_P2
      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}
#endif

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

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  #ifdef CS333_P2
  p->cpu_ticks_total += ticks - p->cpu_ticks_in;
  #endif // CS333_P2
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

#ifdef CS333_P3
// Give up the CPU for one scheduling round.
void
yield(void)
{
  /* modified version of yield() */
  struct proc *curproc = myproc();
  #ifdef CS333_P4
  acquire(&ptable.lock);
  curproc->budget -= (ticks - curproc->cpu_ticks_in);
  if(curproc->budget <= 0 && curproc->priority < MAXPRIO) {
    ++curproc->priority;
    curproc->budget = BUDGET;
  }
  release(&ptable.lock);
  #endif // CS333_P4

  acquire(&ptable.lock);  //DOC: yieldlock
  int rc = stateListRemove(&ptable.list[RUNNING], curproc);
  if(rc < 0) {
    panic("Error: failed to remove process from RUNNING list in yield()\n");
  }
  assertState(curproc, RUNNING);
  curproc->state = RUNNABLE;
  stateListAdd(&ptable.ready[curproc->priority], curproc);

  sched();
  release(&ptable.lock);
}

#else
// Give up the CPU for one scheduling round.
void
yield(void)
{
  /* original version of yield() */
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock
  curproc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}
#endif

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

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

#ifdef CS333_P3
// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  /* modified version of sleep() */
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  #ifdef CS333_P4
  p->budget -= (ticks - p->cpu_ticks_in);
  if(p->budget <= 0 && p->priority < MAXPRIO) {
    ++p->priority;
    p->budget = BUDGET;
  }
  #endif // CS333_P4
  int rc = stateListRemove(&ptable.list[RUNNING], p);
  if(rc < 0) {
    panic("Error: failed to remove process from RUNNING list in sleep()\n");
  }
  assertState(p, RUNNING);
  p->state = SLEEPING;
  stateListAdd(&ptable.list[SLEEPING], p);

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}

#else
// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  /* original version of sleep() */
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}
#endif

#ifdef CS333_P3
//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  /* modified version of wakeup1() */
  struct proc *p;
  p = ptable.list[SLEEPING].head;
  while(p) {
    if(p->chan == chan) {
      int rc = stateListRemove(&ptable.list[SLEEPING], p);
      if(rc < 0) {
        panic("Error: failed to remove process from SLEEPING state list in wakeup1()\n");
      }
      assertState(p, SLEEPING);
      p->state = RUNNABLE;
      stateListAdd(&ptable.ready[p->priority], p);
    }
    p = p->next;
  }
}

#else
//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  /* original version of wakeup1() */
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}
#endif

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

#ifdef CS333_P3
// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  /* modified version of kill() */
  struct proc *p;
  acquire(&ptable.lock);

  p = ptable.list[RUNNING].head;
  while(p) {
    if(p->pid == pid) {
      p->killed = 1;
      release(&ptable.lock);
      return 0;
    }
    p = p->next;
  }

  #ifdef CS333_P4
  for(int i = 0; i <= MAXPRIO; ++i) {
    p = ptable.ready[i].head;
    while(p) {
      if(p->pid == pid) {
        p->killed = 1;
        release(&ptable.lock);
        return 0;
      }
      p = p->next;
    }
  }
  #endif // CS333_P4

  p = ptable.list[SLEEPING].head;
  while(p) {
    if(p->pid == pid) {
      p->killed = 1;
      int rc = stateListRemove(&ptable.list[SLEEPING], p);
      if(rc < 0) {
        panic("Error: failed to remove process from SLEEPING list in kill()\n");
      }
      assertState(p, SLEEPING);
      p->state = RUNNABLE;
      stateListAdd(&ptable.ready[p->priority], p);
      release(&ptable.lock);
      return 0;
    }
    p = p->next;
  }

  p = ptable.list[EMBRYO].head;
  while(p) {
    if(p->pid == pid) {
      p->killed = 1;
      release(&ptable.lock);
      return 0;
    }
    p = p->next;
  }

  p = ptable.list[ZOMBIE].head;
  while(p) {
    if(p->pid == pid) {
      p->killed = 1;
      release(&ptable.lock);
      return 0;
    }
    p = p->next;
  }
  release(&ptable.lock);
  return -1;
}

#else
// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  /* original version of kill() */
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#endif

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
#ifdef CS333_P1
void
padmilliseconds(int milliseconds)
{
  if(milliseconds < 10 && milliseconds > 0)
    cprintf("00");
  if(milliseconds < 100 && milliseconds >= 10)
    cprintf("0");
}

void
procdumpP1(struct proc* p, char* state)
{
  int milliseconds;
  int elapsed;

  elapsed = ticks - p->start_ticks;
  milliseconds = elapsed % 1000;
  elapsed = elapsed/1000;

  cprintf("%d\t%s\t\t%d.", p->pid, p->name, elapsed);

  if(milliseconds < 100 && milliseconds >= 10)
    cprintf("00");
  if(milliseconds < 10)
    cprintf("0");

  cprintf("%d\t%s\t%d\t%p", milliseconds, state, p->sz);
}
#endif // CS333_P1

#ifdef CS333_P2
void
procdumpP2(struct proc* p, char* state)
{
  int milliseconds;
  int elapsed;
  int ppid;
  int cpu_milliseconds;
  int cpu;

  // If parent pointer is NULL, set to PID
  if(p->parent)
    ppid = p->parent->pid;
  else
    ppid = p->pid;
  cprintf("%d\t%s\t", p->pid, p->name);
  // Adjust column width for longer names
  // if(strlen(p->name) > 7)
  // cprintf("\t");
  // Calculate elapsed process time
  elapsed = ticks - p->start_ticks;
  milliseconds = elapsed % 1000;
  elapsed = elapsed/1000;
  // Print
  cprintf("%d\t%d\t%d\t%d.", p->uid, p->gid, ppid, elapsed);
  padmilliseconds(milliseconds);
  // Calculate CPU time
  cpu = p->cpu_ticks_total;
  cpu_milliseconds = cpu % 1000;
  cpu = cpu/1000;
  // Print
  cprintf("%d\t%d.", milliseconds, cpu);
  padmilliseconds(cpu_milliseconds);
  cprintf("%d\t%s\t%d\t%p", cpu_milliseconds, state, p->sz);
}
#endif // CS333_P2

#ifdef CS333_P4
void
procdumpP4(struct proc* p, char* state)
{
  int milliseconds;
  int elapsed;
  int ppid;
  int cpu_milliseconds;
  int cpu;

  // If parent pointer is NULL, set to PID
  if(p->parent)
    ppid = p->parent->pid;
  else
    ppid = p->pid;
  cprintf("%d\t%s\t", p->pid, p->name);
  // Adjust column width for longer names
  // if(strlen(p->name) > 7)
  // cprintf("\t");
  // Calculate elapsed process time
  elapsed = ticks - p->start_ticks;
  milliseconds = elapsed % 1000;
  elapsed = elapsed/1000;
  // Print
  cprintf("%d\t%d\t%d\t%d\t%d.", p->uid, p->gid, ppid, p->priority, elapsed);
  padmilliseconds(milliseconds);
  // Calculate CPU time
  cpu = p->cpu_ticks_total;
  cpu_milliseconds = cpu % 1000;
  cpu = cpu/1000;
  // Print
  cprintf("%d\t%d.", milliseconds, cpu);
  padmilliseconds(cpu_milliseconds);
  cprintf("%d\t%s\t%d\t%p", cpu_milliseconds, state, p->sz);
}
#endif // CS333_P4

#ifdef CS333_P3
static void
printcmd(struct proc *p) {
  while(p) {
    cprintf("%d ", p->pid);
    if(p->next)
      cprintf("-> ");
    p = p->next;
  }
  cprintf("\n");
}

// Control-f: prints the number of processes on the free list
void
freedump(void)
{
  struct proc* p;
  int free_list = 0;
  acquire(&ptable.lock);
  p = ptable.list[UNUSED].head;
  while(p) {
    ++free_list;
    p = p->next;
  }
  cprintf("Free List Size: %d processes\n", free_list);
  release(&ptable.lock);
}

// Control-r: prints the PIDs of all processes on the ready list
void
readydump(void)
{
  struct proc* p;
  acquire(&ptable.lock);
  cprintf("Ready List Processes: \n");
  for(int i = 0; i <= MAXPRIO; ++i) {
    p = ptable.ready[i].head;
    // %d represents MAXPRIO, MAXPRIO-1, MAXPRIO-2, etc.
    cprintf("%d: ", i);
    while(p) {
      cprintf("(%d, %d)", p->pid, p->budget);
      if(p->next)
        cprintf(" -> ");
      else
        cprintf("\n");
      p = p->next;
    }
  cprintf("\n");
  }
  release(&ptable.lock);
}

// Control-s: prints the PIDs of all processes on the sleep list
void
sleepdump(void)
{
  struct proc* p;
  acquire(&ptable.lock);
  p = ptable.list[SLEEPING].head;
  cprintf("Sleep List Processes: \n");
  // helper function to print sleep list
  printcmd(p);
  release(&ptable.lock);
}

// Control-z: prints the PIDs of all processes on the zombie list + their parent PID
void
zombiedump(void)
{
  struct proc* p;
  int ppid = 0;
  acquire(&ptable.lock);
  p = ptable.list[ZOMBIE].head;
  cprintf("Zombie List Processes: \n");
  while(p) {
    if(p->parent)
      ppid = p->parent->pid;
    else
      ppid = p->pid;
    cprintf("(%d, %d) ", p->pid, ppid);
    if(p->next)
      cprintf("-> ");
    p = p->next;
  }
  cprintf("\n");
  release(&ptable.lock);
}
#endif // CS333_P3

void
procdump(void)
{
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

#if defined(CS333_P4)
#define HEADER "\nPID\tName\tUID\tGID\tPPID\tPrio\tElapsed\tCPU\tState\tSize\tPCs\n"
#elif defined(CS333_P2)
#define HEADER "\nPID\tName\tUID\tGID\tPPID\tElapsed\tCPU\tState\tSize\tPCs\n"
#elif defined(CS333_P1)
#define HEADER "\nPID\tName\tElapsed\tState\tSize\tPCs\n"
#else
#define HEADER "\n"
#endif

  cprintf(HEADER);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";

#if defined(CS333_P4)
  procdumpP4(p, state);
#elif defined(CS333_P2)
  procdumpP2(p, state);
#elif defined(CS333_P1)
  procdumpP1(p, state);
#else
  cprintf("%d\t%s\t%s\t", p->pid, p->name, state);
#endif

    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

#ifdef CS333_P2
int
setuid(int*  uid)
{
  acquire(&ptable.lock);
  myproc()->uid = * uid;
  release(&ptable.lock);
  return 0;
}

int
setgid(int* gid)
{
  acquire(&ptable.lock);
  myproc()->gid = * gid;
  release(&ptable.lock);
  return 0;
}

int
getprocs(uint max, struct uproc* table)
{
  int count = 0;
  struct proc* p = ptable.proc;

  acquire(&ptable.lock);
  while(p < &ptable.proc[NPROC] && count < max) {
    if(p-> state != UNUSED && p->state != EMBRYO) {
      table->pid = p->pid;
      table->uid = p->uid;
      table->gid = p->gid;
      ++count;
      if(p->parent)
        table->ppid = p->parent->pid;
      else
        table->ppid = p->pid;
      table->elapsed_ticks = (ticks - p->start_ticks);
      table->CPU_total_ticks = p->cpu_ticks_total;
      safestrcpy(table->state, states[p->state], STRMAX);
      table->size = p->sz;
      safestrcpy(table->name, p->name, STRMAX);
      #ifdef CS333_P4
      table->priority = p->priority;
      #endif // CS333_P4
      ++table;
    }
    ++p;
  }
  release(&ptable.lock);
  return count;
}
#endif // CS333_P2

#ifdef CS333_P4
int
setpriority(int pid, int priority)
{
  acquire(&ptable.lock);
  struct proc* p = ptable.list[RUNNING].head;
  while(p) {
    if(p->pid == pid) {
      p->priority = priority;
      p->budget = BUDGET;
      release(&ptable.lock);
      return 0;
    }
    p = p->next;
  }
  for(int i = 0; i <= MAXPRIO; ++i) {
    p = ptable.ready[i].head;
    while(p) {
      if(p->pid == pid) {
        p->priority = priority;
        p->budget = BUDGET;
        if(priority != i) {
          int rc = stateListRemove(&ptable.ready[i], p);
          if(rc < 0) {
            panic("Error: failed to remove process from RUNNING list in yield()\n");
          }
          stateListAdd(&ptable.ready[priority], p);
          assertState(p, RUNNABLE);
          p->state = RUNNABLE;
          stateListAdd(&ptable.ready[priority], p);
        }
        release(&ptable.lock);
        return 0;
      }
      p = p->next;
    }
   }
   p = ptable.list[SLEEPING].head;
   while(p) {
     if(p->pid == pid) {
       p->priority = priority;
       p->budget = BUDGET;
       release(&ptable.lock);
       return 0;
     }
     p = p->next;
   }
   p = ptable.list[EMBRYO].head;
   while(p) {
     if(p->pid == pid) {
       p->priority = priority;
       p->budget = BUDGET;
       release(&ptable.lock);
       return 0;
     }
     p = p->next;
   }
   p = ptable.list[ZOMBIE].head;
   while(p) {
     if(p->pid == pid) {
       p->priority = priority;
       p->budget = BUDGET;
       release(&ptable.lock);
       return 0;
     }
     p = p->next;
   }
   release(&ptable.lock);
   return -1;
}

int
getpriority(int pid)
{
  int priority;
  acquire(&ptable.lock);
  struct proc* p;
  p = ptable.list[RUNNING].head;
  while(p) {
    if(p->pid == pid) {
      priority = p->priority;
      release(&ptable.lock);
      return priority;
    }
    p = p->next;
  }
  p = ptable.list[SLEEPING].head;
  while(p) {
    if(p->pid == pid) {
      priority = p->priority;
      release(&ptable.lock);
      return priority;
    }
    p = p->next;
  }
  p = ptable.list[RUNNABLE].head;
  while(p) {
    if(p->pid == pid) {
      priority = p->priority;
      release(&ptable.lock);
      return priority;
    }
    p = p->next;
  }
  release(&ptable.lock);
  return -1;
}

static void
assertPriority(struct proc* p, int priority) {
  if(p->priority != priority) {
    cprintf("Failure: p->priority is %s and p->priority should be %s", p->priority, priority);
    panic("Kernel panic\n");
  }
}

static void
promote(void)
{
  struct proc* p;
  p = ptable.list[RUNNING].head;
  while(p) {
    if(p->priority > 0) {
      p->priority -= 1;
      p->budget = BUDGET;
    }
    p = p->next;
  }
  p = ptable.list[SLEEPING].head;
  while(p) {
    if(p->priority > 0) {
      p->priority -= 1;
      p->budget = BUDGET;
    }
    p = p->next;
  }
  for(int i = 1; i <= MAXPRIO; ++i) {
    p = ptable.ready[i].head;
    while(p) {
    // rc is the return code that indicates success or failure
      int rc = stateListRemove(&ptable.ready[i], p);
      if(rc < 0) {
        panic("Error: failed to remove process from READY list in promote()\n");
      }
      assertState(p, RUNNABLE);
      p->state = RUNNABLE;
      stateListAdd(&ptable.ready[i-1], p);
      p->priority -= 1;
      p->budget = BUDGET;
      p = p->next;
    }
  }
}
#endif // CS333_P4
