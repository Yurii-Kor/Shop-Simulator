#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>

//======== Semaphores ==================================================
key_t getKey(const char *pathname, int proj_id);
int createSemaphore(key_t key, const char *semName);
void removeSemaphore(int semid, const char *semName);
void lock(int semid, const char *semName);
void unlock(int semid, const char *semName);

void writerLock(int semidOrder, const char *orderDeskription,
                int semidAccess, const char *accessDeskription,
                int semidLockWriters, const char *lockWritersDeskription,
                int *waitingWriters);

void readerLock(int semidOrder, const char *orderDeskription,
                int semidReaders, const char *readersDeskription,
                int semidAccess, const char *accessDeskription,
                int semidLockWriters, const char *lockWritersDeskription,
                int *countReaders,
                int *waitingWriters);

void readerUnlock(int semidReaders, const char *readersDeskription,
                  int semidAccess, const char *accessDeskription,
                  int *countReaders);

//======== Buying Board ================================================
struct buyingBoard {
    int boardId;
    int customerId;
    int itemId;
    int amount;
    int done;
};

struct buyingBoard* initializeBuyingBoards(int* countBoards);
struct buyingBoard* addBuyingBoard(struct buyingBoard* buyingBoards, int* countBoards, int customerId, int itemId, int amount);
int makeDone(struct buyingBoard* buyingBoards, int* countBoards, int boardId);
void printBuyingBoards(struct buyingBoard* buyingBoards, int* countBoards);
int getNotDoneProductId(struct buyingBoard* buyingBoards, int* countBoards);
int checkIfLastOrderDone(struct buyingBoard* buyingBoards, int* countBoards, int customerId);
int getAmountByBoardId(struct buyingBoard* buyingBoards, int* countBoards, int boardId);
int getItemIdByBoardId(struct buyingBoard* buyingBoards, int* countBoards, int boardId);
int getCountNotDone(struct buyingBoard* buyingBoards, int* countBoards);

//======== Products =================================================
struct item {
    int index;
    char *name;
    float price;
    int totalOrders;
};

struct item *initializeProducts(int countProducts);
struct item *updateTotalOrders(struct item *products, int countProducts, int index, int amount);
void printProducts(struct item *products, int countProducts);
void printMenu(struct item *products, int countProducts);
void printChoice(int id, struct item *products, int countProducts, int itemId, int amount);
int getRandomItem(struct item *products, int countProducts);
float calculateIncome(struct item *products, int countProducts);

//======== Thread functions ==========================================
struct readBuyingBoardsRequest {
    struct timespec start;
    int id;
    char *accountType;
    int semidAccess;
    char *accessDeskription;
    int semidReaders;
    char *readersDeskription;
    int semidOrder;
    char *orderDeskription;
    int semidWriters;
    const char *writersDeskription;
    int *countWriters;
    int *countReaders;
    int *countBoards;
    int *currentCountBoards;
    int *shared_memory_CountBoards;
    struct buyingBoard *currentBuyingBoards;
    struct buyingBoard *shared_memory_BuyingBoard;
};

struct updateBuyingBoardsRequest {
    struct timespec start;
    int id;
    int semidAccess;
    char *accessDeskription;
    int semidOrder;
    char *orderDeskription;
    int semidLockWriters;
    const char *lockWritersDeskription;
    int *countWriters;
    int *currentCountBoards;
    int itemId;
    int amountOrder;
    int *notDoneBoardId;
    int *notDoneAmount;
    int *notDoneItemId;
    int *shared_memory_CountBoards;
    struct buyingBoard *shared_memory_BuyingBoard;
};

struct readMenuRequest {
    struct timespec start;
    int id;
    int semidAccess;
    char *accessDeskription;
    int semidReaders;
    char *readersDeskription;
    int semidOrder;
    char *orderDeskription;
    int semidWriters;
    const char *writersDeskription;
    int *countWriters;
    int *countReaders;
    int *countProducts;
    struct item *currentProducts;
    struct item *shared_memory_Products;
};

struct UpdateProductsRequest {
    struct timespec start;
    int id;
    int semidAccess;
    char *accessDeskription;
    int semidOrder;
    char *orderDeskription;
    int semidLockWriters;
    const char *lockWritersDeskription;
    int *countWriters;
    int *countProducts;
    int itemId;
    int amountOrder;
    struct item *shared_memory_Products;
};

void *thread_readMenu(void *arg);
void *thread_updateTotalOrders(void *arg);
void* thread_readBuyingBoards(void* arg);
void* thread_addBuyingBoard(void* arg);
void* thread_threadMakeDone(void* arg);

//======== Sygnal for subprocess =================================
volatile sig_atomic_t stop = 0;
void signalHandler(int sig) {
    stop = 1;
    printf("Process %d received signal %d, terminating...\n", getpid(), sig);
    exit(0);
}

//======== Others =================================================
void increment(int *value);
void decrement(int *value);
int getRandomNum(int minNum, int maxNum);
struct timespec getCurrentTime();
void printCurrentTime(struct timespec currentTime);
double timer(struct timespec startTime);
void readArguments(int *countProducts, int *countAgents, int *countCustomers, int argc, char *argv[]);


