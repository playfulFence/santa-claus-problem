#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>


/* Define names of semaphores */
#define ACTION_MUTEX_NAME "/actionCountingSemaphore"
#define WORKSHOP_QUEUE_SIG "/WSSIG"
#define HELP_ELVES_SIG "HESIG"
#define SANTA_HITCH "/heyoldfella"
#define LAST_CHRISTMAS_I_GAVE_YOU_MY_HEART "/butTheVeryNextDay"
#define SLEEPING_DOGS "/santaUDrunk"


/* Define some numbers of errors */
#define ARGS_PARSE_ERROR 2
#define SHMEM_CREATE_ERROR 3
#define SHMEM_ATTACH_ERROR 4
#define PROC_FORK_ERROR 5
#define GET_ID_ERROR 6
#define FILE_OPEN_ERROR 7


/* Shared struct */
typedef struct{
    int shmid; /* ID of shared memory */
    int actionCounter; /* Counter of actions */
    int readyDeers; /*  */
    int deersToHitch;
    bool allDeersStatus;
    bool workshopStatus; /* Is workshop opened or closed? */
    sem_t actionCounterMutex; /* To count actions correctly */
    sem_t wShopQueueSig; /* To manage the queue in front of workshop */
    sem_t elfGotHelp; /* Signal to elves */
    sem_t santaHitchDeer; /* To manage deers hitching */
    sem_t santaGoSleep; /* Signal to Santa to go sleep */
    sem_t christmasStartSig; /* Signal, that it's time to start Christmas */
} sharedMem_t;


/* Control of wrong input format */
void argsParsing(int argc, char** argv, int* elves, int* rDeers, int* elvesTime, int* rDeersTime)
{
    if(argc != 5)
    {
        printf("ERROR: Wrong number of arguments!\nFormat: ./proj2 NE NR TE TR\n"
               "NE - number of elves (0 < NE < 1000)\n"
               "NR - number of reindeers (0 < NR < 1000)\n"
               "TE - maximal time(ms), which elf working independently (0 <= TE <= 1000)\n"
               "TR - maximal time(ms), for which reindeer will return home from vacation (0 <= TR <= 1000)\n");
        exit(ARGS_PARSE_ERROR);
    }
    *elves = atoi(argv[1]);
    *rDeers = atoi(argv[2]);
    *elvesTime = atoi(argv[3]);
    *rDeersTime = atoi(argv[4]);

    if(((*elves <= 0) || (*elves >= 1000)) ||
       ((*rDeers <= 0) || (*rDeers >= 1000)) ||
       ((*elvesTime < 0) || (*elvesTime > 1000)) ||
       ((*rDeersTime < 0) || (*rDeersTime > 1000)))
    {
        fprintf(stderr,"ERROR: Wrong value in one of arguments!\nFormat: ./proj2 NE NR TE TR\n"
               "NE - number of elves (0 < NE < 1000)\n"
               "NR - number of reindeers (0 < NR < 1000)\n"
               "TE - maximal time(ms), which elf working independently (0 <= TE <= 1000)\n"
               "TR - maximal time(ms), for which reindeer will return home from vacation (0 <= TR <= 1000)\n");
        exit(ARGS_PARSE_ERROR);
    }
}


FILE* openFile()
{
    FILE *outputFile = fopen("proj2.out", "w");
    if(outputFile == NULL)
    {
        fprintf(stderr, "ERROR: Couldn't create file\n");
        exit(FILE_OPEN_ERROR);
    }

    return outputFile;
}

/* Gets ID of shared memory to be created */
int getID(int* shmid)
{
    /* Receive key*/
    key_t key = ftok("proj2.out", 0);
    if(key == -1)
    {
        fprintf(stderr,"ERROR: Couldn't create key\n");
        return GET_ID_ERROR;
    }

    /* Get ID of shared memory from the key and create shared memory. Size of our struct. */
    *shmid = shmget(key, sizeof(sharedMem_t), IPC_CREAT | 0666);
    if (*shmid < 0) {
        fprintf(stderr,"ERROR: Shared memory couldn't be crated.\n");
        return GET_ID_ERROR;
    }

    return 1;
}


/* Unlink all previous connections of semaphores */
void unlinkPrev()
{
    sem_unlink(ACTION_MUTEX_NAME);
    sem_unlink(WORKSHOP_QUEUE_SIG);
    sem_unlink(HELP_ELVES_SIG);
    sem_unlink(SANTA_HITCH);
    sem_unlink(LAST_CHRISTMAS_I_GAVE_YOU_MY_HEART);
    sem_unlink(SLEEPING_DOGS);
}


void setUpVars(sharedMem_t* block, int shmid, int rDeers)
{
    /* Setting up ID of shared memory */
    block->shmid = shmid;
    /* Setting up necessary variables */
    block->workshopStatus = false;
    block->actionCounter = 0;
    block->readyDeers = 0;
    block->deersToHitch = rDeers;
    block->allDeersStatus = false;
}

