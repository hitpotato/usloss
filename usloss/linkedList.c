#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "src/globals.h"

typedef struct processNode {
    unsigned int pid;
    int priority;
    struct processNode *next;
} Node;

typedef struct list {
    Node *first;
} processPriorityQueue;

/* -------------------------- Function Prototypes ---------------------------------- */

Node *createNewNode();
void deleteNode(Node *node);
void deleteNodeWithPID(processPriorityQueue *list, unsigned int pid);

processPriorityQueue *initializeList();
void freeList(processPriorityQueue *list);
void insertNodeIntoList(processPriorityQueue *list, unsigned int pid, int priority);
Node *getNode(processPriorityQueue *list, int element);
Node *lookAtFirstElement(processPriorityQueue *list);
Node *popFromList(processPriorityQueue *list);
Node *find_status(processPriorityQueue *list, unsigned int status);
Node *find_pid(processPriorityQueue *list, unsigned int pid);
void printList(processPriorityQueue *list);


/* -------------------------- Functions ------------------------------------- */
Node *createNewNode(){
    Node *temp = malloc(sizeof(Node));
    temp->next=NULL;
    return temp;
}

void deleteNode(Node *node){
    free(node);
}

void deleteNodeWithPID(processPriorityQueue *list, unsigned int pid){
    Node *node = list->first;

    //If the head element of the list is the one we want to remove
    if(node->pid == pid){
        list->first = node->next;
        free(node);
        return;
    }

    //Otherwise, iterate through the list
    while(node != NULL){
        if(node->next == NULL)
            break;
        if(node->next->pid == pid){
            Node *temp = node->next;
            node->next = node->next->next;
            free(temp);
            return;
        }
        node = node->next;
    }
}

processPriorityQueue *initializeList() {
    processPriorityQueue *new_list = malloc(sizeof(processPriorityQueue));
    //Node *first = createNewNode();
    //new_list->first = first;
    new_list->first = NULL;
    return new_list;
}

void freeList(processPriorityQueue *list){
    Node *node = list->first;
    Node *next;
    while(node) {
        next = node->next;
        deleteNode(node);
        node=next;
    }
    free(list);
}



/*
 * Duplicate the action a queue takes when an element is popped.
 * Remove and return the first element in the linked list
 */
Node *popFromList(processPriorityQueue *list){
    Node *temp = list->first;

    if(temp->next == NULL){
        Node *new_first = createNewNode();
        list->first = new_first;
    }
    else {
        Node *new_first = temp->next;
        list->first = new_first;
    }

    //Now return the head element of the list
    return temp;
}

void insertNodeIntoList(processPriorityQueue *list, unsigned int pid, int priority){

    Node *temp;
    Node *node;

    //Create a temp processNode with a pid and priority the same
    //as the one that was passed in to us
    temp = createNewNode();
    temp->pid = pid;
    temp->priority = priority;

    //Because we have multiple lists, with predefined priority
    //levels, we don't have to worry about placing something
    //in the proper location. Simply place at the end


    //If the list was empty at first, simpyl set its first element
    //to the new processNode
    if(list->first == NULL) {
        // temp->next = list->first; # No need for this, createNewNode() sets its next to null already
        list->first = temp;
        printf("\n");
        printf("Added node with PID of %d to beginning\n", list->first->pid);
        printf("\n");
        return;
    }

    node = list->first;
    printf("Setting head to a node with a PID of %d\n", node->pid);
    //Otherwise, iterate to the end
    while(node->next != NULL){
        node = node->next;
        printf("Node is now the node with a PID of: %d\n", node->pid);
    }

    //Now, set the last processNode to our created temp processNode
    printf("Inserted a node with PID: %d\t Priority: %d\n", temp->pid, temp->priority);
    printf("\n");
    node->next = temp;
}

// Simple function that returns the first element in the list
Node *lookAtFirstElement(processPriorityQueue *list){
    return list->first;
}

Node *getNode(processPriorityQueue *list, int nodeNumber){
    Node *node = list->first;

    int i = 0;
    while(i < nodeNumber && node != NULL){
        node = node->next;
        i++;
    }

    return node;
}

Node *find_pid(processPriorityQueue *list, unsigned int pid){
    Node *node = list->first;

    while(node->next != NULL){
        if(node->pid == pid)
            return node;
        else
            node = node->next;
    }

    return NULL;
}

void printList(processPriorityQueue *list){
    Node *node = list->first;
    //printf("PID: %d\t Priority: %d\n", node->pid, node->priority);
    int length = 0;

    if(node == NULL){
        printf("List is empty\n");
        return;
    }

    while(node != NULL){
        printf("PID: %d\t Priority: %d\n", node->pid, node->priority);
        node = node->next;
        length++;
    }

    printf("The total length of the list was: %d elements\n", length);
}

int main(){

    processPriorityQueue *priorityList_1 = initializeList();
    processPriorityQueue *priorityList_2 = initializeList();


    insertNodeIntoList(priorityList_1, 1, 1);
    insertNodeIntoList(priorityList_1, 2, 1);
    insertNodeIntoList(priorityList_1, 3, 1);
    insertNodeIntoList(priorityList_1, 4, 1);
    insertNodeIntoList(priorityList_1, 5, 1);



    //Node *temp = lookAtFirstElement(priorityList_1);
    //printf("First processNode has pid of %d and priority of %d\n", temp->pid, temp->priority);

    printList(priorityList_1);
    printList(priorityList_2);
}





