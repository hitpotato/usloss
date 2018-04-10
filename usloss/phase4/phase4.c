/* ------------------------------------------------------------------------
   phase4.c

   University of Arizona
   Computer Science 452

   Rodrigo Silva Mendoza
   Long Chen
   ------------------------------------------------------------------------ */

#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <stdlib.h> /* needed for atoi() */
#include <stdio.h>
#include <memory.h>
#include <string.h> /* needed for memcpy() */


#include "providedPrototypes.h"
#include "../src/usloss.h"
#include "phase1.h"
#include "phase2.h"
#include "../phase2/usyscall.h"
#include "driver.h"


// -------------------------- Function Prototypes ----------//
static int ClockDriver(char *);
static int DiskDriver(char *);
static int TermDriver(char *);
static int TermReader(char *);
static int TermWriter(char *);
extern int start4();

void sleep(USLOSS_Sysargs *);
void diskRead(USLOSS_Sysargs *);
void diskWrite(USLOSS_Sysargs *);
void diskSize(USLOSS_Sysargs *);
void termRead(USLOSS_Sysargs *);
void termWrite(USLOSS_Sysargs *);

int sleepReal(int);
int diskSizeReal(int, int*, int*, int*);
int diskWriteReal(int, int, int, int, void *);
int diskReadReal(int, int, int, int, void *);
int diskReadOrWriteReal(int, int, int, int, void *, int);
int termReadReal(int, int, char *);
int termWriteReal(int, int, char *);

void makeSureCurrentFunctionIsInKernelMode(char *);
void initializeProcessSlot(int);
void switchToUserMode();

void initializeDiskQueue(diskQueue *);
void addToDiskQueue(diskQueue *, procPtr);
procPtr peekAtDiskQueue(diskQueue *);
procPtr removeFromDiskQueue(diskQueue *);

void initializeHeap(heap *);
void appendToHeap(heap *, procPtr);
procPtr peekAtHeap(heap *);
procPtr popFromHeap(heap *);


// ---------------------------------- Globals ----------//
int debug4 = 0;
int running;
int diskZapped;                             // indicates if the disk drivers are 'zapped' or not
int diskPids[USLOSS_DISK_UNITS];            // pids of the disk drivers

procStruct ProcTable[MAXPROC];
heap sleepinProcsHeap;
diskQueue diskQs[USLOSS_DISK_UNITS];        // queues for disk drivers
int termProcTable[USLOSS_TERM_UNITS][3];    // keep track of term procs

int charRecvMbox[USLOSS_TERM_UNITS];        // receive char
int charSendMbox[USLOSS_TERM_UNITS];        // send char
int lineReadMbox[USLOSS_TERM_UNITS];        // read line
int lineWriteMbox[USLOSS_TERM_UNITS];       // write line
int pidMbox[USLOSS_TERM_UNITS];             // pid to block
int termInt[USLOSS_TERM_UNITS];             // interupt for term (control writing)

