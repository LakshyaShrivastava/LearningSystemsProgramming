#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <cstring>
#include <pthread.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <mqueue.h>
#include <errno.h>
#include <signal.h>


#define BYTES_IN_SEAT_ARR 144
#define BYTES_IN_REQUEST 32
#define BYTES_IN_SELLER_CREATION_DATA 4
#define MSG_BUFFER_SIZE 8192 // this is the default value in Posix msg size 



bool exitKeyPressed = false;
int seats[6][6] = {0};
int sellerCount = 1;
std::vector<void*> sellerRequestBuffers = {};
mqd_t sellerRequestEvts;  // queue for incoming request from sellers
mqd_t sellerResponseEvts; // queue for outgoing responses to sellers
mqd_t backendEvts;        // queue for backend events such as initializing sellers

pthread_t processingThread;
pthread_t sellerInitThread;
sem_t* sharedMemLock;

void signal_handler(int signum);
int CreateSharedMem(void*& pDataBuf, void*& pSellerCreationData);
int CreateEventQueues();
int UpdateSharedMem(void* pSeatsBuf, void* pReqBuf, int bookingResult);
int BookSeats(int* bookingRequest);
int BookSeat(int seatNumber);

void* ProcessingThread(void* args);
void* SellerInitThread(void* args);

int main()
{
    // Register signal handler for Ctrl + C
    signal(SIGINT, signal_handler);

    sharedMemLock = sem_open("sharedMemLock", O_CREAT, 0666, 1);
    if (sharedMemLock == SEM_FAILED)
    {
        printf("Error making sharedMemLock\n");
        return EXIT_FAILURE;
    }
    
    void* pDataBuf;
    void* pSellerCreationBuf;
    int res = CreateSharedMem(pDataBuf, pSellerCreationBuf);
    if (res != 0)
    {
        return res;
    }
    
    res = CreateEventQueues();
    if (res != 0)
    {
        return res;
    }

    res = pthread_create(&sellerInitThread, NULL, SellerInitThread, pSellerCreationBuf);
    if (res != 0)
    {
        printf("Error starting sellerInitThread: %d\n", res);
        return EXIT_FAILURE;
    }

    res = pthread_create(&processingThread, NULL, ProcessingThread, pDataBuf);
    if (res != 0)
    {
        printf("Error starting processingThread: %d\n", res);
        return EXIT_FAILURE;
    }

    
    res = pthread_join(sellerInitThread, NULL);
    if (res != 0)
    {
        printf("Error joining sellerInitThread: %d\n", res);
        return EXIT_FAILURE;
    }

    res = pthread_join(processingThread, NULL);
    if (res != 0)
    {
        printf("Error joining processingThread: %d\n", res);
        return EXIT_FAILURE;
    }

    shm_unlink("seatData");
    mq_unlink("/backendEvts");
    mq_unlink("/requestEvts");
    mq_unlink("/responseEvts");

    return EXIT_SUCCESS;
}

// Signal handler function
void signal_handler(int signum)
{
    if (signum == SIGINT) 
    {
        printf("\nCtrl + C pressed. Shutting down...\n");
        exitKeyPressed = true;
        int res = mq_send(backendEvts, "shutDownProcess", strlen("shutDownProcess"), 1);
        res = mq_send(sellerRequestEvts, "shutDownProcess", strlen("shutDownProcess"), 1);
    }
}

