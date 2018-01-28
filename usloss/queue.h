//
// Created by Rodrigo Silva Mendoza on 1/27/18.
//

#ifndef USLOSS_LINKEDLIST_H
#define USLOSS_LINKEDLIST_H

#include <stdbool.h>
#include <jmorecfg.h>

typedef struct processNode {
    unsigned int pid;
    int priority;
    struct processNode *next;
} Node;

typedef struct list {
    Node *first;
} processPriorityQueue;

/* -------------------------- Function Prototypes ---------------------------------- */

extern Node *createNewNode();
extern void deleteNode(Node *node);
extern void deleteNodeWithPID(processPriorityQueue *queue, unsigned int pid);

extern processPriorityQueue *initializeQueue();
extern void freeQueue(processPriorityQueue *queue);
extern void insertNodeIntoQueue(processPriorityQueue *queue, unsigned int pid, int priority);
extern Node *getNode(processPriorityQueue *queue, int element);
extern Node *lookAtFirstElement(processPriorityQueue *queue);
extern Node *popFromQueue(processPriorityQueue *queue);
extern Node *find_pid(processPriorityQueue *queue, unsigned int pid);
extern void printQueue(processPriorityQueue *queue);
extern bool isEmpty(processPriorityQueue *queue);

#endif //USLOSS_LINKEDLIST_H