void start3(void) {
    char	name[128];
    char    termbuf[10];
    char    diskbuf[10];
    int		i;
    int		clockPID;
    int		pid;
    int		status;

    /*
     * Check kernel mode here.
     */
    makeSureCurrentFunctionIsInKernelMode("start3");

    // initialize proc table
    for (i = 0; i < MAXPROC; i++) {
        initializeProcessSlot(i);
    }

    // Create queue of s
    initializeHeap(&sleepinProcsHeap);

    // initialize systemCallVec
    systemCallVec[SYS_SLEEP] = sleep;
    systemCallVec[SYS_DISKREAD] = diskRead;
    systemCallVec[SYS_DISKWRITE] = diskWrite;
    systemCallVec[SYS_DISKSIZE] = diskSize;
    systemCallVec[SYS_TERMREAD] = termRead;
    systemCallVec[SYS_TERMWRITE] = termWrite;

    // mboxes for terminal
    for (i = 0; i < USLOSS_TERM_UNITS; i++) {
        charRecvMbox[i] = MboxCreate(1, MAXLINE);
        charSendMbox[i] = MboxCreate(1, MAXLINE);
        lineReadMbox[i] = MboxCreate(10, MAXLINE);
        lineWriteMbox[i] = MboxCreate(10, MAXLINE);
        pidMbox[i] = MboxCreate(1, sizeof(int));
    }

    /*
     * Create clock device driver
     * I am assuming a semaphore here for coordination.  A mailbox can
     * be used instead -- your choice.
     */
    running = semcreateReal(0);
    clockPID = fork1("Clock driver", ClockDriver, NULL, USLOSS_MIN_STACK, 2);
    if (clockPID < 0) {
        USLOSS_Console("start3(): Can't create clock driver\n");
        USLOSS_Halt(1);
    }
    /*
     * Wait for the clock driver to start. The idea is that ClockDriver
     * will V the semaphore "running" once it is running.
     */
    sempReal(running);

    /*
     * Create the disk device drivers here.  You may need to increase
     * the stack size depending on the complexity of your
     * driver, and perhaps do something with the pid returned.
     */
    int temp;
    for (i = 0; i < USLOSS_DISK_UNITS; i++) {
        sprintf(diskbuf, "%d", i);
        pid = fork1("Disk driver", DiskDriver, diskbuf, USLOSS_MIN_STACK, 2);
        if (pid < 0) {
            USLOSS_Console("start3(): Can't create disk driver %d\n", i);
            USLOSS_Halt(1);
        }

        diskPids[i] = pid;
        sempReal(running); // wait for driver to start running

        // get number of tracks
        diskSizeReal(i, &temp, &temp, &ProcTable[pid % MAXPROC].diskTrack);
    }


    /*
     * Create terminal device drivers.
     */

    for (i = 0; i < USLOSS_TERM_UNITS; i++) {
        sprintf(termbuf, "%d", i);

        // Create the terminal driver
        termProcTable[i][0] = fork1(name, TermDriver, termbuf, USLOSS_MIN_STACK, 2);
        if (termProcTable[i][0] < 0) {
            USLOSS_Console("start3(): Can't create term driver %d\n", i);
            USLOSS_Halt(1);
        }

        // Create the terminal reader
        termProcTable[i][1] = fork1(name, TermReader, termbuf, USLOSS_MIN_STACK, 2);
        if (termProcTable[i][1] < 0) {
            USLOSS_Console("start3(): Can't create term reader %d\n", i);
            USLOSS_Halt(1);
        }

        // Create the terminal writer
        termProcTable[i][2] = fork1(name, TermWriter, termbuf, USLOSS_MIN_STACK, 2);
        if (termProcTable[i][2] < 0) {
            USLOSS_Console("start3(): Can't create term writer %d\n", i);
            USLOSS_Halt(1);

        }

        // Wait for the driver, reader and writer to start
        sempReal(running);
        sempReal(running);
        sempReal(running);
    }


    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * I'm assuming kernel-mode versions of the system calls
     * with lower-case first letters, as shown in provided_prototypes.h
     */
    pid = spawnReal("start4", start4, NULL, 4 * USLOSS_MIN_STACK, 3);
    pid = waitReal(&status);

    /*
     * Zap the device drivers
     */

    status = 0;

    // zap clock driver
    zap(clockPID);
    join(&status);

    // zap disk drivers
    for (i = 0; i < USLOSS_DISK_UNITS; i++) {
        semvReal(ProcTable[diskPids[i]].blockSem);
        zap(diskPids[i]);
        join(&status);
    }

    // zap termreader
    for (i = 0; i < USLOSS_TERM_UNITS; i++) {
        MboxSend(charRecvMbox[i], NULL, 0);
        zap(termProcTable[i][1]);
        join(&status);
    }

    // zap termwriter
    for (i = 0; i < USLOSS_TERM_UNITS; i++) {
        MboxSend(lineWriteMbox[i], NULL, 0);
        zap(termProcTable[i][2]);
        join(&status);
    }

    // zap termdriver, etc
    char filename[50];
    for(i = 0; i < USLOSS_TERM_UNITS; i++)
    {
        int ctrl = 0;
        ctrl = USLOSS_TERM_CTRL_RECV_INT(ctrl);
        int w = USLOSS_DeviceOutput(USLOSS_TERM_DEV, i, (void *)((long) ctrl));
        w += 5;
        // file stuff
        sprintf(filename, "term%d.in", i);
        FILE *f = fopen(filename, "a+");
        fprintf(f, "last line\n");
        fflush(f);
        fclose(f);

        // actual termdriver zap
        zap(termProcTable[i][0]);
        join(&status);
    }

    // eventually, at the end:
    quit(0);

}

/* Clock Driver */
static int ClockDriver(char *arg) {
    int result;
    int status;

    // Let the parent know we are running and enable interrupts.
    semvReal(running);
    int w = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    w += 5;

    // Infinite loop until we are zap'd
    while(! isZapped()) {
        result = waitDevice(USLOSS_CLOCK_DEV, 0, &status);
        if (result != 0) {
            return 0;
        }

        // Compute the current time and wake up any processes whose time has come.
        procPtr proc;
        int time;
        int w = USLOSS_DeviceInput(0, 0, &time);
        w += 5;
        while (sleepinProcsHeap.size > 0 && time >= peekAtHeap(&sleepinProcsHeap)->wakeTime) {
            proc = popFromHeap(&sleepinProcsHeap);
            if (debug4)
                USLOSS_Console("ClockDriver: Waking up process %d\n", proc->pid);
            semvReal(proc->blockSem);
        }
    }
    return 0;
}

