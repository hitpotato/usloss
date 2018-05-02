#include "usloss.h"
#include <vm.h>
#include <stdlib.h>
#include <phase5.h>

#include "vm.h"
#include "phase1.h"
#include "phase5.h"
#include "../phase1/usloss.h"

#define TAG 0

extern int debug5;
extern Process processes[MAXPROC];
extern FTE *frameTable;
extern DTE *diskTable;
extern void *vmRegion;
extern VmStats vmStats;


/* Fills the given PTE with default values */
void clearPage(PTE *page) {
    page->state = UNUSED;
    page->frame = -1;
    page->diskBlock = -1;
}


void
p1_fork(int pid)
{
    if (debug5)
        USLOSS_Console("p1_fork() called: pid = %d\n", pid);

    if (vmRegion > 0) {
        Process *proc = &processes[pid % MAXPROC];
        proc->pid = pid;

        if (debug5)
            USLOSS_Console("p1_fork(): creating page table with %d pages\n", proc->numPages);

        // create the process's page table
        proc->pageTable = malloc( proc->numPages * sizeof(PTE));
        if (debug5)
            USLOSS_Console("p1_fork(): malloced page table, clearing pages... \n");
        int i;
        for (i = 0; i < proc->numPages; i++) {
            clearPage(&proc->pageTable[i]);
        }
        if (debug5)
            USLOSS_Console("p1_fork(): done \n");
    }

} /* p1_fork */


void
p1_switch(int old, int new)
{
    // if (debug5)
    //     USLOSS_Console("p1_switch() called: old = %d, new = %d\n", old, new);

    if (vmRegion == NULL)
        return;

    vmStats.switches++;
    int i;

    // unload old process's mappings
    if (old > 0) {
        int dummy, dummy2, result; // used to check mappings
        Process *oldProc = &processes[old % MAXPROC];
        if (oldProc->pageTable != NULL) {
            for (i = 0; i < oldProc->numPages; i++) {
                // check if there is a valid mapping
                result = USLOSS_MmuGetMap(TAG, i, &dummy, &dummy2);
                if (result != USLOSS_MMU_ERR_NOMAP) {
                    USLOSS_MmuUnmap(TAG, i);
                    if (debug5)
                        USLOSS_Console("p1_switch(): unmapped page %d for proc %d \n", i, old);
                }
            }
        }
    }

    // map new process's pages
    if (new > 0) {
        Process *newProc = &processes[new % MAXPROC];
        if (newProc->pageTable != NULL) {
            for (i = 0; i < newProc->numPages; i++) {
                if (newProc->pageTable[i].state == INFRAME) { // check if there is a valid mapping
                    USLOSS_MmuMap(TAG, i, newProc->pageTable[i].frame, USLOSS_MMU_PROT_RW);
                    if (debug5)
                        USLOSS_Console("p1_switch(): mapped page %d to frame %d for proc %d \n", i, newProc->pageTable[i].frame, new);
                }
            }
        }
    }

} /* p1_switch */

void p1_quit(int pid)
{
    int i, frame, dummy, result;

    if (debug5)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);

    if (vmRegion > 0) {
        // clear the page table
        Process *proc = &processes[pid % MAXPROC];
        if (proc->pageTable == NULL)
            return;
        for (i = 0; i < proc->numPages; i++) {
            // free the frames
            result = USLOSS_MmuGetMap(TAG, i, &frame, &dummy);
            if (result != USLOSS_MMU_ERR_NOMAP) {
                // free the disk block
                if (proc->pageTable[i].diskBlock > -1) {
                    diskTable[proc->pageTable[i].diskBlock].pid = -1;
                    diskTable[proc->pageTable[i].diskBlock].page = -1;
                    //vmStats.freeDiskBlocks++;
                }

                USLOSS_MmuUnmap(TAG, i); // unmap

                // free the frame
                frameTable[frame].pid = -1;
                frameTable[frame].state = UNUSED;
                frameTable[frame].page = -1;
                vmStats.freeFrames++;
                if (debug5)
                    USLOSS_Console("p1_quit(): freed frame %d, free frames = %d \n", frame, vmStats.freeFrames);
            }

            clearPage(&proc->pageTable[i]);
        }

        if (debug5)
            USLOSS_Console("p1_quit(): cleared pages \n");

        // destroy the page table
        free(proc->pageTable);

        if (debug5)
            USLOSS_Console("p1_quit(): freed page table \n");
    }
} /* p1_quit */