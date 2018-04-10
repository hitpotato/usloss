

#ifndef USLOSS_DRIVER_H
#define USLOSS_DRIVER_H

typedef struct procStruct procStruct;
typedef struct procStruct * procPtr;
typedef struct diskQueue diskQueue;


struct diskQueue {
    procPtr head;
    procPtr tail;
    procPtr curr;
    int     size;
    int     type;
};

// Heap
typedef struct heap heap;
struct heap {
    int size;
    procPtr procs[MAXPROC];
};

struct procStruct {
    int             pid;
    int 		    mboxID;
    int             blockSem;
    int		        wakeTime;
    int 		    diskTrack;
    int 		    diskFirstSec;
    int 		    diskSectors;
    void 		    *diskBuffer;
    procPtr 	    prevDiskPtr;
    procPtr 	    nextDiskPtr;
    USLOSS_DeviceRequest diskRequest;
};

#endif //USLOSS_DRIVER_H
