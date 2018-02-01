/* Patrick's DEBUG printing constant... */
#define DEBUG 0

typedef struct procStruct procStruct;

typedef struct procStruct * procPtr;

typedef struct processQueue processQueue;

struct processQueue {
	procPtr headProcess;
	procPtr tailProcess;
	int 	length;
	int 	typeOfQueue;
};

/* Process struct */
struct procStruct {
	procPtr         nextProcPtr;
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
	procPtr         parentPtr;
	procPtr 		nextDeadSiblingPtr;
	procPtr			nextZapPtr;
	processQueue 	childrenQueue;      /* queue of the process's children */
	processQueue	deadChildrenQueue;	/* list of children who have quit in the order they have quit*/
	processQueue	zapQueue;
 	int 			quitStatus;		    /* whatever the process returns when it quits */
	int				zapStatus;          // 1 zapped; 0 not zapped
	int 			timeInitialized;    // the time the current time slice started
	int 			cpuTime;            // the total amount of time the process has been running
	int 			sliceTime;          // how long the process has been running in the current time slice
};

/* Some useful constants.  Add more as needed... */
#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY (MINPRIORITY + 1)

// Constants to determine what kind of readylist was used
#define READYLIST 0
#define CHILDRENLIST 1
#define DEADCHILDRENLIST 2
#define ZAPLIST 3

#define TIMESLICE 80000

/* process statuses */
#define EMPTY 0
#define READY 1
#define RUNNING 2
#define TIMESLICED 3
#define QUIT 4
#define JOINBLOCKED 5
#define ZAPBLOCKED 6

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
   Name -           initializeProcessQueue
   Purpose -        Just initialize a ready list
   Parameters -
                    processQueue* queue:
                        Pointer to queue to be initialized
                    queueType
                        An int specifying what the queue is to be used for
   Returns -        Nothing
   Side Effects -   The passed queue is now no longer NULL
   ------------------------------------------------------------------------ */
void initializeProcessQueue(processQueue* queue, int queueType) {
	queue->headProcess = NULL;
	queue->tailProcess = NULL;
	queue->length = 0;
	queue->typeOfQueue = queueType;
}

/* ------------------------------------------------------------------------
   Name -           appendProcessToQueue
   Purpose -        Adds a process to the end of a queue
   Parameters -
                    processQueue* queue:
                        Pointer to queue to be added to
                    procPty process
                        Pointer to the process that will be added
   Returns -        Nothing
   Side Effects -   The length of the queue increases by 1.
   ------------------------------------------------------------------------ */
void appendProcessToQueue(processQueue *queue, procPtr process) {

	if (queue->headProcess == NULL && queue->tailProcess == NULL) {
		queue->headProcess = queue->tailProcess = process;
	}

    else {
		if (queue->typeOfQueue == READYLIST)
			queue->tailProcess->nextProcPtr = process;
		else if (queue->typeOfQueue == CHILDRENLIST)
			queue->tailProcess->nextSiblingPtr = process;
		else if (queue->typeOfQueue == ZAPLIST)
			queue->tailProcess->nextZapPtr = process;
		else
			queue->tailProcess->nextDeadSiblingPtr = process;
		queue->tailProcess = process;
	}

	queue->length++;
}

/* ------------------------------------------------------------------------
   Name -           popFromQueue
   Purpose -        Remove and return the first element from the queue
   Parameters -
                    processQueue *queue
                        A pointer to the queue who we want to modify
   Returns -
                    procPtr:
                        Pointer to the element that we want
   Side Effects -   Length of queue is decreased by 1
   ------------------------------------------------------------------------ */
procPtr popFromQueue(processQueue *queue) {

	procPtr temp = queue->headProcess;

	// If there is no head element, return null
	if (queue->headProcess == NULL) {
		return NULL;
	}

	//  If the list is of size 1, set the first and last nodes
	// 		equal to null to ensure that the list is represented as empty
	if (queue->headProcess == queue->tailProcess) {
		queue->headProcess = queue->tailProcess = NULL;
	}

	else {
		if (queue->typeOfQueue == READYLIST)
			queue->headProcess = queue->headProcess->nextProcPtr;
		else if (queue->typeOfQueue == CHILDRENLIST)
			queue->headProcess = queue->headProcess->nextSiblingPtr;
		else if (queue->typeOfQueue == ZAPLIST)
			queue->headProcess = queue->headProcess->nextZapPtr;
		else 
			queue->headProcess = queue->headProcess->nextDeadSiblingPtr;
	}

	// Decrease the length of the list  and return the head element
	queue->length--;
	return temp;
}

/* ------------------------------------------------------------------------
   Name -           removeChildFromQueue
   Purpose -        Removes a specified node/child from the queue of children
   Parameters -
                    processQueue* queue:
                        Pointer to queue to be modified
                    procPty process
                        Pointer to the process that will be removed
   Returns -        Nothing
   Side Effects -   Length of queue decreases by 1
   ------------------------------------------------------------------------ */
void removeChildFromQueue(processQueue *queue, procPtr childProcess) {

    // If the given queue is NULL or is not of type CHILDRENLIST, return
    //      without doing anything
    if (queue->headProcess == NULL || queue->typeOfQueue != CHILDRENLIST)
		return;

    // If the very first element is the element we want, simply pop it
	if (queue->headProcess == childProcess) {
        popFromQueue(queue);
		return;
	}

    // Create pointers that will be used to iterate from the list
	procPtr temp = queue->headProcess;
	procPtr tempsNextSibling = queue->headProcess->nextSiblingPtr;

    // Iterate through the list while not null
	while (tempsNextSibling != NULL) {
        // If the nextSibling is the child we want to remove
		if (tempsNextSibling == childProcess) {
			if (tempsNextSibling == queue->tailProcess)
				queue->tailProcess = temp;
			else
				temp->nextSiblingPtr = tempsNextSibling->nextSiblingPtr->nextSiblingPtr;
			queue->length--;
		}
		temp = tempsNextSibling;
		tempsNextSibling = tempsNextSibling->nextSiblingPtr;
	}
}

/* ------------------------------------------------------------------------
   Name -           peekAtHead
   Purpose -        Returns a pointer to the first element in queue
   Parameters -
                    processQueue *queue
                        The queue we want to look at the first element of
   Returns -        A pointer to the head of the given queue
   Side Effects -   None
   ------------------------------------------------------------------------ */
/* Return the headProcess of the given queue. */
procPtr peekAtHead(processQueue *queue) {

    // Check to make sure that the list is not empty
    if (queue->headProcess == NULL) {
		return NULL;
	}

	return queue->headProcess;
}
