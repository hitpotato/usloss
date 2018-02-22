/* ------------------------------------------------------------------------
   phase2.c

   University of Arizona
   Computer Science 452

   Rodrigo Silva Mendoza 
   Long Chen
   ------------------------------------------------------------------------ */

#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <message.h>

//For cLion


#include "handler.c"


/* ------------------------- Prototypes ----------------------------------- */
int     start1 (char *);
void    enableInterrupts();
void    disableInterrupts();
int     debugEnabled();
void    makeSureCurrentFunctionIsInKernelMode(char *name);
int     inKernelMode();
void initializeBox(int i);
void initializeSlots(int i);



/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;


mailbox     MailBoxTable[MAXMBOX];  // mail box
mailSlot    MailSlotTable[MAXSLOTS]; // mail slots
mboxProc    mboxProcTable[MAXPROC];  // the processes

int numBoxes, numSlots;

int nextMboxID = 0, nextSlotID = 0, nextProc = 0;

void (*systemCallVec[MAXSYSCALLS])(USLOSS_Sysargs *args);


/* -------------------------- Functions ----------------------------------- */

/* ------------------------------------------------------------------------
   Name - start1
   Purpose - Initializes mailboxes and interrupt vector.
             Start the phase2 test process.
   Parameters - one, default arg passed by fork1, not used here.
   Returns - one to indicate normal quit.
   Side Effects - lots since it initializes the phase2 data structures.
   ----------------------------------------------------------------------- */
int start1(char *arg)
{
    int kidPid;
    int status;

    if (debugEnabled())
        USLOSS_Console("start1(): at beginning\n");
    makeSureCurrentFunctionIsInKernelMode("start1");

    disableInterrupts();

    // Initialize mailbox table
    for(int i = 0; i < MAXMBOX; i++){
        initializeBox(i);
    }

    // Initialize mail slots
    for(int i = 0; i < MAXSLOTS; i++){
        initializeSlots(i);
    }

    // Initialize the number of slots and boxes
    numBoxes = 0, numSlots = 0;


    // Initialize the mail box table, slots, & other data structures.

    // Initialize USLOSS_IntVec and system call handlers,
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler2;
    USLOSS_IntVec[USLOSS_DISK_UNITS] = diskHandler;
    USLOSS_IntVec[USLOSS_TERM_UNITS] = termHandler;
    USLOSS_IntVec[USLOSS_SYSCALL_INT] = syscallHandler;

    // allocate mailboxes for interrupt handlers.  Etc...
    IOmailboxes[CLOCKBOX] = MboxCreate(0, sizeof(int));    // one clock unit
    IOmailboxes[TERMBOX] = MboxCreate(0, sizeof(int));      // 4 terminal units
    IOmailboxes[TERMBOX+1] = MboxCreate(0, sizeof(int));
    IOmailboxes[TERMBOX+2] = MboxCreate(0, sizeof(int));
    IOmailboxes[TERMBOX+3] = MboxCreate(0, sizeof(int));
    IOmailboxes[DISKBOX] = MboxCreate(0, sizeof(int));      // two disk units
    IOmailboxes[DISKBOX+1] = MboxCreate(0, sizeof(int));

    // Set all system calls to nullsys, fill next phase
    for(int i = 0; i < MAXSYSCALLS; i++){
        systemCallVec[i] = nullsys;
    }

    enableInterrupts();

    // Create a process for start2, then block on a join until start2 quits
    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): fork'ing start2 process\n");
    kidPid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);
    if ( join(&status) != kidPid ) {
        USLOSS_Console("start2(): join returned something other than ");
        USLOSS_Console("start2's pid\n");
    }

    return 0;
} /* start1 */


