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
#include <handler.c>

//For cLion




/* ------------------------- Prototypes ----------------------------------- */
int     start1 (char *);
int     inKernelMode();
void    initializeBox(int i);
void    initializeSlot(int i);
int     isBadMBoxID(int mbox_id);
int     send(int mbox_id, void *msg_ptr, int msg_size, int condSend);
int     receive(int mbox_id, void *msg_ptr, int msg_length, int condRecieve);
int     sendMessageToProcess(mboxProcPtr process, void *msg_ptr, int msg_length);
int     findAndInitializeFreeMailSlot(void *msg_ptr, int msg_size);

/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;

// Tables of mailBoxes, mailSlots and Processes
mailbox     MailBoxTable[MAXMBOX];
mailSlot    MailSlotTable[MAXSLOTS];
mboxProc    mboxProcTable[MAXPROC];

// Keep track of the total of boxes and slots used
int numBoxes, numSlots;

// Keep track of the ID's to be used for the next thing
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
        initializeSlot(i);
    }

    // Initialize the number of slots and boxes
    numBoxes = 0, numSlots = 0;

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

    enableInterrupts();     // Re-enable interrupts

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

    // Get mailbox with corresponding ID
    mailbox *mailbox = &MailBoxTable[nextMboxID];

    // Initialize the fields of the box
    mailbox->mboxID = nextMboxID++;
    mailbox->totalSlots = slots;
    mailbox->slotSize = slot_size;
    mailbox->status = ACTIVE;
    initializeQueue(&mailbox->slots, SLOTQUEUE);
    initializeQueue(&mailbox->blockedProcsSend, PROCQUEUE);
    initializeQueue(&mailbox->blockedProcsRecieve, PROCQUEUE);


    numBoxes++;     // Increase the number of active boxes

    if(debugEnabled())
        USLOSS_Console("MboxCreate(): Created mailbox with id: %d, totalSlots: %d, slot_size: %d, "
                               "numBoxes: %d\n", mailbox->mboxID, mailbox->totalSlots, mailbox->slotSize, numBoxes);

    enableInterrupts();
    return mailbox->mboxID;
} /* MboxCreate */


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
   Name - MboxCondSend
   Purpose - Put a message into a slot for the indicated mailbox.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args, -2 if message not sent
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size)
{
    return send(mbox_id, msg_ptr, msg_size, 1);
} /* MboxCondSend */


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
    return receive(mbox_id, msg_ptr, msg_size, 0);
} /* MboxReceive */

/* ------------------------------------------------------------------------
   Name - MboxCondReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args,
                -2 if receive failed.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxCondReceive(int mbox_id, void *msg_ptr, int msg_size)
{
    return receive(mbox_id, msg_ptr, msg_size, 1);
} /* MboxCondReceive */

/* ------------------------------------------------------------------------
   Name - MboxRelease
   Purpose - Release a mailbox
   Parameters - id of the mailbox that needs to be released
   Returns - -1 if the mbox_id is invalid, -3 if called was zap'd,
                0 otherwise
   Side Effects - Zaps any processes that are blocked on the mailbox
   ----------------------------------------------------------------------- */
int MboxRelease(int mbox_id){

    disableInterrupts();
    makeSureCurrentFunctionIsInKernelMode("MboxRelease():");

    // Check if the mailboxID is invalid
    if(isBadMBoxID(mbox_id)){
        if(debugEnabled())
            USLOSS_Console("MboxRelease(): Called with invalid m_boxID: %d, returning -1\n",
                           mbox_id);
        return -1;
    }

    // Get the mailbox
    mailbox *mailbox = &MailBoxTable[mbox_id];

    // Check if the mailbox is in use
    if(mailbox == NULL || mailbox->status == INACTIVE){
        if (debugEnabled())
            USLOSS_Console("MboxRelease(): Mailbox %d is already released, returning -1\n",
                           mbox_id);
        return -1;
    }

    // Empty all the slots in the mailbox
    while (mailbox->slots.length > 0){
        slotPtr slot = (slotPtr)popFromQueue(&mailbox->slots);
        initializeSlot(slot->slotID);
    }

    // Empty the mailbox itself
    initializeBox(mbox_id);

    if (debugEnabled())
        USLOSS_Console("MboxRelease: realsed mailbox %d\n", mbox_id);

    // Unblock any processes that were blocked on a send
    while (mailbox->blockedProcsSend.length > 0){
        mboxProcPtr blockedProcess = (mboxProcPtr)popFromQueue(&mailbox->blockedProcsSend);
        unblockProc(blockedProcess->pid);
        disableInterrupts();
    }

    // Unblock any processes that were blocked on a recieve
    while (mailbox->blockedProcsRecieve.length > 0){
        mboxProcPtr blockedProcess = (mboxProcPtr)popFromQueue(&mailbox->blockedProcsRecieve);
        unblockProc(blockedProcess->pid);
        disableInterrupts();
    }

    enableInterrupts();
    return 0;

}