//======== Main ====================================================
int main(int argc, char const *argv[]) {
    int simulationTime = 29;
    int countProducts = 10;
    int countBoards = 0;
    int countCustomers = 4;
    int countAgents = 2;
    int childCount = 0;
    pid_t childPIDs[countCustomers + countAgents];

    readArguments(&countProducts, &countAgents, &countCustomers, argc, (char **)argv);

    srand(time(NULL));

        //  keys :
    key_t keyShMemBuyingBoard = getKey("keys/keyShMemBuyingBoard.txt", 150);
    key_t keyShMemCountBoards = getKey("keys/keyShMemCountBoards.txt", 151);
    key_t keyAccessBuyingBoard = getKey("keys/keyAccessBuyingBoard.txt", 152);
    key_t keyReadersBuyingBoard = getKey("keys/keyReadersBuyingBoard.txt", 153);
    key_t keyOrderBuyingBoard = getKey("keys/keyOrderBuyingBoard.txt", 154);
    key_t keyWritersBuyingBoard = getKey("keys/keyLockWritersBuyingBoard.txt", 155);

    key_t keyProducts = getKey("keys/keyShMemProducts.txt", 160);
    key_t keyAccessProducts = getKey("keys/keyAccessProducts.txt", 161);
    key_t keyReadersProducts = getKey("keys/keyReadersProducts.txt", 162);
    key_t keyOrderProducts = getKey("keys/keyOrderProducts.txt", 163);
    key_t keyWritersProducts = getKey("keys/keyLockWritersProducts.txt", 164);

    printf("\nProject info:\n\nCustomers       : %2d persons\nSales Agent     : %2d persons\nCount Products  : %2d items\nSimulation time : %2d sec\n\n",
                        countCustomers, countAgents, countProducts, simulationTime);
    printf("Hello! I am the Manager or the \"Main Process\", my pid = %d\n", getpid());


    //=========pipes for BuyingBoards & Products=============================
    int buyingBoards_pipe[2];
    int products_pipe[2];

    if (pipe(buyingBoards_pipe) == -1 || pipe(products_pipe) == -1) {
        perror("pipe creation failed");
        exit(1);
    }

    //=========BuyingBoard==================================================
        //  Intialize :
    int done;
    struct buyingBoard* buyingBoards = initializeBuyingBoards(&countBoards);
    for (int i = 0; i < countCustomers; i++) {
        buyingBoards = addBuyingBoard(buyingBoards, &countBoards, i, 1, 1);
        done = makeDone(buyingBoards, &countBoards, i);
    }

        //  Shared memory :
    int shmidBuyingBoard = shmget(keyShMemBuyingBoard, 100 * sizeof(struct buyingBoard), IPC_CREAT | 0666);
    if (shmidBuyingBoard == -1) {
        perror("shmget_create_BuyingBoard");
        exit(1);
    }

    struct buyingBoard *shared_memory_BuyingBoard = (struct buyingBoard *)shmat(shmidBuyingBoard, NULL, 0);
    if (shared_memory_BuyingBoard == (void *)-1) {
        perror("shmat_BuyingBoard");
        exit(1);
    }

    write(buyingBoards_pipe[1], buyingBoards, countBoards * sizeof(struct buyingBoard));
    read(buyingBoards_pipe[0], shared_memory_BuyingBoard, countBoards * sizeof(struct buyingBoard));
    free(buyingBoards);

    int shmidCountBoards = shmget(keyShMemCountBoards, sizeof(int), IPC_CREAT | 0666);
    if (shmidCountBoards == -1) {
        perror("shmget_create_CountBoards");
        exit(1);
    }

    int *shared_memory_CountBoards = (int *)shmat(shmidCountBoards, NULL, 0);
    if (shared_memory_CountBoards == (void *)-1) {
        perror("shmat_CountBoards");
        exit(1);
    }

    memcpy(shared_memory_CountBoards, &countBoards, sizeof(int));

        //  Semaphores :
    int countReadersBuyingBoard = 0;
    int countWritersBuyingBoard = 0;

    int semidAccessBuyingBoard = createSemaphore(keyAccessBuyingBoard, "AccessBuyingBoard");
    unlock(semidAccessBuyingBoard, "AccessBuyingBoard");

    int semidReadersBuyingBoard = createSemaphore(keyReadersBuyingBoard, "ReadersBuyingBoard");
    unlock(semidReadersBuyingBoard, "ReadersBuyingBoard");

    int semidOrderBuyingBoard = createSemaphore(keyOrderBuyingBoard, "OrderBuyingBoard");
    unlock(semidOrderBuyingBoard, "OrderBuyingBoard");

    int semidWritersBuyingBoard = createSemaphore(keyWritersBuyingBoard, "WritersBuyingBoard");
    unlock(semidWritersBuyingBoard, "WritersBuyingBoard");

    //=========Products==================================================
        //  Intialize :
    struct item *products = initializeProducts(countProducts);

        //  Shared memory :
    int shmidProducts = shmget(keyProducts, countProducts * sizeof(struct item), IPC_CREAT | 0666);
    if (shmidProducts == -1) {
        perror("shmget_create_Products");
        exit(1);
    }

    struct item *shared_memory_Products = (struct item *)shmat(shmidProducts, NULL, 0);
    if (shared_memory_Products == (void *)-1) {
        perror("shmat_Products");
        exit(1);
    }

    write(products_pipe[1], products, countProducts * sizeof(struct item));
    read(products_pipe[0], shared_memory_Products, countProducts * sizeof(struct item));
    free(products);

        //  Semaphores :
    int countReadersProducts = 0;
    int countWritersProducts = 0;

    int semidAccessProducts = createSemaphore(keyAccessProducts, "AccessProducts");
    unlock(semidAccessProducts, "AccessProducts");

    int semidReadersProducts = createSemaphore(keyReadersProducts, "ReadersProducts");
    unlock(semidReadersProducts, "ReadersProducts");

    int semidOrderProducts = createSemaphore(keyOrderProducts, "OrderProducts");
    unlock(semidOrderProducts, "OrderProducts");

    int semidWritersProducts = createSemaphore(keyWritersProducts, "WritersProducts");
    unlock(semidWritersProducts, "WritersProducts");

    //========Check shared memory=====================================

    printf("\n\n===================================================\n\n");

    printf("Content of shared memory Products :\n\n");
    printProducts(shared_memory_Products, countProducts);

    printf("\nContent of shared memory Buying Boards :\n\n");
    printBuyingBoards(shared_memory_BuyingBoard, shared_memory_CountBoards);

    printf("\n\n===================================================\n\n");

    //========Processes===============================================

    printf("Simulation starts\n\n===================================================\n\n");

    printf("Start Time  :  ");
    struct timespec start = getCurrentTime();
    printCurrentTime(start);
    printf("\n\n");

    
    //  Customer :

    printf("\n");
    for (int j = 0; j < countCustomers; j++) {
        pid_t pid = fork();
        if (pid == 0) {
            signal(SIGTERM, signalHandler);
            srand(time(NULL) ^ getpid());

            int customerId = j;
            int customerFunction = 0;

            printf(" I'm a customer, my Id = %3d, my pid = %3d, parent pid = %3d\n", customerId, getpid(), getppid());

            struct item *currentProducts = (struct item *)malloc(countProducts * sizeof(struct item));
            int randomItem;
            int randomAmount;

            int currentCountBoards;
            struct buyingBoard* currentBuyingBoards  = (struct buyingBoard *)malloc(150 * sizeof(struct buyingBoard));;
            int isLastOrderDone;

            struct buyingBoard* local_buyingBoards;

            while (!stop) {
                int randomSleepTime = getRandomNum(3, 6);
                if (customerFunction == 0) sleep(randomSleepTime);

                switch (customerFunction) {
                    case 0:   // reader problem : read menu and choose item
                        {
                                //  data for request :
                            struct readMenuRequest *request0 = (struct readMenuRequest *)malloc(sizeof(struct readMenuRequest));
                            request0->start = start;
                            request0->id = customerId;

                            request0->semidAccess = semidAccessProducts;
                            request0->accessDeskription = "AccessProducts_readMenu";
                            request0->semidReaders = semidReadersProducts;
                            request0->readersDeskription = "ReadersProducts_readMenu";
                            request0->semidOrder = semidOrderProducts;
                            request0->orderDeskription = "OrderProducts_readMenu";
                            request0->semidWriters = semidWritersProducts;
                            request0->writersDeskription = "WritersProducts_readMenu";
                            request0->countWriters = &countWritersProducts;
                            request0->countReaders = &countReadersProducts;

                            request0->countProducts = &countProducts;
                            request0->currentProducts = currentProducts;
                            request0->shared_memory_Products = shared_memory_Products;
                            
                                // thread :
                            pthread_t threadReadMenu;
                            pthread_create(&threadReadMenu, NULL, &thread_readMenu, (void *)request0);
                            pthread_join(threadReadMenu, NULL);

                            free(request0);

                                // action :
                            randomItem = getRandomItem(currentProducts, countProducts);
                            randomAmount = getRandomNum(1, 4);
                            printf("%8.4f sec :  ", timer(start));
                            printChoice(customerId, currentProducts, countProducts, randomItem, randomAmount);

                            customerFunction = 1;  
                        }
                    break;
                    
                    case 1:   // reader problem : read orders and check last order
                        {
                                //  data for request :
                            struct readBuyingBoardsRequest *request1 = (struct readBuyingBoardsRequest *)malloc(sizeof(struct readBuyingBoardsRequest));
                            request1->start = start;
                            request1->id = customerId;
                            request1->accountType = "Customer";

                            request1->semidAccess = semidAccessBuyingBoard;
                            request1->accessDeskription = "AccessBuyingBoards_readBuyingBoards";
                            request1->semidReaders = semidReadersBuyingBoard;
                            request1->readersDeskription = "ReadersBuyingBoards_readBuyingBoards";
                            request1->semidOrder = semidOrderBuyingBoard;
                            request1->orderDeskription = "OrderBuyingBoards_readBuyingBoards";
                            request1->semidWriters = semidWritersBuyingBoard;
                            request1->writersDeskription = "WritersBuyingBoards_readBuyingBoards";
                            request1->countWriters = &countWritersBuyingBoard;
                            request1->countReaders = &countReadersBuyingBoard;

                            request1->countBoards = shared_memory_CountBoards;
                            request1->currentCountBoards = &currentCountBoards;
                            request1->currentBuyingBoards = currentBuyingBoards;

                            request1->shared_memory_CountBoards = shared_memory_CountBoards;
                            request1->shared_memory_BuyingBoard = shared_memory_BuyingBoard;

                                // thread :
                            pthread_t threadReadBuyingBoards;
                            pthread_create(&threadReadBuyingBoards, NULL, &thread_readBuyingBoards, (void *)request1);
                            pthread_join(threadReadBuyingBoards, NULL);

                            free(request1);

                                // action :
                            isLastOrderDone = checkIfLastOrderDone(currentBuyingBoards, &currentCountBoards, customerId);

                            int choice = getRandomNum(0, 1);

                            if (isLastOrderDone == -1) {
                                printf("%8.4f sec :  Customer id : %d : I haven't made orders yet so I'm going to make a new one | pid = %d, ppid = %d\n",
                                                    timer(start), customerId, getpid(), getppid());
                                if (choice) customerFunction = 2;
                                else {
                                    printf("%8.4f sec :! Customer id : %d : I decided not to make Purchase | pid = %d, ppid = %d\n",
                                                    timer(start), customerId, getpid(), getppid());
                                    customerFunction = 0;
                                }
                            } else if (isLastOrderDone == 0) {
                                printf("%8.4f sec :  Customer id : %d : My previous Order has Not been Done yet so I'll wait a bit and make a new choice | pid = %d, ppid = %d\n",
                                                    timer(start), customerId, getpid(), getppid());
                                customerFunction = 0;
                            } else {
                                printf("%8.4f sec :  Customer id : %d : My previous Order has been Done so I'm going to make a new one | pid = %d, ppid = %d\n",
                                                    timer(start), customerId, getpid(), getppid());
                                if (choice) customerFunction = 2;
                                else {
                                    printf("%8.4f sec :! Customer id : %d : I decided not to make Purchase | pid = %d, ppid = %d\n",
                                                    timer(start), customerId, getpid(), getppid());
                                    customerFunction = 0;
                                }
                            }
                        }
                    break;

                    case 2:   // writer problem : add new Buying Board to shared memory
                        {
                                //  data for request :
                            struct updateBuyingBoardsRequest *request2 = (struct updateBuyingBoardsRequest *)malloc(sizeof(struct updateBuyingBoardsRequest));
                            request2->start = start;
                            request2->id = customerId;

                            request2->semidAccess = semidAccessBuyingBoard;
                            request2->accessDeskription = "AccessBuyingBoards_addBuyingBoard";
                            request2->semidOrder = semidOrderBuyingBoard;
                            request2->orderDeskription = "OrderBuyingBoards_addBuyingBoard";
                            request2->semidLockWriters = semidWritersBuyingBoard;
                            request2->lockWritersDeskription = "LockWritersBuyingBoards_addBuyingBoard";
                            request2->countWriters = &countWritersBuyingBoard;

                            request2->itemId = randomItem;
                            request2->amountOrder = randomAmount;
                            request2->currentCountBoards = &countBoards;

                            request2->shared_memory_CountBoards = shared_memory_CountBoards;
                            request2->shared_memory_BuyingBoard = shared_memory_BuyingBoard;

                                // thread :
                            pthread_t threadAddBuyingBoard;
                            pthread_create(&threadAddBuyingBoard, NULL, &thread_addBuyingBoard, (void *)request2);
                            pthread_join(threadAddBuyingBoard, NULL);

                            free(request2);

                                // action :
                            printf("%8.4f sec :  Customer id : %d : I've added a New Buying Board | pid = %d, ppid = %d\n",
                                            timer(start), customerId, getpid(), getppid());

                            customerFunction = 0;
                        }
                    break;
                }
            }
            exit(0);
        } else if (pid > 0) {
            childPIDs[childCount++] = pid;
            printf("Created Customer %d : pid = %d\n", j, pid);
        } else {
            perror("fork failed for customer");
            exit(1);
        }
    }

    // Sales Agent :    
    
    for (int i = 0; i < countAgents; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            signal(SIGTERM, signalHandler);
            srand(time(NULL) ^ getpid());

            int agentId = i;
            int agentFunction = 1;

            printf(" I'm the Sales Agent, my Id = %d, my pid = %d, parent pid = %d\n", i, getpid(), getppid());

            int currentCountBoards;
            struct buyingBoard* currentBuyingBoards = (struct buyingBoard *)malloc(150 * sizeof(struct buyingBoard));;
            int notDoneBoardId;
            int notDoneAmount;
            int notDoneItemId;

            while (!stop) {
                int randomSleepTime = getRandomNum(1, 2);
                if (agentFunction == 1) sleep(randomSleepTime);

                switch (agentFunction) {
                    case 0:   // writer problem : update Total Orders
                        {
                                //  data for request :
                            struct UpdateProductsRequest *request0 = (struct UpdateProductsRequest *)malloc(sizeof(struct UpdateProductsRequest));
                            request0->start = start;
                            request0->id = agentId;

                            request0->semidAccess = semidAccessProducts;
                            request0->accessDeskription = "AccessProducts_updateProducts";
                            request0->semidOrder = semidOrderProducts;
                            request0->orderDeskription = "OrderProducts_updateProducts";
                            request0->semidLockWriters = semidWritersProducts;
                            request0->lockWritersDeskription = "LockWritersProducts_updateProducts";
                            request0->countWriters = &countWritersProducts;

                            request0->countProducts = &countProducts;
                            request0->itemId = notDoneItemId;
                            request0->amountOrder = notDoneAmount;
                            request0->shared_memory_Products = shared_memory_Products;

                                // thread :
                            pthread_t threadUpdateTotalOrders;
                            pthread_create(&threadUpdateTotalOrders, NULL, &thread_updateTotalOrders, (void *)request0);
                            pthread_join(threadUpdateTotalOrders, NULL);

                            free(request0);

                                // action :
                            printf("%8.4f sec :  Sales Agent id : %d : I've finished updating the Total Orders | pid = %d, ppid = %d\n",
                                            timer(start), agentId, getpid(), getppid());

                            agentFunction = 1;
                        }
                    break;

                    case 1:   // reader problem : read Buying Boards : 
                        {
                                //  data for request :
                            struct readBuyingBoardsRequest *request1 = (struct readBuyingBoardsRequest *)malloc(sizeof(struct readBuyingBoardsRequest));
                            request1->start = start;
                            request1->id = agentId;
                            request1->accountType = "Sales Agent";

                            request1->semidAccess = semidAccessBuyingBoard;
                            request1->accessDeskription = "AccessBuyingBoards_readBuyingBoards";
                            request1->semidReaders = semidReadersBuyingBoard;
                            request1->readersDeskription = "ReadersBuyingBoards_readBuyingBoards";
                            request1->semidOrder = semidOrderBuyingBoard;
                            request1->orderDeskription = "OrderBuyingBoards_readBuyingBoards";
                            request1->semidWriters = semidWritersBuyingBoard;
                            request1->writersDeskription = "WritersBuyingBoards_readBuyingBoards";
                            request1->countWriters = &countWritersBuyingBoard;
                            request1->countReaders = &countReadersBuyingBoard;

                            request1->countBoards = shared_memory_CountBoards;
                            request1->currentCountBoards = &currentCountBoards;
                            request1->currentBuyingBoards = currentBuyingBoards;
                            request1->shared_memory_CountBoards = shared_memory_CountBoards;
                            request1->shared_memory_BuyingBoard = shared_memory_BuyingBoard;

                                // thread :
                            pthread_t threadReadBuyingBoards;
                            pthread_create(&threadReadBuyingBoards, NULL, &thread_readBuyingBoards, (void *)request1);
                            pthread_join(threadReadBuyingBoards, NULL);

                            free(request1);

                                // action :
                            notDoneBoardId = getNotDoneProductId(currentBuyingBoards, &currentCountBoards);
                            notDoneAmount = getAmountByBoardId(currentBuyingBoards, &currentCountBoards, notDoneBoardId);
                            notDoneItemId = getItemIdByBoardId(currentBuyingBoards, &currentCountBoards, notDoneBoardId);

                            if (notDoneBoardId == -1) {
                                printf("%8.4f sec :  Sales Agent id : %d : I haven't found New Orders | pid = %d, ppid = %d\n",
                                                timer(start), agentId, getpid(), getppid());
                                agentFunction = 1;
                            } else {
                                printf("%8.4f sec :  Sales Agent id : %d : I've found New Order : ID : %d : item : %d : amount : %d | pid = %d, ppid = %d\n",
                                                timer(start), agentId, notDoneBoardId, notDoneItemId, notDoneAmount, getpid(), getppid());
                                agentFunction = 2;
                            }
                        }
                    break;

                    case 2:   // writer problem : make Order Done
                        {
                                //  data for request :
                            struct updateBuyingBoardsRequest *request2 = (struct updateBuyingBoardsRequest *)malloc(sizeof(struct updateBuyingBoardsRequest));
                            request2->start = start;
                            request2->id = agentId;

                            request2->semidAccess = semidAccessBuyingBoard;
                            request2->accessDeskription = "AccessBuyingBoards_makeDone";
                            request2->semidOrder = semidOrderBuyingBoard;
                            request2->orderDeskription = "OrderBuyingBoards_makeDone";
                            request2->semidLockWriters = semidWritersBuyingBoard;
                            request2->lockWritersDeskription = "LockWritersBuyingBoards_makeDone";
                            request2->countWriters = &countWritersBuyingBoard;

                            request2->notDoneBoardId = &notDoneBoardId;
                            request2->notDoneAmount = &notDoneAmount;
                            request2->notDoneItemId = &notDoneItemId;

                            request2->shared_memory_CountBoards = shared_memory_CountBoards;
                            request2->shared_memory_BuyingBoard = shared_memory_BuyingBoard;

                                // thread :
                            pthread_t threadMakeDone;
                            pthread_create(&threadMakeDone, NULL, &thread_threadMakeDone, (void *)request2);
                            pthread_join(threadMakeDone, NULL);

                            free(request2);

                                // action :
                            if (notDoneBoardId == -1) {
                                printf("%8.4f sec :  Agent id : %d : I've found the Previos Order was done and I haven't found a new one | pid = %d, ppid = %d\n",
                                                timer(start), agentId, getpid(), getppid());
                                agentFunction = 1;
                            } else {
                                printf("%8.4f sec :  Agent id : %d : I've made the Order Done : ID : %d | pid = %d, ppid = %d\n",
                                                timer(start), agentId, notDoneBoardId, getpid(), getppid());
                                agentFunction = 0;
                            }
                        }
                    break;
                }
            }
            exit(0);
        } else if (pid > 0) {
            childPIDs[childCount++] = pid;
            printf("Created Sales Agent %d : pid = %d\n", i, pid);
        } else {
            perror("fork failed for agent");
            exit(1);
        }
    }

    // Set up signal handling for the parent process to terminate child processes
    signal(SIGINT, signalHandler);

    sleep(simulationTime);

    // Send termination signal to child processes
    printf("\n");
    for (int i = 0; i < childCount; i++) {
        if (childPIDs[i] > 0) {
            kill(childPIDs[i], SIGTERM);
            printf("Sent SIGTERM to process %d\n", childPIDs[i]);
        }
    }

    printf("\n");
    // Wait for all child processes to complete
    for (int i = 0; i < childCount; i++) {
        if (childPIDs[i] > 0) {
            int status;
            waitpid(childPIDs[i], &status, 0);
            printf("Process %d has terminated.\n", childPIDs[i]);
        }
    }

    printf("\n===================================================\n\nSimulation is finished\n\n");

    printf("Finish Time  :  ");
    struct timespec finish = getCurrentTime();
    double finalSemulationTime = timer(start);
    printCurrentTime(start);
    printf("Final Time of the simulation : %f", finalSemulationTime);
    printf("\n\n");

    printf("Result of the Simulation :\n\n");
    float income = calculateIncome(shared_memory_Products, countProducts);
    int countNotDoneOrders = getCountNotDone(shared_memory_BuyingBoard, shared_memory_CountBoards);
    printf("Total income :    %3.2f NIS\n", income);
    printf("Total orders :    %3d\n", *shared_memory_CountBoards);
    printf("Not Done orders : %3d\n", countNotDoneOrders);

    printf("\n===================================================\n\n");

    printf("Content of shared memory Products :\n\n");
    printProducts(shared_memory_Products, countProducts);

    printf("\nContent of shared memory Buying Boards :\n\n");
    printBuyingBoards(shared_memory_BuyingBoard, shared_memory_CountBoards);

    // Clean up shared memory and semaphores
    shmdt(shared_memory_BuyingBoard);
    shmctl(shmidBuyingBoard, IPC_RMID, NULL);
    shmdt(shared_memory_Products);
    shmctl(shmidProducts, IPC_RMID, NULL);
    shmdt(shared_memory_CountBoards);
    shmctl(shmidCountBoards, IPC_RMID, NULL);

    removeSemaphore(semidAccessProducts,    "rm_AccessProducts");
    removeSemaphore(semidReadersProducts,   "rm_ReadersProducts");
    removeSemaphore(semidOrderProducts,     "rm_OrderProducts");
    removeSemaphore(semidWritersProducts,   "rm_kWritersProducts");
    removeSemaphore(semidAccessBuyingBoard, "rm_AccessBuyingBoard");
    removeSemaphore(semidReadersBuyingBoard,"rm_ReadersBuyingBoard");
    removeSemaphore(semidOrderBuyingBoard,  "rm_OrderBuyingBoard");
    removeSemaphore(semidWritersBuyingBoard,"rm_WritersBuyingBoard");

    return 0;
}