/* Initialize all semaphores */
void semInit(sharedMem_t* block)
{
    sem_init(&block->actionCounterMutex, 1, 1);

    sem_init(&block->wShopQueueSig, 1, 0);

    sem_init(&block->elfGotHelp, 1, 0);

    sem_init(&block->santaGoSleep, 1, 0);

    sem_init(&block->santaHitchDeer, 1, 0);

    sem_init(&block->christmasStartSig, 1, 0);
}

void santaProcess(sharedMem_t * mainBlock, FILE* outputFile, int elves)
{
    /* SANTA PROCESS */
    sem_wait(&mainBlock->actionCounterMutex);
    fprintf(outputFile, "%d: Santa: going to sleep\n", ++mainBlock->actionCounter);
    sem_post(&mainBlock->actionCounterMutex);

    /* If we have ONLY 1 or 2 elves, Santa won't be waked up by elves */
    if (elves >= 3)
    {
        /* This behaviour will repeat till all deers not home */
        while (mainBlock->allDeersStatus == false)
        {
            /* Waiting 3 elves in queue, then waking up */
            sem_wait(&mainBlock->wShopQueueSig);
            if (mainBlock->allDeersStatus == true) break;
            sem_wait(&mainBlock->wShopQueueSig);
            if (mainBlock->allDeersStatus == true) break;
            sem_wait(&mainBlock->wShopQueueSig);
            if (mainBlock->allDeersStatus == true) break;

            /* Helping elves after being waked up by them */
            sem_wait(&mainBlock->actionCounterMutex);
            fprintf(outputFile, "%d: Santa: helping elves\n", ++mainBlock->actionCounter);
            sem_post(&mainBlock->actionCounterMutex);

            /* Let dat elvs no, dat dey gat helpd, u dunno, nocap */
            sem_post(&mainBlock->elfGotHelp);
            sem_post(&mainBlock->elfGotHelp);
            sem_post(&mainBlock->elfGotHelp);

            /* If elf received help and it wrote down,
             * then send signal to Santa via this semaphore.
             * Made to keep right sequence of actions */
            sem_wait(&mainBlock->santaGoSleep);
            sem_wait(&mainBlock->santaGoSleep);
            sem_wait(&mainBlock->santaGoSleep);

            sem_wait(&mainBlock->actionCounterMutex);
            fprintf(outputFile, "%d: Santa: going to sleep\n", ++mainBlock->actionCounter);
            sem_post(&mainBlock->actionCounterMutex);

        }

        /* I used this semaphore to not create another one.
         * Waiting for signal from deers, that all deers arrived from holidays*/
        sem_wait(&mainBlock->christmasStartSig);
        sem_wait(&mainBlock->actionCounterMutex);
        fprintf(outputFile,"%d: Santa: closing workshop\n", ++mainBlock->actionCounter);
        mainBlock->workshopStatus = true;
        sem_post(&mainBlock->actionCounterMutex);

        /* Also to not create another one semaphore
         * I just use one. Give elves know, that workshop is closed*/
        sem_post(&mainBlock->elfGotHelp);

        /* Santa sends signal do deers, that he's going to hitch them */
        sem_post(&mainBlock->santaHitchDeer);


        /* Waiting for signal from Deers, that every of them is hitched */
        sem_wait(&mainBlock->christmasStartSig);
        sem_wait(&mainBlock->actionCounterMutex);
        fprintf(outputFile,"%d: Santa: Christmas started\n", ++mainBlock->actionCounter);
        sem_post(&mainBlock->actionCounterMutex);


        exit(0);
    }
}

void elfProcess(sharedMem_t* mainBlock, FILE* outputFile, int elvesTime, int i)
{
    /* Setting rand() function for every process */

    srand(time(0) + i * 228);

    sem_wait(&mainBlock->actionCounterMutex);
    fprintf(outputFile,"%d: Elf %d: started\n", ++mainBlock->actionCounter, i + 1);
    sem_post(&mainBlock->actionCounterMutex);


    while (1)
    {

        if(elvesTime != 0)
        {
            /* Simulation of independent work */
            usleep((rand() % elvesTime) * 1000);
        }

        sem_wait(&mainBlock->actionCounterMutex);
        fprintf(outputFile,"%d: Elf %d: need help\n", ++mainBlock->actionCounter, i + 1);
        sem_post(&mainBlock->actionCounterMutex);
        /* Stood in line "in front of workshop"(so, kinda) */
        sem_post(&mainBlock->wShopQueueSig);

        /* Waiting for Santa to help OR getting signal, that workshop is closed */
        sem_wait(&mainBlock->elfGotHelp);

        /* checking if workshop is closed. If it's not, elf just getting help from Santa */
        if(mainBlock->workshopStatus == true)
        {
            sem_wait(&mainBlock->actionCounterMutex);
            fprintf(outputFile,"%d: Elf %d: taking holidays\n", ++mainBlock->actionCounter, i + 1);
            sem_post(&mainBlock->actionCounterMutex);
            /* Sending signal to the next elf (so as not to complicate Santa process (LAZINESS IS DA KEY!!!)) */
            sem_post(&mainBlock->elfGotHelp);
            break;
        }

        sem_wait(&mainBlock->actionCounterMutex);
        fprintf(outputFile,"%d: Elf %d: get help\n", ++mainBlock->actionCounter, i + 1);
        sem_post(&mainBlock->actionCounterMutex);

        /* aka "BOSS , THANK YOU, APPRECIATE THAT, GO TO SLEEP" signal */
        sem_post(&mainBlock->santaGoSleep);

    }

    exit(0);
}