/* Disk Driver */
static int DiskDriver(char *arg) {
    int result;
    int status;
    int unit = atoi( (char *) arg);     // Unit is passed as arg.

    // get set up in proc table
    initializeProcessSlot(getpid());
    procPtr me = &ProcTable[getpid() % MAXPROC];
    initializeDiskQueue(&diskQs[unit]);

    if (debug4) {
        USLOSS_Console("DiskDriver: unit %d started, pid = %d\n", unit, me->pid);
    }

    // Let the parent know we are running and enable interrupts.
    semvReal(running);
    int w = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    w += 5;

    // Infinite loop until we are zap'd
    while(!isZapped()) {
        // block on sem until we get request
        sempReal(me->blockSem);
        if (debug4) {
            USLOSS_Console("DiskDriver: unit %d unblocked, zapped = %d, queue size = %d\n", unit, isZapped(), diskQs[unit].size);
        }
        if (isZapped()) // check  if we were zapped
            return 0;

        // get request off queue
        if (diskQs[unit].size > 0) {
            procPtr proc = peekAtDiskQueue(&diskQs[unit]);
            int track = proc->diskTrack;

            if (debug4) {
                USLOSS_Console("DiskDriver: taking request from pid %d, track %d\n", proc->pid, proc->diskTrack);
            }

            // handle tracks request
            if (proc->diskRequest.opr == USLOSS_DISK_TRACKS) {
                int w = USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &proc->diskRequest);
                w += 5;

                result = waitDevice(USLOSS_DISK_DEV, unit, &status);
                if (result != 0) {
                    return 0;
                }
            }

            else { // handle read/write requests
                while (proc->diskSectors > 0) {
                    // seek to needed track
                    USLOSS_DeviceRequest request;
                    request.opr = USLOSS_DISK_SEEK;
                    request.reg1 = &track;
                    int w = USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &request);
                    w += 5;

                    // wait for result
                    result = waitDevice(USLOSS_DISK_DEV, unit, &status);
                    if (result != 0) {
                        return 0;
                    }

                    if (debug4) {
                        USLOSS_Console("DiskDriver: seeked to track %d, status = %d, result = %d\n", track, status, result);
                    }

                    // read/write the sectors
                    int s;
                    for (s = proc->diskFirstSec; proc->diskSectors > 0 && s < USLOSS_DISK_TRACK_SIZE; s++) {
                        proc->diskRequest.reg1 = (void *) ((long) s);
                        int w = USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &proc->diskRequest);
                        w += 5;
                        result = waitDevice(USLOSS_DISK_DEV, unit, &status);
                        if (result != 0) {
                            return 0;
                        }

                        if (debug4) {
                            USLOSS_Console("DiskDriver: read/wrote sector %d, status = %d, result = %d, buffer = %s\n", s, status, result, proc->diskRequest.reg2);
                        }

                        proc->diskSectors--;
                        proc->diskRequest.reg2 += USLOSS_DISK_SECTOR_SIZE;
                    }

                    // request first sector of next track
                    track++;
                    proc->diskFirstSec = 0;
                }
            }

            if (debug4)
                USLOSS_Console("DiskDriver: finished request from pid %d\n", proc->pid, result, status);

            removeFromDiskQueue(&diskQs[unit]); // remove proc from queue
            semvReal(proc->blockSem); // unblock caller
        }

    }

    semvReal(running); // unblock parent
    return 0;
}

/* Terminal Driver */
static int TermDriver(char *arg) {
    int result;
    int status;
    int unit = atoi( (char *) arg);     // Unit is passed as arg.

    semvReal(running);
    if (debug4)
        USLOSS_Console("TermDriver (unit %d): running\n", unit);

    while (!isZapped()) {

        result = waitDevice(USLOSS_TERM_INT, unit, &status);
        if (result != 0) {
            return 0;
        }

        // Try to receive character
        int recv = USLOSS_TERM_STAT_RECV(status);
        if (recv == USLOSS_DEV_BUSY) {
            MboxCondSend(charRecvMbox[unit], &status, sizeof(int));
        }
        else if (recv == USLOSS_DEV_ERROR) {
            if (debug4)
                USLOSS_Console("TermDriver RECV ERROR\n");
        }

        // Try to send character
        int xmit = USLOSS_TERM_STAT_XMIT(status);
        if (xmit == USLOSS_DEV_READY) {
            MboxCondSend(charSendMbox[unit], &status, sizeof(int));
        }
        else if (xmit == USLOSS_DEV_ERROR) {
            if (debug4)
                USLOSS_Console("TermDriver XMIT ERROR\n");
        }
    }

    return 0;
}

