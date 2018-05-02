/*
 * vm.h
 */

#ifndef _VM_H
#define _VM_H
/*
 * All processes use the same tag.
 */
#define TAG 0

/*
 * Different states for a page.
 */
#define UNUSED 500
#define INCORE 501
#define INFRAME 502
#define USED 503
#define SWAPDISK 1
/* You'll probably want more states */


/*
 * Page table entry.
 */
typedef struct PTE {
    int  state;      // See above.
    int  frame;      // Frame that stores the page (if any). -1 if none.
    int  diskBlock;  // Disk block that stores the page (if any). -1 if none.
    // Add more stuff here
} PTE;

/*
 * Frame table entry
 */
// Create a struct for your frame table:
typedef struct FTE {
    int pid;        // pid of process using the frame, -1 if none
    int state;      // whether it is free/in use
    int page;       // the page using this frame
} FTE;

/*
 * Disk table entry
 */
typedef struct DTE {
    int pid;        // pid of process using this disk block, -1 if none
    int page;      // the page using this disk block
    int track;      // what track the page is on
    int sector;     // sector it starts on
} DTE;
/*
 * Per-process information.
 */
typedef struct Process {
    int  numPages;   // Size of the page table.
    PTE  *pageTable; // The page table for the process.
    // Add more stuff here */
    int pid;
} Process;

/*
 * Information about page faults. This message is sent by the faulting
 * process to the pager to request that the fault be handled.
 */
typedef struct FaultMsg {
    int  pid;        // Process with the problem.
    void *addr;      // Address that caused the fault.
    int  replyMbox;  // Mailbox to send reply.
    // Add more stuff here.
    int pageNum;    // The page the fault happened on
} FaultMsg;

#define CheckMode() assert(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)

#endif /* _PHASE4_H */

