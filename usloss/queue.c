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