/* Terminal Reader */
static int TermReader(char * arg) {
    int unit = atoi( (char *) arg);     // Unit is passed as arg.
    int i;
    int receive; // char to receive
    char line[MAXLINE]; // line being created/read
    int next = 0; // index in line to write char

    for (i = 0; i < MAXLINE; i++) {
        line[i] = '\0';
    }

    semvReal(running);
    while (!isZapped()) {
        // receieve characters
        MboxReceive(charRecvMbox[unit], &receive, sizeof(int));
        char ch = USLOSS_TERM_STAT_CHAR(receive);
        line[next] = ch;
        next++;

        // receive line
        if (ch == '\n' || next == MAXLINE) {
            if (debug4)
                USLOSS_Console("TermReader (unit %d): line send\n", unit);

            line[next] = '\0'; // end with null
            MboxSend(lineReadMbox[unit], line, next);

            // reset line
            for (i = 0; i < MAXLINE; i++) {
                line[i] = '\0';
            }
            next = 0;
        }


    }
    return 0;
}

/* Terminal Writer */
static int TermWriter(char * arg) {
    int unit = atoi( (char *) arg);     // Unit is passed as arg.
    int size;
    int ctrl = 0;
    int next;
    int status;
    char line[MAXLINE];

    semvReal(running);
    if (debug4)
        USLOSS_Console("TermWriter (unit %d): running\n", unit);

    while (!isZapped()) {
        size = MboxReceive(lineWriteMbox[unit], line, MAXLINE); // get line and size

        if (isZapped())
            break;

        // enable xmit interrupt and receive interrupt
        ctrl = USLOSS_TERM_CTRL_XMIT_INT(ctrl);
        int w = USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void *) ((long) ctrl));
        w += 5;

        // xmit the line
        next = 0;
        while (next < size) {
            MboxReceive(charSendMbox[unit], &status, sizeof(int));

            // xmit the character
            int x = USLOSS_TERM_STAT_XMIT(status);
            if (x == USLOSS_DEV_READY) {
                //USLOSS_Console("%c string %d unit\n", line[next], unit);

                ctrl = 0;
                //ctrl = USLOSS_TERM_CTRL_RECV_INT(ctrl);
                ctrl = USLOSS_TERM_CTRL_CHAR(ctrl, line[next]);
                ctrl = USLOSS_TERM_CTRL_XMIT_CHAR(ctrl);
                ctrl = USLOSS_TERM_CTRL_XMIT_INT(ctrl);

                int w = USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void *) ((long) ctrl));
                w += 5;
            }

            next++;
        }

        // enable receive interrupt
        ctrl = 0;
        if (termInt[unit] == 1)
            ctrl = USLOSS_TERM_CTRL_RECV_INT(ctrl);
        w = USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void *) ((long) ctrl));
        w += 5;

        termInt[unit] = 0;
        int pid;
        MboxReceive(pidMbox[unit], &pid, sizeof(int));
        semvReal(ProcTable[pid % MAXPROC].blockSem);


    }

    return 0;
}

/* sleep function value extraction */
void sleep(USLOSS_Sysargs * args) {
    makeSureCurrentFunctionIsInKernelMode("sleep");
    int seconds = (long) args->arg1;
    int retval = sleepReal(seconds);
    args->arg4 = (void *) ((long) retval);
    switchToUserMode();
}

/* real sleep function */
int sleepReal(int seconds) {
    makeSureCurrentFunctionIsInKernelMode("sleepReal");

    if (debug4)
        USLOSS_Console("sleepReal: called for process %d with %d seconds\n", getpid(), seconds);

    if (seconds < 0) {
        return -1;
    }

    // init/get the process
    if (ProcTable[getpid() % MAXPROC].pid == -1) {
        initializeProcessSlot(getpid());
    }
    procPtr proc = &ProcTable[getpid() % MAXPROC];

    // set wake time
    int time;
    int w = USLOSS_DeviceInput(0, 0, &time);
    w += 5;
    proc->wakeTime = time + seconds*1000000;
    if (debug4)
        USLOSS_Console("sleepReal: set wake time for process %d to %d, adding to heap...\n", proc->pid, proc->wakeTime);

    appendToHeap(&sleepinProcsHeap, proc); // add to sleep heap
    if (debug4)
        USLOSS_Console("sleepReal: Process %d going to sleep until %d\n", proc->pid, proc->wakeTime);
    sempReal(proc->blockSem); // block the process
    w = USLOSS_DeviceInput(0, 0, &time);
    w += 5;
    if (debug4)
        USLOSS_Console("sleepReal: Process %d woke up, time is %d\n", proc->pid, time);
    return 0;
}