/* ------------------------------------------------------------------------
   Name - MboxCreate
   Purpose - gets a free mailbox from the table of mailboxes and initializes it 
   Parameters - maximum number of slots in the mailbox and the max length of a msg
                sent to the mailbox.
   Returns - -1 to indicate that no mailbox was created, or a value >= 0 as the
             mailbox id.
   Side Effects - initializes one element of the mail box array. 
   ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size)
{
    disableInterrupts();
    makeSureCurrentFunctionIsInKernelMode("MboxCreate()");

    // Error Checking
    if(numBoxes == MAXMBOX || slots < 0 || slot_size < 0 || slot_size > MAX_MESSAGE){
        if(debugEnabled())
            USLOSS_Console("MboxCreate(): Illegal arguments given or max boxes reached.");
        return -1;
    }

    // If the index is taken, find the first avaialable index
    if(nextMboxID >= MAXMBOX || MailBoxTable[nextMboxID].status == ACTIVE){
        for(int i = 0; i < MAXMBOX; i++){
            if(MailBoxTable[i].status == INACTIVE){
                nextMboxID = i;
                break;
            }
        }
    }

    // Get mailbox
    mailbox *box = &MailBoxTable[nextMboxID];

    // Initialize the fields of the box
    box->mboxID = nextMboxID++;
    box->totalSlots = slots;
    box->slotSize = slot_size;
    box->status = ACTIVE;
    initializeQueue(&box->slots, SLOTQUEUE);
    initializeQueue(&box->blockedProcsSend, PROCQUEUE);
    initializeQueue(&box->blockedProcsRecieve, PROCQUEUE);

    numBoxes++;

    if(debugEnabled())
        USLOSS_Console("MboxCreate(): Created mailbox with id: %d, totalSlots: %d, slot_size: %d, "
                               "numBoxes: %d\n", box->mboxID, box->totalSlots, box->slotSize, numBoxes);

    enableInterrupts();
    return box->mboxID;
} /* MboxCreate */


int findAndInitializeFreeMailSlot(void *msg_ptr, int msg_size){

    disableInterrupts();
    makeSureCurrentFunctionIsInKernelMode("findAndInitializeFreeMailSlot()");

    // If the index is taken, find the first available index
    if(nextSlotID >= MAXSLOTS || MailSlotTable[nextSlotID].status == USED){
        for(int i = 0; i < MAXSLOTS; i++){
            if(MailSlotTable[i].status == EMPTY){
                nextSlotID = i;
                break;
            }
        }
    }

    slotPtr slot = &MailSlotTable[nextSlotID];
    slot->slotID = nextSlotID++;
    slot->status = USED;
    slot->messageSize = msg_size;
    numSlots++;

    memcpy(slot->message, msg_ptr, msg_size);

    if(debugEnabled())
        USLOSS_Console("createSlot(): Created new slot for message size: %d, slotID: %d, total slots: %d\n",
                        msg_size, slot->slotID, numSlots);

    return slot->slotID;
}


int sendMessageToProcess(mboxProcPtr proc, void *msg_ptr, int msg_length){

    // Check for error cases
    if(proc == NULL || proc->msg_ptr == NULL || proc->msg_length < msg_length){
        if(debugEnabled())
            USLOSS_Console("sendMessageToProcess(): Invalid arguments, returning -1\n");
        proc->msg_length = -1;
        return -1;
    }

    // Copy the message
    memcpy(proc->msg_ptr, msg_ptr, msg_length);
    proc->msg_length = msg_length;

    if(debugEnabled())
        USLOSS_Console("sendToProc(): Gave message size %d to process %d\n", msg_length, proc->pid);

    return 0;
}


