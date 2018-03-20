#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <structures.h>
#include <stddef.h>


#include "../src/usyscall.h"
#include "phase1.h"
#include "structures.h"
#include "phase3.h"
#include "phase2.h"
#include "../src/usloss.h"
#include "libuser.h"

/* ------------------------- Prototypes ----------------------------------- */
void makeSureCurrentFunctionIsInKernelMode(char *);
void nullsys3(USLOSS_Sysargs *);
void spawn(USLOSS_Sysargs *);
void wait(USLOSS_Sysargs *);
void terminate(USLOSS_Sysargs *);
void semCreate(USLOSS_Sysargs *);
void semP(USLOSS_Sysargs *);
void semV(USLOSS_Sysargs *);
void semFree(USLOSS_Sysargs *);

int semFreeReal(int);
int semCreateReal(int);
void semPReal(int);
void semVReal(int);
int spawnReal(char *, int(*)(char *), char *, int, int);
int waitReal(int *);
void terminateReal(int);

int spawnLaunch(char *);
void getTimeOfDay(USLOSS_Sysargs *);
void cpuTime(USLOSS_Sysargs *);
void getPID(USLOSS_Sysargs *);

void emptyProc3(int);
void initProc(int);
void switchToUserMode();

// Queue functions
void initProcQueue3(processQueue*, int);
void appendProcessToQueue3(processQueue *, procPtr3);
procPtr3 popFromQueue3(processQueue *);
procPtr3 peekAtHead3(processQueue *);
void removeChild3(processQueue*, procPtr3);


/* -------------------------- Globals ------------------------------------- */
int debug3 = 0;

int numSems;
semaphore SemTable[MAXSEMS];
procStruct3 ProcTable3[MAXPROC];

int
start2(char *arg)
{
    int pid;
    int status;
    /*
     * Check kernel mode here.
     */
    makeSureCurrentFunctionIsInKernelMode("start2");

    /*
     * Data structure initialization as needed...
     */

    // populate system call vec
    int i;
    for (i = 0; i < USLOSS_MAX_SYSCALLS; i++) {
        systemCallVec[i] = nullsys3;
    }
    systemCallVec[SYS_SPAWN] = spawn;
    systemCallVec[SYS_WAIT] = wait;
    systemCallVec[SYS_TERMINATE] = terminate;
    systemCallVec[SYS_SEMCREATE] = semCreate;
    systemCallVec[SYS_SEMP] = semP;
    systemCallVec[SYS_SEMV] = semV;
    systemCallVec[SYS_SEMFREE] = semFree;
    systemCallVec[SYS_GETTIMEOFDAY] = getTimeOfDay;
    systemCallVec[SYS_CPUTIME] = cpuTime;
    systemCallVec[SYS_GETPID] = getPID;

    // populate proc table
    for (i = 0; i < MAXPROC; i++) {
        emptyProc3(i);
    }

    // populate semaphore table
    for (i = 0; i < MAXSEMS; i++) {
        SemTable[i].id = -1;
        SemTable[i].value = -1;
        SemTable[i].startingValue = -1;
        SemTable[i].private_mBoxID = -1;
        SemTable[i].mutex_mBoxID = -1;
    }

    numSems = 0;

    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * Assumes kernel-mode versions of the system calls
     * with lower-case names.  I.e., Spawn is the user-mode function
     * called by the test cases; spawn is the kernel-mode function that
     * is called by the syscallHandler; spawnReal is the function that
     * contains the implementation and is called by spawn.
     *
     * Spawn() is in libuser.c.  It invokes USLOSS_Syscall()
     * The system call handler calls a function named spawn() -- note lower
     * case -- that extracts the arguments from the sysargs pointer, and
     * checks them for possible errors.  This function then calls spawnReal().
     *
     * Here, we only call spawnReal(), since we are already in kernel mode.
     *
     * spawnReal() will create the process by using a call to fork1 to
     * create a process executing the code in spawnLaunch().  spawnReal()
     * and spawnLaunch() then coordinate the completion of the phase 3
     * process table entries needed for the new process.  spawnReal() will
     * return to the original caller of Spawn, while spawnLaunch() will
     * begin executing the function passed to Spawn. spawnLaunch() will
     * need to switch to user-mode before allowing user code to execute.
     * spawnReal() will return to spawn(), which will put the return
     * values back into the sysargs pointer, switch to user-mode, and 
     * return to the user code that called Spawn.
     */
    if (debug3)
        USLOSS_Console("Spawning start3...\n");
    pid = spawnReal("start3", start3, NULL, USLOSS_MIN_STACK, 3);

    /* Call the waitReal version of your wait code here.
     * You call waitReal (rather than Wait) because start2 is running
     * in kernel (not user) mode.
     */
    pid = waitReal(&status);

    if (debug3)
        USLOSS_Console("Quitting start2...\n");

    quit(pid);
    return -1;
} /* start2 */


