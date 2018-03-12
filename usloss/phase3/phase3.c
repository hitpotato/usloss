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

// --------------- Function Prototypes ---------------------//
void makeSureCurrentFunctionIsInKernelMode(char *name);
void switchToUserMode();

// Required phase3 functions
void nullsys3(USLOSS_Sysargs *args);
void spawn(USLOSS_Sysargs *args);
void wait(USLOSS_Sysargs *args);
void terminate(USLOSS_Sysargs *args);
void semCreate(USLOSS_Sysargs *args);
void semP(USLOSS_Sysargs *args);
void semV(USLOSS_Sysargs *args);
void semFree(USLOSS_Sysargs *args);
void getTimeOfDay(USLOSS_Sysargs *args);
void cpuTime(USLOSS_Sysargs *args);
void getPID(USLOSS_Sysargs *args);

// The real functions
int spawnReal(char *, int(*)(char *), char *, int, int);
int waitReal(int *);
void terminateReal(int);

int spawnLaunch(char *);

void initializeAndEmptyProcessSlot(int);
void initializeAndEmptySemaphoreSlot(int);

void appendProcessToQueue3(processQueue* queue, procPtr3 process);
procPtr3 peekAtHead3(processQueue* queue);
void removeChild3(processQueue* queue, procPtr3 child);
procPtr3 popFromQueue3(processQueue* queue);
void initializeQueue3(processQueue* queue, int type);


// ---------------- Globals -------------------//
int debug3 = 0;

semaphore SemTable[MAXSEMS];
int numOfSems;
procStruct3 ProcTable3[MAXPROC];


int start2(char *arg)
{
    int pid;
    int status;

    // Check for kernel mode
    makeSureCurrentFunctionIsInKernelMode("start2");

    // ------------- Initialize Data Structures ----------------//
    for (int i = 0; i < USLOSS_MAX_SYSCALLS; i++){
        systemCallVec[i] = nullsys3;     // Assign nullsys3 to every elem of systemCallVec
    }
    systemCallVec[SYS_SPAWN]        = spawn;
    systemCallVec[SYS_WAIT]         = wait;
    systemCallVec[SYS_TERMINATE]    = terminate;
    systemCallVec[SYS_TERMINATE]    = semCreate;
    systemCallVec[SYS_SEMP]         = semP;
    systemCallVec[SYS_SEMV]         = semV;
    systemCallVec[SYS_SEMFREE]      = semFree;
    systemCallVec[SYS_GETTIMEOFDAY] = getTimeOfDay;
    systemCallVec[SYS_CPUTIME]      = cpuTime;
    systemCallVec[SYS_GETPID]       = getPID;

    for(int i = 0; i < MAXPROC; i++){
        initializeAndEmptyProcessSlot(i);
    }

    for(int i = 0; i < MAXSEMS; i++){
        initializeAndEmptySemaphoreSlot(i);
    }

    numOfSems = 0;


    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * Assumes kernel-mode versions of the system calls
     * with lower-case names.  I.e., Spawn is the user-mode function
     * called by the test cases; spawn is the kernel-mode function that
     * is called by the syscallHandler (via the systemCallVec array);s
     * spawnReal is the function that contains the implementation and is
     * called by spawn.
     *
     * Spawn() is in libuser.c.  It invokes USLOSS_Syscall()
     * The system call handler calls a function named spawn() -- note lower
     * case -- that extracts the arguments from the sysargs pointer, and
     * checks them for possible errors.  This function then calls spawnReal().
     *
     * Here, start2 calls spawnReal(), since start2 is in kernel mode.
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
    if(debug3)
        USLOSS_Console("Spawning start3...\n");

    pid = spawnReal("start3", start3, NULL, USLOSS_MIN_STACK, 3);

    /* Call the waitReal version of your wait code here.
     * You call waitReal (rather than Wait) because start2 is running
     * in kernel (not user) mode.
     */
    pid = waitReal(&status);

    if(debug3)
        USLOSS_Console("Quitting start2...\n");

    quit(pid);      // Quit with the pid
    return -1;

} /* start2 */

void spawn(USLOSS_Sysargs *args){
    makeSureCurrentFunctionIsInKernelMode("spawn");

    int (*func)(char *)     = args->arg1;
    char *arg               = args->arg2;
    int stack_size          = (int) ((long)args->arg3);
    int priority            = (int) ((long)args->arg4);
    char *name              = (char *) (args->arg5);

    if(debug3)
        USLOSS_Console("spawn(): args are: name = %s, stack_size = %d, priority = "
                               "%d\n", name, stack_size, priority);

    int pid = spawnReal(name, func, arg, stack_size, priority);
    int status = 0;

    if(debug3)
        USLOSS_Console("spawn(): spawned pid %d\n, pid");

    if(isZapped())
        terminateReal(1);

    switchToUserMode();

    args->arg1 = (void *) ((long) pid);
    args->arg4 = (void *) ((long) status);
}