/* extract values from sysargs and call diskReadReal */
void diskRead(USLOSS_Sysargs * args) {
    makeSureCurrentFunctionIsInKernelMode("diskRead");

    int sectors = (long) args->arg2;
    int track = (long) args->arg3;
    int first = (long) args->arg4;
    int unit = (long) args->arg5;

    int retval = diskReadReal(unit, track, first, sectors, args->arg1);

    if (retval == -1) {
        args->arg1 = (void *) ((long) retval);
        args->arg4 = (void *) ((long) -1);
    } else {
        args->arg1 = (void *) ((long) retval);
        args->arg4 = (void *) ((long) 0);
    }
    switchToUserMode();
}

/* extract values from sysargs and call diskWriteReal */
void diskWrite(USLOSS_Sysargs * args) {
    makeSureCurrentFunctionIsInKernelMode("diskWrite");

    int sectors = (long) args->arg2;
    int track = (long) args->arg3;
    int first = (long) args->arg4;
    int unit = (long) args->arg5;

    int retval = diskWriteReal(unit, track, first, sectors, args->arg1);

    if (retval == -1) {
        args->arg1 = (void *) ((long) retval);
        args->arg4 = (void *) ((long) -1);
    } else {
        args->arg1 = (void *) ((long) retval);
        args->arg4 = (void *) ((long) 0);
    }
    switchToUserMode();
}

int diskWriteReal(int unit, int track, int first, int sectors, void *buffer) {
    makeSureCurrentFunctionIsInKernelMode("diskWriteReal");
    return diskReadOrWriteReal(unit, track, first, sectors, buffer, 1);
}

int diskReadReal(int unit, int track, int first, int sectors, void *buffer) {
    makeSureCurrentFunctionIsInKernelMode("diskWriteReal");
    return diskReadOrWriteReal(unit, track, first, sectors, buffer, 0);
}

/*------------------------------------------------------------------------
    diskReadOrWriteReal: Reads or writes to the desk depending on the
                        value of write; write if write == 1, else read.
    Returns: -1 if given illegal input, 0 otherwise
 ------------------------------------------------------------------------*/
int diskReadOrWriteReal(int unit, int track, int first, int sectors, void *buffer, int write) {
    if (debug4)
        USLOSS_Console("diskReadOrWriteReal: called with unit: %d, track: %d, first: %d, sectors: %d, write: %d\n", unit, track, first, sectors, write);

    // check for illegal args
    if (unit < 0 || unit > 1 || track < 0 || track > ProcTable[diskPids[unit]].diskTrack ||
        first < 0 || first > USLOSS_DISK_TRACK_SIZE || buffer == NULL  ||
        (first + sectors)/USLOSS_DISK_TRACK_SIZE + track > ProcTable[diskPids[unit]].diskTrack) {
        return -1;
    }

    procPtr driver = &ProcTable[diskPids[unit]];

    // init/get the process
    if (ProcTable[getpid() % MAXPROC].pid == -1) {
        initializeProcessSlot(getpid());
    }
    procPtr proc = &ProcTable[getpid() % MAXPROC];

    if (write)
        proc->diskRequest.opr = USLOSS_DISK_WRITE;
    else
        proc->diskRequest.opr = USLOSS_DISK_READ;

    proc->diskRequest.reg2 = buffer;
    proc->diskTrack = track;
    proc->diskFirstSec = first;
    proc->diskSectors = sectors;
    proc->diskBuffer = buffer;

    addToDiskQueue(&diskQs[unit], proc); // add to disk queue
    semvReal(driver->blockSem);  // wake up disk driver
    sempReal(proc->blockSem); // block

    int status;
    int result = USLOSS_DeviceInput(USLOSS_DISK_DEV, unit, &status);

    if (debug4)
        USLOSS_Console("diskReadOrWriteReal: finished, status = %d, result = %d\n", status, result);

    return result;
}

/* extract values from sysargs and call diskSizeReal */
void diskSize(USLOSS_Sysargs * args) {
    makeSureCurrentFunctionIsInKernelMode("diskSize");

    int unit = (long) args->arg1;
    int sector, track, disk;
    int retval = diskSizeReal(unit, &sector, &track, &disk);

    args->arg1 = (void *) ((long) sector);
    args->arg2 = (void *) ((long) track);
    args->arg3 = (void *) ((long) disk);
    args->arg4 = (void *) ((long) retval);
    switchToUserMode();
}

/*------------------------------------------------------------------------
    diskSizeReal: Puts values into pointers for the size of a sector,
    number of sectors per track, and number of tracks on the disk for the
    given unit.
    Returns: -1 if given illegal input, 0 otherwise
 ------------------------------------------------------------------------*/