/* ------------------------------------------------------------------------
   Name - spawn
   Purpose - Extracts arguments and checks for correctness.
   Parameters - USLOSS_Sysargs containing parameters for fork1
   Returns - arg1: pid of forked process, arg4: success (0) or failure (-1)
   ----------------------------------------------------------------------- */
void spawn(USLOSS_Sysargs *args)
{
    makeSureCurrentFunctionIsInKernelMode("spawn");

    int (*func)(char *) = args->arg1;
    char *arg = args->arg2;
    int stack_size = (int) ((long)args->arg3);
    int priority = (int) ((long)args->arg4);
    char *name = (char *)(args->arg5);

    if (debug3)
        USLOSS_Console("spawn(): args are: name = %s, stack size = %d, priority = %d\n", name, stack_size, priority);

    int pid = spawnReal(name, func, arg, stack_size, priority);
    int status = 0;

    if (debug3)
        USLOSS_Console("spawn(): spawnd pid %d\n", pid);

    // terminate self if zapped
    if (isZapped())
        terminateReal(1);

    // switch to user mode
    switchToUserMode();

    // swtich back to kernel mode and put values for Spawn
    args->arg1 = (void *) ((long)pid);
    args->arg4 = (void *) ((long)status);
}

int spawnReal(char *name, int (*func)(char *), char *arg, int stack_size, int priority)
{
    makeSureCurrentFunctionIsInKernelMode("spawnReal");

    if (debug3)
        USLOSS_Console("spawnReal(): forking process %s... \n", name);

    // fork the process and get its pid
    int pid = fork1(name, spawnLaunch, arg, stack_size, priority);

    if (debug3)
        USLOSS_Console("spawnReal(): forked process name = %s, pid = %d\n", name, pid);

    // return -1 if fork failed
    if (pid < 0)
        return -1;

    // now we have the pid, we can get the child table entry
    procPtr3 child = &ProcTable3[pid % MAXPROC];
    appendProcessToQueue3(&ProcTable3[getpid() % MAXPROC].childrenQueue, child); // add to children queue

    // if spawnLaunch hasn't done it yet, set up proc table entry
    if (child->pid < 0) {
        if (debug3)
            USLOSS_Console("spawnReal(): initializing proc table entry for pid %d\n", pid);
        initProc(pid);
    }

    child->startFunc = func; // give proc its starting function
    child->parentPtr = &ProcTable3[getpid() % MAXPROC]; // set child's parent pointer

    // unblock the process so spawnLaunch can start it
    MboxCondSend(child->mboxID, 0, 0);

    return pid;
}


/* ------------------------------------------------------------------------
   Name - spawnLaunch
   Purpose - launches user mode processes and terminates it
   Parameters - not used
   Returns - 0
   ----------------------------------------------------------------------- */