//=========Semaphores===================================================
key_t getKey(const char *pathname, int proj_id) {
    key_t key = ftok(pathname, proj_id);
    if (key == -1) {
        perror("ftok failed");
        exit(1);
    }
    return key;
}

int createSemaphore(key_t key, const char *semName) {
    int semid = semget(key, 1, IPC_CREAT | 0666);
    if (semid == -1) {
        perror(strcat("semget ", semName));
        exit(1);
    }
    return semid;
}

void removeSemaphore(int semid, const char *semName) {
    if (semctl(semid, 0, IPC_RMID) == -1) {
        perror(strcat("semctl IPC_RMID ", semName));
        exit(1);
    }
}

void lock(int semid, const char *semName) {
    struct sembuf sembuf_operation;
    sembuf_operation.sem_num = 0;
    sembuf_operation.sem_op = -1;
    sembuf_operation.sem_flg = IPC_NOWAIT;

    if (semop(semid, &sembuf_operation, 1) == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            //printf("Semaphore %s is already locked\n", semName);
        } else {
            perror(strcat("semop ", semName));
            exit(1);
        }
    }
}

void unlock(int semid, const char *semName) {
    struct sembuf sembuf_operation;
    sembuf_operation.sem_num = 0;
    sembuf_operation.sem_op = 1;
    sembuf_operation.sem_flg = 0;

    if (semop(semid, &sembuf_operation, 1) == -1) {
        perror(strcat("semop ", semName));
        exit(1);
    }
}

