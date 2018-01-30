/* Patrick's DEBUG printing constant... */
#include "queue.h"
#include "usloss.h"
#include "phase1.h"

#define DEBUG 0

typedef struct procStruct procStruct;

typedef struct procStruct * procPtr;

struct procStruct {
    procPtr         nextProcPtr;
    procPtr         childProcPtr;
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
    procPtr         quitChildPtr;
    procPtr         nextZapPtr;
    procPtr         nextDeadSibling;
    short           parentPID;         /* parent process id */
    int             numberOfChildren;   /* The number of children this process has */
    int             zapStatus;            /* 0 if not zapped. 1 if zapped */
    processQueue    childrenQueue;
    processQueue    deadChildrenQueue;
    processQueue    zappedProcessesQueue;
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

/* Some useful constants.  Add more as needed... */
#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY (MINPRIORITY + 1)

#define QUIT 0
#define READY 1
#define BLOCKED = -1
#define EMPTY -2

#define NOPARENTPROCESS -1


