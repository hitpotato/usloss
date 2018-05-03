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


/*----------------------------------------------------------------------
 * p1_fork
 *
 * Creates a page table for a process, if the virtual memory system
 * has been initialized.
 *
 * Results: None
 *
 * Side effects: pageTable malloced and initialized for process
 *----------------------------------------------------------------------*/
void p1_fork(int pid)
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
        for (int i = 0; i < proc->numPages; i++) {
            clearPage(&proc->pageTable[i]);
        }
        if (debug5)
            USLOSS_Console("p1_fork(): done \n");
    }

} /* p1_fork */


/*----------------------------------------------------------------------
 * p1_switch
 *
 * Unmaps and Maps pages to frames in the MMU for processes that are context
 * switching. A process must be set as a virtual memory process by the fault
 * handler in phase5.c to unmap and map.
 *
 * Results: None
 *
 * Side effects: mapping are added or removed from the MMU
 *----------------------------------------------------------------------*/
void p1_switch(int old, int new)
{

    if (vmRegion == NULL) return;

    vmStats.switches++;     // Increase the number of switches

    // Clear the old processes mappings
    if (old > 0) {
        int dummy;
        int result;
        Process *oldProc = &processes[old % MAXPROC];

        // If the old process had a page table that contained information
        if (oldProc->pageTable != NULL) {
            for (int i = 0; i < oldProc->numPages; i++) {
                result = USLOSS_MmuGetMap(TAG, i, &dummy, &dummy);     // check if there is a valid mapping
                if (result != USLOSS_MMU_ERR_NOMAP) {
                    USLOSS_MmuUnmap(TAG, i);    // Unmap if a mapping existed
                    if (debug5)
                        USLOSS_Console("p1_switch(): unmapped page %d for proc %d \n", i, old);
                }
            }
        }
    }

    // Map the pages for the new process
    if (new > 0) {
        Process *newProc = &processes[new % MAXPROC];
        if (newProc->pageTable != NULL) {
            for (int i = 0; i < newProc->numPages; i++) {
                if (newProc->pageTable[i].state == INFRAME) { // Check to see if the mapping is valid
                    USLOSS_MmuMap(TAG, i, newProc->pageTable[i].frame, USLOSS_MMU_PROT_RW);
                    if (debug5)
                        USLOSS_Console("p1_switch(): mapped page %d to frame %d for proc %d \n", i, newProc->pageTable[i].frame, new);
                }
            }
        }
    }

} /* p1_switch */

/*----------------------------------------------------------------------
 * p1_quit
 *
 * Removes mappings to the MMU for processes that are quiting.
 *
 * Results: None
 *
 * Side effects: mappings are removed from the MMU
 *----------------------------------------------------------------------*/
void p1_quit(int pid)
{
    int frame, dummy, result;

    if (debug5)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);

    if (vmRegion > 0) {
        // clear the page table
        Process *proc = &processes[pid % MAXPROC];
        if (proc->pageTable == NULL)
            return;
        for (int i = 0; i < proc->numPages; i++) {
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