int diskSizeReal(int unit, int *sector, int *track, int *disk) {
    makeSureCurrentFunctionIsInKernelMode("diskSizeReal");

    // check for illegal args
    if (unit < 0 || unit > 1 || sector == NULL || track == NULL || disk == NULL) {
        if (debug4)
            USLOSS_Console("diskSizeReal: given illegal argument(s), returning -1\n");
        return -1;
    }

    procPtr driver = &ProcTable[diskPids[unit]];

    // get the number of tracks for the first time
    if (driver->diskTrack == -1) {
        // init/get the process
        if (ProcTable[getpid() % MAXPROC].pid == -1) {
            initializeProcessSlot(getpid());
        }
        procPtr proc = &ProcTable[getpid() % MAXPROC];

        // set variables
        proc->diskTrack = 0;
        USLOSS_DeviceRequest request;
        request.opr = USLOSS_DISK_TRACKS;
        request.reg1 = &driver->diskTrack;
        proc->diskRequest = request;

        addToDiskQueue(&diskQs[unit], proc); // add to disk queue
        semvReal(driver->blockSem);  // wake up disk driver
        sempReal(proc->blockSem); // block

        if (debug4)
            USLOSS_Console("diskSizeReal: number of tracks on unit %d: %d\n", unit, driver->diskTrack);
    }

    *sector = USLOSS_DISK_SECTOR_SIZE;
    *track = USLOSS_DISK_TRACK_SIZE;
    *disk = driver->diskTrack;
    return 0;
}

void termRead(USLOSS_Sysargs * args) {
    if (debug4)
        USLOSS_Console("termRead\n");

    makeSureCurrentFunctionIsInKernelMode("termRead");

    char *buffer = (char *) args->arg1;
    int size = (long) args->arg2;
    int unit = (long) args->arg3;

    long retval = termReadReal(unit, size, buffer);

    if (retval == -1) {
        args->arg2 = (void *) ((long) retval);
        args->arg4 = (void *) ((long) -1);
    } else {
        args->arg2 = (void *) ((long) retval);
        args->arg4 = (void *) ((long) 0);
    }
    switchToUserMode();
}

int termReadReal(int unit, int size, char *buffer) {
    if (debug4)
        USLOSS_Console("termReadReal\n");

    makeSureCurrentFunctionIsInKernelMode("termReadReal");

    if (unit < 0 || unit > USLOSS_TERM_UNITS - 1 || size < 0)
        return -1;

    char line[MAXLINE];
    int ctrl = 0;

    //enable term interrupts
    if (termInt[unit] == 0) {
        if (debug4)
            USLOSS_Console("termReadReal enable interrupts\n");
        ctrl = USLOSS_TERM_CTRL_RECV_INT(ctrl);
        int w = USLOSS_DeviceOutput(USLOSS_TERM_DEV, unit, (void *) ((long) ctrl));
        w += 5;
        termInt[unit] = 1;
    }

    int retval = MboxReceive(lineReadMbox[unit], &line, MAXLINE);

    if (debug4)
        USLOSS_Console("termReadReal (unit %d): size %d retval %d \n",
                       unit, size, retval);

    if (retval > size)
        retval = size;

    memcpy(buffer, line, retval);

    return retval;
}

void termWrite(USLOSS_Sysargs * args) {

    if (debug4)
        USLOSS_Console("termWrite\n");

    makeSureCurrentFunctionIsInKernelMode("termWrite");

    char *text = (char *) args->arg1;
    int size = (long) args->arg2;
    int unit = (long) args->arg3;

    long retval = termWriteReal(unit, size, text);

    if (retval == -1) {
        args->arg2 = (void *) ((long) retval);
        args->arg4 = (void *) ((long) -1);
    }
    else {
        args->arg2 = (void *) ((long) retval);
        args->arg4 = (void *) ((long) 0);
    }

    switchToUserMode();
}

int termWriteReal(int unit, int size, char *text) {
    if (debug4)
        USLOSS_Console("termWriteReal\n");
    makeSureCurrentFunctionIsInKernelMode("termWriteReal");

    if (unit < 0 || unit > USLOSS_TERM_UNITS - 1 || size < 0) {
        return -1;
    }

    int pid = getpid();
    MboxSend(pidMbox[unit], &pid, sizeof(int));

    MboxSend(lineWriteMbox[unit], text, size);
    sempReal(ProcTable[pid % MAXPROC].blockSem);
    return size;
}

/* ------------------------------------------------------------------------
   Name - requireKernelMode
   Purpose - Checks if we are in kernel mode and prints an error messages
              and halts USLOSS if not.
   Parameters - The name of the function calling it, for the error message.
   Side Effects - Prints and halts if we are not in kernel mode
   ------------------------------------------------------------------------ */