void writerLock(int semidOrder, const char *orderDeskription,
                int semidAccess, const char *accessDeskription,
                int semidWriters, const char *writersDeskription,
                int *waitingWriters) {

    lock(semidWriters, writersDeskription);
    increment(waitingWriters);
    unlock(semidWriters, writersDeskription);

    lock(semidOrder, orderDeskription);
    lock(semidAccess, accessDeskription);

    lock(semidWriters, writersDeskription);
    decrement(waitingWriters);
    unlock(semidWriters, writersDeskription);

    unlock(semidOrder, orderDeskription);
}

void readerLock(int semidOrder, const char *orderDeskription,
                int semidReaders, const char *readersDeskription,
                int semidAccess, const char *accessDeskription,
                int semidWriters, const char *lockWritersDeskription,
                int *countReaders, int *waitingWriters) { 
    lock(semidWriters, lockWritersDeskription);
    while (*waitingWriters > 0) {
        unlock(semidWriters, lockWritersDeskription);
        usleep(100);
        lock(semidWriters, lockWritersDeskription);
    }
    unlock(semidWriters, lockWritersDeskription);

    lock(semidOrder, orderDeskription);
    lock(semidReaders, readersDeskription);
    if (*countReaders == 0) lock(semidAccess, accessDeskription);
    increment(countReaders);
    unlock(semidReaders, readersDeskription);
    unlock(semidOrder, orderDeskription);
}

