/* ------------------------------------------------------------------------
   phase1.c

   University of Arizona
   Computer Science 452
   Spring 2018

   Rodrigo Silva Mendoza
   Long Chen

   ------------------------------------------------------------------------ */

#include "kernel.h"
#include <stdlib.h>
#include <string.h>

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (char *);
void dispatcher(void);
void launch();
static void checkDeadlock();

//Our functions
void initializeProcessTableEntry(int entryNumber);
int inKernelMode();
int debugEnabled();
void disableInterrupts();
void enableInterrupts();
void haltAndPrintKernelError();
int findAvailableProcSlot();
void insertInReadyList(procPtr process);
int blockCurrentProcess(int updatedStatus);
void removeFromProcessTable(int pid);
int countNumberOfProcessRunningAndActive();

//Interrupt Handlers
void clockHandler(int dev, void *arg);

/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 1;

// the process table
procStruct ProcTable[MAXPROC];

// Process lists
static processQueue ReadyList[6];        // Create a ReadyList for each priority

// current process ID
// It should always point to the entry in the process table of
// the currently executing process.
procPtr Current = NULL;


// the next pid to be assigned
unsigned int nextPid = SENTINELPID;


// Some ints for the clock

/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
             Start up sentinel process and the test process.
   Parameters - argc and argv passed in by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup(int argc, char *argv[])
{
    int result;             /* value returned by call to fork1() */

    /* initialize the process table */
    if (debugEnabled())
        USLOSS_Console("startup(): initializing process table, ProcTable[]\n");
    for(int i = 0; i < MAXPROC; i++){
        initializeProcessTableEntry(i); 
    }

    // Initialize ReadyList
    // For each element of ReadyList, turn it into a queue of type READYLISt
    if (debugEnabled())
        USLOSS_Console("startup(): initializing the Ready list\n");
    for(int i = 0; i < 6; i++){
        initializeProcessQueue(&ReadyList[i], READYLIST);
    }

    // Initialize the clock interrupt handler
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler;

    // startup a sentinel process
    if (debugEnabled())
        USLOSS_Console("startup(): calling fork1() for sentinel\n");
    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK, SENTINELPRIORITY);
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
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish(int argc, char *argv[])
{
    if(!inKernelMode())
        haltAndPrintKernelError();

    if (debugEnabled())
        USLOSS_Console("in finish...\n");
} /* finish */

