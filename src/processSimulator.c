#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <semaphore.h>
#include "linkedlist.h"
#include "coursework.h"

//---------------------------------------------- declaration of timeval
struct timeval oBaseTime;
//---------------------------------------------- declaration of process arrays
struct process *processTable[SIZE_OF_PROCESS_TABLE];
struct process *currentProcess[NUMBER_OF_CPUS];
//---------------------------------------------- declaration of queues
struct element *readyQueue[MAX_PRIORITY * 2];

struct element *freePIDHead = NULL, *freePIDTail = NULL;
struct element *newQueueHead = NULL, *newQueueTail = NULL;
struct element *terminatedQueueHead = NULL, *terminatedQueueTail = NULL;
//---------------------------------------------- declaration of global counters
int generatedProc = 0;
int admittedProc = 0;
int executedProc = 0;
int terminatedProc = 0;

long averageRT = 0;
long averageTAT = 0;
//---------------------------------------------- declaration of threads
pthread_t shortTS[NUMBER_OF_CPUS];

pthread_t procGen;
pthread_t longTS;
pthread_t terminationD;
pthread_t boosterD;
//---------------------------------------------- declaration of mutex locks
pthread_mutex_t readyLock;
pthread_mutex_t freePIDLock;
pthread_mutex_t newLock;
pthread_mutex_t terminatedLock;
pthread_mutex_t pcbLock;
//---------------------------------------------- declaration of semaphores
sem_t processGenerate;
sem_t processAdmit;
sem_t processExecute;
sem_t processTerminate;
//---------------------------------------------- function prototypes
int mSleep(long mSec);
void printHeadersSVG();
void printProcessSVG(int iCPUId, struct process *pProcess, struct timeval oStartTime, struct timeval oEndTime);
void printPrioritiesSVG();
void printRasterSVG();
void printFootersSVG();
void initializePID();
void queueGenerator();
void freePIDSpace();
void *processGenerator();
void *longTermScheduler();
void *shortTermScheduler(void *cpuID);
void *boosterDaemon();
void *terminationDaemon();
//-----------------------------------------------------------------------
int main (void) //parameters int argc, char *argv[] are unused in this program
{
//----------------------------------------------------------------------- initialize semaphores
    sem_init(&processGenerate, 0, 1);
    sem_init(&processAdmit, 0 ,0);
    sem_init(&processExecute, 0, 0);
    sem_init(&processTerminate, 0 ,0);
//----------------------------------------------------------------------- initialize mutex locks
    pthread_mutex_init(&readyLock, NULL);
    pthread_mutex_init(&freePIDLock, NULL);
    pthread_mutex_init(&newLock, NULL);
    pthread_mutex_init(&terminatedLock, NULL);
    pthread_mutex_init(&pcbLock, NULL);
//----------------------------------------------------------------------- initialize svg
    printHeadersSVG();
    printPrioritiesSVG();
    printRasterSVG();
//----------------------------------------------------------------------- generate PIDs and queues
    initializePID();
    queueGenerator();
//----------------------------------------------------------------------- initialize current process
    for(int i = 0; i < NUMBER_OF_CPUS; i++)
    {
        currentProcess[i] = NULL;
    }
//----------------------------------------------------------------------- get time for the simulation
    gettimeofday(&oBaseTime, NULL);
//----------------------------------------------------------------------- create threads
    pthread_create(&procGen, NULL, processGenerator, NULL);
    pthread_create(&longTS, NULL, longTermScheduler, NULL);
    for(int i = 0; i < NUMBER_OF_CPUS; i++)
    {
        int *cpuID = malloc(sizeof(int));
        *cpuID = i + 1;
        pthread_create(&shortTS[i], NULL, shortTermScheduler, cpuID);
    }
    pthread_create(&boosterD, NULL, boosterDaemon, NULL);
    pthread_create(&terminationD, NULL, terminationDaemon, NULL);
//----------------------------------------------------------------------- join threads
    pthread_join(procGen, NULL);
    pthread_join(longTS, NULL);
    for(int j = 0; j < NUMBER_OF_CPUS; j++)
    {
        pthread_join(shortTS[j], NULL);
    }
    pthread_join(boosterD, NULL);
    pthread_join(terminationD, NULL);
//----------------------------------------------------------------------- destroy mutex locks
    pthread_mutex_destroy(&readyLock);
    pthread_mutex_destroy(&freePIDLock);
    pthread_mutex_destroy(&newLock);
    pthread_mutex_destroy(&terminatedLock);
    pthread_mutex_destroy(&pcbLock);
//----------------------------------------------------------------------- destroy semaphores
    sem_destroy(&processGenerate);
    sem_destroy(&processAdmit);
    sem_destroy(&processExecute);
    sem_destroy(&processTerminate);
//----------------------------------------------------------------------- print footers
    printFootersSVG();
//----------------------------------------------------------------------- free remaining memory in free pid list
    freePIDSpace();
//-----------------------------------------------------------------------
    return 0;
}