void readerUnlock(int semidReaders, const char *readersDeskription,
                  int semidAccess, const char *accessDeskription,
                  int *countReaders) {
    lock(semidReaders, readersDeskription);
    decrement(countReaders);
    if (*countReaders == 0) unlock(semidAccess, accessDeskription);
    unlock(semidReaders, readersDeskription);
}

//=========BuyingBoard==================================================
struct buyingBoard* initializeBuyingBoards(int* countBoards) {
    *countBoards = 0;

    struct buyingBoard* buyingBoards = (struct buyingBoard*)malloc(sizeof(struct buyingBoard));
    if (buyingBoards == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    return buyingBoards;
}

struct buyingBoard* addBuyingBoard(struct buyingBoard* buyingBoards, int* countBoards, int customerId, int itemId, int amount) {
    increment(countBoards);

    struct buyingBoard* newBuyingBoards = (struct buyingBoard*)malloc((*countBoards) * sizeof(struct buyingBoard));
    if (newBuyingBoards == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    for (int i = 0; i < *countBoards - 1; ++i) {
        newBuyingBoards[i] = buyingBoards[i];
    }

    struct buyingBoard newItem;
    newItem.boardId = *countBoards - 1;
    newItem.customerId = customerId;
    newItem.itemId = itemId;
    newItem.amount = amount;
    newItem.done = 0;

    newBuyingBoards[*countBoards - 1] = newItem;

    free(buyingBoards);

    return newBuyingBoards;
}

int makeDone(struct buyingBoard* buyingBoards, int* countBoards, int boardId) {
    int currentCountBoards = *countBoards;
    for (int i = 0; i < currentCountBoards; ++i) {
        if (buyingBoards[i].boardId == boardId) {
            if (buyingBoards[i].done == 0) {
                buyingBoards[i].done = 1;
                return 1;
            } else {
                return 0;
            }
        }
    }
    return -1;
}

void printBuyingBoards(struct buyingBoard* buyingBoards, int* countBoards) {
    int currentCountBoards = *countBoards;
    for (int i = 0; i < currentCountBoards; ++i) {
        if (buyingBoards[i].done == 0) {
            printf("Board ID : %3d |  Customer ID : %3d |  Item ID : %3d |  Amount : %3d |  Done : False\n",
                        buyingBoards[i].boardId, buyingBoards[i].customerId, buyingBoards[i].itemId, buyingBoards[i].amount);
        } else if (buyingBoards[i].done == 1) {
            printf("Board ID : %3d |  Customer ID : %3d |  Item ID : %3d |  Amount : %3d |  Done : True\n",
                        buyingBoards[i].boardId, buyingBoards[i].customerId, buyingBoards[i].itemId, buyingBoards[i].amount);
        } else {
            printf("Board ID : %3d |  Customer ID : %3d |  Item ID : %3d |  Amount : %3d |  Done : %3d\n",
                        buyingBoards[i].boardId, buyingBoards[i].customerId, buyingBoards[i].itemId, buyingBoards[i].amount, buyingBoards[i].done);
        }
    }
}


int getNotDoneProductId(struct buyingBoard* buyingBoards, int* countBoards) {
    int currentCountBoards = *countBoards;
    for (int i = 0; i < currentCountBoards; ++i) {
        if (buyingBoards[i].done == 0) {
            return buyingBoards[i].boardId;
        }
    }
    return -1;
}

int checkIfLastOrderDone(struct buyingBoard* buyingBoards, int* countBoards, int customerId) {
    int isDone = -1;
    int currentCountBoards = *countBoards;
    for (int i = 0; i < currentCountBoards; ++i) {
        if (buyingBoards[i].customerId == customerId) {
            isDone = buyingBoards[i].done;
        }
    }
    return isDone;
}

int getAmountByBoardId(struct buyingBoard* buyingBoards, int* countBoards, int boardId) {
    for (int i = 0; i < *countBoards; i++) {
        if (buyingBoards[i].boardId == boardId) {
            return buyingBoards[i].amount;
        }
    }
    return -1;
}

int getItemIdByBoardId(struct buyingBoard* buyingBoards, int* countBoards, int boardId) {
    for (int i = 0; i < *countBoards; i++) {
        if (buyingBoards[i].boardId == boardId) {
            return buyingBoards[i].itemId;
        }
    }
    return -1;
}

int getCountNotDone(struct buyingBoard* buyingBoards, int* countBoards) {
    int countNotDone = 0;
    int currentCountBoards = *countBoards;
    
    for (int i = 0; i < currentCountBoards; ++i) {
        if (buyingBoards[i].done == 0) countNotDone++;
    }
    
    return countNotDone;
}

//=========Products==================================================
struct item* initializeProducts(int countProducts) {
    int maxProducts = 10;
    if (countProducts > maxProducts) {
        countProducts = maxProducts;
    }