/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, ProcTable is changed, Current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*startFunc)(char *), char *arg,
          int stacksize, int priority)
{

    if (debugEnabled())
        USLOSS_Console("fork1(): creating process %s\n", name);

    /*
     * Ask if we are in kernal mode
     * If we are not, halt USLOSS
     */
    if(!inKernelMode()){
        haltAndPrintKernelError();
    }

    //Make sure that interrupts are disabled during the fork process
    disableInterrupts();

    //Error check the priority
    // MAXPRIORITY = 1
    // MINPRIORITY = 5
    if(priority < MAXPRIORITY || priority > MINPRIORITY)
        //Make sure that that we are not forking sentinenl,
        //Which has a priority of 6
        if(strcmp("sentinel", name) != 0)
            return -1;

    // Return if stack size is too small
    if (stacksize < USLOSS_MIN_STACK) {
        if(debugEnabled())
            USLOSS_Console("Stack size passed to fork1() too small\n");
        return -1;
    }

    // Is there room in the process table? What is the next PID?
    int procSlot = findAvailableProcSlot();

    //If procSlot is -1, there was no available slot
    if(procSlot == -1){
        if(debugEnabled())
            USLOSS_Console("ProcTable is full!\n");
        return -1;
    }

    // fill-in entry in process table */
    if ( strlen(name) >= (MAXNAME - 1) ) {
        USLOSS_Console("fork1(): Process name is too long.  Halting...\n");
        USLOSS_Halt(1);
    }

    if(debugEnabled())
        USLOSS_Console("fork1(): Creating process with pid: %d in slot: %d with priority"
                               ": %d\n", nextPid, procSlot, priority);

    //Set proc name and startFunct
    strcpy(ProcTable[procSlot].name, name);
    ProcTable[procSlot].startFunc = startFunc;

    if(debugEnabled())
        USLOSS_Console("fork1(): Newly created process name is: %s\n", ProcTable[procSlot].name);

    //Set process starting argument
    if ( arg == NULL )
        ProcTable[procSlot].startArg[0] = '\0';
    else if ( strlen(arg) >= (MAXARG - 1) ) {
        USLOSS_Console("fork1(): argument too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    else
        strcpy(ProcTable[procSlot].startArg, arg);

    //Set the process stack and stacksize
    ProcTable[procSlot].stackSize = stacksize;
    ProcTable[procSlot].stack = (char*) malloc (stacksize);

    //Shouln't happen, but make sure that malloc is successful
    if(ProcTable[procSlot].stack == NULL){
        if(debugEnabled()) {
            USLOSS_Console("fork1(): Malloc failed when allocating stack size. Halting....\n");
        }
        USLOSS_Halt(1);
    }

    // Set the status, the pid and the priority
    ProcTable[procSlot].status = READY;
    ProcTable[procSlot].pid = (short) nextPid;
    ProcTable[procSlot].priority = priority;


    //If this is either sentinenel or start1, there is no parent
    if(strcmp("sentinel", name) == 0 || strcmp("start1", name) == 0)
        ProcTable[procSlot].parentPID = NOPARENTPROCESS;

    //Otherwise, need to find the parent ID
    else{
        // Set the Parent Id to the current Processes ID
        ProcTable[procSlot].parentPID = Current->pid;
        // Add this process to the Currents queue of children
        appendProcessToQueue(&Current->childrenQueue, &ProcTable[procSlot]);
        // Set the parentPtr to the Current process
        ProcTable[procSlot].parentPtr = Current;
    }

    if(debugEnabled())
        USLOSS_Console("fork1(): %s has a parentPID of %d\n", ProcTable[procSlot].name, ProcTable[procSlot].parentPID);


    //If the parent is not null, increase the number of children it has
    if(Current != NULL){
        Current->numberOfChildren++;
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

    if(debugEnabled())
        USLOSS_Console("fork1(): Just called p1_fork\n");

    // More stuff to do here...
    //Add the process to the ready table
    //
    appendProcessToQueue(&ReadyList[priority-1], &ProcTable[procSlot]);

    if(debugEnabled())
        USLOSS_Console("fork1(): Appended new process to the list\n");

    //If the process is not the sentinel, dispatch it
    if(strcmp("sentinel", name) != 0){
        dispatcher();
    }



    if(debugEnabled())
        USLOSS_Console("fork1(): Finished called the dispatcher for non-sentinel processes\n");

    //Re-enable interrupts
    enableInterrupts();


    if(debugEnabled())
        USLOSS_Console("fork1(): Re-enabled interrupts\n");

    return ProcTable[procSlot].pid;  // Return the PID of the newly slotted process
} /* fork1 */

/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
    int result;

    if (debugEnabled())
        USLOSS_Console("launch(): started\n");

    // Enable interrupts

    // Call the function passed to fork1, and capture its return value
    result = Current->startFunc(Current->startArg);

    if (debugEnabled())
        USLOSS_Console("Process %d returned to launch\n", Current->pid);

    quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If 
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the 
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
             -1 if the process was zapped in the join
             -2 if the process has no children
   Side Effects - If no child process has quit before join is called, the 
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *status)
{
    //Check to make sure kernel mode is enabled
    if(!inKernelMode())
        haltAndPrintKernelError();

    //Halt interrupts
    disableInterrupts();

    if(debugEnabled())
        USLOSS_Console("join(): In join, current has a pid of: %d\n", Current->pid);

    //Check if the process has any children
    if(Current->childrenQueue.length == 0 && Current->deadChildrenQueue.length == 0){
        if(debugEnabled())
            USLOSS_Console("join(): The process has no children\n");
        return -2;
    }

    //Make sure the process has not been zapped
    if(isZapped())
        return -1;

    // If current has no children that are dead, block and wait
    if(Current->deadChildrenQueue.length == 0){
        blockCurrentProcess(BLOCKEDBYJOIN);
    }

    // Get the first dead child from the dead children queue
    procPtr firstChild = popFromQueue(&Current->deadChildrenQueue);
    *status = firstChild->quitReturnValue;
    int pidOfChild = firstChild->pid;
    initializeProcessTableEntry(pidOfChild);        // The initialization also wipes all existing info

    //If the process has been zapped during this
    if(Current->zappedProcessesQueue.length > 0){
        return -1;
    }

    return pidOfChild;
} /* join */


/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int status)
{

    if(!inKernelMode())
        haltAndPrintKernelError();

    if(debugEnabled())
        USLOSS_Console("Quiting a process with pid: %d\n", Current->pid);

    int priorityQueueSlot = Current->priority - 1;

//    if(Current->childrenQueue.length > 0){
//        USLOSS_Console("Quit was called when processing with children.\n");
//        USLOSS_Halt(1);
//    }

    // Its possible the Children Queue was not properly cleared out
    //  so, loop through the table, to find any processes that have a status
    //      that isnt QUIT. If we find something, there is an active child
    procPtr childOfCurrent = peekAtHead(&Current->childrenQueue);
    while (childOfCurrent != NULL){
        if(childOfCurrent->status != QUIT){
            USLOSS_Console("Process with pid: %d has active children. Halting.\n", Current->pid);
            USLOSS_Halt(1);
        }

        childOfCurrent = childOfCurrent->nextSiblingPtr;        // Iterate to the next sibling
    }

    Current->status     = QUIT;
    Current->quitStatus = status;
    popFromQueue(&ReadyList[priorityQueueSlot]);

    // If this process is a child of another, need to remove from parents Children queue
    if(Current->parentPtr != NULL){
        removeChildFromQueue(&Current->parentPtr->childrenQueue, Current);      // Remove self/Current from parent
        appendProcessToQueue(&Current->parentPtr->deadChildrenQueue, Current);  // Append self to Dead Children queue

        // If the Parent is blocked because it is waiting for us/Child to quit, go ahead and unblock it
        // Also add the parent back to the Ready List
        if(Current->parentPtr->status == BLOCKEDBYJOIN){
            Current->parentPtr->status = READY;
            appendProcessToQueue(&ReadyList[Current->parentPtr->priority - 1], Current->parentPtr);
        }
    }

    // Remove dead children current has from process table
    while (Current->deadChildrenQueue.length > 0){
        procPtr child = popFromQueue(&Current->deadChildrenQueue);
        initializeProcessTableEntry(child->pid);
    }

    // Unblock all processes that zap'd this one
    // Then set them to ready, and add them back to priority list
    while (Current->zappedProcessesQueue.length > 0){
        procPtr zap = popFromQueue(&Current->zappedProcessesQueue);
        zap->status = READY;
        appendProcessToQueue(&ReadyList[zap->priority - 1], zap);
    }

    // If current has no parent, then delete it
    if(Current->parentPtr == NULL){
        initializeProcessTableEntry(Current->pid);
    }

    p1_quit(Current->pid);

    dispatcher(); // Run next process
} /* quit */


/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void)
{
    procPtr nextProcess = NULL;

    // Make sure we are in kernel mode
    if(!inKernelMode())
        haltAndPrintKernelError();

    // Disable all interrupts
    disableInterrupts();


    if(debugEnabled())
        USLOSS_Console("dispatcher(): In dispatcher(). Checked for kernel mode and disabled interrupts\n");

    if(Current != NULL){
        USLOSS_Console("dispatcher(): Current is not equal to null\n");
    }

    // Check to see if Current is currently running
    if(Current != NULL && Current->status == RUNNING){
        if(debugEnabled())
            USLOSS_Console("dispatcher(): Current->status is RUNNING\n");
        Current->status = READY;
        int pos = Current->priority - 1;
        appendProcessToQueue(&ReadyList[pos],popFromQueue(&ReadyList[pos])); // Removes from front. Places at back.
    }


    // Find the highest priority process
    for(int i = 0; i < 6; i++ ){
        if(ReadyList[i].length > 0){
            if(debugEnabled())
                USLOSS_Console("dispatcher(): ReadyList of priority: %d has the next process\n", i+1);
            nextProcess = peekAtHead(&ReadyList[i]);
            break;
        }
    }

    if(debugEnabled())
        USLOSS_Console("dispatcher(): Found the next process with highest priority. Name: %s\n", nextProcess->name);

    if(nextProcess == NULL){
        if(debugEnabled())
            USLOSS_Console("Ready list(s) are empty in dispatcher!\n");
        return;
    }

    procPtr previousProcess = Current;
    Current = nextProcess;
    Current->status = RUNNING;

    int deviceInputStatus = 0;
    if(previousProcess != Current){
        // If the old Process had real contents ?
        if(previousProcess->pid != -1)
            previousProcess->cpuTime += USLOSS_DeviceInput(0, 0, &deviceInputStatus)- previousProcess->timeInitialized;
        Current->totalSliceTime = 0;
        Current->timeInitialized = USLOSS_DeviceInput(0, 0, &deviceInputStatus);
    }

    p1_switch(Current->pid, nextProcess->pid);
    enableInterrupts();
    USLOSS_ContextSwitch(&previousProcess->state, &Current->state);
} /* dispatcher */