int CreateSharedMem(void*& pDataBuf, void*& pSellerCreationData)
{
    int dataBuf_fd = shm_open("seatData", O_CREAT|O_RDWR, 0666);
    if (dataBuf_fd == -1)
    {
        perror("Error creating seatData mem.\n");
        return EXIT_FAILURE;
    }
    if (ftruncate(dataBuf_fd, BYTES_IN_SEAT_ARR)==-1)
    {
        perror("Error truncating seatData.\n");
        return EXIT_FAILURE;
    }
    pDataBuf = mmap(NULL, BYTES_IN_SEAT_ARR, PROT_READ|PROT_WRITE, MAP_SHARED, dataBuf_fd, 0);
    if (pDataBuf == MAP_FAILED)
    {
        perror("Error mapping seats memory.\n");
        return EXIT_FAILURE;
    }
    
    int sellerCreation_fd = shm_open("sellerCreationData", O_CREAT|O_RDWR, 0666);
    printf("sellerCreation FD: %d\n", sellerCreation_fd);
    if (sellerCreation_fd == -1)
    {
        perror("Error creating sellerCreation mem.\n");
        return EXIT_FAILURE;
    }
    if (ftruncate(sellerCreation_fd, BYTES_IN_SELLER_CREATION_DATA)==-1)
    {
        perror("Error truncating seatData.\n");
        return EXIT_FAILURE;
    }
    pSellerCreationData = mmap(0, BYTES_IN_SELLER_CREATION_DATA, PROT_READ|PROT_WRITE, MAP_SHARED, sellerCreation_fd, 0);
    if (pSellerCreationData == MAP_FAILED)
    {
        perror("Error mapping creationData memory.\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int CreateEventQueues()
{
    backendEvts = mq_open("/backendEvts", O_CREAT | O_RDWR, 0666, NULL);
    if (backendEvts == -1)
    {
        printf("Error creating backendEvts queue: %d\n", errno);
        return EXIT_FAILURE;
    }
    sellerRequestEvts = mq_open("/requestEvts", O_CREAT | O_RDWR, 0666, NULL); // should be read only, gave both temp
    if (sellerRequestEvts == -1)
    {
        printf("Error creating sellerRequestEvts queue: %d\n", errno);
        return EXIT_FAILURE;
    }
    sellerResponseEvts = mq_open("/responseEvts", O_CREAT | O_RDWR, 0666, NULL); // should be write only
    if (sellerResponseEvts == -1)
    {
        printf("Error creating sellerResponseEvts queue: %d\n", errno);
        return EXIT_FAILURE;
    }    
    return EXIT_SUCCESS;
}

int UpdateSharedMem(void* pSeatsBuf, void*pReqBuf, int bookingResult)
{
	printf("Updating Shared Memory.\n");
	memcpy(pSeatsBuf, seats, BYTES_IN_SEAT_ARR);
	printf("Updated seats array. New vals:\n");

	int count = 1;
	int* seats = (int*)pSeatsBuf;
	for (int r = 0; r < 6; r++)
	{
		for (int c = 0; c < 6; c++)
		{
			if (seats == nullptr)
			{
				break;
			}
			if (*seats == 0)
			{
				printf(" %d ", count);
			}
			else
			{
				printf(" * ");
			}
			count++;
			seats++;
		}
		printf("\n");
	}

	printf("Updating request data with the result.\n");

	int* request = (int*)pReqBuf;
	request[7] = bookingResult;

	printf("Request body with return val:");
	for (int i = 0; i < 8; i++)
	{
		printf("%d ", request[i]);
	}
	printf("\n");

	return EXIT_SUCCESS;
}

int BookSeats(int* request)
{
    // request format = [#seats, , , , , , ,returnval]
    int totalSeats = request[0];
    if (totalSeats == 0)
    {
        printf("No seats to book. Exiting.\n");
        return 0;
    }

    int res = 0;
    printf("Booking %d seats.\n", totalSeats);
    for(int i = 1; i <= totalSeats+1; i++)
    {
        res = BookSeat(request[i]);
        if (res != 0)
        {
            printf("Backend Error: could not book seat %d, already booked.\n", request[i]);
            return -1;
        }
        
    }

    return 1; // success code different  to keep it different from mem init val
}

int BookSeat(int seatNumber)
{
    int* curr = &seats[0][0];
    curr += seatNumber-1;
    if (*curr != 0)
    {
        return EXIT_FAILURE;
    }
    *curr = 1;
    return EXIT_SUCCESS;
}

void* ProcessingThread(void* args)
{
    char msgBuffer[MSG_BUFFER_SIZE];
    printf("Processing: waiting for startProcessing evt.\n");

    while (!exitKeyPressed)
    {
        ssize_t res = mq_receive(backendEvts, msgBuffer, MSG_BUFFER_SIZE, NULL);
        if (res == -1)
        {
           printf("Processing: Error occured while waiting for startProcessing: %d\n", errno);
           return nullptr;
        }
        if (strcmp(msgBuffer,"startProcessing") != 0)
        {
            res = mq_send(backendEvts, msgBuffer, strlen(msgBuffer), 0);
            if (res == -1)
            {
                printf("Processing: Error occured while requeuing evt: %d\n", errno);
                return nullptr;
            }
            continue;
        }
        else if (strcmp(msgBuffer, "shutDownProcess") == 0)
        {
            return nullptr;
        }
        else 
        {
            break;
        }
    }

    printf("Processing: starting processing.\n");
    
    int* sharedSeatData = (int*) args;
    printf("Processing:  Waiting for incoming request.\n");
    struct mq_attr attr;
    while (!exitKeyPressed)
    {
        ssize_t res = mq_receive(sellerRequestEvts, msgBuffer, MSG_BUFFER_SIZE, NULL); 
        printf("Recieved %s event.\n", msgBuffer);
        if (res == -1)
        {
           printf("Processing: Error occured while waiting for incoming request: %d\n", errno);
           return nullptr;
        }
        mq_getattr(sellerRequestEvts, &attr);
        printf("\tMsgs currently in q: %ld.\n", attr.mq_curmsgs);
        if (strcmp(msgBuffer, "shutDownProcess")==0)
        {
            printf("Processing: Shut down Recieved: exiting");
            return nullptr;
        }

        if (strcmp(msgBuffer, "1") == 0)
        {
            printf("Found correct event.\n");
        }

        
        // msg is going to be seller id of the process requesting the booking
        int index = std::stoi(msgBuffer);

        printf("Processing: Received request from seller %d.\n", index);
        printf("Processing: Getting the request from its buffer.\n");
		void* pSellerRequestBuf = sellerRequestBuffers[index-1];
		int* request = (int*)pSellerRequestBuf;

		printf("Request body: ");
		for (int i = 0; i < 8; i++)
		{
			printf("%d ", request[i]);
		}
		printf("\n");

		printf("Backend: Waiting for lock.\n");
		if (sem_wait(sharedMemLock) == -1)
		{
			perror("Processing: Error waiting for hSharedMemLock.\n");
		}
		printf("Backend: Acquired lock, booking seats.\n");
		int ret = BookSeats(request);
		UpdateSharedMem(sharedSeatData, request, ret);

        int length = snprintf(NULL, 0, "%d", index);
		char* responseEvtName = (char*)malloc(length + 1);

		if (responseEvtName != NULL) 
        {
			sprintf(responseEvtName,"%d", index); // backend returns seller # of which request was just processed
		}
		else 
        {
			printf("SellerInit: Memory allocation failed\n");
			return nullptr;
		}

        printf("Backend: Setting response event.\n");
        ret = mq_send(sellerResponseEvts, responseEvtName, strlen(responseEvtName)+1, 0);

        sem_post(sharedMemLock);
		printf("Backend: Released lock.\n");
    }

    return NULL;
}

void* SellerInitThread(void* args)
{
    ssize_t res;
    int* sellerCreationData = (int*) args;
    char msgBuffer[MSG_BUFFER_SIZE];
    printf("SellerInit: Waiting for event to trigger.\n");
    while (!exitKeyPressed)
    {
        
        res = mq_receive(backendEvts, msgBuffer, MSG_BUFFER_SIZE, NULL);
        //printf("Recieved msg: %s\n", msgBuffer);
        if (res == -1)
        {
            printf("SellerInit: Error while waiting for startProcessing: %d\n", errno);
            return nullptr;
        }
        if (strcmp(msgBuffer, "sellerStarted")!=0)
        {
            res = mq_send(backendEvts, msgBuffer, strlen(msgBuffer), 0);
            if (res == -1)
            {
                printf("SellerInit: Error while requeuing evt: %d\n", errno);
                return nullptr;
            }
            else 
            {
                continue;
            }
        }
        else if (strcmp(msgBuffer, "shutDownProcess")==0)
        {
            printf("SellerInit: Recieved shutdown. Exiting.\n");
            return nullptr;
        }

        printf("SellerInit: Seller started!\n");
        
        int length = snprintf(NULL, 0, "seller%d", sellerCount);
		if (length < 0) {
			printf("SellerInit: snprintf failed\n");
			return nullptr;
		}

		char* name = (char*)malloc(length + 1);
		if (name == NULL) {
			printf("SellerInit: Memory allocation failed\n");
			return nullptr;
		}

		int result = sprintf(name, "seller%d", sellerCount);
		if (result < 0) {
			printf("SellerInit: sprintf_s failed with error %d\n", result);
			free(name);
			return nullptr;
		}

        int req_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
        if (req_fd == -1)
        {
            printf("SellerInit: failed to make request memory for %s", name);
            return nullptr;
        }
        if (ftruncate(req_fd, BYTES_IN_REQUEST) == -1)
        {
            printf("SellerInit: ftruncate failed for %s", name);
            return nullptr;
        }
        void* reqBuf = mmap(NULL, BYTES_IN_REQUEST, PROT_READ|PROT_WRITE, MAP_SHARED, req_fd, 0);
        if (reqBuf == MAP_FAILED)
        {
            printf("SellerInit: Map Failed for %s", name);
            return nullptr;
        }
        printf("Created request memory for %s.\n", name);
        sellerRequestBuffers.push_back(reqBuf);
        
        *sellerCreationData = sellerCount;
        sellerCount++;
        const char* evtMemCreated = "memCreatedForSeller";
        int ret = mq_send(sellerResponseEvts, evtMemCreated, strlen(evtMemCreated), 0);
        if (ret == -1)
        {
            printf("SellerInit: Error setting memCreatedForSeller: %d\n", errno);
            return nullptr;
        }
        const char* evtStartProcessing = "startProcessing";
        ret = mq_send(backendEvts, evtStartProcessing, strlen(evtStartProcessing), 0);
        if (ret == -1)
        {
            printf("SellerInit: Error setting evtStartProcessing.\n");
            return nullptr;
        }
    }
    return nullptr;
}