int spawnLaunch(char *startArg) {
    makeSureCurrentFunctionIsInKernelMode("spawnLaunch");

    if (debug3)
        USLOSS_Console("spawnLaunch(): launched pid = %d\n", getpid());

    // terminate self if zapped
    if (isZapped())
        terminateReal(1);

    // get the proc info
    procPtr3 proc = &ProcTable3[getpid() % MAXPROC];

    // if spawnReal hasn't done it yet, set up proc table entry
    if (proc->pid < 0) {
        if (debug3)
            USLOSS_Console("spawnLaunch(): initializing proc table entry for pid %d\n", getpid());
        initProc(getpid());

        // block until spawnReal is done
        MboxReceive(proc->mboxID, 0, 0);
    }

    // switch to user mode
    switchToUserMode();

    if (debug3)
        USLOSS_Console("spawnLaunch(): starting process %d...\n", proc->pid);

    // call the function to start the process
    int status = proc->startFunc(startArg);

    if (debug3)
        USLOSS_Console("spawnLaunch(): terminating process %d with status %d\n", proc->pid, status);

    Terminate(status); // terminate the process if it hasn't terminated itself
    return 0;
}


/* ------------------------------------------------------------------------
   Name - wait
   Purpose - waits for a child process to terminate
   Parameters - arg1: pointer for pid, arg2: pointer for status
   Returns - arg1: pid, arg2: status, arg4: success (0) or failure (-1)
   ------------------------------------------------------------------------ */
void wait(USLOSS_Sysargs *args)
{
    makeSureCurrentFunctionIsInKernelMode("wait");

    int *status = args->arg2;
    int pid = waitReal(status);

    if (debug3) {
        USLOSS_Console("wait(): joined with child pid = %d, status = %d\n", pid, *status);
    }

    args->arg1 = (void *) ((long) pid);
    args->arg2 = (void *) ((long) *status);
    args->arg4 = (void *) ((long) 0);

    // terminate self if zapped
    if (isZapped())
        terminateReal(1);

    // switch back to user mode
    switchToUserMode();
}

int waitReal(int *status)
{
    makeSureCurrentFunctionIsInKernelMode("waitReal");

    if (debug3)
        USLOSS_Console("in waitReal\n");
    int pid = join(status);
    return pid;
}


/* ------------------------------------------------------------------------
   Name - terminate
   Purpose - terminates the invoking process and all of its children
   Parameters - arg1: termination code
   Returns - nothing
   ------------------------------------------------------------------------ */
void terminate(USLOSS_Sysargs *args)
{
    makeSureCurrentFunctionIsInKernelMode("terminate");

    int status = (int)((long)args->arg1);
    terminateReal(status);
    // switch back to user mode
    switchToUserMode();
}

void terminateReal(int status)
{
    makeSureCurrentFunctionIsInKernelMode("terminateReal");

    if (debug3)
        USLOSS_Console("terminateReal(): terminating pid %d, status = %d\n", getpid(), status);

    // zap all children
    procPtr3 proc = &ProcTable3[getpid() % MAXPROC];
    while (proc->childrenQueue.size > 0) {
        procPtr3 child = popFromQueue3(&proc->childrenQueue);
        zap(child->pid);
    }
    // remove self from parent's list of children
    removeChild3(&(proc->parentPtr->childrenQueue), proc);
    quit(status);
}


/* ------------------------------------------------------------------------
   Name - semCreate
   Purpose - create a new semaphore
   Parameters - USLOSS_Sysargs containing arguments
   Returns - arg1: semaphore handle, arg4: success (0) or failure (-1)
   ------------------------------------------------------------------------ */
void semCreate(USLOSS_Sysargs *args)
{
    makeSureCurrentFunctionIsInKernelMode("semCreate");

    int value = (long) args->arg1;

    // fails if value is negative or no sems avaliable
    if (value < 0 || numSems == MAXSEMS) {
        args->arg4 = (void*) (long) -1;
    }
    else {
        numSems++;
        int handle = semCreateReal(value);
        args->arg1 = (void*) (long) handle;
        args->arg4 = 0;
    }

    if (isZapped()) {
        terminateReal(0);
    }
    else {
        switchToUserMode();
    }
}