/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
             processes are blocked.  The other is to detect and report
             simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
                   and halt.
   ----------------------------------------------------------------------- */
int sentinel (char *dummy)
{
    if (debugEnabled())
        USLOSS_Console("sentinel(): called\n");
    while (1)
    {
        checkDeadlock();
        USLOSS_WaitInt();
    }
} /* sentinel */


/* check to determine if deadlock has occurred... */
static void checkDeadlock()
{

    int activeProcesses = countNumberOfProcessRunningAndActive();
    if(activeProcesses > 1){
        USLOSS_Console("There are more %d processes running. Only sentinel should be left\n", activeProcesses);
        USLOSS_Halt(1);
    }

    USLOSS_Console("All processes completed!\n");
    USLOSS_Halt(0);
} /* checkDeadlock */



//######################################################################################################################
//######################################################################################################################
//######################################################################################################################

/*
 * Disables the interrupts.
 */
void disableInterrupts()
{
    // turn the interrupts OFF iff we are in kernel mode
    // if not in kernel mode, print an error message and
    // halt USLOSS
    if(!inKernelMode()){
        //Display an error message to console
        USLOSS_Console("Kernel Error: Not in kernel mode, interrupts cannot be disabled\n");
        //Halt process with an non-zero argument, this dumps a core file
        USLOSS_Halt(1);
    }
    else {
        //Set the current enable bit to 0, which disables interrupts
        USLOSS_PsrSet(USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_INT);
    }

} /* disableInterrupts */

