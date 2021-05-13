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





#define ACTION_MUTEX_NAME "/actionCountingSemaphore"
#define WORKSHOP_QUEUE_SIG "/WSSIG"
#define HELP_ELVES_SIG "HESIG"
#define SANTA_HITCH "/heyoldfella"
#define LAST_CHRISTMAS_I_GAVE_YOU_MY_HEART "/butTheVeryNextDay"
#define SLEEPING_DOGS "/santaUDrunk"





#define ARGS_PARSE_ERROR 1
#define SHMEM_CREATE_ERROR 2
#define SHMEM_ATTACH_ERROR 3
#define PROC_FORK_ERROR 4




// TODO: FTOK CREATING OF KEY
#define SHARED_MEM_KEY 5454





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





typedef struct{
    int shmid;
    int actionCounter;
    int readyDeers;
    int waitingElves;
    int deersToHitch;
    int elvesToHelp;
    bool allDeersStatus;
    bool workshopStatus; /* Is workshop opened or closed? */
    sem_t* actionCounterMutex;
    sem_t* wShopQueueSig;
    sem_t* elfGotHelp;
    sem_t* santaHitchDeer;
    sem_t* santaGoSleep;
    sem_t* christmasStartSig;
} sharedMem_t;





int main(int argc, char** argv) {
    int elves;
    int rDeers;
    int elvesTime;
    int rDeersTime;

    argsParsing(argc, argv, &elves, &rDeers, &elvesTime, &rDeersTime);

    FILE *outputFile = fopen("proj2.out", "w");
    if(outputFile == NULL)
    {
        fprintf(stderr, "ERROR: Couldn't create file\n");
        exit(5);
    }
    setbuf(outputFile, 0);

    fprintf(stderr,"After fileopen\n");


    sem_unlink(ACTION_MUTEX_NAME);
    sem_unlink(WORKSHOP_QUEUE_SIG);
    sem_unlink(HELP_ELVES_SIG);
    sem_unlink(SANTA_HITCH);
    sem_unlink(LAST_CHRISTMAS_I_GAVE_YOU_MY_HEART);
    sem_unlink(SLEEPING_DOGS);


    key_t key = ftok("proj2.c", 0);
    if(key == -1)
    {
        fprintf(stderr,"ERROR: Couldn't create key\n");
        return 5;
    }

    /* Create shared memory */
    int shmid = shmget(key, sizeof(sharedMem_t), IPC_CREAT | 0666);
    if (shmid < 0) {
        fprintf(stderr,"ERROR: Shared memory couldn't be crated.\n");
        return SHMEM_CREATE_ERROR;
    }

    /* Attach shared memory */
    sharedMem_t *mainBlock = shmat(shmid, NULL, 0);
    if (mainBlock == (sharedMem_t *) -1) {
        fprintf(stderr,"Shared memory couldn't be attached");
        return SHMEM_ATTACH_ERROR;
    }


    /* Setting up ID of shared memory */
    mainBlock->shmid = shmid;
    /* Setting up necessary variables */
    mainBlock->workshopStatus = false;
    mainBlock->actionCounter = 0;
    mainBlock->readyDeers = 0;
    mainBlock->elvesToHelp = 0;
    mainBlock->waitingElves = 0;
    mainBlock->deersToHitch = rDeers;
    mainBlock->allDeersStatus = false;

    /* Setting up semaphores */
    mainBlock->actionCounterMutex = malloc(sizeof(sem_t));
    if(mainBlock->actionCounterMutex == NULL)
    {
        fprintf(stderr,"ERROR: Couldn't allocate memory\n");
        return 6;
    }
    mainBlock->actionCounterMutex = sem_open(ACTION_MUTEX_NAME, IPC_CREAT, 0666, 1);

    mainBlock->wShopQueueSig = malloc(sizeof(sem_t));
    if(mainBlock->wShopQueueSig == NULL)
    {
        fprintf(stderr,"ERROR: Couldn't allocate memory\n");
        return 6;
    }


    mainBlock->wShopQueueSig = sem_open(WORKSHOP_QUEUE_SIG, IPC_CREAT, 0666, 0);
    if(mainBlock->wShopQueueSig < (sem_t*) 0)
    {
        fprintf(stderr,"ERROR: Couldn't create semaphore\n");
        return 7;
    }

    mainBlock->elfGotHelp = malloc(sizeof(sem_t));
    if(mainBlock->elfGotHelp == NULL)
    {
        fprintf(stderr,"ERROR: Couldn't allocate memory\n");
        return 6;
    }

    mainBlock->elfGotHelp = sem_open(HELP_ELVES_SIG, IPC_CREAT, 0666, 0);
    if(mainBlock->elfGotHelp < (sem_t*) 0)
    {
        fprintf(stderr,"ERROR: Couldn't create semaphore\n");
        return 7;
    }

    mainBlock->santaGoSleep = malloc(sizeof(sem_t));
    if(mainBlock->santaGoSleep == NULL)
    {
        fprintf(stderr,"ERROR: Couldn't allocate memory\n");
        return 6;
    }
    mainBlock->santaGoSleep = sem_open(SLEEPING_DOGS, IPC_CREAT, 0666, 0);
    if(mainBlock->santaGoSleep < (sem_t*) 0)
    {
        fprintf(stderr,"ERROR: Couldn't create semaphore\n");
        return 7;
    }


    mainBlock->santaHitchDeer = malloc(sizeof(sem_t));
    if(mainBlock->santaHitchDeer == NULL)
    {
        fprintf(stderr,"ERROR: Couldn't allocate memory\n");
        return 6;
    }
    mainBlock->santaHitchDeer = sem_open(SANTA_HITCH, IPC_CREAT, 0666, 0);
    if(mainBlock->santaHitchDeer < (sem_t*) 0)
    {
        fprintf(stderr,"ERROR: Couldn't create semaphore\n");
        return 7;
    }


    mainBlock->christmasStartSig = malloc(sizeof(sem_t));
    if(mainBlock->christmasStartSig == NULL)
    {
        fprintf(stderr,"ERROR: Couldn't allocate memory\n");
        return 6;
    }
    mainBlock->christmasStartSig = sem_open(LAST_CHRISTMAS_I_GAVE_YOU_MY_HEART, IPC_CREAT, 0666, 0);
    if(mainBlock->christmasStartSig < (sem_t*) 0)
    {
        fprintf(stderr,"ERROR: Couldn't create semaphore\n");
        return 7;
    }




    int mainProcID = fork();
    if (mainProcID == -1)
    {
        printf("ERROR: Main process can't be forked");
        return 4;
    }
    if (mainProcID == 0)
    {
        //Santa process
        sem_wait(mainBlock->actionCounterMutex);
        fprintf(outputFile,"%d: Santa: going to sleep\n", ++mainBlock->actionCounter);
        sem_post(mainBlock->actionCounterMutex);

        if(elves >= 3)
        {
            while(mainBlock->allDeersStatus == false)
            {
                sem_wait(mainBlock->wShopQueueSig);
                if (mainBlock->allDeersStatus == true) break;
                sem_wait(mainBlock->wShopQueueSig);
                if (mainBlock->allDeersStatus == true) break;
                sem_wait(mainBlock->wShopQueueSig);
                if (mainBlock->allDeersStatus == true) break;

                mainBlock->elvesToHelp = 3;

                sem_wait(mainBlock->actionCounterMutex);
                fprintf(outputFile,"%d: Santa: helping elves\n", ++mainBlock->actionCounter);
                sem_post(mainBlock->actionCounterMutex);
                sem_post(mainBlock->elfGotHelp);
                mainBlock->waitingElves--;
                sem_post(mainBlock->elfGotHelp);
                mainBlock->waitingElves--;
                sem_post(mainBlock->elfGotHelp);
                mainBlock->waitingElves--;


                sem_wait(mainBlock->santaGoSleep);
                sem_wait(mainBlock->santaGoSleep);
                sem_wait(mainBlock->santaGoSleep);

                sem_wait(mainBlock->actionCounterMutex);
                fprintf(outputFile,"%d: Santa: going to sleep\n", ++mainBlock->actionCounter);
                sem_post(mainBlock->actionCounterMutex);

            }
        }


        sem_wait(mainBlock->christmasStartSig);
        sem_wait(mainBlock->actionCounterMutex);
        fprintf(outputFile,"%d: Santa: closing workshop\n", ++mainBlock->actionCounter);
        mainBlock->workshopStatus = true;
        sem_post(mainBlock->actionCounterMutex);
        sem_post(mainBlock->elfGotHelp);

        sem_post(mainBlock->santaHitchDeer);


        sem_wait(mainBlock->christmasStartSig);
        sem_wait(mainBlock->actionCounterMutex);
        fprintf(outputFile,"%d: Santa: Christmas started\n", ++mainBlock->actionCounter);
        sem_post(mainBlock->actionCounterMutex);



        exit(0);
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
                return PROC_FORK_ERROR;
            }

            if(elfProcID == 0)
            {

                srand(time(0) + i * 228);

                sem_wait(mainBlock->actionCounterMutex);
                fprintf(outputFile,"%d: Elf %d: started\n", ++mainBlock->actionCounter, i + 1);
                sem_post(mainBlock->actionCounterMutex);


                while (1)
                {

                    // TODO: Тернарный оператор
                    if(elvesTime != 0)
                    {
                        usleep((rand() % elvesTime) * 1000);
                    }

                    sem_wait(mainBlock->actionCounterMutex);
                    fprintf(outputFile,"%d: Elf %d: need help\n", ++mainBlock->actionCounter, i + 1);
                    sem_post(mainBlock->actionCounterMutex);
                    mainBlock->waitingElves++;
                    sem_post(mainBlock->wShopQueueSig);



                    sem_wait(mainBlock->elfGotHelp);
                    /* checking if workshop is closed  */
                    if(mainBlock->workshopStatus == true)
                    {
                        sem_wait(mainBlock->actionCounterMutex);
                        fprintf(outputFile,"%d: Elf %d: taking holidays\n", ++mainBlock->actionCounter, i + 1);
                        sem_post(mainBlock->actionCounterMutex);
                        sem_post(mainBlock->elfGotHelp);
                        break;
                    }

                    sem_wait(mainBlock->actionCounterMutex);
                    fprintf(outputFile,"%d: Elf %d: get help\n", ++mainBlock->actionCounter, i + 1);
                    sem_post(mainBlock->actionCounterMutex);

                    sem_post(mainBlock->santaGoSleep);

                }



                exit(0);
            }
        }

        int deerProcID;
        for(int i = 0; i < rDeers; i++)
        {
            deerProcID = fork();
            if(deerProcID == -1)
            {
                fprintf(stderr, "ERROR: Reindeer process couldn't be created\n");
                return PROC_FORK_ERROR;
            }
            if(deerProcID == 0)
            {
                // Reindeer processes

                srand(time(0) + i * 1337);

                sem_wait(mainBlock->actionCounterMutex);
                fprintf(outputFile,"%d: RD %d: rstarted\n", ++mainBlock->actionCounter, i + 1);
                sem_post(mainBlock->actionCounterMutex);

                if(rDeersTime != 0)
                {
                    usleep(((rand() % (rDeersTime/2)) + rDeersTime/2) * 1000);
                }
                sem_wait(mainBlock->actionCounterMutex);
                fprintf(outputFile,"%d: RD %d: return home\n", ++mainBlock->actionCounter, i + 1);
                mainBlock->readyDeers++;
                if(mainBlock->readyDeers == rDeers)
                {
                    mainBlock->allDeersStatus = true;
                    sem_post(mainBlock->christmasStartSig);
                }
                sem_post(mainBlock->actionCounterMutex);



                sem_wait(mainBlock->santaHitchDeer);
                sem_wait(mainBlock->actionCounterMutex);
                fprintf(outputFile,"%d: RD %d: get hitched\n", ++mainBlock->actionCounter, i + 1);
                mainBlock->deersToHitch--;
                sem_post(mainBlock->actionCounterMutex);

                sem_post(mainBlock->santaHitchDeer);

                if(mainBlock->deersToHitch == 0) sem_post(mainBlock->christmasStartSig);

                exit(0);
            }
        }
    }

    while(wait(NULL) >= 0);

    fprintf(stderr,"After ending while\n");

    fclose(outputFile);


    sem_close(mainBlock->actionCounterMutex);
    sem_close(mainBlock->wShopQueueSig);
    sem_close(mainBlock->elfGotHelp);
    sem_close(mainBlock->santaHitchDeer);
    sem_close(mainBlock->christmasStartSig);
    sem_close(mainBlock->santaGoSleep);





    //Remove the shared memory
    shmid = mainBlock->shmid;
    if(shmdt(mainBlock)){
        return 1;
    }
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}