/**
 * Function to generate PIDs of the size of process table, and add them all to the free pid queue.
 */
void initializePID()
{
    //fills the free pid queue, which has length process table, with the free PIDs, starting from 0.
    for(int i = 0; i < SIZE_OF_PROCESS_TABLE; i++)
    {
        int *freePID = (int *)malloc(sizeof(int)); //memory allocation to free pid.
        *freePID = i; //assigning the value stored in i to free pid.
        addLast((int *)freePID, &freePIDHead, &freePIDTail); //adding it to the freePID queue, casting, REMOVED cast to void *, because of possible risk.
    }
}

/**
 * Function to free the memory allocated to all the PIDs
 */
void freePIDSpace()
{
    //loop the size of process table, removes all freePIDs from list and frees them
    while(freePIDHead != NULL && freePIDTail != NULL)
    {
        int *removedPID = removeFirst(&freePIDHead, &freePIDTail);
        free(removedPID);
    }
}

/**
 * Function to generate 32 ready queues
 */
void queueGenerator()
{
    //populate the ready queue array, with the number of queues equal to max priority, multiply by 2 to store head and tail.
    for(int i = 0; i < MAX_PRIORITY * 2; i++)
    {
        struct element *readyQueueHead = NULL; //create the ready queue head and initialize to null.
        struct element *readyQueueTail = NULL; //create the ready queue tail and initialize to null.
        readyQueue[i] = readyQueueHead; //add the ready queue head to the ready queue array at index i.
        readyQueue[++i] = readyQueueTail; //add the ready queue tail to the ready queue array at index i + 1.
    }
}

/**
 * Simulation, generates pre defined number of processes
 * Creates processes as soon as it can
 * Looks for free PID, adds process to new queue and process table
 */
void *processGenerator()
{
    while (generatedProc < NUMBER_OF_PROCESSES)
    {
        sem_wait(&processGenerate);

        int *genPID;
        struct process *genProcess;

        while (freePIDHead != NULL && freePIDTail != NULL && (generatedProc < NUMBER_OF_PROCESSES))
        {
            pthread_mutex_lock(&freePIDLock);
            genPID = (int *)removeFirst(&freePIDHead, &freePIDTail);
            pthread_mutex_unlock(&freePIDLock);

            genProcess = generateProcess(genPID);

            pthread_mutex_lock(&pcbLock);
            processTable[*genPID] = genProcess;
            pthread_mutex_unlock(&pcbLock);

            pthread_mutex_lock(&newLock);
            addLast(genProcess->pPID, &newQueueHead, &newQueueTail);
            pthread_mutex_unlock(&newLock);

            printf("TXT: Generated: Process Id = %d, Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d\n",
                   *genPID, genProcess->iPriority, genProcess->iPreviousBurstTime, genProcess->iRemainingBurstTime); //access values

            generatedProc++;
        }
        sem_post(&processAdmit);
    }
    return NULL;
}

/**
 * Removes process from ready queue and adds to the new queue
 * Thread at regular intervals of 50 ms
 */
