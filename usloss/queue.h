//
// Created by Rodrigo Silva Mendoza on 1/27/18.
//

#ifndef USLOSS_QUEUE_H
#define USLOSS_QUEUE_H

#include <stdbool.h>
#include "kernel.h"

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

extern void     initializeProcessQueue(processQueue* queue, int queueType);
extern void     appendProcessToQueue(processQueue* queue, procPtr process);
extern procPtr  popFromQueue(processQueue* queue);
extern void     removeChildFromQueue(processQueue* queue, procPtr child);
extern procPtr  peekAtHead(processQueue* queue);


#endif USLOSS_QUEUE_H