    struct item* products = (struct item*)malloc(countProducts * sizeof(struct item));

    for (int i = 0; i < countProducts; i++) {
        products[i].index = i;
        switch (i) {
            case 0:
                products[i].name = strdup("Dress");
                products[i].price = 70.0;
                break;
            case 1:
                products[i].name = strdup("T-Shirt");
                products[i].price = 17.50;
                break;
            case 2:
                products[i].name = strdup("Jacket");
                products[i].price = 52.0;
                break;
            case 3:
                products[i].name = strdup("Pants");
                products[i].price = 29.0;
                break;
            case 4:
                products[i].name = strdup("Shorts");
                products[i].price = 29.50;
                break;
            case 5:
                products[i].name = strdup("Shirt");
                products[i].price = 16.0;
                break;
            case 6:
                products[i].name = strdup("Skirt");
                products[i].price = 26.0;
                break;
            case 7:
                products[i].name = strdup("Jeans");
                products[i].price = 66.0;
                break;
            case 8:
                products[i].name = strdup("Sweater");
                products[i].price = 66.0;
                break;
            case 9:
                products[i].name = strdup("Tie");
                products[i].price = 19.50;
                break;
            default:
                break;
        }
        products[i].totalOrders = 0;
    }

    return products;
}

struct item* updateTotalOrders(struct item* products, int countProducts, int index, int amount) {
    int isFound = 0;
    for (int i = 0; i < countProducts; i++) {
        if (products[i].index == index) {
            products[i].totalOrders += amount;
            isFound = 1;
            break;
        }
    }

    if (!isFound) {
        printf("Invalid index: %d\n", index);
    }

    return products;
}

void printProducts(struct item* products, int countProducts) {
    for (int i = 0; i < countProducts; i++) {
        printf("Product %3d  |  Name: %8s  |  Price: %3.2f  |  Total Orders: %d\n",
               products[i].index,
               products[i].name,
               products[i].price,
               products[i].totalOrders);
    }
}

void printMenu(struct item *products, int countProducts) {
    for (int i = 0; i < countProducts; i++) {
        printf("%-10s | %-10.2f", products[i].name, products[i].price);
        if (i % 2 == 1 || i == countProducts - 1) {
            printf("\n");
        } else {
            printf(" | ");
        }
    }
}

void printChoice(int id, struct item *products, int countProducts, int itemId, int amount) {
    int found = 0; 

    for (int i = 0; i < countProducts; i++) {
        if (products[i].index == itemId) {
            printf("I am the Customer id %d : I chose the product (id : %d) :   Name: %s, Price: %.2f, Amount: %d | pid = %d, ppid = %d\n",
                            id, itemId, products[i].name, products[i].price, amount, getpid(), getppid());
            found = 1; 
            break;
        }
    }

    if (!found) {
        printf("Invalid item ID: %d\n", itemId);
    }
}

int getRandomItem(struct item *products, int countProducts) {
    if (countProducts <= 0) {
        printf("No products available.\n");
        return -1;
    }
    int randomIndex = getRandomNum(0, countProducts - 1);
    return products[randomIndex].index;
}

float calculateIncome(struct item *products, int countProducts) {
    float income = 0.0;
    for (int i = 0; i < countProducts; i++) {
        income += products[i].price * products[i].totalOrders;
    }
    return income;
}

//=========Thread functions==================================================

