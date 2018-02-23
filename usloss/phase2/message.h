
#ifndef MESSAGE_H
#define MESSAGE_H

#include "phase2.h"
#include <stdlib.h>


#define DEBUG2 1

#define INACTIVE 0
#define ACTIVE 1

#define EMPTY 0
#define USED  1

#define FULL_BOX 11
#define NO_MESSAGES 12

#define CLOCKBOX 0
#define DISKBOX 1
#define TERMBOX 3

#define SLOTQUEUE 0
#define PROCQUEUE 1

typedef struct mailSlot     *slotPtr;
typedef struct mailbox      mailbox;
typedef struct mboxProc     *mboxProcPtr;
typedef struct mailSlot     mailSlot;
typedef struct mboxProc     mboxProc;
typedef struct processQueue processQueue;

int IOmailboxes[7];     // mboxIDs for the IO devices
int IOblocked = 0;      // number of processes blocked on IO mailboxes

struct mboxProc {
    mboxProcPtr     nextMboxProc;
    int             pid;
    void            *msg_ptr;   // Where to place recieved message
    int             msg_length;
    slotPtr         messageRecieved;    // Slot containing message
};


struct processQueue {
    void    *headProcess;
    void    *tailProcess;
    int     length;
    int     typeOfQueue;
};


struct mailbox {
    int         mboxID;
    // other items as needed...
    int         status;
    int         totalSlots;
    int         slotSize;
    processQueue       slots;
    processQueue       blockedProcsSend;
    processQueue       blockedProcsRecieve;
};

struct mailSlot {
    int         mboxID;
    int         status;
    // other items as needed...
    int         slotID;
    slotPtr     nextSlotPtr;
    char        message[MAX_MESSAGE];
    int         messageSize;
};


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


// Functions used by both handler.c and phase2.c
extern void disableInterrupts(void);
extern void enableInterrupts(void);
extern void makeSureCurrentFunctionIsInKernelMode(char *);
extern int debugEnabled();

//  Queue Functions
extern void initializeQueue(processQueue *q, int queueType);
extern void appendProcessToQueue(processQueue *queue, void* process);
extern void* popFromQueue(processQueue *queue);
extern void* peekAtHead(processQueue *queue);

// Handler Functions
extern void nullsys(USLOSS_Sysargs *args);
extern void clockHandler2(int dev, void *arg);
extern void diskHandler(int dev, void *arg);
extern void termHandler(int dev, void *arg);
extern void syscallHandler(int dev, void *arg);
extern int check_io(void);

#endif /* MESSAGE_H */



