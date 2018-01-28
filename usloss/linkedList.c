#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

typedef struct node {
    unsigned int pid;
    int priority;
    struct node *next;
} Node;

typedef struct list {
    Node *first;
} List;

Node *create_node();
void delete_node(Node *node);
void remove_node(List *list, unsigned int pid);

List *create_list();
void delete_list(List *list);


void append_list(List *list, unsigned int pid, int priority);
void insert(List *list, unsigned int pid, int priority);
Node *getNode(List *list, int element);
Node *peek(List *list);
Node *pop(List* list);
Node *find_status(List *list, unsigned int status);
Node *find_pid(List *list, unsigned int pid);
void print_list(List *list);


List *create_list() {
    List *new_list = malloc(sizeof(List));
    Node *first = create_node();
    new_list->first = first;
    return new_list;
}

void delete_list(List *list){
    Node *node = list->first;
    Node *next;
    while(node) {
        next = node->next;
        delete_node(node);
        node=next;
    }
    free(list);
}

Node *create_node(){
    Node *temp = malloc(sizeof(Node));
    temp->next=NULL;
    return temp;
}

void delete_node(Node *node){
    free(node);
}

void remove_node(List *list, unsigned int pid){
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

/*
 * Duplicate the action a queue takes when an element is popped.
 * Remove and return the first element in the linked list
 */
Node *pop(List *list){
    Node *temp = list->first;

    if(temp->next == NULL){
        Node *new_first = create_node();
        list->first = new_first;
    }
    else {
        Node *new_first = temp->next;
        list->first = new_first;
    }

    //Now return the head element of the list
    return temp;
}

void insert(List *list, unsigned int pid, int priority){
    Node *temp;
    Node *node;

    //Create a temp node with a pid and priority the same
    //as the one that was passed in to us
    temp = create_node();
    temp->pid = pid;
    temp->priority = priority;

    //Because we have multiple lists, with predefined priority
    //levels, we don't have to worry about placing something
    //in the proper location. Simply place at the end
    node = list->first;

    //If the list was empty at first, simpyl set its first element
    //to the new node
    if(node == NULL) {
        list->first = temp;
        return;
    }

    //Otherwise, iterate to the end
    while(node->next != NULL){
        node = node->next;
    }

    //Now, set the last node to our created temp node
    node->next = temp;
}

// Simple function that returns the first element in the list
Node *lookAtFirstElement(List *list){
    return list->first;
}

Node *getNode(List *list, int nodeNumber){
    Node *node = list->first;

    int i = 0;
    while(i < nodeNumber && node != NULL){
        node = node->next;
        i++;
    }

    return node;
}

Node *find_pid(List *list, unsigned int pid){
    Node *node = list->first;

    while(node->next != NULL){
        if(node->pid == pid)
            return node;
        else
            node = node->next;
    }

    return NULL;
}

void print_list(List *list){
    Node *node = list->first;

    int length = 0;

    while(node->next != NULL){
        printf("PID: %d\t Priority: %d\n", node->pid, node->priority);
        node = node->next;
        length++;
    }

    printf("The total length of the list was: %d elements\n", length);
}

int main(){
    List *priorityList_1 = create_list();


    insert(priorityList_1, 1, 1);
    insert(priorityList_1, 2, 1);
    insert(priorityList_1, 3, 1);
    insert(priorityList_1, 4, 1);
    insert(priorityList_1, 5, 1);

    Node *temp = lookAtFirstElement(priorityList_1);
    printf("First node has pid of %d and priority of %d\n", temp->pid, temp->priority);

    print_list(priorityList_1);
}