int semCreateReal(int value) {
    makeSureCurrentFunctionIsInKernelMode("semCreateReal");

    int i;
    int private_mBoxID = MboxCreate(value, 0);
    int mutex_mBoxID = MboxCreate(1, 0);

    MboxSend(mutex_mBoxID, NULL, 0);

    for (i = 0; i < MAXSEMS; i++) {
        if (SemTable[i].id == -1) {
            SemTable[i].id = i;
            SemTable[i].value = value;
            SemTable[i].startingValue = value;
            SemTable[i].private_mBoxID = private_mBoxID;
            SemTable[i].mutex_mBoxID = mutex_mBoxID;
            initProcQueue3(&SemTable[i].blockedProcesses, BLOCKED);
            break;
        }
    }

    int j;
    for (j = 0; j < value; j++) {
        MboxSend(private_mBoxID, NULL, 0);
    }

    MboxReceive(mutex_mBoxID, NULL, 0);

    return SemTable[i].id;
}


/* ------------------------------------------------------------------------
   Name - semP
   Purpose - 
   Parameters - USLOSS_Sysargs containing arguments
   Returns - nothing
   ------------------------------------------------------------------------ */
void semP(USLOSS_Sysargs *args)
{
    makeSureCurrentFunctionIsInKernelMode("semP");

    int handle = (long) args->arg1;

    if (handle < 0)
        args->arg4 = (void*) (long) -1;
    else {
        args->arg4 = 0;
        semPReal(handle);
    }

    if (isZapped()) {
        terminateReal(0);
    }
    else {
        switchToUserMode();
    }
}

void semPReal(int handle) {
    makeSureCurrentFunctionIsInKernelMode("semPReal");

    // get mutex on this semaphore
    MboxSend(SemTable[handle].mutex_mBoxID, NULL, 0);

    // block if value is 0
    if (SemTable[handle].value == 0) {
        appendProcessToQueue3(&SemTable[handle].blockedProcesses, &ProcTable3[getpid() % MAXPROC]);
        MboxReceive(SemTable[handle].mutex_mBoxID, NULL, 0);

        int result = MboxReceive(SemTable[handle].private_mBoxID, NULL, 0);

        // terminate if the semaphore freed while we were blocked
        if (SemTable[handle].id < 0)
            terminateReal(1);

        // get mutex again when we unblock
        MboxSend(SemTable[handle].mutex_mBoxID, NULL, 0);

        if (result < 0) {
            USLOSS_Console("semP(): bad receive");
        }
    }

    else {
        SemTable[handle].value -= 1 ;

        int result = MboxReceive(SemTable[handle].private_mBoxID, NULL, 0);
        if (result < 0) {
            USLOSS_Console("semP(): bad receive");
        }
    }


    MboxReceive(SemTable[handle].mutex_mBoxID, NULL, 0);
}


/* ------------------------------------------------------------------------
   Name - semV
   Purpose - 
   Parameters - USLOSS_Sysargs containing arguments
   Returns - nothing
   ------------------------------------------------------------------------ */
void semV(USLOSS_Sysargs *args)
{
    makeSureCurrentFunctionIsInKernelMode("semV");

    int handle = (long) args->arg1;

    if (handle < 0)
        args->arg4 = (void*) (long) -1;
    else
        args->arg4 = 0;

    semVReal(handle);

    if (isZapped()) {
        terminateReal(0);
    }
    else {
        switchToUserMode();
    }
}

void semVReal(int handle) {
    makeSureCurrentFunctionIsInKernelMode("semVReal");

    MboxSend(SemTable[handle].mutex_mBoxID, NULL, 0);

    // unblock blocked proc
    if (SemTable[handle].blockedProcesses.size > 0) {
        popFromQueue3(&SemTable[handle].blockedProcesses);
        MboxReceive(SemTable[handle].mutex_mBoxID, NULL, 0);
        MboxSend(SemTable[handle].private_mBoxID, NULL, 0);
        MboxSend(SemTable[handle].mutex_mBoxID, NULL, 0);
    }
    else {
        SemTable[handle].value += 1 ;
    }

    MboxReceive(SemTable[handle].mutex_mBoxID, NULL, 0);
}


