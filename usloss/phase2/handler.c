#include <stdio.h>
#include <phase1.h>
#include <phase2.h>
#include "message.h"
#include "usyscall.h"


extern int debugflag2;
extern void disableInterrupts(void);
extern void enableInterrupts(void);
extern void makeSureCurrentFunctionIsInKernelMode(char *);
extern int debugEnabled();

#define CLOCKBOX 0
#define DISKBOX 1
#define TERMBOX 3

int IOmailboxes[7];     // mboxIDs for the IO devices
int IOblocked = 0;      // number of processes blocked on IO mailboxes

// Calls recieve on the mailbox associated with the given unit of the device type
extern int waitDevice(int type, int unit, int *status){

    disableInterrupts();
    makeSureCurrentFunctionIsInKernelMode("waitDevice()");



    int box = -10;
    if(type == USLOSS_CLOCK_DEV)
        box = CLOCKBOX;
    else if(type == USLOSS_DISK_DEV)
        box = DISKBOX;
    else if(type == USLOSS_TERM_DEV)
        box = TERMBOX;
    else{
        USLOSS_Console("waitDevice(): Invalid device type: %d. Halting...\n", type);
        USLOSS_Halt(1);
    }
    

    IOblocked++;
    MboxReceive(IOmailboxes[box+unit], status, sizeof(int));
    IOblocked--;

    enableInterrupts(); // re-enable interrupts
    if(isZapped())
        return -1;

    return 0;
}

/* an error method to handle invalid syscalls */
void nullsys(USLOSS_Sysargs *args)
{
    USLOSS_Console("nullsys(): Invalid syscall %d. Halting...\n", args->number);
    USLOSS_Halt(1);
} /* nullsys */


void clockHandler2(int dev, void *arg)
{

    disableInterrupts();
    makeSureCurrentFunctionIsInKernelMode("clockHandler2()");
   if (DEBUG2 && debugflag2)
      USLOSS_Console("clockHandler2(): called\n");

    // Make sure this is the clock device
    if(dev != USLOSS_CLOCK_DEV){
        if(debugEnabled())
            USLOSS_Console("clockHandler2(): Called by other device, returning\n");
        return;
    }

    // Send message at intervals of 5
    int whatever;
    static int count = 0;
    count++;
    if (count == 5){
        int status;
        whatever = USLOSS_DeviceInput(dev, 0, &status);    // Get the status
        MboxCondSend(IOmailboxes[CLOCKBOX], &status, sizeof(int));
        count = 0;      // Reset the count
    }

    timeSlice();
    enableInterrupts();


} /* clockHandler */


void diskHandler(int dev, void *arg)
{

    disableInterrupts();
    makeSureCurrentFunctionIsInKernelMode("diskHandler()");
    if(debugEnabled())
        USLOSS_Console("diskHandler(): called\n");

    // Make sure this is the disk device
    if(dev != USLOSS_DISK_DEV){
        if(debugEnabled())
            USLOSS_Console("diskHandler(): Called by other device, returning\n");
        return;
    }

    // Get the device status
    long unit = (long)arg;
    int status;
    int valid = USLOSS_DeviceInput(dev, unit, &status);

    // Make sure the unit number was valid
    if(valid == USLOSS_DEV_INVALID){
        if(debugEnabled())
            USLOSS_Console("diskHandler(): Unit number invalid, returning\n");
        return;
    }

    // Send to the devices mailbox
    MboxCondSend(IOmailboxes[DISKBOX+unit], &status, sizeof(int));
    enableInterrupts();

} /* diskHandler */


void termHandler(int dev, void *arg)
{

    disableInterrupts();
    makeSureCurrentFunctionIsInKernelMode("termHandler()");
   if (DEBUG2 && debugflag2)
      USLOSS_Console("termHandler(): called\n");

    // Make sure this is the terminal device
    if(dev != USLOSS_TERM_DEV){
        if(debugEnabled())
            USLOSS_Console("termHandler(): Called by other device, returning\n");
        return;
    }

    long unit = (long)arg;
    int status;
    int valid = USLOSS_DeviceInput(dev, unit, &status);

    // Make sure the unit number is valid
    if( valid == USLOSS_DEV_INVALID){
        if(debugEnabled())
            USLOSS_Console("termHandler(): Unit number invalid, returning\n");
        return;
    }

    MboxCondSend(IOmailboxes[TERMBOX+unit], &status, sizeof(int));
    enableInterrupts();

} /* termHandler */


void syscallHandler(int dev, void *arg)
{

    disableInterrupts();
    makeSureCurrentFunctionIsInKernelMode("syscallHandler()");
   if (DEBUG2 && debugflag2)
      USLOSS_Console("syscallHandler(): called\n");

    // Make Sure this is the system call device
    if(dev != USLOSS_SYSCALL_INT){
        if(debugEnabled())
            USLOSS_Console("syscallHandler(): Called by other device.Returning\n");
        return;
    }

    USLOSS_Sysargs *sysptr = (USLOSS_Sysargs*) arg;

    // Check for correct system call number
    if(sysptr->number < 0 || sysptr->number >= MAXSYSCALLS){
        USLOSS_Console("syscallHandler(): sys number %d is wrong.  Halting...\n", sysptr->number);
        USLOSS_Halt(1);
    }

    nullsys((USLOSS_Sysargs*)arg);
    enableInterrupts();
} /* syscallHandler */

/* ------------------------------------------------------------------------
   Name - check_io
   Purpose - Determine if there any processes blocked on any of the
             interrupt mailboxes.
   Returns - 1 if one (or more) processes are blocked; 0 otherwise
   Side Effects - none.

   Note: Do nothing with this function until you have successfully completed
   work on the interrupt handlers and their associated mailboxes.
   ------------------------------------------------------------------------ */
int check_io(void)
{
    if (debugEnabled())
        USLOSS_Console("check_io(): called\n");

    if(IOblocked > 0)
        return 1;

    return 0;
} /* check_io */