void makeSureCurrentFunctionIsInKernelMode(char *name)
{
    if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0 ) {
        USLOSS_Console("%s: called while in user mode, by process %d. Halting...\n",
                       name, getpid());
        USLOSS_Halt(1);
    }
}

/* ------------------------------------------------------------------------
   Name - setUserMode
   Purpose - switches to user mode
   Parameters - none
   Side Effects - switches to user mode
   ------------------------------------------------------------------------ */
void switchToUserMode()
{
    int w = USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_MODE );
    w += 5;
}

/* ------------------------------------------------------------------------
   Name -           initializeProcessSlot
   Purpose -        Emptys/initializes a process slot
   Parameters -
                    index of process in the table of processes
   Returns -        nothing
   Side Effects -
   ----------------------------------------------------------------------- */
void initializeProcessSlot(int pid) {
    makeSureCurrentFunctionIsInKernelMode("initProc()");

    int i = pid % MAXPROC;

    ProcTable[i].pid = pid;
    ProcTable[i].mboxID = MboxCreate(0, 0);
    ProcTable[i].blockSem = semcreateReal(0);
    ProcTable[i].wakeTime = -1;
    ProcTable[i].diskTrack = -1;
    ProcTable[i].nextDiskPtr = NULL;
    ProcTable[i].prevDiskPtr = NULL;
}


/* ------------------------------------------------------------------------
   Name -           initializeDiskQueue
   Purpose -        Just initialize a queue
   Parameters -
                    diskQueue* q:
                        Pointer to diskQueue to be initialized
   Returns -        Nothing
   Side Effects -   The passed diskQueue is now no longer NULL
   ------------------------------------------------------------------------ */
void initializeDiskQueue(diskQueue *q) {
    q->head = NULL;
    q->tail = NULL;
    q->curr = NULL;
    q->size = 0;
}

/* ------------------------------------------------------------------------
   Name -           addToDiskQueue
   Purpose -        Adds a process to the end of a diskQueue
   Parameters -
                    diskQueue* q:
                        Pointer to diskQueue to be added to
                    procPtr p
                        Pointer to the process that will be added
   Returns -        Nothing
   Side Effects -   The length of the processHeap increases by 1.
   ------------------------------------------------------------------------ */
void addToDiskQueue(diskQueue *q, procPtr p) {
    if (debug4)
        USLOSS_Console("addDiskQ: adding pid %d, track %d to queue\n", p->pid, p->diskTrack);

    // first add
    if (q->head == NULL) {
        q->head = q->tail = p;
        q->head->nextDiskPtr = q->tail->nextDiskPtr = NULL;
        q->head->prevDiskPtr = q->tail->prevDiskPtr = NULL;
    }
    else {
        // find the right location to add
        procPtr prev = q->tail;
        procPtr next = q->head;
        while (next != NULL && next->diskTrack <= p->diskTrack) {
            prev = next;
            next = next->nextDiskPtr;
            if (next == q->head)
                break;
        }
        if (debug4)
            USLOSS_Console("addDiskQ: found place, prev = %d\n", prev->diskTrack);
        prev->nextDiskPtr = p;
        p->prevDiskPtr = prev;
        if (next == NULL)
            next = q->head;
        p->nextDiskPtr = next;
        next->prevDiskPtr = p;
        if (p->diskTrack < q->head->diskTrack)
            q->head = p;
        if (p->diskTrack >= q->tail->diskTrack)
            q->tail = p;
    }
    q->size++;
    if (debug4)
        USLOSS_Console("addDiskQ: add complete, size = %d\n", q->size);
}

/* ------------------------------------------------------------------------
   Name -           peekAtDiskQueue
   Purpose -        Returns a pointer to the first element in the diskQueue
   Parameters -
                    diskQueue *q
                        The diskQueue we want to look at the first element of
   Returns -        A pointer to the headProcess of the given processQueue
   Side Effects -   None
   ------------------------------------------------------------------------ */
procPtr peekAtDiskQueue(diskQueue *q) {
    if (q->curr == NULL) {
        q->curr = q->head;
    }

    return q->curr;
}

/* ------------------------------------------------------------------------
   Name -           removeFromDiskQueue
   Purpose -        Remove and return the first element from the diskQueue
   Parameters -
                    diskQueue *q
                        A pointer to the diskQueue who we want to modify
   Returns -
                    procPtr:
                        Pointer to the element that we want
   Side Effects -   Length of processQueue is decreased by 1
   ------------------------------------------------------------------------ */