void *longTermScheduler()
{
    sem_wait(&processAdmit);

    while (admittedProc < NUMBER_OF_PROCESSES)
    {
        mSleep(LONG_TERM_SCHEDULER_INTERVAL);

        int *readyPID;
        struct process *readyProcess;

        while (newQueueHead != NULL && newQueueTail != NULL && (admittedProc < NUMBER_OF_PROCESSES))
        {
            pthread_mutex_lock(&newLock);
            readyPID = (int *)removeFirst(&newQueueHead, &newQueueTail);
            pthread_mutex_unlock(&newLock);

            pthread_mutex_lock(&pcbLock);
            readyProcess = processTable[*readyPID];
            pthread_mutex_unlock(&pcbLock);

            if(readyProcess != NULL)
            {
                pthread_mutex_lock(&readyLock);
                addLast(readyProcess->pPID, &(readyQueue[(readyProcess->iPriority) * 2]), &(readyQueue[((readyProcess->iPriority) * 2) + 1]));
                pthread_mutex_unlock(&readyLock);

                printf("TXT: Admitted: Process Id = %d, Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d \n",
                       *readyPID, readyProcess->iPriority, readyProcess->iPreviousBurstTime, readyProcess->iRemainingBurstTime);

                for(int i = 0; i < NUMBER_OF_CPUS; i++)
                {
                    if(currentProcess[i] != NULL)
                    {
                        if(currentProcess[i]->iPriority > readyProcess->iPriority)
                        {
                            preemptJob(currentProcess[i]); //preempt for FCFS
                            //printf("Preempted Job with PID: %d!\n", *(readyProcess->pPID));
                        }
                    }
                }
            }
            admittedProc++;
        }
        sem_post(&processExecute);
    }
    return NULL;
}

/**
 * Removes form ready queue
 * Multiple CPUs simulated by creating multiple STS threads
 * When successful finish, add process to terminated queue, else add back to ready queue
 */
