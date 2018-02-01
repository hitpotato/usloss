/* Patrick's DEBUG printing constant... */
#ifndef _KERNEL_H
#define _KERNEL_H

#include "phase1.h"
#include <stdlib.h>
#define DEBUG 1

typedef struct procStruct procStruct;

typedef struct procStruct * procPtr;

typedef struct processQueue processQueue;

struct processQueue {
    procPtr headProcess;
    procPtr tailProcess;
    int     length;
    int     typeOfQueue;
};

#define READYLIST           0
#define CHILDRENLIST        1
#define DEADCHILDRENLIST    2
#define ZAPLIST             3

/* -------------------------- Function Prototypes ---------------------------------- */

void     initializeProcessQueue(processQueue* queue, int queueType);
void     appendProcessToQueue(processQueue* queue, procPtr process);
procPtr  popFromQueue(processQueue* queue);
void     removeChildFromQueue(processQueue* queue, procPtr child);
procPtr  peekAtHead(processQueue* queue);



struct procStruct {
    procPtr         nextProcPtr;
    procPtr         childProcPtr;
    procPtr         nextSiblingPtr;
    char            name[MAXNAME];     /* process's name */
    char            startArg[MAXARG];  /* args passed to process */
    USLOSS_Context  state;             /* current context for process */
    short           pid;               /* process id */
    int             priority;
    int (* startFunc) (char *);         /* function where process begins -- launch */
    char           *stack;
    unsigned int    stackSize;
    int             status;             /* READY, BLOCKED, QUIT, etc. */

    /* other fields as needed... */
    procPtr         quitChildPtr;
    procPtr         nextZapPtr;
    procPtr         nextDeadSibling;
    procPtr         parentPtr;
    short           parentPID;         /* parent process id */
    int             numberOfChildren;   /* The number of children this process has */
    int             zapStatus;            /* 0 if not zapped. 1 if zapped */
    int             quitReturnValue;    /* The value the process returns after quiting */
    int             quitStatus;
    int             timeInitialized;
    int             totalTimeRunning;
    int             totalSliceTime;
    int             cpuTime;
    processQueue    childrenQueue;
    processQueue    deadChildrenQueue;
    processQueue    zappedProcessesQueue;

};

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


//##################################################################################################

/* ------------------------------------------------------------------------
   Name - initializeProcessQueue
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ------------------------------------------------------------------------ */
void initializeProcessQueue(processQueue* queue, int queueType){
    queue->headProcess  = NULL;
    queue->tailProcess  = NULL;
    queue->length       = 0;
    queue->typeOfQueue  = queueType;
}

/* ------------------------------------------------------------------------
   Name - appendProcessToQueue
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ------------------------------------------------------------------------ */
void appendProcessToQueue(processQueue* queue, procPtr process){

    // If the list is empty, set the first and tail elements to the given process
    if(queue->headProcess == NULL && queue->tailProcess == NULL) {
        queue->headProcess = process;
        queue->tailProcess = process;
    }

        // If the list is not empty
    else{
        //Check the type of the list before deciding what to do
        switch(queue->typeOfQueue){
            case READYLIST :
                queue->tailProcess->nextProcPtr = process;
            case CHILDRENLIST :
                queue->tailProcess->nextSiblingPtr = process;
            case ZAPLIST :
                queue->tailProcess->nextZapPtr = process;
            case DEADCHILDRENLIST :
                queue->tailProcess->nextDeadSibling = process;
        }
        queue->tailProcess = process;
    }


    queue->length++;        // Increase the size of the list
}

/* ------------------------------------------------------------------------
   Name - popFromQueue
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ------------------------------------------------------------------------ */
procPtr popFromQueue(processQueue* queue){

    procPtr temp = queue->headProcess;

    // Check to make sure the queue is not empty
    if(temp == NULL)
        return NULL;

    // If the queue is only of length 1, return
    // the head and set the head and tail to null
    if(queue->headProcess == queue->tailProcess) {
        queue->headProcess = NULL;
        queue->tailProcess = NULL;
    }

        /*
         * Otherwise, we need to set our head element to the element following it
         * Essentially: a->b->c->..... We return a, and our new list needs to look
         *  like: b->c->....
         */
    else {
        switch (queue->typeOfQueue){
            case READYLIST :
                queue->headProcess = queue->headProcess->nextProcPtr;
            case CHILDRENLIST :
                queue->headProcess = queue->headProcess->nextSiblingPtr;
            case ZAPLIST :
                queue->headProcess = queue->headProcess->nextZapPtr;
            case DEADCHILDRENLIST :
                queue->headProcess = queue->headProcess->nextDeadSibling;
        }
    }

    queue->length--;    // Decrement the size of the list
    return temp;        // Return the head of the list

}

/* ------------------------------------------------------------------------
   Name - removeChildFromQueue
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ------------------------------------------------------------------------ */
void removeChildFromQueue(processQueue* queue, procPtr child){

    // If the list is empty return
    if(queue->headProcess == NULL)
        return;

    // If the list is not of type children return
    if(queue->typeOfQueue != CHILDRENLIST)
        return;

    // If the head element is the child we want to remove
    if(queue->headProcess == child){
        popFromQueue(queue);        // Call pop to remove the head of the queue
        return;
    }

    procPtr temp = queue->headProcess;
    procPtr tempSiblingPointer = queue->headProcess->nextSiblingPtr;

    // Iterate through the queue
    while (tempSiblingPointer != NULL){
        if(tempSiblingPointer == child){
            if(tempSiblingPointer == queue->tailProcess){
                queue->tailProcess = temp;
            }
            else {
                temp->nextSiblingPtr = tempSiblingPointer->nextSiblingPtr->nextSiblingPtr;
            }
            queue->length--;        // Decrement the size of the queue
            return;
        }
        temp = tempSiblingPointer;
        tempSiblingPointer = temp->nextSiblingPtr;
    }
}

/* ------------------------------------------------------------------------
   Name - peekAtHead
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ------------------------------------------------------------------------ */
procPtr peekAtHead(processQueue* queue){

    return queue->headProcess;      // There is the chance that the return will be NULL
}

/* Some useful constants.  Add more as needed... */
#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY (MINPRIORITY + 1)

#define TIMESLICE 80000

#define QUIT 0
#define READY 1
#define RUNNING 2
#define BLOCKED = -1
#define EMPTY -2

#define MAXTIMEALLOTED 80000            // Reprents an 80ms max time in

#define NOPARENTPROCESS -1

#define BLOCKEDBYJOIN    11
#define BLOCKEDBYZAP     12

#endif