procPtr removeFromDiskQueue(diskQueue *q) {
    if (q->size == 0)
        return NULL;

    if (q->curr == NULL) {
        q->curr = q->head;
    }

    if (debug4)
        USLOSS_Console("removeDiskQ: called, size = %d, curr pid = %d, curr track = %d\n", q->size, q->curr->pid, q->curr->diskTrack);

    procPtr temp = q->curr;

    if (q->size == 1) { // remove only node
        q->head = q->tail = q->curr = NULL;
    }

    else if (q->curr == q->head) { // remove head
        q->head = q->head->nextDiskPtr;
        q->head->prevDiskPtr = q->tail;
        q->tail->nextDiskPtr = q->head;
        q->curr = q->head;
    }

    else if (q->curr == q->tail) { // remove tail
        q->tail = q->tail->prevDiskPtr;
        q->tail->nextDiskPtr = q->head;
        q->head->prevDiskPtr = q->tail;
        q->curr = q->head;
    }

    else { // remove other
        q->curr->prevDiskPtr->nextDiskPtr = q->curr->nextDiskPtr;
        q->curr->nextDiskPtr->prevDiskPtr = q->curr->prevDiskPtr;
        q->curr = q->curr->nextDiskPtr;
    }

    q->size--;

    if (debug4)
        USLOSS_Console("removeDiskQ: done, size = %d, curr pid = %d, curr track = %d\n", q->size, temp->pid, temp->diskTrack);

    return temp;
}


/* ------------------------------------------------------------------------
   Name -           initializeHeap
   Purpose -        Just initialize a process heap
   Parameters -
                    heap* h:
                        Pointer to heap to be initialized
   Returns -        Nothing
   Side Effects -   The passed heap is now no longer NULL
   ------------------------------------------------------------------------ */
void initializeHeap(heap *h) {
    h->size = 0;
}

/* ------------------------------------------------------------------------
   Name -           appendToHeap
   Purpose -        Adds a process to the end of a processHeap
   Parameters -
                    heap* processHeap:
                        Pointer to processQueue to be added to
                    procPtr process
                        Pointer to the process that will be added
   Returns -        Nothing
   Side Effects -   The length of the processHeap increases by 1.
   ------------------------------------------------------------------------ */
void appendToHeap(heap *processHeap, procPtr process) {

    int i, parent;
    for (i = processHeap->size; i > 0; i = parent) {
        parent = (i-1)/2;
        if (processHeap->procs[parent]->wakeTime <= process->wakeTime)
            break;

        processHeap->procs[i] = processHeap->procs[parent];     // Move the parent towards the bottom
    }

    processHeap->procs[i] = process;                            // Place the process at the end
    processHeap->size++;                                        // Increase the size of the process

    if (debug4)
        USLOSS_Console("heapAdd: Added proc %d to heap at index %d, "
                               "size = %d\n", process->pid, i, processHeap->size);
}


/* ------------------------------------------------------------------------
   Name -           peekAtHeap
   Purpose -        Return a pointer to the first element
   Parameters -
                    heap *h
                        A pointer to the heap who we want to modify
   Returns -
                    procPtr:
                        Pointer to the element that we want
   Side Effects -   Length of heap is decreased by 1
   ------------------------------------------------------------------------ */
procPtr peekAtHeap(heap *h) {
    return h->procs[0];
}


/* ------------------------------------------------------------------------
   Name -           popFromHeap
   Purpose -        Remove and return the first element from the processHeap
   Parameters -
                    heap *processHeap
                        A pointer to the heap who we want to modify
   Returns -
                    procPtr:
                        Pointer to the element that we want
   Side Effects -   Length of heap is decreased by 1
   ------------------------------------------------------------------------ */
procPtr popFromHeap(heap *processHeap) {

    if (processHeap->size == 0)
        return NULL;


    procPtr removedProcess = processHeap->procs[0];                  // Remove first process
    processHeap->size--;                                            // Decrease size of heap
    processHeap->procs[0] = processHeap->procs[processHeap->size]; // Put the last elment in the first

    // Re-create the heap
    int i = 0, left, right, min = 0;
    while (i*2 <= processHeap->size) {
        // Get the children
        left = i*2 + 1;
        right = i*2 + 2;

        // Get the smallest child
        if (left <= processHeap->size && processHeap->procs[left]->wakeTime < processHeap->procs[min]->wakeTime)
            min = left;
        if (right <= processHeap->size && processHeap->procs[right]->wakeTime < processHeap->procs[min]->wakeTime)
            min = right;

        // Swap current with min child
        if (min != i) {
            procPtr temp = processHeap->procs[i];
            processHeap->procs[i] = processHeap->procs[min];
            processHeap->procs[min] = temp;
            i = min;
        }
        else {
            break;
        }

    }

    if (debug4)
        USLOSS_Console("heapRemove: Called, returning pid %d, size = %d\n",
                       removedProcess->pid, processHeap->size);

    return removedProcess;
}
