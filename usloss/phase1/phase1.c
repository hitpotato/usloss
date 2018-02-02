/* ------------------------------------------------------------------------
   phase1.c

   University of Arizona
   Computer Science 452
   Spring 2018

   Rodrigo Silva Mendoza
   Long Chen

   ------------------------------------------------------------------------ */

#include "phase1.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (char *);
extern int start1 (char *);
void dispatcher(void);
void launch();
static void checkDeadlock();
void enableInterrupts(void);
void makeSureCurrentFunctionIsInKernelMode(char *);
void clockHandler(int dev, void *arg);
int readtime();
void initializeAndEmptyProcessSlot(int);
int block(int);
int debugEnabled();
int findAvailableProcSlot();
int inKernelMode();
void  disableInterrupts(void);

/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 0;

// the process table
procStruct ProcTable[MAXPROC];

// Process lists
processQueue ReadyList[SENTINELPRIORITY];


// The number of process table spots taken
int numProcs;

// current process ID
procPtr Current;

// Int used to store the returns from USLOSS_DEVICEINPUT calls
int status = 0;

// the next pid to be assigned
unsigned int nextPid = SENTINELPID;


/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
             Start up sentinel process and the test process.
   Parameters - none, called by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup()
{
    // Require kernel mode 
    makeSureCurrentFunctionIsInKernelMode("startup()");

    int result; // value returned by call to fork1()

    //--------------------------- Initialize process Table -------------//
    if (debugEnabled())
        USLOSS_Console("startup(): initializing process table, ProcTable[]\n");
    for (int i = 0; i < MAXPROC; i++) {
        initializeAndEmptyProcessSlot(i);
    }

    //--------------------------- Initialize ReadyList -------------//
    if (debugEnabled())
        USLOSS_Console("startup(): initializing the Ready list\n");

    for (int i = 0; i < SENTINELPRIORITY; i++) {
        initializeProcessQueue(&ReadyList[i], READYLIST);
    }

    numProcs = 0;                       // Set the number of processes to 0
    Current = &ProcTable[MAXPROC-1];    // Make sure current is initialized

    // Initialize the clock interrupt handler
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler;

    // startup a sentinel process
    if (debugEnabled())
        USLOSS_Console("startup(): calling fork1() for sentinel\n");
    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                    SENTINELPRIORITY);
    if (result < 0) {
        if (debugEnabled()) {
            USLOSS_Console("startup(): fork1 of sentinel returned error, ");
            USLOSS_Console("halting...\n");
        }
        USLOSS_Halt(1);
    }

    // start the test process
    if (debugEnabled())
        USLOSS_Console("startup(): calling fork1() for start1\n");
    result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
    if (result < 0) {
        USLOSS_Console("startup(): fork1 for start1 returned an error, ");
        USLOSS_Console("halting...\n");
        USLOSS_Halt(1);
    }

    USLOSS_Console("startup(): Should not see this message! ");
    USLOSS_Console("Returned from fork1 call that created start1\n");

    return;
} /* startup */

/* ------------------------------------------------------------------------
   Name -           launch
   Purpose -        Dummy function to enable interrupts and launch a given process
                    upon startup.
   Parameters -     none
   Returns -        nothing
   Side Effects -   enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
    // Require kernel mode
    makeSureCurrentFunctionIsInKernelMode("launch()");
    disableInterrupts();

    if (debugEnabled())
        USLOSS_Console("launch(): starting current process: %d\n\n", Current->pid);

    // Enable interrupts
    enableInterrupts();

    // Call the function passed to fork1, and capture its return value
    int result = Current->startFunc(Current->startArg);

    if (debugEnabled())
        USLOSS_Console("Process %d returned to launch\n", Current->pid);

    quit(result);
} /* launch */