/* ------------------------------------------------------------------------
   Name - findAndInitializeFreeMailSlot
   Purpose - gets a free slot from the table of mail slots and initializes it
   Parameters - pointer to the message to be placed in the slot and the message size.
   Returns - ID of the new slot.
   Side Effects - initializes a single element of the mail slot array.
   ----------------------------------------------------------------------- */
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

    slotPtr newSlot = &MailSlotTable[nextSlotID];
    newSlot->slotID = nextSlotID++;
    newSlot->status = USED;
    newSlot->messageSize = msg_size;
    numSlots++;

    memcpy(newSlot->message, msg_ptr, msg_size);        // Copy the message into the slot

    if(debugEnabled())
        USLOSS_Console("createSlot(): Created new slot for message size: %d, slotID: %d, total slots: %d\n",
                        msg_size, newSlot->slotID, numSlots);

    return newSlot->slotID;
}


/* ------------------------------------------------------------------------
   Name - sendMessageToProcess
   Purpose - sends a message directly to a process
   Parameters - pointer to the process, message to put in the slot,
                and the message size.
   Returns - 0 for success or -1 for failure
   Side Effects - A process now contains a new message
   ----------------------------------------------------------------------- */
int sendMessageToProcess(mboxProcPtr process, void *msg_ptr, int msg_length){

    // Check for error cases
    if(process == NULL || process->msg_ptr == NULL || process->msg_length < msg_length){
        if(debugEnabled())
            USLOSS_Console("sendMessageToProcess(): Invalid arguments, returning -1\n");
        process->msg_length = -1;
        return -1;
    }

    // Copy the message
    memcpy(process->msg_ptr, msg_ptr, msg_length);
    process->msg_length = msg_length;

    if(debugEnabled())
        USLOSS_Console("sendToProc(): Gave message size %d to process %d\n", msg_length, process->pid);

    return 0;
}


int send(int mbox_id, void *msg_ptr, int msg_size, int condSend){

    disableInterrupts();
    makeSureCurrentFunctionIsInKernelMode("MboxSend()");
    if(debugEnabled())
        USLOSS_Console("send(): Called with mbox_id: %d, msg_ptr: %d, msg_size: %d, condSend: %d\n",
                        mbox_id, msg_ptr, msg_size, condSend);

    // Check for invalid mbox_id
    if(isBadMBoxID(mbox_id)){
        if(debugEnabled())
            USLOSS_Console("MboxSend(): Called with invalid mbox_id: %d, returning -1\n", mbox_id);
        enableInterrupts();
        return -1;
    }

    // Get the mailbox
    mailbox *mailbox = &MailBoxTable[mbox_id];

    // Check for Invalid Arguments
    if(mailbox->status == INACTIVE || msg_size < 0 || msg_size > mailbox->slotSize){
        if(debugEnabled())
            USLOSS_Console("MboxSend(): Called with an invalid argument, returning -1\n", mbox_id);
        enableInterrupts();
        return -1;
    }

    // Figure out what to do with a blocked reciever
    if(mailbox->blockedProcsRecieve.length > 0 && (mailbox->slots.length < mailbox->totalSlots || mailbox->totalSlots == 0)){
        mboxProcPtr proc = (mboxProcPtr)popFromQueue(&mailbox->blockedProcsRecieve);

        // Give the message to the sender
        int result = sendMessageToProcess(proc, msg_ptr, msg_size);
        if(debugEnabled())
            USLOSS_Console("MboxSend(): Unblocking process %d that was blocked on receive\n", proc->pid);
        unblockProc(proc->pid);
        enableInterrupts();
        if(result < 0)
            return -1;
        return 0;
    }

    // If all the slots are taken, block caller until slots are avaialable
    if(mailbox->slots.length == mailbox->totalSlots){
        if(condSend){
            if(debugEnabled())
                USLOSS_Console("MboxSend(): Conditional send failed, returning -2\n");
            enableInterrupts();
            return -2;
        }

        // Initialize process details
        mboxProc mailboxProcess;
        mailboxProcess.nextMboxProc = NULL;
        mailboxProcess.pid = getpid();
        mailboxProcess.msg_ptr = msg_ptr;
        mailboxProcess.msg_length = msg_size;

        if(debugEnabled())
            USLOSS_Console("MboxSend(): All slots are full, blocking pid %d\n", mailboxProcess.pid);

        appendProcessToQueue(&mailbox->blockedProcsSend, &mailboxProcess);

        blockMe(FULL_BOX);
        disableInterrupts();

        // Return -3 if the process zap'd or the mailbox released whil blocked on the mailbox
        if(isZapped() || mailbox->status == INACTIVE){
            if(debugEnabled())
                USLOSS_Console("MboxSend(): process %d was zapped while blocked on a send, returning -3\n",
                               mailboxProcess.pid);
            enableInterrupts();
            return -3;
        }
        enableInterrupts();
        return 0;
    }

    // If the mail slot table overflows, that is an error that should halt USLOSS
    if(numSlots == MAXSLOTS){
        if(condSend){
            if(debugEnabled())
                USLOSS_Console("MboxSend(): No slots available for condSend send to box %d, returning "
                                       "-2\n", mbox_id);
            return -2;
        }
        USLOSS_Console("MboxSend(): Mail slot table overflow. Halting...\n");
        USLOSS_Halt(1);
    }

    // Create a new slot and add message to it
    int slotId = findAndInitializeFreeMailSlot(msg_ptr, msg_size);
    slotPtr newSlot = &MailSlotTable[slotId];
    appendProcessToQueue(&mailbox->slots, newSlot);    // Add the slot to the mailbox

    enableInterrupts();
    return 0;
}

