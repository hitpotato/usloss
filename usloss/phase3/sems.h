
#ifndef STRUCTURES_H
#define STRUCTURES_H

typedef struct procStruct3          procStruct3;
typedef struct procStruct3          * procPtr3;
typedef struct processQueue         processQueue;
typedef struct semaphore            semaphore;

#define BLOCKED 0
#define CHILDREN 1

struct processQueue {
    procPtr3        head;
    procPtr3        tail;
    int 	        size;
    int 	        type;
};

struct procStruct3 {
    int                 pid;
    int 		        mboxID;         /* 0 slot mailbox belonging to this process */
    int (* startFunc)   (char *);       /* function where process begins */
    procPtr3     	    nextProcPtr;
    procPtr3            nextSiblingPtr;
    procPtr3            parentPtr;
    processQueue 		childrenQueue;
};

struct semaphore {
    int 		        id;
    int 		        value;
    int 		        startingValue;
    processQueue        blockedProcesses;
    int 		        private_mBoxID;
    int 		        mutex_mBoxID;
};

#endif /* STRUCTURES */