    //  reader problem:
void* thread_readMenu(void* arg) {
    struct readMenuRequest* args = (struct readMenuRequest*)arg;

    struct timespec start = args->start;
    int id = args->id;

    int semidAccess = args->semidAccess;
    char* accessDeskription = args->accessDeskription;
    int semidReaders = args->semidReaders;
    char* readersDeskription = args->readersDeskription;
    int semidOrder = args->semidOrder;
    char* orderDeskription = args->orderDeskription;
    int semidWriters = args->semidWriters;
    const char *writersDeskription = args->writersDeskription;
    int *waitingWriters = args->countWriters;
    int* countReaders = args->countReaders;

    int* countProducts = args->countProducts;
    struct item* currentProducts = args->currentProducts;
    struct item* shared_memory_Products = args->shared_memory_Products;

    // reader lock:
    readerLock(semidOrder, orderDeskription,
               semidReaders, readersDeskription,
               semidAccess, accessDeskription,
               semidWriters, writersDeskription,
               waitingWriters,
               countReaders);

    printf("%8.4f sec :     thread : Customer id: %d : I'm going to read the menu | pid = %d, ppid = %d\n",
                    timer(start), id, getpid(), getppid());

    sleep(1);
    memcpy(currentProducts, shared_memory_Products, *countProducts * sizeof(struct item));

    // reader unlock:
    readerUnlock(semidReaders, readersDeskription,
                 semidAccess, accessDeskription,
                 countReaders);

    pthread_exit(NULL);
}

    //  writer problem:
void* thread_updateTotalOrders(void* arg) {
    struct UpdateProductsRequest* args = (struct UpdateProductsRequest*)arg;

    struct timespec start = args->start;
    int id = args->id;

    int semidAccess = args->semidAccess;
    char* accessDeskription = args->accessDeskription;
    int semidOrder = args->semidOrder;
    char* orderDeskription = args->orderDeskription;
    int semidLockWriters = args->semidLockWriters;
    const char *lockWritersDeskription = args->lockWritersDeskription;
    int *waitingWriters = args->countWriters;
    int* countProducts = args->countProducts;
    int itemId = args->itemId;
    int amountOrder = args->amountOrder;

    struct item* shared_memory_Products = args->shared_memory_Products;

    // writer lock:
    writerLock(semidOrder, orderDeskription,
               semidAccess, accessDeskription,
               semidLockWriters, lockWritersDeskription,
               waitingWriters);

    printf("%8.4f sec :     thread : Sales Agent id: %d : I'm going to update the total orders of the item : %d; amount : %d items | pid = %d, ppid = %d\n",
                    timer(start), id, itemId, amountOrder, getpid(), getppid());

    //usleep(100);
    //sleep(1);

    struct item* currentProducts = (struct item*)malloc(*countProducts * sizeof(struct item));
    memcpy(currentProducts, shared_memory_Products, *countProducts * sizeof(struct item));

    currentProducts = updateTotalOrders(currentProducts, *countProducts, itemId, amountOrder);
    memcpy(shared_memory_Products, currentProducts, *countProducts * sizeof(struct item));

    free(currentProducts);

    // writer unlock:
    unlock(semidAccess, accessDeskription);

    pthread_exit(NULL);
}

    //  reader problem:
void* thread_readBuyingBoards(void* arg) {
    struct readBuyingBoardsRequest* args = (struct readBuyingBoardsRequest*)arg;

    struct timespec start = args->start;
    int id = args->id;
    char* accountType = args->accountType;

    int semidAccess = args->semidAccess;
    char* accessDeskription = args->accessDeskription;
    int semidReaders = args->semidReaders;
    char* readersDeskription = args->readersDeskription;
    int semidOrder = args->semidOrder;
    char* orderDeskription = args->orderDeskription;
    int semidWriters = args->semidWriters;
    const char *writersDeskription = args->writersDeskription;
    int *waitingWriters = args->countWriters;
    int* countReaders = args->countReaders;

    int *countBoards = args->countBoards;
    int *currentCountBoards = args->currentCountBoards;
    struct buyingBoard* currentBuyingBoards = args->currentBuyingBoards;
    int *shared_memory_CountBoards = args->shared_memory_CountBoards;
    struct buyingBoard *shared_memory_BuyingBoard = args->shared_memory_BuyingBoard;

    // reader lock:
    readerLock(semidOrder, orderDeskription,
               semidReaders, readersDeskription,
               semidAccess, accessDeskription,
               semidWriters, writersDeskription,
               waitingWriters,
               countReaders);

    printf("%8.4f sec :     thread : %s id: %d : I'm going to read Buying Boards | pid = %d, ppid = %d\n",
                    timer(start), accountType, id, getpid(), getppid());

    //usleep(100);
    //sleep(1);

    memcpy(currentCountBoards, shared_memory_CountBoards, sizeof(int));
    memcpy(currentBuyingBoards, shared_memory_BuyingBoard, *countBoards * sizeof(struct buyingBoard));   

    // reader unlock:
    readerUnlock(semidReaders, readersDeskription,
                 semidAccess, accessDeskription,
                 countReaders);

    pthread_exit(NULL);
}