int receive(int mbox_id, void *msg_ptr, int msg_length, int condRecieve){

    disableInterrupts();
    makeSureCurrentFunctionIsInKernelMode("MboxRecieve()");
    slotPtr slot;

    // Check for an invalid mbox_id
    if(mbox_id < 0 || mbox_id >= MAXMBOX){
        if(debugEnabled())
            USLOSS_Console("MboxRecieve(): Invalid mbox_id: %d, returning -1\n", mbox_id);
        enableInterrupts();
        return -1;
    }

    mailbox *box = &MailBoxTable[mbox_id];
    int size;

    // Make sure the box is active
    if(box->status == INACTIVE){
        if(debugEnabled())
            USLOSS_Console("MboxRecieve(): Invalid box id: %d, returning -1\n,", mbox_id);
        enableInterrupts();
        return -1;
    }

    // If mailbox has 0 slots
    if(box->totalSlots == 0){
        mboxProc mproc;

        // Initialize mailbox values
        mproc.nextMboxProc  = NULL;
        mproc.pid           = getpid();
        mproc.msg_ptr       = msg_ptr;
        mproc.msg_length    = msg_length;

        // If there is a process that has sent, unblock it and get its message
        if(box->blockedProcsSend.length > 0){
            mboxProcPtr proc = (mboxProcPtr)popFromQueue(&box->blockedProcsSend);
            sendMessageToProcess(&mproc, proc->msg_ptr, proc->msg_length);
            if(debugEnabled())
                USLOSS_Console("MboxRecieve(): Unblocking process %d that was blocked on "
                                       "send to 0 slot mailbox\n", proc->pid);
            unblockProc(proc->pid);
        }

        // Otherwise block the reciever (if not conditional)
        else if(!condRecieve){
            if(debugEnabled())
                USLOSS_Console("MboxRecieve(): Blocking process %d on 0 slot mailbox\n"
                ,mproc.pid);
            appendProcessToQueue(&box->blockedProcsRecieve, &mproc);
            blockMe(NO_MESSAGES);

            // If the process is zapped or the mailbox is inactive
            if(isZapped() || box->status == INACTIVE){
                if(debugEnabled())
                    USLOSS_Console("MboxSend(): Process %d was zapped while blocked on a send,"
                                           "returning -3\n", mproc.pid);
                enableInterrupts();
                return -3;
            }
        }

        enableInterrupts();
        return mproc.msg_length;
    }

    // Block if there are no messages available
    if(box->slots.length == 0){
        mboxProc mproc;

        // Initialize mailbox values
        mproc.nextMboxProc      = NULL;
        mproc.pid               = getpid();
        mproc.msg_ptr           = msg_ptr;
        mproc.msg_length        = msg_length;
        mproc.messageRecieved   = NULL;

        // Check for a 0 slot mailbox
        if(box->totalSlots == 0 && box->blockedProcsSend.length > 0){
            mboxProcPtr proc = (mboxProcPtr)popFromQueue(&box->blockedProcsSend);
            sendMessageToProcess(&mproc, proc->msg_ptr, proc->msg_length);
            if(debugEnabled())
                USLOSS_Console("MboxRecieve(): Unblocking process %d tht was blocked on send to"
                                       "0 slot mailbox\n", proc->pid);
            unblockProc(proc->pid);
            enableInterrupts();
            return mproc.msg_length;
        }

        // On conditional receive, return -2
        if(condRecieve){
            if(debugEnabled())
                USLOSS_Console("MboxRecieve(): Conditional receive failed, returning -2\n");
            enableInterrupts();
            return -2;
        }

        if(debugEnabled())
            USLOSS_Console("MboxRecieve(): No messages available, blocking pid %d...\n",
                           mproc.pid);

        // Append to the queue of blocked processes on receive
        appendProcessToQueue(&box->blockedProcsRecieve, &mproc);
        blockMe(NO_MESSAGES);
        disableInterrupts();

        // Return -3 if the process is zap'd or the mailbox released while blocked on the mailbox
        if(isZapped() || box->status == INACTIVE){
            if(debugEnabled())
                USLOSS_Console("MboxRecieve(): Either process %d was zapped, mailbox was freed"
                                       "or we did not receive message, returning -2", mproc.pid);
            enableInterrupts();
            return -3;
        }

        return mproc.msg_length;
    }
    else
        slot = popFromQueue(&box->slots);   // Get the mailSlot

    // Check if the slot has enough space
    if(slot == NULL || slot->status == EMPTY || msg_length < slot->messageSize){
        if(debugEnabled() && (slot == NULL || slot->status == EMPTY))
            USLOSS_Console("MboxRecieve(): Mail slot null or empty, returning -1\n");
        else if(debugEnabled())
            USLOSS_Console("MboxRecieve(): No room for message. Room provided: %d, "
                                   "message size: %d, returning -1\n",
                           msg_length, slot->messageSize);
        enableInterrupts();
        return -1;
    }

    // Copy the message
    size = slot->messageSize;
    memcpy(msg_ptr, slot->message, size);

    // Free the mail slot
    initializeSlot(slot->slotID);

    // Unblock a process that was blocked on a send to this mailbox
    if(box->blockedProcsSend.length > 0){
        mboxProcPtr proc = (mboxProcPtr)popFromQueue(&box->blockedProcsSend);

        // Create slot for the sender's message
        int slotID = findAndInitializeFreeMailSlot(proc->msg_ptr, proc->msg_length);
        slotPtr slot = &MailSlotTable[slotID];
        appendProcessToQueue(&box->slots, slot);

        // Unblock the sender
        if(debugEnabled())
            USLOSS_Console("MboxRecieve(): Unblocking process %d that was blocked on send\n",
                            proc->pid);
        unblockProc(proc->pid);
    }

    enableInterrupts();
    return size;
}


int isBadMBoxID(int mbox_id){
    return (mbox_id < 0 || mbox_id >= MAXMBOX);
}


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
   Name -           initializeSlot
   Purpose -        Emptys/initializes a mail slot
   Parameters -
                    index of mailslot in the table of mailslots
   Returns -        nothing
   Side Effects -   Reduces the number of mailslots active by 1
   ----------------------------------------------------------------------- */
void initializeSlot(int i){
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
    int whatever;
    // Check to make sure we are in kernel mode first
    if(!inKernelMode()) {
        USLOSS_Console("Kernel Error: Not in kernel mode, cannot disable interrupts\n");
        USLOSS_Halt(1);
    }

    else
        whatever = USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_INT );
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
    int whatever;
    // turn the interrupts ON iff we are in kernel mode
    if(!inKernelMode()) {
        //not in kernel mode
        USLOSS_Console("Kernel Error: Not in kernel mode, cannot disable interrupts\n ");
        USLOSS_Halt(1);
    }
    else
        whatever = USLOSS_PsrSet( USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT );
} /* enableInterrupts */