void *shortTermScheduler(void *cpuID)
{
    sem_wait(&processExecute);

    int tempRunningCPU = *(int *)cpuID;

    int *runningPID;
    struct process *runningProcess;
    struct timeval startTime, endTime;

    while(executedProc < NUMBER_OF_PROCESSES)
    {
        for (int priorityLevel = 0; priorityLevel < MAX_PRIORITY; priorityLevel++)
        {
            if (readyQueue[priorityLevel * 2])
            {
                pthread_mutex_lock(&readyLock);
                runningPID = (removeFirst(&readyQueue[priorityLevel * 2], &readyQueue[(priorityLevel * 2) + 1]));
                pthread_mutex_unlock(&readyLock);

                pthread_mutex_lock(&pcbLock);
                runningProcess = processTable[*runningPID];
                pthread_mutex_unlock(&pcbLock);

                break;
            }
        }

        if (runningProcess)
        {
            if(runningProcess->iPriority < (MAX_PRIORITY / 2)) // FCFS
            {
                currentProcess[tempRunningCPU] = runningProcess;
                runNonPreemptiveJob(runningProcess, &startTime, &endTime);
                currentProcess[tempRunningCPU] = NULL;

                long responseTime = getDifferenceInMilliSeconds(runningProcess->oTimeCreated, runningProcess->oFirstTimeRunning);
                long turnaroundTime = getDifferenceInMilliSeconds(runningProcess->oFirstTimeRunning, runningProcess->oLastTimeRunning);

                if (runningProcess->iInitialBurstTime == runningProcess->iPreviousBurstTime && runningProcess->iRemainingBurstTime == 0) //running first time and finishing
                {
                    printf("TXT: Consumer %d, Process Id = %d (FCFS), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d, Response Time = %ld, Turnaround Time = %ld\n",
                           tempRunningCPU, *runningPID, runningProcess->iPriority, runningProcess->iPreviousBurstTime, runningProcess->iRemainingBurstTime,
                           responseTime, turnaroundTime);
                    printProcessSVG(tempRunningCPU, runningProcess, startTime, endTime);
                }
                else if (runningProcess->iInitialBurstTime == runningProcess->iPreviousBurstTime && runningProcess->iRemainingBurstTime != 0) //running first time but not finishing
                {
                    printf("TXT: Consumer %d, Process Id = %d (FCFS), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d, Response Time = %ld\n",
                           tempRunningCPU, *runningPID, runningProcess->iPriority, runningProcess->iPreviousBurstTime, runningProcess->iRemainingBurstTime,
                           responseTime);
                    printProcessSVG(tempRunningCPU, runningProcess, startTime, endTime);
                }
                else if (runningProcess->iInitialBurstTime != runningProcess->iPreviousBurstTime && runningProcess->iRemainingBurstTime != 0) //has run before but not finishing
                {
                    printf("TXT: Consumer %d, Process Id = %d (FCFS), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d\n",
                           tempRunningCPU, *runningPID, runningProcess->iPriority, runningProcess->iPreviousBurstTime, runningProcess->iRemainingBurstTime);
                    printProcessSVG(tempRunningCPU, runningProcess, startTime, endTime);
                }
                else if (runningProcess->iInitialBurstTime != runningProcess->iPreviousBurstTime && runningProcess->iRemainingBurstTime == 0) //running for the last time and finishing
                {
                    printf("TXT: Consumer %d, Process Id = %d (FCFS), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d, Turnaround Time = %ld\n",
                           tempRunningCPU, *runningPID, runningProcess->iPriority, runningProcess->iPreviousBurstTime, runningProcess->iRemainingBurstTime,
                           turnaroundTime);
                    printProcessSVG(tempRunningCPU, runningProcess, startTime, endTime);
                }

                if (runningProcess->iRemainingBurstTime <= 0)
                {
                    pthread_mutex_lock(&terminatedLock);
                    addLast(runningProcess->pPID, &terminatedQueueHead, &terminatedQueueTail); //finished queue if time finished
                    pthread_mutex_unlock(&terminatedLock);
                    executedProc++;
                }
                else if (runningProcess->iPreempt == 1)
                {
                    int priorityLevel = 0; // Top fcfs priority

                    pthread_mutex_lock(&readyLock);
                    addLast(runningProcess->pPID, &readyQueue[priorityLevel * 2], &readyQueue[(priorityLevel * 2) + 1]);
                    pthread_mutex_unlock(&readyLock);
                }
                else
                {
                    pthread_mutex_lock(&readyLock);
                    addFirst(runningProcess->pPID, &readyQueue[runningProcess->iPriority * 2], &readyQueue[(runningProcess->iPriority * 2) + 1]); //add it to front of priority queue if time not finished
                    pthread_mutex_unlock(&readyLock);
                }
            }
            else // RR
            {
                currentProcess[tempRunningCPU] = runningProcess;
                runPreemptiveJob(runningProcess, &startTime, &endTime);
                currentProcess[tempRunningCPU] = NULL;

                long responseTime = getDifferenceInMilliSeconds(runningProcess->oTimeCreated, runningProcess-> oFirstTimeRunning);
                long turnaroundTime = getDifferenceInMilliSeconds(runningProcess->oFirstTimeRunning, runningProcess->oLastTimeRunning);

                if (runningProcess->iInitialBurstTime == runningProcess->iPreviousBurstTime && runningProcess->iRemainingBurstTime == 0) //running first time and finishing
                {
                    printf("TXT: Consumer %d, Process Id = %d (RR), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d, Response Time = %ld, Turnaround Time = %ld\n",
                           tempRunningCPU, *runningPID, runningProcess->iPriority, runningProcess->iPreviousBurstTime, runningProcess->iRemainingBurstTime,
                           responseTime, turnaroundTime);
                    printProcessSVG(tempRunningCPU, runningProcess, startTime, endTime);
                }
                else if (runningProcess->iInitialBurstTime == runningProcess->iPreviousBurstTime && runningProcess->iRemainingBurstTime != 0) //running first time but not finishing
                {
                    printf("TXT: Consumer %d, Process Id = %d (RR), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d, Response Time = %ld\n",
                           tempRunningCPU, *runningPID, runningProcess->iPriority, runningProcess->iPreviousBurstTime, runningProcess->iRemainingBurstTime,
                           responseTime);
                    printProcessSVG(tempRunningCPU, runningProcess, startTime, endTime);
                }
                else if (runningProcess->iInitialBurstTime != runningProcess->iPreviousBurstTime && runningProcess->iRemainingBurstTime != 0) //has run before but not finishing
                {
                    printf("TXT: Consumer %d, Process Id = %d (RR), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d\n",
                           tempRunningCPU, *runningPID, runningProcess->iPriority, runningProcess->iPreviousBurstTime, runningProcess->iRemainingBurstTime);
                    printProcessSVG(tempRunningCPU, runningProcess, startTime, endTime);
                }
                else if (runningProcess->iInitialBurstTime != runningProcess->iPreviousBurstTime && runningProcess->iRemainingBurstTime == 0) //running for the last time and finishing
                {
                    printf("TXT: Consumer %d, Process Id = %d (RR), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d, Turnaround Time = %ld\n",
                           tempRunningCPU, *runningPID, runningProcess->iPriority, runningProcess->iPreviousBurstTime, runningProcess->iRemainingBurstTime,
                           turnaroundTime);
                    printProcessSVG(tempRunningCPU, runningProcess, startTime, endTime);
                }

                if (runningProcess->iRemainingBurstTime == 0)
                {
                    pthread_mutex_lock(&terminatedLock);
                    addLast(runningProcess->pPID, &terminatedQueueHead, &terminatedQueueTail);
                    pthread_mutex_unlock(&terminatedLock);

                    executedProc++;
                }
                else
                {
                    pthread_mutex_lock(&readyLock);
                    addLast(runningProcess->pPID, &readyQueue[runningProcess->iPriority * 2], &readyQueue[(runningProcess->iPriority * 2) + 1]);
                    pthread_mutex_unlock(&readyLock);
                }
            }
            runningProcess = NULL;
        }
        sem_post(&processTerminate);
    }
    //printf("\nClosing CPU thread %d\n", tempRunningCPU);
    free(cpuID);
    return NULL;
}