/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the length of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*startFunc)(char *), char *arg,
          int stacksize, int priority)
{

    // Require kernel mode
    makeSureCurrentFunctionIsInKernelMode("fork1()");
    disableInterrupts();


    int procSlot = -1;

    // If there is not empty spot on the table, return with error
    if (numProcs >= MAXPROC) {
        if (debugEnabled())
            USLOSS_Console("fork1(): No empty slot on the process table.\n");
        return -1;
    }

    if (debugEnabled())
        USLOSS_Console("fork1(): creating process %s\n", name);

    // Return if startFunc is null
    if (startFunc == NULL) {
        if (debugEnabled())
            USLOSS_Console("fork1(): Start function is null.\n");
        return -1;
    }

    // Return if name is null
    if (name == NULL) {
        if (debugEnabled())
            USLOSS_Console("fork1(): Process name is null.\n");
        return -1;
    }

    // Return if stacksize is too small
    if (stacksize < USLOSS_MIN_STACK) { // found in usloss.h
        if (debugEnabled())
            USLOSS_Console("fork1(): Stack length too small.\n");
        return -2;
    }

    // Return if priority is too big or to small
    if ((priority > MINPRIORITY || priority < MAXPRIORITY)){
        // Ignore the case where sentinel has priority 6
        if( startFunc != sentinel) {
            if (debugEnabled())
                USLOSS_Console("fork1(): Priority is out of range.\n");
            return -1;
        }
    }

    // fill-in entry in process table */
    if ( strlen(name) >= (MAXNAME - 1) ) {
        if (debugEnabled())
            USLOSS_Console("fork1(): Process name is too long.  Halting...\n");
        USLOSS_Halt(1);
    }


    // If all the above checks were fine, find a slot where this process fits
    procSlot = findAvailableProcSlot();

    // Let the debugger know
    if (debugEnabled())
        USLOSS_Console("fork1(): Creating process pid %d in slot %d, slot status %d\n", nextPid, procSlot, ProcTable[procSlot].status);


    //---------------------------- Fill in values for process --------------------------//

    strcpy(ProcTable[procSlot].name, name);
    ProcTable[procSlot].startFunc = startFunc;
    ProcTable[procSlot].pid = nextPid++;
    ProcTable[procSlot].priority = priority;
    ProcTable[procSlot].status = READY;

    numProcs++;     // Increment the number of process

    // Error check and fill in the starting argument
    if ( arg == NULL )
        ProcTable[procSlot].startArg[0] = '\0';
    else if ( strlen(arg) >= (MAXARG - 1) ) {
        if (debugEnabled())
            USLOSS_Console("fork1(): argument too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    else
        strcpy(ProcTable[procSlot].startArg, arg);

    // Set the stack
    ProcTable[procSlot].stackSize = stacksize;
    ProcTable[procSlot].stack = (char*) malloc (stacksize);

    // Check to make sure the malloc call did not fail
    if (ProcTable[procSlot].stack == NULL) {
        if (debugEnabled())
            USLOSS_Console("fork1(): Malloc failed.  Halting...\n");
        USLOSS_Halt(1);
    }


    // Initialize context for this process, but use launch function pointer for
    // the initial value of the process's program counter (PC)
    USLOSS_ContextInit(&(ProcTable[procSlot].state),
                       ProcTable[procSlot].stack,
                       ProcTable[procSlot].stackSize,
                       NULL,
                       launch);

    // for future phase(s)
    p1_fork(ProcTable[procSlot].pid);

    // More stuff to do here...
    // Add this(fork1()) process to the list of children of the parent who called fork1()
    if (Current->pid > -1) {
        appendProcessToQueue(&Current->childrenQueue, &ProcTable[procSlot]);
        ProcTable[procSlot].parentPtr = Current; // set parent pointer
    }

    // Add the process to the correct priority list
    int priorityListNumber = priority - 1;
    appendProcessToQueue(&ReadyList[priorityListNumber], &ProcTable[procSlot]);


    // Call the dispatcher to decide which process runs next
    // Make sure that this is not the sentinel
    if (startFunc != sentinel) {
        dispatcher();
    }


    enableInterrupts();         // Re-enable interrupts

    return ProcTable[procSlot].pid;  // return child's pid
} /* fork1 */


/* ------------------------------------------------------------------------
   Name -           dispatcher
   Purpose -        dispatches ready processes.  The process with the highest
                    priority (the first on the ready list) is scheduled to
                    run.  The old process is swapped out and the new process
                    swapped in.
   Parameters -     none
   Returns -        nothing
   Side Effects -   the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void)
{
    // Require kernel mode and disable interrupts
    makeSureCurrentFunctionIsInKernelMode("dispatcher()");
    disableInterrupts();

    procPtr nextProcess = NULL;

    // if current is still running, remove it from ready list and put it back on the end
    if (Current->status == RUNNING) {
        Current->status = READY;
        int currentPriorityListSpt = Current->priority - 1;
        popFromQueue(&ReadyList[currentPriorityListSpt]);
        appendProcessToQueue(&ReadyList[currentPriorityListSpt], Current);
    }

    // Loop through the ques, and in the highest non empty one, set nextProcess
    //      to the head element of that queue
    for (int i = 0; i < SENTINELPRIORITY; i++) {
        if (ReadyList[i].length > 0) {
            nextProcess = peekAtHead(&ReadyList[i]);
            break;
        }
    }

    // Print message and return if the ready list is empty
    if (nextProcess == NULL) {
        if (debugEnabled())
            USLOSS_Console("dispatcher(): ready list is empty!\n");
        return;
    }

    if (debugEnabled())
        USLOSS_Console("dispatcher(): next process is %s\n", nextProcess->name);

    // Update the Current process pointer
    procPtr previousProcess = Current;
    Current = nextProcess;
    Current->status = RUNNING; // set status to RUNNING

    // Set time variables
    if (previousProcess != Current) {
        if (previousProcess->pid > -1)
            previousProcess->cpuTime += USLOSS_DeviceInput(0, 0, &status) - previousProcess->timeInitialized;
        Current->sliceTime = 0;
        Current->timeInitialized = USLOSS_DeviceInput(0, 0, &status);
    }

    // Switch, re-enable interrupts and call USLOSS_ContextSwitch
    p1_switch(previousProcess->pid, Current->pid);
    enableInterrupts();
    USLOSS_ContextSwitch(&previousProcess->state, &Current->state);
} /* dispatcher */


/* ------------------------------------------------------------------------
   Name -           join
   Purpose -        Wait for a child process (if one has been forked) to quit.  If
                    one has already quit, don't wait.
   Parameters -     a pointer to an int where the termination code of the
                    quitting process is to be stored.
   Returns -        the process id of the quitting child joined on.
                    -1 if the process was zapped in the join
                    -2 if the process has no children
   Side Effects -   If no child process has quit before join is called, the
                    parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *status)
{
    // Require kernel mode and disable interrupts
    makeSureCurrentFunctionIsInKernelMode("join()");
    disableInterrupts();

    if (debugEnabled())
        USLOSS_Console("join(): In join, pid = %d\n", Current->pid);

    // If the current process does not have any children, return -2
    if (Current->childrenQueue.length == 0 && Current->deadChildrenQueue.length == 0) {
        if (debugEnabled())
            USLOSS_Console("join(): No children\n");
        return -2;
    }

    // if current has children, but non are dead, block itself and wait
    if (Current->deadChildrenQueue.length == 0) {
        block(JOINBLOCKED);
        if (debugEnabled())
            USLOSS_Console("pid %d blocked at priority %d \n" , Current->pid, Current->priority - 1);
    }

    // get the first dead child
    procPtr child = popFromQueue(&Current->deadChildrenQueue);
    *status = child->quitStatus;
    int childPid = child->pid;


    // Blank out the childs slot in the process table
    initializeAndEmptyProcessSlot(childPid);


    if (Current->zapQueue.length != 0 ) {
        childPid = -1;
    }
    return childPid;
} /* join */


/* ------------------------------------------------------------------------
   Name -           quit
   Purpose -        Stops the child process and notifies the parent of the death by
                    putting child quit info on the parents child completion code
                    list.
   Parameters -     the code to return to the grieving parent
   Returns -        nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int status)
{
    // Require kernel mode and disable interrupts
    makeSureCurrentFunctionIsInKernelMode("quit()");
    disableInterrupts();

    if (debugEnabled())
        USLOSS_Console("quit(): quitting process pid = %d\n", Current->pid);

    // Make sure that the process has no active children
    procPtr childPtr = peekAtHead(&Current->childrenQueue);
    while (childPtr != NULL) {
        if (childPtr->status != QUIT) {
            USLOSS_Console("quit(): process %d, '%s', has active children. Halting...\n", Current->pid, Current->name);
            USLOSS_Halt(1);
        }
        childPtr = childPtr->nextSiblingPtr;
    }

    Current->status = QUIT;                              // change process status to QUIT
    Current->quitStatus = status;                       // store the given quit status
    popFromQueue(&ReadyList[Current->priority - 1]);    // remove self from ready list


    // If this process has a parent, we need to delete "ourselves" from the parents queue of children
    //  And after deletion, add oursleves to the DEAD children queue
    if (Current->parentPtr != NULL) {
        removeChildFromQueue(&Current->parentPtr->childrenQueue, Current);
        appendProcessToQueue(&Current->parentPtr->deadChildrenQueue, Current);

        // If the parent was blocked because it was waiting for "us" to die, unblock it
        //  then add it to the list of processes that are ready to run
        if (Current->parentPtr->status == JOINBLOCKED) {
            Current->parentPtr->status = READY;
            appendProcessToQueue(&ReadyList[Current->parentPtr->priority - 1], Current->parentPtr);
        }
    }

    // If this process had any dead children, loop through the dead children queue
    //      and remove them from the dead children queue as well as the process table
    while (Current->deadChildrenQueue.length > 0) {
        procPtr child = popFromQueue(&Current->deadChildrenQueue);
        initializeAndEmptyProcessSlot(child->pid);
    }

    // If this process was zapped by others, unblock them and add them back to the ready list
    //      for all processe that were zapped
    while (Current->zapQueue.length > 0) {
        procPtr processThatZappedCurrent = popFromQueue(&Current->zapQueue);
        processThatZappedCurrent->status = READY;
        appendProcessToQueue(&ReadyList[processThatZappedCurrent->priority - 1], processThatZappedCurrent);
    }

    // If current has no parent, remove it from the process table list
    if (Current->parentPtr == NULL)
        initializeAndEmptyProcessSlot(Current->pid);

    // Call the p1_quit function
    p1_quit(Current->pid);

    dispatcher(); // call dispatcher and let it decide what to run next
} /* quit */


/* ------------------------------------------------------------------------
   Name -       zap
   Purpose -
   Parameters -
   Returns -  -1: the calling process itself was zapped while in zap.
               0: the zapped process has called quit.
   Side Effects -
   ----------------------------------------------------------------------- */
int zap(int pid) {
    if (debugEnabled())
        USLOSS_Console("zap(): called\n");

    // Require kernel mode and disable interrupts
    makeSureCurrentFunctionIsInKernelMode("zap()");
    disableInterrupts();

    procPtr process = &ProcTable[pid % MAXPROC];

    if (Current->pid == pid) {
        USLOSS_Console("zap(): process %d tried to zap itself.  Halting...\n", pid);
        USLOSS_Halt(1);
    }


    if (process->status == EMPTY || process->pid != pid) {
        USLOSS_Console("zap(): process being zapped does not exist.  Halting...\n");
        USLOSS_Halt(1);
    }

    if (process->status == QUIT) {
        if (Current->zapQueue.length > 0)
            return -1;
        else
            return 0;
    }

    appendProcessToQueue(&process->zapQueue, Current);
    block(ZAPBLOCKED);

    if (Current->zapQueue.length > 0)
        return -1;


    return 0;
}


/* ------------------------------------------------------------------------
   Name -           block
   Purpose -        blocks the current process
   Parameters -     new status
   Returns -            -1: if process was zapped while blocked.
                        0: otherwise.
   Side Effects -   blocks/changes ready list/calls dispatcher
   ----------------------------------------------------------------------- */
int block(int newStatus) {
    if (debugEnabled())
        USLOSS_Console("block(): called\n");

    // Require kernel mode 
    makeSureCurrentFunctionIsInKernelMode("block()");
    disableInterrupts();

    Current->status = newStatus;
    popFromQueue(&ReadyList[(Current->priority - 1)]);
    dispatcher();

    if (Current->zapQueue.length > 0) {
        return -1;
    }

    return 0;
}


/* ------------------------------------------------------------------------
   Name -           blockMe(int newStatus)
   Purpose -        Blocks the calling process.
   Parameters -     pid of the process to unblock
   Returns -           -1: if the process was zapped while blocked
                        0: if otherwise
   Side Effects -   Calls block, which blocks this process
   ----------------------------------------------------------------------- */
int blockMe(int newStatus) {
    if (debugEnabled())
        USLOSS_Console("blockMe(): called\n");

    // Require kernel mode 
    makeSureCurrentFunctionIsInKernelMode("blockMe()");
    disableInterrupts();

    if (newStatus < 10) {
        USLOSS_Console("newStatus < 10 \n");
        USLOSS_Halt(1);
    }

    return block(newStatus);
}


/* ------------------------------------------------------------------------
   Name -           unblockProc
   Purpose -        unblocks the given process
   Parameters -     pid of the process to unblock
   Returns -           -2: if the indicated process was not blocked, does not exist,
                        is the current process, or is blocked on a status less than
                        or equal to 10. Thus, a process that is zap-blocked
                        or join-blocked cannot be unblocked with this function call.
                       -1:        if the calling process was zapped.
                        0:         otherwise
   Side Effects -   calls dispatcher
   ----------------------------------------------------------------------- */
int unblockProc(int pid) {
    // Require kernel mode 
    makeSureCurrentFunctionIsInKernelMode("unblockProc(): called\n");
    disableInterrupts();

    int i = pid % MAXPROC; // get process
    if (ProcTable[i].pid != pid || ProcTable[i].status <= 10) // check that it exists
        return -2;

    // unblock
    ProcTable[i].status = READY;
    appendProcessToQueue(&ReadyList[ProcTable[i].priority - 1], &ProcTable[i]);
    dispatcher();

    if (Current->zapQueue.length > 1) // return -1 if we were zapped
        return -1;
    return 0;
}


/* ------------------------------------------------------------------------
   Name -           sentinel
   Purpose -        The purpose of the sentinel routine is two-fold.  One
                    responsibility is to keep the system going when all other
                    processes are blocked.  The other is to detect and report
                    simple deadlock states.
   Parameters -     none
   Returns -        nothing
   Side Effects -  if system is in deadlock, print appropriate error
                   and halt.
   ----------------------------------------------------------------------- */
int sentinel (char *dummy)
{
    // Require kernel mode 
    makeSureCurrentFunctionIsInKernelMode("sentinel()");

    if (debugEnabled())
        USLOSS_Console("sentinel(): called\n");
    while (1)
    {
        checkDeadlock();
        USLOSS_WaitInt();
    }
} /* sentinel */


/* ------------------------------------------------------------------------
   Name -       checkDeadlock
   Purpose -    Checks to determine whether deadlock occured
   Parameters - None
   Returns -    Nothing
   Side Effects - Halts USLOSS if deadlock did occur
   ------------------------------------------------------------------------ */
static void checkDeadlock()
{
    // If there are more processes than just the sentinel running, print error and halt
    if (numProcs > 1) {
        USLOSS_Console("checkDeadlock(): numProc = %d. Only Sentinel should be left. Halting...\n", numProcs);
        USLOSS_Halt(1);
    }

    USLOSS_Console("All processes completed.\n");
    USLOSS_Halt(0);
} /* checkDeadlock */


/* ------------------------------------------------------------------------
   Name - readtime
   Purpose - Returns the CPU time (in milliseconds) stored in Current
   Parameters - none
   Returns - nothing
   Side Effects -
   ----------------------------------------------------------------------- */
int readtime()
{
    return Current->cpuTime/1000;
} /* readtime */

/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish()
{
    // Require kernel mode
    makeSureCurrentFunctionIsInKernelMode("finish()");

    if (debugEnabled())
        USLOSS_Console("in finish...\n");
} /* finish */


//------------------------------------------- Helper Functions-------------------------------------/


/* ------------------------------------------------------------------------
   Name -           isZapped
   Purpose -        Checks whether the current process is zapped or not
   Parameters -     None
   Returns -        Zapstatus
   Side Effects -   None
   ----------------------------------------------------------------------- */
int isZapped() {
    // If the process has a list of people that zapped it thats not empty
    //  it is, by definition, zapped
    return (Current->zapQueue.length > 0);
}


/* ------------------------------------------------------------------------
   Name -           getpid
   Purpose -        return the pid of the Current process
   Parameters -     None
   Returns -        current process pid
   Side Effects -   None
   ----------------------------------------------------------------------- */
int getpid() {
    return Current->pid;
}

/* ------------------------------------------------------------------------
   Name - readCurStartTime
   Purpose - returns the time (in microseconds) at which the currently
                executing process began its current time slice
   Parameters - none
   Returns - ^
   Side Effects - none
   ----------------------------------------------------------------------- */
int readCurStartTime() {
    return Current->timeInitialized/1000;
}


/* ------------------------------------------------------------------------
   Name - clockHandler
   Purpose - Checks if the current process has exceeded its time slice.
            Calls dispatcher() if necessary.
   Parameters -
   Returns - nothing
   Side Effects - may call dispatcher()
   ----------------------------------------------------------------------- */
void clockHandler(int dev, void *arg)
{
    if(debugEnabled())
        USLOSS_Console("clockHandler(): Inside clock handler\n");

    timeSlice();
} /* clockHandler */


/* ------------------------------------------------------------------------
   Name - timeSlice
   Purpose - This operation calls the dispatcher if the currently executing
            process has exceeded its time slice; otherwise, it simply returns.
   Parameters - none
   Returns - nothing
   Side Effects - may call dispatcher
   ----------------------------------------------------------------------- */
void timeSlice() {
    if (debugEnabled())
        USLOSS_Console("timeSlice(): called\n");

    // Require kernel mode and disable interrupts
    makeSureCurrentFunctionIsInKernelMode("timeSlice()");
    disableInterrupts();

    Current->sliceTime = USLOSS_DeviceInput(0, 0, &status) - Current->timeInitialized;

    if (Current->sliceTime > TIMESLICE) { // current has exceeded its timeslice
        if (debugEnabled())
            USLOSS_Console("timeSlice(): time slicing\n");
        Current->sliceTime = 0; // reset slice time
        dispatcher();

    }
    else
        enableInterrupts(); // re-enable interrupts
} /* timeSlice */


/* ------------------------------------------------------------------------
   Name - initializeAndEmptyProcessSlot
   Purpose - Cleans out the ProcTable entry of the given process.
   Parameters - pid of process to remove
   Returns - nothing
   Side Effects - changes ProcTable
   ----------------------------------------------------------------------- */
void initializeAndEmptyProcessSlot(int pid) {

    makeSureCurrentFunctionIsInKernelMode("initializeAndEmptyProcessSlot()");
    disableInterrupts();

    int i = pid % MAXPROC;

    ProcTable[i].status = EMPTY;
    ProcTable[i].pid = -1;              // Set the PID to -1 to show that it hasnt been assigned

    //-------------------- Set pointers to NULL ---------------------------//
    ProcTable[i].nextProcPtr = NULL;
    ProcTable[i].nextSiblingPtr = NULL;
    ProcTable[i].nextDeadSiblingPtr = NULL;
    ProcTable[i].startFunc = NULL;
    ProcTable[i].parentPtr = NULL;


    //------------------ Miscellaneous Values  ----------------------------//
    ProcTable[i].zapStatus = 0;
    ProcTable[i].timeInitialized = -1;
    ProcTable[i].cpuTime = -1;
    ProcTable[i].sliceTime = 0;
    ProcTable[i].name[0] = 0;
    ProcTable[i].priority = -1;
    ProcTable[i].stack = NULL;
    ProcTable[i].stackSize = -1;

    //------------------ Initialize the processQueus ---------------------//
    initializeProcessQueue(&ProcTable[i].childrenQueue, CHILDRENLIST);
    initializeProcessQueue(&ProcTable[i].deadChildrenQueue, DEADCHILDRENLIST);
    initializeProcessQueue(&ProcTable[i].zapQueue, ZAPLIST);

    // Decrease the number of processes.
    numProcs--;
}


/* ------------------------------------------------------------------------
   Name - dumpProcesses
   Purpose - Prints information about each process on the process table,
             Test 10 calls it
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void dumpProcesses()
{
    // Instead of coverting int into its corresponding string in each
    // iteration of the loop, create an array that maps the int to the string
    const char *statusString[7];
    statusString[EMPTY] = "EMPTY";
    statusString[READY] = "READY";
    statusString[RUNNING] = "RUNNING";
    statusString[JOINBLOCKED] = "JOIN_BLOCK";
    statusString[QUIT] = "QUIT";
    statusString[ZAPBLOCKED] = "ZAP_BLOCK";

    // The form of the Table looks as below
    // PID  Parent  Priority    Status  #Kids  CPUtime Name

    // Print out the Process Table Header
    USLOSS_Console("%-6s%-8s%-16s%-16s%-8s%-8s%s\n", "PID", "Parent",
           "Priority", "Status", "# Kids", "CPUtime", "Name");

    // Loop through  the process table
    for (int i = 0; i < MAXPROC; i++) {
        int p;
        char s[20];

        // Check if the process has parents
        if (ProcTable[i].parentPtr != NULL) {
            p = ProcTable[i].parentPtr->pid;
            if (ProcTable[i].status > 10)
                sprintf(s, "%d", ProcTable[i].status);
        }
        // If the process has no parents, the parendPID is -1
        else if(strcmp("sentinel", ProcTable[i].name) == 0){
            p = -2;
        }
        else if(strcmp("start1", ProcTable[i].name) == 0){
            p = -2;
        }
        else
            p = -1;
        if (ProcTable[i].status > 10)
            USLOSS_Console(" %-7d%-9d%-13d%-18s%-9d%-5d%s\n", ProcTable[i].pid, p,
                ProcTable[i].priority, s, ProcTable[i].childrenQueue.length, ProcTable[i].cpuTime,
                ProcTable[i].name);
        else
            USLOSS_Console(" %-7d%-9d%-13d%-18s%-9d%-5d%s\n", ProcTable[i].pid, p,
                ProcTable[i].priority, statusString[ProcTable[i].status],
                ProcTable[i].childrenQueue.length, ProcTable[i].cpuTime, ProcTable[i].name);
    }
}


/* ------------------------------------------------------------------------
   Name -           findAvailableProcSlot
   Purpose -        Finds the next available processTable slot
   Parameters -     none
   Returns -        an int that represents a slot on the table
   Side Effects -   The table slot at that int will be subsequently filled
   ----------------------------------------------------------------------- */
int findAvailableProcSlot(){
    // find an empty slot in the process table
    int procSlot = -1;
    procSlot = nextPid % MAXPROC;
    while (ProcTable[procSlot].status != EMPTY) {
        nextPid++;
        procSlot = nextPid % MAXPROC;
    }

    return procSlot;

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
                       name, Current->pid);
        USLOSS_Halt(1);
    }
} /* makeSureCurrentFunctionIsInKernelMode */

