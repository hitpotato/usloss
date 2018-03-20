
#ifndef STRUCTURES_H
#define STRUCTURES_H

typedef struct procStruct3          procStruct3;
typedef struct procStruct3          * procPtr3;
typedef struct processQueue         processQueue;
typedef struct semaphore            semaphore;

#define BLOCKED 0
#define CHILDREN 1

// Queue for processes
struct processQueue {
    procPtr3        head;
    procPtr3        tail;
    int 	        size;
    int 	        type;
};

// Struct for the processes
struct procStruct3 {
    int                 pid;
    int 		        mboxID;
    int (* startFunc)   (char *);
    procPtr3     	    nextProcPtr;
    procPtr3            nextSiblingPtr;
    procPtr3            parentPtr;
    processQueue 		childrenQueue;
};

// Struct to represent a semaphore
struct semaphore {
    int 		        id;
    int 		        value;
    int 		        startingValue;
    processQueue        blockedProcesses;
    int 		        private_mBoxID;
    int 		        mutex_mBoxID;
};

#endif /* STRUCTURES */