/**
 * Runs at regular intervals
 * Boosts process priority level
 */
void *boosterDaemon()
{
    while(terminatedProc < NUMBER_OF_PROCESSES)
    {
        mSleep(BOOST_INTERVAL / 1000);

        int *boostPID;
        struct process *boostProcess;

        int priorityLevel = (MAX_PRIORITY / 2) + 1;
        while (priorityLevel < MAX_PRIORITY)
        {
            if (readyQueue[priorityLevel * 2] != NULL && readyQueue[(priorityLevel * 2) + 1] != NULL)
            {
                pthread_mutex_lock(&readyLock);
                boostPID = (int *)(removeFirst(&readyQueue[priorityLevel * 2], &readyQueue[(priorityLevel * 2) + 1]));
                pthread_mutex_unlock(&readyLock);

                pthread_mutex_lock(&pcbLock);
                boostProcess = processTable[*boostPID];
                pthread_mutex_unlock(&pcbLock);

                pthread_mutex_lock(&readyLock);
                addLast(boostProcess->pPID, &readyQueue[MAX_PRIORITY], &readyQueue[MAX_PRIORITY + 1]);
                pthread_mutex_unlock(&readyLock);

                printf("TXT: Boost: Process Id = %d, Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d \n",
                       *boostPID, boostProcess->iPriority, boostProcess->iPreviousBurstTime, boostProcess->iRemainingBurstTime);
            }
            priorityLevel++;
        }
    }
    return NULL;
}

/**
 * Thread at regular intervals
 * Remove from terminated queue and process table
 * Adds process PID back to free PID queue
 * Calculates process statistics
 */
void *terminationDaemon ()
{
    sem_wait(&processTerminate);

    while (terminatedProc < NUMBER_OF_PROCESSES)
    {
        mSleep(TERMINATION_INTERVAL);

        int *termPID;
        struct process *termProcess;

        while (terminatedQueueHead != NULL && terminatedQueueTail != NULL)
        {
            pthread_mutex_lock(&terminatedLock);
            termPID = (int *)removeFirst(&terminatedQueueHead, &terminatedQueueTail);
            pthread_mutex_unlock(&terminatedLock);

            pthread_mutex_lock(&pcbLock);
            termProcess = processTable[*termPID];
            pthread_mutex_unlock(&pcbLock);

            printf("TXT: Terminated: Process Id = %d, Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d \n",
                   *termPID, termProcess->iPriority, termProcess->iPreviousBurstTime, termProcess->iRemainingBurstTime);

            long responseTime = getDifferenceInMilliSeconds(termProcess->oTimeCreated, termProcess->oFirstTimeRunning);
            long turnaroundTime = getDifferenceInMilliSeconds(termProcess->oFirstTimeRunning, termProcess->oLastTimeRunning);

            averageRT += responseTime;
            averageTAT += turnaroundTime;

            free(termProcess);

            pthread_mutex_lock(&pcbLock);
            processTable[*termPID] = NULL;
            pthread_mutex_unlock(&pcbLock);

            pthread_mutex_lock(&freePIDLock);
            addLast(termPID, &freePIDHead, &freePIDTail);
            pthread_mutex_unlock(&freePIDLock);

            terminatedProc++;
        }
        sem_post(&processGenerate);
        if(terminatedProc == NUMBER_OF_PROCESSES)
        {
            averageRT *= 1000;
            averageTAT *= 1000;

            averageRT /= NUMBER_OF_PROCESSES;
            averageTAT /= NUMBER_OF_PROCESSES;

            printf("TXT: Average response time = %f, Average turnaround time = %f \n", (double) averageRT, (double) averageTAT);
        }
    }
    return NULL;
}