int send(int mbox_id, void *msg_ptr, int msg_size, int conditional){

    disableInterrupts();
    makeSureCurrentFunctionIsInKernelMode("MboxSend()");
    if(debugEnabled())
        USLOSS_Console("send(): Called with mbox_id: %d, msg_ptr: %d, msg_size: %d, conditional: %d\n",
                        mbox_id, msg_ptr, msg_size, conditional);

    // Check for invalid mbox_id
    if(mbox_id < 0 || mbox_id >= MAXMBOX){
        if(debugEnabled())
            USLOSS_Console("MboxSend(): Called with invalid mbox_id: %d, returning -1\n", mbox_id);
        enableInterrupts();
        return -1;
    }

    // Get the mailbox
    mailbox *box = &MailBoxTable[mbox_id];

    // Check for Invalid Arguments
    if(box->status == INACTIVE || msg_size < 0 || msg_size > box->slotSize){
        if(debugEnabled())
            USLOSS_Console("MboxSend(): Called with an invalid argument, returning -1\n", mbox_id);
        enableInterrupts();
        return -1;
    }

    // Handle blocked reciever
    if(box->blockedProcsRecieve.length > 0 && (box->slots.length < box->totalSlots || box->totalSlots == 0)){
        mboxProcPtr proc = (mboxProcPtr)popFromQueue(&box->blockedProcsRecieve);

        // Give the message to the sender
        int result = sendMessageToProcess(proc, msg_ptr, msg_size);
        if(debugEnabled())
            USLOSS_Console("MboxSend(): Unblocking process %d that was blocked on recieve\n", proc->pid);
        unblockProc(proc->pid);
        enableInterrupts();
        if(result < 0)
            return -1;
        return 0;
    }

    // If all the slots are taken, block caller until slots are avaialable
    if(box->slots.length == box->totalSlots){
        if(conditional){
            if(debugEnabled())
                USLOSS_Console("MboxSend(): Conditional send failed, returning -2\n");
            enableInterrupts();
            return -2;
        }

        // Initialize process details
        mboxProc mproc;
        mproc.nextMboxProc = NULL;
        mproc.pid = getpid();
        mproc.msg_ptr = msg_ptr;
        mproc.msg_length = msg_size;

        if(debugEnabled())
            USLOSS_Console("MboxSend(): All slots are full, blocking pid %d\n", mproc.pid);

        appendProcessToQueue(&box->blockedProcsSend, &mproc);

        blockMe(FULL_BOX);
        disableInterrupts();

        // Return -3 if the process zap'd or the mailbox released whil blocked on the mailbox
        if(isZapped() || box->status == INACTIVE){
            if(debugEnabled())
                USLOSS_Console("MboxSend(): process %d was zapped while blocked on a send, returning -3\n",
                               mproc.pid);
            enableInterrupts();
            return -3;
        }
        enableInterrupts();
        return 0;
    }

    // If the mail slot table overflows, that is an error that should halt USLOSS
    if(numSlots == MAXSLOTS){
        if(conditional){
            if(debugEnabled())
                USLOSS_Console("MboxSend(): No slots available for conditional send to box %d, returning "
                                       "-2\n", mbox_id);
            return -2;
        }
        USLOSS_Console("MboxSend(): Mail slot table overflow. Halting...\n");
        USLOSS_Halt(1);
    }

    // Create a new slot and add message to it
    int slotId = findAndInitializeFreeMailSlot(msg_ptr, msg_size);
    slotPtr slot = &MailSlotTable[slotId];
    appendProcessToQueue(&box->slots, slot);    // Add the slot to the mailbox

    enableInterrupts();
    return 0;
}



/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size)
{
    return send(mbox_id, msg_ptr, msg_size, 0);
} /* MboxSend */




/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual length of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size)
{
    return 0;
} /* MboxReceive */

/* ------------------------------------------------------------------------
   Name - check_io
   Purpose - Determine if there any processes blocked on any of the
             interrupt mailboxes.
   Returns - 1 if one (or more) processes are blocked; 0 otherwise
   Side Effects - none.

   Note: Do nothing with this function until you have successfully completed
   work on the interrupt handlers and their associated mailboxes.
   ------------------------------------------------------------------------ */
int check_io(void)
{
    if (DEBUG2 && debugflag2)
        USLOSS_Console("check_io(): called\n");
    return 0;
} /* check_io */

/* ------------------------------------------------------------------------
   Name -           initializeBox
   Purpose -        Emptys/initializes a Mailbox
   Parameters -
                    index of mailbox in the table of mailboxes
   Returns -        nothing
   Side Effects -   Reduces the number of mailboxes active by 1
   ----------------------------------------------------------------------- */
void initializeBox(int i){
    MailBoxTable[i].mboxID      = -1;
    MailBoxTable[i].status      = INACTIVE;
    MailBoxTable[i].totalSlots  = -1;
    MailBoxTable[i].slotSize    = -1;
    numBoxes--;
}

/* ------------------------------------------------------------------------
   Name -           initializeSlots
   Purpose -        Emptys/initializes a mail slot
   Parameters -
                    index of mailslot in the table of mailslots
   Returns -        nothing
   Side Effects -   Reduces the number of mailslots active by 1
   ----------------------------------------------------------------------- */
void initializeSlots(int i){
    MailSlotTable[i].mboxID     = -1;
    MailSlotTable[i].status     = EMPTY;
    MailSlotTable[i].slotID     = -1;
    numSlots--;
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
   Name - debugEnabled
   Purpose - Checks to see if the debug flasgs are turned on
   Parameters - none
   Returns - nothing
   Side Effects - When test cases are run, debug info is printed
   ----------------------------------------------------------------------- */
int debugEnabled(){
    int debug = 0;
    if(DEBUG2 && debugflag2)
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