int spawnReal(char *name, int (*func)(char *), char *arg, int stack_size, int priority){
    makeSureCurrentFunctionIsInKernelMode("spawnReal");

    if(debug3)
        USLOSS_Console("spawnReal(): forking process %s... \n", name);

    // Fork the process then get its pid
    int pid = fork1(name, spawnLaunch, arg, stack_size, priority);

    if(debug3)
        USLOSS_Console("spawnReal(): forked process name = %s, pid = %d\n", name, pid);

    if(pid < 0)             // If fork failed
        return -1;

    procPtr3 child = &ProcTable3[pid % MAXPROC];
    appendProcessToQueue3(&ProcTable3[getpid() % MAXPROC].childrenQueue, child);    // Add to the children queue

    // If spawnLaunch has not yet set up the process table entry
    if(child->pid < 0){
        if (debug3)
            USLOSS_Console("spawnReal(): initializing proc table entry for pid %d\n", pid);
        initializeAndEmptyProcessSlot(pid);
    }

    child->startFunc = func;
    child->parentPtr = &ProcTable3[getpid() % MAXPROC];

    MboxCondSend(child->mboxID, 0, 0);

    return pid;
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
   Name - initializeAndEmptyProcessSlot
   Purpose - Cleans out the ProcTable entry of the given process.
   Parameters - pid of process to remove
   Returns - nothing
   Side Effects - changes ProcTable
   ----------------------------------------------------------------------- */
void initializeAndEmptyProcessSlot(int pid) {

    makeSureCurrentFunctionIsInKernelMode("initializeAndEmptyProcessSlot()");

    int i = pid % MAXPROC;

    ProcTable3[i].pid = -1;
    ProcTable3[i].mboxID = -1;
    ProcTable3[i].startFunc = NULL;
    ProcTable3[i].nextProcPtr = NULL;
}

void initializeAndEmptySemaphoreSlot(int i) {
    makeSureCurrentFunctionIsInKernelMode("initializeAndEmptyProcessSlot()");

    SemTable[i].id = -1;
    SemTable[i].value = -1;
    SemTable[i].startingValue = -1;
    SemTable[i].private_mBoxID = -1;
    SemTable[i].mutex_mBoxID = -1;
}

void switchToUserMode(){
    USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE );
}

/* ------------------------------------------------------------------------
   Name -           initializeQueue
   Purpose -        Just initialize a ready list
   Parameters -
                    processQueue* processQueue:
                        Pointer to processQueue to be initialized
                    queueType
                        An int specifying what the processQueue is to be used for
   Returns -        Nothing
   Side Effects -   The passed processQueue is now no longer NULL
   ------------------------------------------------------------------------ */
void initializeQueue3(processQueue* queue, int type){
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->type = type;
}

/* ------------------------------------------------------------------------
   Name -           appendProcessToQueue
   Purpose -        Adds a process to the end of a processQueue
   Parameters -
                    processQueue* processQueue:
                        Pointer to processQueue to be added to
                    procPtr process
                        Pointer to the process that will be added
   Returns -        Nothing
   Side Effects -   The length of the processQueue increases by 1.
   ------------------------------------------------------------------------ */
void appendProcessToQueue3(processQueue* queue, procPtr3 process){
    if (queue->head == NULL && queue->tail == NULL){
        queue->head = process;
        queue->tail = process;
    }
    else {
        if (queue->type == BLOCKED)
            queue->tail->nextProcPtr = process;
        else if (queue->type == CHILDREN)
            queue->tail->nextSiblingPtr = process;
        queue->tail = process;
    }
    queue->size++;
}

/* ------------------------------------------------------------------------
   Name -           popFromQueue
   Purpose -        Remove and return the first element from the processQueue
   Parameters -
                    processQueue *processQueue
                        A pointer to the processQueue who we want to modify
   Returns -
                    procPtr:
                        Pointer to the element that we want
   Side Effects -   Length of processQueue is decreased by 1
   ------------------------------------------------------------------------ */
procPtr3 popFromQueue3(processQueue* queue) {
    procPtr3 temp = queue->head;

    if (queue->head == NULL)
        return NULL;
    if (queue->head == queue->tail)
        queue->head = queue->tail = NULL;
    else {
        if (queue->type == BLOCKED)
            queue->head = queue->head->nextProcPtr;
        else if (queue->type == CHILDREN)
            queue->head = queue->head->nextSiblingPtr;
    }
    queue->size--;
    return temp;
}

void removeChild3(processQueue* queue, procPtr3 child) {
    if (queue->head == NULL || queue->type != CHILDREN)
        return;

    // If the child we want to remove is the head, just pop
    if (queue->head == child) {
        popFromQueue3(queue);
        return;
    }

    procPtr3 prev = queue->head;
    procPtr3 p = queue->head->nextSiblingPtr;

    while (p != NULL) {
        if (p == child) {
            if (p == queue->tail)
                queue->tail = prev;
            else
                prev->nextSiblingPtr = p->nextSiblingPtr->nextSiblingPtr;
            queue->size--;
        }
        prev = p;
        p = p->nextSiblingPtr;
    }
}

procPtr3 peekAtHead3(processQueue* queue){
    if (queue->head == NULL)
        return NULL;
    return queue->head;
}