void enableInterrupts(){

    //We can only turn interrupts on if we are in kernal mode, so do that first
    if(!inKernelMode()){
        //Display an error message to console
        USLOSS_Console("Kernel Error: Not in kernel mode, interrupts cannot be enabled\n");
        //Halt process with an non-zero argument, this dumps a core file
        USLOSS_Halt(1);
    }
    else{
        //This sets the current enable interrupt bit to 1, enabling interrupts
        USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    }
}

/* ------------------------------------------------------------------------
 * Name - initializeProcessTableEntry 
 * Purpose - The singular purpose of this function is to 
 *              initialize every variable contained within 
 *              the procStruct that acts as the Process Table
 *              for each element of the process table. 
 * Parameters - The index of the entry to be initialized 
 * Returns - nothing 
 * Side effects - If called on an existing indice, 
 *                  it will erease all data. 
   ----------------------------------------------------------------------- */
void initializeProcessTableEntry(int entryNumber){

    int i = entryNumber % MAXPROC;

    // Set the PID to -1 to show it hasnt been assigned
    // Set status to empty
    ProcTable[i].pid                = -1;
    ProcTable[i].status             = EMPTY;

    // Set all the pointers to NULL
    ProcTable[i].parentPtr          = NULL;
    ProcTable[i].nextZapPtr         = NULL;
    ProcTable[i].nextProcPtr        = NULL;
    ProcTable[i].childProcPtr       = NULL;
    ProcTable[i].quitChildPtr       = NULL;
    ProcTable[i].nextSiblingPtr     = NULL;
    ProcTable[i].nextDeadSibling    = NULL;

    // Miscellaneous values
    ProcTable[i].stack              = NULL;
    ProcTable[i].stackSize          = 1;
    ProcTable[i].priority           = -1;
    ProcTable[i].zapStatus          = 0;
    ProcTable[i].numberOfChildren   = 0;
    ProcTable[i].parentPID          = -1;
    ProcTable[i].name[0]            = '\n';
    ProcTable[i].quitStatus         = -1;
    ProcTable[i].timeInitialized    = -1;
    ProcTable[i].totalTimeRunning   = -1;
    ProcTable[i].totalSliceTime     = -1;


    // Initialize the queues attatched to the process
    initializeProcessQueue(&ProcTable[i].childrenQueue, CHILDRENLIST);
    initializeProcessQueue(&ProcTable[i].deadChildrenQueue, DEADCHILDRENLIST);
    initializeProcessQueue(&ProcTable[i].zappedProcessesQueue, ZAPLIST);
}


/*
 * Interrupt handlers are passed two parameters
 * The first parameter indicates the type of argument,
 * the second parameter varies depending on the type of interrupt
 *
 */
void clockHandler(int dev, void *arg){

    if(debugEnabled())
        USLOSS_Console("clockHandler has been called\n");

    if(!inKernelMode())
        haltAndPrintKernelError();

    disableInterrupts();

    int statusFromDeviceInput = 0;
    //USLOSS_DeviceInptu - >For the clock, the value will be the number of microseconds since USLOSS was started.
    /*
     * USLOSS_DeviceInput() takes three arguments.
     * The first specifies the interrupt (the clock in this case).
     * The second is the unit number -- for the clock, there is only one clock, so put in 0.
     * The third is a value returned by USLOSS_DeviceInput().
     */

    //Your clockHandler code will call USLOSS_DeviceInput() each time the clock interrupt happens.
    // It will compare the result with the time at which the currently running process began its current time slice.
    // This implies that the start time was recorded when the process was selected to run (you have to do this...)
    Current->totalSliceTime = USLOSS_DeviceInput(0, 0, &statusFromDeviceInput) - Current->timeInitialized;




}