    //  writer problem:
void* thread_addBuyingBoard(void* arg) {
    struct updateBuyingBoardsRequest* args = (struct updateBuyingBoardsRequest*)arg;

    struct timespec start = args->start;
    int id = args->id;

    int semidAccess = args->semidAccess;
    char* accessDeskription = args->accessDeskription;
    int semidOrder = args->semidOrder;
    char* orderDeskription = args->orderDeskription;
    int semidLockWriters = args->semidLockWriters;
    const char *lockWritersDeskription = args->lockWritersDeskription;
    int *waitingWriters = args->countWriters;

    int itemId = args->itemId;
    int amountOrder = args->amountOrder;
    int *shared_memory_CountBoards = args->shared_memory_CountBoards;
    struct buyingBoard *shared_memory_BuyingBoard = args->shared_memory_BuyingBoard;

    int currentCountBoards;

    // writer lock:
    writerLock(semidOrder, orderDeskription,
               semidAccess, accessDeskription,
               semidLockWriters, lockWritersDeskription,
               waitingWriters);

    printf("%8.4f sec :     thread : Customer id: %d : I'm going to add new Buying Board of the item : %d; amount : %d items | pid = %d, ppid = %d\n",
                    timer(start), id, itemId, amountOrder, getpid(), getppid());

    memcpy(&currentCountBoards, shared_memory_CountBoards, sizeof(int));

    struct buyingBoard* currentBuyingBoards = (struct buyingBoard*)malloc((currentCountBoards + 1) * sizeof(struct buyingBoard));

    //usleep(100);
    //sleep(1);

    memcpy(currentBuyingBoards, shared_memory_BuyingBoard, currentCountBoards * sizeof(struct buyingBoard));
    currentBuyingBoards = addBuyingBoard(currentBuyingBoards, &currentCountBoards, id, itemId, amountOrder);
    //printBuyingBoards(currentBuyingBoards, &currentCountBoards);

    memcpy(shared_memory_CountBoards, &currentCountBoards, sizeof(int));
    memcpy(shared_memory_BuyingBoard, currentBuyingBoards, currentCountBoards * sizeof(struct buyingBoard));

    free(currentBuyingBoards);

    // writer unlock:
    unlock(semidAccess, accessDeskription);

    pthread_exit(NULL);
}
    //  writer problem:
void* thread_threadMakeDone(void* arg) {
    struct updateBuyingBoardsRequest* args = (struct updateBuyingBoardsRequest*)arg;

    struct timespec start = args->start;
    int id = args->id;

    int semidAccess = args->semidAccess;
    char* accessDeskription = args->accessDeskription;
    int semidOrder = args->semidOrder;
    char* orderDeskription = args->orderDeskription;
    int semidLockWriters = args->semidLockWriters;
    const char *lockWritersDeskription = args->lockWritersDeskription;
    int *waitingWriters = args->countWriters;

    int *notDoneBoardId = args->notDoneBoardId;
    int *notDoneAmount = args->notDoneAmount;
    int *notDoneItemId = args->notDoneItemId; 

    int *shared_memory_CountBoards = args->shared_memory_CountBoards;
    struct buyingBoard *shared_memory_BuyingBoard = args->shared_memory_BuyingBoard;

    int currentCountBoards;

    // writer lock:
    writerLock(semidOrder, orderDeskription,
               semidAccess, accessDeskription,
               semidLockWriters, lockWritersDeskription,
               waitingWriters);

    printf("%8.4f sec :     thread : Agent id: %d : I'm going to make Order id: %d Done | pid = %d, ppid = %d\n",
                    timer(start), id, *notDoneBoardId, getpid(), getppid());

    usleep(100);
    //sleep(1);

    memcpy(&currentCountBoards, shared_memory_CountBoards, sizeof(int));

    struct buyingBoard* currentBuyingBoards = (struct buyingBoard*)malloc(currentCountBoards * sizeof(struct buyingBoard));

    memcpy(currentBuyingBoards, shared_memory_BuyingBoard, currentCountBoards * sizeof(struct buyingBoard));

    int isDone = makeDone(currentBuyingBoards, &currentCountBoards, *notDoneBoardId);
    if (isDone == 0) {
        int tmp_boardId = *notDoneBoardId;
        *notDoneBoardId = getNotDoneProductId(currentBuyingBoards, &currentCountBoards);
        if (*notDoneBoardId == -1) {
            printf("%8.4f sec :     thread : Agent id: %d : I've found that the Order id: %d is already Done and there are any New Order | pid = %d, ppid = %d\n",
                            timer(start), id, tmp_boardId, getpid(), getppid());
        } else {
            *notDoneAmount = getAmountByBoardId(currentBuyingBoards, &currentCountBoards, *notDoneBoardId);
            isDone = makeDone(currentBuyingBoards, &currentCountBoards, *notDoneBoardId);

            if (isDone != -1) printf("%8.4f sec :     thread : Agent id: %d : Error Update Done | pid = %d, ppid = %d", timer(start), id, getpid(), getppid());
            else {
                *notDoneItemId = getItemIdByBoardId(currentBuyingBoards, &currentCountBoards, *notDoneBoardId);
                printf("%8.4f sec :   thread : Agent id: %d : I've found that the Order id: %d is already Done but I've found the New Order ID : %d : item : %d : amount : %d | pid = %d, ppid = %d\n",
                                timer(start), id, tmp_boardId, *notDoneBoardId, *notDoneItemId, *notDoneAmount, getpid(), getppid());
            }
        }
    } else if (isDone == -1) printf("%8.4f sec :     thread : Agent id: %d : Error : I havent found board ID : %d in the Boards List | pid = %d, ppid = %d",
                                            timer(start), id, *notDoneBoardId, getpid(), getppid());
    

    memcpy(shared_memory_BuyingBoard, currentBuyingBoards, currentCountBoards * sizeof(struct buyingBoard));

    free(currentBuyingBoards);

    // writer unlock:
    unlock(semidAccess, accessDeskription);

    pthread_exit(NULL);
}

//=========Others==================================================
void increment(int* value) {
    (*value)++;
}

void decrement(int* value) {
    (*value)--;
}

int getRandomNum(int minNum, int maxNum) {
    return rand() % (maxNum - minNum + 1) + minNum;
}

struct timespec getCurrentTime() {
    struct timespec currentTime;
    if (clock_gettime(0, &currentTime) == -1) {
        perror("clock_gettime");
        exit(1);
    }
    return currentTime;
}

void printCurrentTime(struct timespec currentTime) {
    struct tm *localTime = localtime(&currentTime.tv_sec);
    printf("%02d:%02d:%02d.%02ld\n", localTime->tm_hour, localTime->tm_min, localTime->tm_sec, currentTime.tv_nsec / 10000000);
}

double timer(struct timespec startTime) {
    struct timespec currentTime = getCurrentTime();
    double elapsed = (currentTime.tv_sec - startTime.tv_sec) + (currentTime.tv_nsec - startTime.tv_nsec) / 1e9;
    return elapsed;
}

int isInteger(const char *str) {
    if (str == NULL || *str == '\0') {
        return 0;
    }

    char *endptr;
    errno = 0; 
    long val = strtol(str, &endptr, 10);

    if (errno != 0 || *endptr != '\0') {
        return 0; 
    }

    if (val < INT_MIN || val > INT_MAX) {
        return 0;
    }

    return 1;
}

void readArguments(int *countProducts, int *countAgents, int *countCustomers, int argc, char *argv[]) {

    if (isInteger(argv[1])) {
        int countProductsReceived = atoi(argv[1]);
        if (countProductsReceived > 0 && countProductsReceived < 11) {
            *countProducts = countProductsReceived;
        } else {
            printf("Argument 1 = %d : Products amount can not be less than 1 and more than 10 : Default value : 10\n", countProductsReceived);
        }
    } else {
        printf("Argument 1 is not a valid integer.\n");
    }

    if (isInteger(argv[2])) {
        int countAgentsReceived = atoi(argv[2]);
        if (countAgentsReceived > 0 && countAgentsReceived < 4) {
            *countAgents = countAgentsReceived;
        } else {
            printf("Argument 2 = %d : amount of Sales Agents can not be less than 1 and more than 3 : Default value : 2\n", countAgentsReceived);
        }
    } else {
        printf("Argument 2 is not a valid integer.\n");
    }

    if (isInteger(argv[3])) {
        int countCustomersReceived = atoi(argv[3]);
        if (countCustomersReceived > 0 && countCustomersReceived < 11) {
            *countCustomers = countCustomersReceived;
        } else {
            printf("Argument 3 = %d : amount of Customers can not be less than 1 and more than 10 : Default value : 4\n", countCustomersReceived);
        }
    } else {
        printf("Argument 3 is not a valid integer.\n");
    }
}