/* ------------------------------------------------------------------------
   Name - debugEnabled
   Purpose - Checks to see if the debug flasgs are turned on
   Parameters - none
   Returns - nothing
   Side Effects - When test cases are run, debug info is printed
   ----------------------------------------------------------------------- */
int debugEnabled(){
    int debug = 0;
    if(DEBUG && debugflag)
        debug = 1;
    return debug;
}

/* ------------------------------------------------------------------------
   Name -       disableInterrupts
   Purpose -    Disables the OS device interrupts
   Parameters - None
   Returns -    Nothing
   Side Effects - Disables interrupts, modifies psr bits
   ------------------------------------------------------------------------ */
void disableInterrupts()
{
    // Check to make sure we are in kernel mode first
    if(!inKernelMode()) {
        USLOSS_Console("Kernel Error: Not in kernel mode, cannot disable interrupts\n");
        USLOSS_Halt(1);
    }

    else
        USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_INT );
} /* disableInterrupts */


/* ------------------------------------------------------------------------
   Name -       enableInterrupts
   Purpose -    Enables the OS device interrupts
   Parameters - None
   Returns -    Nothing
   Side Effects - Enables interrupts, modifies psr bits
   ------------------------------------------------------------------------ */
void enableInterrupts()
{
    // turn the interrupts ON iff we are in kernel mode
    if(!inKernelMode()) {
        //not in kernel mode
        USLOSS_Console("Kernel Error: Not in kernel mode, cannot disable interrupts\n ");
        USLOSS_Halt(1);
    }
    else
        USLOSS_PsrSet( USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT );
} /* enableInterrupts */
