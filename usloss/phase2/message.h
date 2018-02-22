
#include "phase2.h"
#include <stdlib.h>


#define DEBUG2 1

typedef struct mailSlot     *slotPtr;
typedef struct mailbox      mailbox;
typedef struct mboxProc     *mboxProcPtr;
typedef struct mailSlot     mailSlot;
typedef struct mboxProc     mboxProc;
typedef struct queue        queue;

struct mboxProc {
    mboxProcPtr     nextMboxProc;
    int             pid;
    void            *msg_ptr;   // Where to place recieved message
    int             msg_length;
    slotPtr         messageRecieved;    // Slot containing message
};

#define SLOTQUEUE 0
#define PROCQUEUE 1

struct queue {
    void    *head;
    void    *tail;
    int     length;
    int     typeOfQueue;
};


struct mailbox {
    int         mboxID;
    // other items as needed...
    int         status;
    int         totalSlots;
    int         slotSize;
    queue       slots;
    queue       blockedProcsSend;
    queue       blockedProcsRecieve;
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
                    processQueue* queue:
                        Pointer to queue to be initialized
                    queueType
                        An int specifying what the queue is to be used for
   Returns -        Nothing
   Side Effects -   The passed queue is now no longer NULL
   ------------------------------------------------------------------------ */
void initializeQueue(queue *q, int queueType) {
    q->head = NULL;
    q->tail = NULL;
    q->length = 0;
    q->typeOfQueue = queueType;
}