/*
 * Checks whether we are currently in kernel mode
 * Returns true if we are
 * False otherwise
 */
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
 * Name:            haltAndPrintKernelError
 * Purpose:         Print an error to console, and halt USLOSS
 * Parameter:       None
 * Returns:         Nothing
 * Side Effects:    USLOSS is Halted and Error is printed to console
 * ------------------------------------------------------------------------ */
void haltAndPrintKernelError(){
    USLOSS_Console("ERROR: Not in kernel mode!\n");
    USLOSS_Halt(1);
}

/* ------------------------------------------------------------------------
 * Name:            findAvailableProcSlot
 * Purpose:         Look through the proc table, find an empty slot and
 *                      return the index of said slot. Return -1 if there
 *                      is no empty slot found
 *                  Also iterate nextPid so that it matches the resulting
 *                      number of Processes
 * Parameter:       None
 * Returns:         index in table (int) or -1
 * Side Effects:    If -1 is returned, fork1() is stopped
 * ------------------------------------------------------------------------ */
int findAvailableProcSlot() {
    int procSlot = -1;
    int startPid = nextPid;

    if(debugEnabled())
        USLOSS_Console("findAvailableProcSlot(): nextPid is: %d\n", startPid);

    //Need to loop between startPid and MAXPROC to find an empty slot
    for(int i = startPid % MAXPROC; i < MAXPROC; i++){
        if(ProcTable[i].status == EMPTY) {
            procSlot = i;
            nextPid++;
            return procSlot;
        }
    }

    //If nothing was found then look between 0 and startPid
    if(procSlot == -1){
        for(int i = 0; i < startPid; i++){
            if(ProcTable[i].status == EMPTY){
                procSlot = i;
                nextPid++;
                break;
            }
        }
    }

    return procSlot;
}

int debugEnabled(){
    int debug = 0;
    if(DEBUG && debugflag)
        debug = 1;
    return debug;
}

//void insertInReadyList(procPtr process){
//    if(debugEnabled())
//        USLOSS_Console("Adding a process to the "
//                       "readly list with a pid %d and priority %d\n", process->pid, process->priority);
//
//    //Check if the list is empty
//    //If it is, simply add to the head of the list and return
//    if(ReadyList == NULL){
//        ReadyList = process;
//        return;
//    }
//
//    //If the new process has a priority that is greater than
//    //  that of the head nodes
//    if(process->priority < ReadyList->priority) {
//        process->nextProcPtr = ReadyList;
//        ReadyList = process;
//    }
//
//    //Otherwise, the process has a priority lower than the head
//    //Iterate through until we find the sport for proper insertion
//    else {
//
//        //Create a temp pointing to head
//        procPtr temp = ReadyList;
//
//        while(temp->nextProcPtr != NULL){
//            if(temp->nextProcPtr->priority > process->priority)
//                break;
//            temp = temp->nextProcPtr;
//        }
//
//        //Swap pointers to insert process into the list
//        process->nextProcPtr = temp->nextProcPtr;
//        temp->nextProcPtr = process;
//    }
//}

//Returns 0/false if not zapped, 1 otherwise
int isZapped() {
    return Current->zapStatus;
}

/* ------------------------------------------------------------------------
   Name - blockCurrentProcess
   Purpose - Blocks the current process
   Parameters -
   Returns - -1 if the process was zapped while it was blocked
             0: in every other case
   Side Effects -
   ------------------------------------------------------------------------ */
int blockCurrentProcess(int updatedStatus){
    if(debugEnabled())
        USLOSS_Console("Block has been called on process with pid:%d\n", Current->pid);

    if(!inKernelMode())
        haltAndPrintKernelError();

    disableInterrupts();

    Current->status = updatedStatus;
    int ReadyListIndex = Current->priority - 1;
    popFromQueue(&ReadyList[ReadyListIndex]);
    dispatcher();

    if(Current->zappedProcessesQueue.length > 0)
        return -1;

    return 0;

}

int countNumberOfProcessRunningAndActive(){

    int runningAndActive = 0;

    // Iterate through and count all processes that
    //      have a status other than empty, meaning that
    //      they are actually processes
    for(int i = 0; i < MAXPROC; i++){
        if(ProcTable[i].status != EMPTY)
            runningAndActive++;
    }
    return runningAndActive;
}