/**
 * Sleep for the requested number of milliseconds, using nano sleep
 * Returns 0 on successful sleep
 * Could change to a void function for this purpose
 * req = requested, rem = remaining, ret = return
 */
int mSleep(long mSec)
{
    struct timespec req, rem; //requested ts and remaining ts

    //L suffix maintains long / long double precision
    req.tv_sec = mSec / 1000; //convert mSec to value in timeval seconds, must be non-negative
    req.tv_nsec = (mSec % 1000) * 1000000L; //normalized value, since timeval nanoseconds must be in range of 0 to 999,999,999

    //initialize remaining time to 0
    rem.tv_sec = 0;
    rem.tv_nsec = 0;

    int ret = nanosleep(&req, &rem); //exit with 0 means successful sleep
    return ret;
}

// SVG functions below act as the visualization module for the process simulator

void printHeadersSVG()
{
    printf("SVG: <!DOCTYPE html>\n");
    printf("SVG: <html>\n");
    printf("SVG: <body>\n");
    printf("SVG: <svg width=\"13000\" height=\"2000\">\n"); //can change height from 1100 to 2000 and width from 10000 to 13000
}

void printProcessSVG(int iCPUId, struct process *pProcess, struct timeval oStartTime, struct timeval oEndTime)
{
    int iXOffset = getDifferenceInMilliSeconds(oBaseTime, oStartTime) + 30;
    int iYOffsetPriority = (pProcess->iPriority + 1) * 16 - 12;
    int iYOffsetCPU = (iCPUId - 1) * (480 + 50);
    int iWidth = getDifferenceInMilliSeconds(oStartTime, oEndTime);
    printf("SVG: <rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"8\" style=\"stroke:rgb(255, 255, 255); stroke-width:1; fill:rgb(%d, 0, %d);\"/>\n", iXOffset /* x */, iYOffsetCPU + iYOffsetPriority /* y */, iWidth, *(pProcess->pPID) - 1 /* rgb */, *(pProcess->pPID) - 1 /* rgb */);
}

void printPrioritiesSVG()
{
    for(int iCPU = 1; iCPU <= NUMBER_OF_CPUS; iCPU++)
    {
        for(int iPriority = 0; iPriority < MAX_PRIORITY; iPriority++)
        {
            int iYOffsetPriority = (iPriority + 1) * 16 - 4;
            int iYOffsetCPU = (iCPU - 1) * (480 + 50);
            printf("SVG: <text x=\"0\" y=\"%d\" fill=\"black\">%d</text>\n", iYOffsetCPU + iYOffsetPriority, iPriority);
        }
    }
}

void printRasterSVG()
{
    for(int iCPU = 1; iCPU <= NUMBER_OF_CPUS; iCPU++)
    {
        for(int iPriority = 0; iPriority < MAX_PRIORITY; iPriority++)
        {
            int iYOffsetPriority = (iPriority + 1) * 16 - 8;
            int iYOffsetCPU = (iCPU - 1) * (480 + 50);
            printf("SVG: <line x1=\"%d\" y1=\"%d\" x2=\"13000\" y2=\"%d\" style=\"stroke:rgb(125, 125, 125); stroke-width:1;\"/>\n", 16, iYOffsetCPU + iYOffsetPriority, iYOffsetCPU + iYOffsetPriority);
        }
    }
}

void printFootersSVG()
{
    printf("SVG: Sorry, your browser does not support inline SVG.\n");
    printf("SVG: </svg>\n");
    printf("SVG: </body>\n");
    printf("SVG: </html>\n");
}
