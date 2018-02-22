
#ifndef MESSAGE_H
#define MESSAGE_H

#include "phase2.h"
#include <stdlib.h>


#define DEBUG2 1

typedef struct mailSlot     *slotPtr;
typedef struct mailbox      mailbox;
typedef struct mboxProc     *mboxProcPtr;
typedef struct mailSlot     mailSlot;
typedef struct mboxProc     mboxProc;
typedef struct processQueue processQueue;

struct mboxProc {
    mboxProcPtr     nextMboxProc;
    int             pid;
    void            *msg_ptr;   // Where to place recieved message
    int             msg_length;
    slotPtr         messageRecieved;    // Slot containing message
};

#define SLOTQUEUE 0
#define PROCQUEUE 1

struct processQueue {
    void    *headProcess;
    void    *tailProcess;
    int     length;
    int     typeOfQueue;
};


struct mailbox {
    int         mboxID;
    // other items as needed...
    int         status;
    int         totalSlots;
    int         slotSize;
    processQueue       slots;
    processQueue       blockedProcsSend;
    processQueue       blockedProcsRecieve;
};

struct mailSlot {
    int         mboxID;
    int         status;
    // other items as needed...
    int         slotID;
    slotPtr     nextSlotPtr;
    char        message[MAX_MESSAGE];
    int         messageSize;
};

#define INACTIVE 0
#define ACTIVE 1

#define EMPTY 0
#define USED  1

#define FULL_BOX 11
#define NO_MESSAGES 12

struct psrBits {
    unsigned int curMode:1;
    unsigned int curIntEnable:1;
    unsigned int prevMode:1;
    unsigned int prevIntEnable:1;
    unsigned int unused:28;
};

union psrValues {
    struct psrBits bits;
    unsigned int integerPart;
};

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
void initializeQueue(processQueue *q, int queueType) {
    q->headProcess  = NULL;
    q->tailProcess  = NULL;
    q->length       = 0;
    q->typeOfQueue  = queueType;
}

/* ------------------------------------------------------------------------
   Name -           appendProcessToQueue
   Purpose -        Adds a process to the end of a processQueue
   Parameters -
                    processQueue* processQueue:
                        Pointer to processQueue to be added to
                    procPty process
                        Pointer to the process that will be added
   Returns -        Nothing
   Side Effects -   The length of the processQueue increases by 1.
   ------------------------------------------------------------------------ */
void appendProcessToQueue(processQueue *queue, void* process) {

    if (queue->headProcess == NULL && queue->tailProcess == NULL) {
        queue->headProcess = queue->tailProcess = process;
    }

    else {
        if (queue->typeOfQueue == SLOTQUEUE)
            ((slotPtr)(queue->tailProcess))->nextSlotPtr = process;
        else if (queue->typeOfQueue == PROCQUEUE)
            ((mboxProcPtr)(queue->tailProcess))->nextMboxProc = process;
        queue->tailProcess = process;
    }

    queue->length++;
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
void* popFromQueue(processQueue *queue) {

    void* temp = queue->headProcess;

    // If there is no headProcess element, return null
    if (queue->headProcess == NULL) {
        return NULL;
    }

    //  If the list is of length 1, set the first and last nodes
    // 		equal to null to ensure that the list is represented as empty
    if (queue->headProcess == queue->tailProcess) {
        queue->headProcess = queue->tailProcess = NULL;
    }

    else {
        if (queue->typeOfQueue == SLOTQUEUE)
            queue->headProcess = ((slotPtr)(queue->headProcess))->nextSlotPtr;
        else if (queue->typeOfQueue == PROCQUEUE)
            queue->headProcess = ((mboxProcPtr)(queue->headProcess))->nextMboxProc;
    }

    // Decrease the length of the list  and return the headProcess element
    queue->length--;
    return temp;
}


/* ------------------------------------------------------------------------
   Name -           peekAtHead
   Purpose -        Returns a pointer to the first element in processQueue
   Parameters -
                    processQueue *processQueue
                        The processQueue we want to look at the first element of
   Returns -        A pointer to the headProcess of the given processQueue
   Side Effects -   None
   ------------------------------------------------------------------------ */
/* Return the headProcess of the given processQueue. */
void* peekAtHead(processQueue *queue) {

    // Check to make sure that the list is not empty
    if (queue->headProcess == NULL) {
        return NULL;
    }

    return queue->headProcess;
}

#endif /* GRANDPARENT_H */