void deerProcess(sharedMem_t* mainBlock, FILE* outputFile, int rDeersTime, int rDeers, int i)
{
    /* REINDEER PROCESS */

    /* Setting rand() function for every process */
    srand(time(0) + i * 1337);

    sem_wait(&mainBlock->actionCounterMutex);
    fprintf(outputFile,"%d: RD %d: rstarted\n", ++mainBlock->actionCounter, i + 1);
    sem_post(&mainBlock->actionCounterMutex);

    if(rDeersTime != 0)
    {
        /* Simulating time on holidays */
        usleep(((rand() % (rDeersTime/2)) + rDeersTime/2) * 1000);
    }
    sem_wait(&mainBlock->actionCounterMutex);
    fprintf(outputFile,"%d: RD %d: return home\n", ++mainBlock->actionCounter, i + 1);
    /* Increments counter of deers, which are already home */
    mainBlock->readyDeers++;
    /* If all deers are home... */
    if(mainBlock->readyDeers == rDeers)
    {
        mainBlock->allDeersStatus = true;
        /* aka "HITCH US, PLEASE. WE WANT TO DELIVER HAPPINESS!!!" signal */
        sem_post(&mainBlock->christmasStartSig);
    }
    sem_post(&mainBlock->actionCounterMutex);



    sem_wait(&mainBlock->santaHitchDeer);
    sem_wait(&mainBlock->actionCounterMutex);
    fprintf(outputFile,"%d: RD %d: get hitched\n", ++mainBlock->actionCounter, i + 1);
    mainBlock->deersToHitch--;
    sem_post(&mainBlock->actionCounterMutex);

    /* Sends signal to the next deer(also, so as not to complicate Santa process */
    sem_post(&mainBlock->santaHitchDeer);

    /* All deers are hitched, so... KAWABANGA!!! */
    if(mainBlock->deersToHitch == 0) sem_post(&mainBlock->christmasStartSig);

    exit(0);
}


/* Close opened file and destroy all the semaphores */
void clearRes(sharedMem_t* mainBlock, FILE* outputFile)
{
    fclose(outputFile);

    sem_destroy(&mainBlock->actionCounterMutex);
    sem_destroy(&mainBlock->wShopQueueSig);
    sem_destroy(&mainBlock->elfGotHelp);
    sem_destroy(&mainBlock->santaHitchDeer);
    sem_destroy(&mainBlock->christmasStartSig);
    sem_destroy(&mainBlock->santaGoSleep);
}




int main(int argc, char** argv)
{
    unlinkPrev();

    int elves;
    int rDeers;
    int elvesTime;
    int rDeersTime;
    int shmid = 0;


    argsParsing(argc, argv, &elves, &rDeers, &elvesTime, &rDeersTime);

    FILE *outputFile = openFile();
    /* To keep correct printing */
    setbuf(outputFile, 0);

    if (getID(&shmid) == GET_ID_ERROR) exit(SHMEM_CREATE_ERROR);

    sharedMem_t *mainBlock = shmat(shmid, NULL, 0);
    if (mainBlock == (sharedMem_t *) -1) {
        fprintf(stderr,"Shared memory couldn't be attached");
        exit(SHMEM_ATTACH_ERROR);  
    }

    setUpVars(mainBlock, shmid, rDeers);

    semInit(mainBlock);



    int mainProcID = fork();
    if (mainProcID == -1)
    {
        printf("ERROR: Main process can't be forked");
        clearRes(mainBlock, outputFile);
        return PROC_FORK_ERROR;
    }
    if (mainProcID == 0)
    {
        santaProcess(mainBlock, outputFile, elves);
    }
    else
    {
        int elfProcID;
        for(int i = 0; i < elves; i++)
        {
            elfProcID = fork();
            if(elfProcID == -1)
            {
                fprintf(stderr, "ERROR: Elf process couldn't be created\n");
                clearRes(mainBlock, outputFile);
                return PROC_FORK_ERROR;
            }

            if(elfProcID == 0)
            {
                elfProcess(mainBlock, outputFile, elvesTime, i);
            }
        }
        int deerProcID;
        for(int i = 0; i < rDeers; i++)
        {
            deerProcID = fork();
            if(deerProcID == -1)
            {
                fprintf(stderr, "ERROR: Reindeer process couldn't be created\n");
                clearRes(mainBlock, outputFile);
                return PROC_FORK_ERROR;
            }
            if(deerProcID == 0)
            {
                deerProcess(mainBlock, outputFile, rDeersTime, rDeers, i);
            }
        }
    }

    /* Wait till every process is done */
    while(wait(NULL) >= 0);

    clearRes(mainBlock, outputFile);

    /*Remove the shared memory*/
    shmid = mainBlock->shmid;

    if(shmdt(mainBlock)){
        return 1;
    }
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}