/* ------------------------------------------------------------------------
   Name - semFree
   Purpose - 
   Parameters - USLOSS_Sysargs containing arguments
   Returns - nothing
   ------------------------------------------------------------------------ */
void semFree(USLOSS_Sysargs *args)
{
    makeSureCurrentFunctionIsInKernelMode("semFree");

    int handle = (long) args->arg1;

    if (handle < 0)
        args->arg4 = (void*) (long) -1;
    else {
        args->arg4 = 0;
        int value = semFreeReal(handle);
        args->arg4 = (void*) (long) value;
    }

    if (isZapped()) {
        terminateReal(0);
    }
    else {
        switchToUserMode();
    }
}

int semFreeReal(int handle) {
    makeSureCurrentFunctionIsInKernelMode("semFreeReal");

    int mutexID = SemTable[handle].mutex_mBoxID;
    MboxSend(mutexID, NULL, 0);

    int privID = SemTable[handle].private_mBoxID;

    SemTable[handle].id = -1;
    SemTable[handle].value = -1;
    SemTable[handle].startingValue = -1;
    SemTable[handle].private_mBoxID = -1;
    SemTable[handle].mutex_mBoxID = -1;
    numSems--;

    // terminate procs waiting on this semphore
    if (SemTable[handle].blockedProcesses.size > 0) {
        while (SemTable[handle].blockedProcesses.size > 0) {
            popFromQueue3(&SemTable[handle].blockedProcesses);
            int result = MboxSend(privID, NULL, 0);
            if (result < 0) {
                USLOSS_Console("semFreeReal(): send error");
            }
        }
        MboxReceive(mutexID, NULL, 0);
        return 1;
    }

    else {
        MboxReceive(mutexID, NULL, 0);
        return 0;
    }
}


/* ------------------------------------------------------------------------
   Name - getTimeOfDay
   Purpose - 
   Parameters - USLOSS_Sysargs containing arguments
   Returns - nothing
   ------------------------------------------------------------------------ */
void getTimeOfDay(USLOSS_Sysargs *args)
{
    makeSureCurrentFunctionIsInKernelMode("getTimeOfDay()");
    int status;
    USLOSS_DeviceInput(0, 0, &status);
    *((int *)(args->arg1)) = status;
}


/* ------------------------------------------------------------------------
   Name - cpuTime
   Purpose - 
   Parameters - USLOSS_Sysargs containing arguments
   Returns - nothing
   ------------------------------------------------------------------------ */
void cpuTime(USLOSS_Sysargs *args)
{
    makeSureCurrentFunctionIsInKernelMode("cpuTime");
    *((int *)(args->arg1)) = readtime();
}


/* ------------------------------------------------------------------------
   Name - getPID
   Purpose - 
   Parameters - USLOSS_Sysargs containing arguments
   Returns - nothing
   ------------------------------------------------------------------------ */
void getPID(USLOSS_Sysargs *args)
{
    makeSureCurrentFunctionIsInKernelMode("getPID");
    *((int *)(args->arg1)) = getpid();
}


/* an error method to handle invalid syscalls */
void nullsys3(USLOSS_Sysargs *args)
{
    USLOSS_Console("nullsys(): Invalid syscall %d. Terminating...\n", args->number);
    terminateReal(1);
}


/* initializes proc struct */
void initProc(int pid) {
    makeSureCurrentFunctionIsInKernelMode("initProc()");

    int i = pid % MAXPROC;

    ProcTable3[i].pid = pid;
    ProcTable3[i].mboxID = MboxCreate(0, 0);
    ProcTable3[i].startFunc = NULL;
    ProcTable3[i].nextProcPtr = NULL;
    initProcQueue3(&ProcTable3[i].childrenQueue, CHILDREN);
}


