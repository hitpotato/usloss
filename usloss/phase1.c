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
#include <stdbool.h>


#include "kernel.h"
#include "usloss.h"
#include "queue.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (char *);
void dispatcher(void);
void launch();
static void checkDeadlock();
void initializeProcessTableEntry(int entryNumber);
bool inKernelMode();

//Interrupt Handlers
void clockHandler(int dev, void *arg);

/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 1;

// the process table
procStruct ProcTable[MAXPROC];

// Process lists
static procPtr READYLIST;


// current process ID
procPtr Current;

// the next pid to be assigned
unsigned int nextPid = SENTINELPID;

//Priority Lists
processPriorityQueue *priorityList_1;
processPriorityQueue *priorityList_2;
processPriorityQueue *priorityList_3;
processPriorityQueue *priorityList_4;
processPriorityQueue *priorityList_5;
processPriorityQueue *priorityList_6;


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
    int result; /* value returned by call to fork1() */

    /* initialize the process table */
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing process table, ProcTable[]\n");
    //Now that the debug message has been printed, initialize the process table 
    for(int i = 0; i < MAXPROC; i++){
        initializeProcessTableEntry(i); 
    }

    // Initialize the Ready list, etc.
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing the Ready list\n");

    //Initialize the six priority queues
    priorityList_1 = initializeQueue();
    priorityList_2 = initializeQueue();
    priorityList_3 = initializeQueue();
    priorityList_4 = initializeQueue();
    priorityList_5 = initializeQueue();
    priorityList_6 = initializeQueue();

    // Initialize the clock interrupt handler
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler;

    // startup a sentinel process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for sentinel\n");
    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                    SENTINELPRIORITY);
    if (result < 0) {
        if (DEBUG && debugflag) {
            USLOSS_Console("startup(): fork1 of sentinel returned error, ");
            USLOSS_Console("halting...\n");
        }
        USLOSS_Halt(1);
    }
  
    // start the test process
    if (DEBUG && debugflag)
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
    if (DEBUG && debugflag)
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
    int procSlot = -1;

    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): creating process %s\n", name);

    // test if in kernel mode; halt if in user mode
//    if (((USLOSS_PsrGet() << 31) >> 31) == 0){
//        USLOSS_Console("fork1(): in user mode, halting %s\n", name);
//        USLOSS_Halt(1);
//    }

    // test if in kernel mode; halt if in user mode
    if(!inKernelMode()){
        haltAndPrintError();
    }

    //Error check the priority
    // MAXPRIORITY = 1
    // MINPRIORITY = 5
    if(priority < MAXPRIORITY || priority > MINPRIORITY)
        //Make sure that that we are not forking sentinenl,
        //Which has a priority of 6
        if(startFunc != sentinel)
            return -1;

    // Return if stack size is too small
    if (stacksize < USLOSS_MIN_STACK)
        return -1;

    // Is there room in the process table? What is the next PID?
    int i = 0;
    while (ProcTable[nextPid % sizeof(ProcTable)].status != 0){
        if (i > 50)
            return -1;
        nextPid += 1;
        i++;
    }
    procSlot = nextPid % sizeof(ProcTable);

    // fill-in entry in process table */
    if ( strlen(name) >= (MAXNAME - 1) ) {
        USLOSS_Console("fork1(): Process name is too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    strcpy(ProcTable[procSlot].name, name);
    ProcTable[procSlot].startFunc = startFunc;
    if ( arg == NULL )
        ProcTable[procSlot].startArg[0] = '\0';
    else if ( strlen(arg) >= (MAXARG - 1) ) {
        USLOSS_Console("fork1(): argument too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    else
        strcpy(ProcTable[procSlot].startArg, arg);
    ProcTable[procSlot].nextProcPtr = NULL;
    ProcTable[procSlot].nextSiblingPtr = NULL;
    ProcTable[procSlot].childProcPtr = NULL;
    ProcTable[procSlot].status = 1;
    ProcTable[procSlot].pid = (short) nextPid;
    ProcTable[procSlot].priority = priority;
    ProcTable[procSlot].stackSize = stacksize;
    ProcTable[procSlot].stack = (char*) malloc (stacksize);
    ProcTable[procSlot].state = NULL;

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
    nextPid += 1;
    dispatcher();
    unsigned int interrupt_bit = 1;
    USLOSS_PsrSet(USLOSS_PsrGet() & (interrupt_bit << 1));


    return nextPid - 1;  // -1 is not correct! Here to prevent warning.
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

    if (DEBUG && debugflag)
        USLOSS_Console("launch(): started\n");

    // Enable interrupts

    // Call the function passed to fork1, and capture its return value
    result = Current->startFunc(Current->startArg);

    if (DEBUG && debugflag)
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
    return -1;  // -1 is not correct! Here to prevent warning.
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
    p1_quit(Current->pid);
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

    p1_switch(Current->pid, nextProcess->pid);
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
    if (DEBUG && debugflag)
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
} /* checkDeadlock */


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

    ProcTable[entryNumber].pid = -1;
    ProcTable[entryNumber].status = QUIT;
}

/*
 * Interrupt handlers are passed two parameters
 * The first parameter indicates the type of argument,
 * the second parameter varies depending on the type of interrupt
 *
 */
void clockHandler(int dev, void *arg){



}

/*
 * Checks whether we are currently in kernel mode
 * Returns true if we are
 * False otherwise
 */
bool inKernelMode(){
    bool kernelMode = false;

    //This ands the 0x1 with the PSR from USloss, giving us
    //the current mode bit.
    //If this bit is 0, we are in user mode
    //If it is something else, then we are in kernel mode
    if((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) != 0)
        kernelMode = true;

    return kernelMode;

}

/* ------------------------------------------------------------------------
 * Name:            haltAndPrintError
 * Purpose:         Print an error to console, and halt USLOSS
 * Parameter:       None
 * Returns:         Nothing
 * Side Effects:    USLOSS is Halted and Error is printed to console
 * ------------------------------------------------------------------------ */
void haltAndPrintError(){
    USLOSS_Console("ERROR: Not in kernel mode!\n");
    USLOSS_Halt(1);
}



