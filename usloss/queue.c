#include <stdlib.h>
#include <stdio.h>
#include "queue.h"

/* ------------------------------------------------------------------------
   Name - initializeProcessQueue
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ------------------------------------------------------------------------ */
void     initializeProcessQueue(processQueue* queue, int queueType){
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
void     appendProcessToQueue(processQueue* queue, procPtr process){

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

    //increase the size of the queue
    queue->length++;
}

/* ------------------------------------------------------------------------
   Name - popFromQueue
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ------------------------------------------------------------------------ */
procPtr   popFromQueue(processQueue* queue){

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

}

/* ------------------------------------------------------------------------
   Name - removeChildFromQueue
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ------------------------------------------------------------------------ */
void     removeChildFromQueue(processQueue* queue, procPtr child);

/* ------------------------------------------------------------------------
   Name - peekAtHead
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ------------------------------------------------------------------------ */
procPtr  peekAtHead(processQueue* queue);