/* empties proc struct */
void emptyProc3(int pid) {
    makeSureCurrentFunctionIsInKernelMode("emptyProc()");

    int i = pid % MAXPROC;

    ProcTable3[i].pid = -1;
    ProcTable3[i].mboxID = -1;
    ProcTable3[i].startFunc = NULL;
    ProcTable3[i].nextProcPtr = NULL;
}


/* ------------------------------------------------------------------------
   Name -           inKernelMode
   Purpose -        Checks to see whether we are in kernel mode or not
   Parameters -     none
   Returns -
                    1/True if we are kernel mode
                    0/false if we are not in kernel mode
   Side Effects -   Some functions may not run if they are not in kernel mode
   ----------------------------------------------------------------------- */
int inKernelMode(){
    int kernelMode = 0;

    //This ands the 0x1 with the PSR from USloss, giving us
    //the current mode bit.
    //If this bit is 0, we are in user mode
    //If it is something else, then we are in kernel mode
    if((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) != 0)
        kernelMode = 1;

    return kernelMode;

}

/* ------------------------------------------------------------------------
   Name - makeSureCurrentFunctionIsInKernelMode
   Purpose - Checks if we are in kernel mode and prints an error messages
              and halts USLOSS if not.
              It should be called by every function in phase 1.
   Parameters - The name of the function calling it, for the error message.
   Side Effects - Prints and halts if we are not in kernel mode
   ------------------------------------------------------------------------ */
void makeSureCurrentFunctionIsInKernelMode(char *name)
{
    if(!inKernelMode()) {
        USLOSS_Console("%s: called while in user mode, by process %d. Halting...\n",
                       name, getpid());
        USLOSS_Halt(1);
    }
} /* makeSureCurrentFunctionIsInKernelMode */


/* ------------------------------------------------------------------------
   Name - setUserMode
   Purpose - switches to user mode
   Parameters - none
   Side Effects - none
   ------------------------------------------------------------------------ */
void switchToUserMode()
{
    USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE );
}

/* Initialize the given processQueue */
void initProcQueue3(processQueue* q, int type) {
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
    q->type = type;
}

/* Add the given procPtr3 to the back of the given queue. */
void appendProcessToQueue3(processQueue *q, procPtr3 p) {
    if (q->head == NULL && q->tail == NULL) {
        q->head = q->tail = p;
    } else {
        if (q->type == BLOCKED)
            q->tail->nextProcPtr = p;
        else if (q->type == CHILDREN)
            q->tail->nextSiblingPtr = p;
        q->tail = p;
    }
    q->size++;
}

/* Remove and return the head of the given queue. */
procPtr3 popFromQueue3(processQueue *q) {
    procPtr3 temp = q->head;
    if (q->head == NULL) {
        return NULL;
    }
    if (q->head == q->tail) {
        q->head = q->tail = NULL;
    }
    else {
        if (q->type == BLOCKED)
            q->head = q->head->nextProcPtr;
        else if (q->type == CHILDREN)
            q->head = q->head->nextSiblingPtr;
    }
    q->size--;
    return temp;
}

/* Remove the child process from the queue */
void removeChild3(processQueue* q, procPtr3 child) {
    if (q->head == NULL || q->type != CHILDREN)
        return;

    if (q->head == child) {
        popFromQueue3(q);
        return;
    }

    procPtr3 prev = q->head;
    procPtr3 p = q->head->nextSiblingPtr;

    while (p != NULL) {
        if (p == child) {
            if (p == q->tail)
                q->tail = prev;
            else
                prev->nextSiblingPtr = p->nextSiblingPtr->nextSiblingPtr;
            q->size--;
        }
        prev = p;
        p = p->nextSiblingPtr;
    }
}

/* Return the head of the given queue. */
procPtr3 peekAtHead3(processQueue *q) {
    if (q->head == NULL) {
        return NULL;
    }
    return q->head;
}

