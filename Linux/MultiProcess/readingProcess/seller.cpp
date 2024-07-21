#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <mqueue.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>

#define BYTES_IN_SEAT_ARR 144
#define BYTES_IN_REQUEST 32
#define BYTES_IN_SELLER_CREATION_DATA 4
#define MSG_BUFFER_SIZE 8192                // this is the default value in Posix msg size 

pthread_t bookingThread;
sem_t* sharedMemLock;
mqd_t backendEvts;                          // this is where we will send sellerStarted, and recieve memCreated
mqd_t requestEvts;                          // we will send request messages here  
mqd_t responseEvts;                         // we will recieve response messages here
int sellerID;

typedef struct {
    int* sharedSeatData;
    int* requestData;
}bookingParams;

bookingParams* InitSharedMemAndQueues();
void PrintCurrentSeatMap(int* seats);
void BuildRequest(int count, int* seatsToBook, int* request);
void ResetRequestData(int* request);
void* BookingThread(void* args);

int main ()
{
    printf("Seller: Main start.\n");
    sharedMemLock = sem_open("sharedMemLock", 0); // 0-> backend must start first, can be changed
    if (sharedMemLock == SEM_FAILED)
    {
        printf("Seller: Error making sharedMemLock\n");
        return EXIT_FAILURE;
    }

    bookingParams* params = InitSharedMemAndQueues();
    if (params == nullptr)
    {
        return EXIT_FAILURE;
    }
    
    printf("Seller: start booking thread.\n");
    int ret = pthread_create(&bookingThread, NULL, BookingThread, params);
    if (ret == -1)
    {
        printf("Seller: Failed to Create booking thread.\n");
        return EXIT_FAILURE;
    }

    ret = pthread_join(bookingThread, NULL);
    if (ret == -1)
    {
        printf("Seller: Failed to join booking thread.\n");
        return EXIT_FAILURE;
    }

    free(params);
    return 0;
}

bookingParams* InitSharedMemAndQueues()
{
    bookingParams* params = (bookingParams*)malloc(sizeof(bookingParams));
    if (params == nullptr)
	{
		printf("Frontend Error: malloc failed for bookingParams.\n");
		return nullptr;
	}
    int dataBuf_fd = shm_open("seatData", O_RDWR, 0666);
    if (dataBuf_fd == -1)
    {
        perror("Frontend Error: Error opening seatData mem.\n");
        return nullptr;
    }
    params->sharedSeatData = (int*)mmap(0, BYTES_IN_SEAT_ARR, PROT_READ|PROT_WRITE, MAP_SHARED, dataBuf_fd, 0);
    
    int creationData_fd = shm_open("sellerCreationData", O_RDWR, 0666);
    printf("Seller Creation FD: %d\n", creationData_fd);
    if (creationData_fd == -1)
    {
        perror("Frontend Error: Error opening sellerCreationData mem.\n");
        return nullptr;
    }
    int* pCreationData = (int*)mmap(0,BYTES_IN_SELLER_CREATION_DATA, PROT_READ|PROT_WRITE, MAP_SHARED, creationData_fd, 0);
    backendEvts = mq_open("/backendEvts", O_RDWR, 0666, NULL);
    if (backendEvts == -1)
    {
        printf("Frontend Error: Error creating backendEvts queue: %d\n", errno);
        return nullptr;
    }
    requestEvts = mq_open("/requestEvts", O_RDWR, 0666, NULL); // should be write only, gave both temp
    if (requestEvts == -1)
    {
        printf("Frontend Error: Error creating requestEvts queue: %d\n", errno);
        return nullptr;
    }
    responseEvts = mq_open("/responseEvts", O_RDWR, 0666, NULL); // should be read only
    if (responseEvts == -1)
    {
        printf("Frontend Error: Error creating responseEvts queue: %d\n", errno);
        return nullptr;
    }
    const char* msg = "sellerStarted";
    int ret = mq_send(backendEvts, msg, strlen(msg), 0);
    if (ret == -1)
    {
        printf("Frontend Error: %d could not send sellerStarted.\n", errno);
        return nullptr;
    }
    char evtReceived[MSG_BUFFER_SIZE];
    printf("Seller: waiting for backend to allocate data.\n");
    while (1)
    {
        ret = mq_receive(responseEvts, evtReceived, MSG_BUFFER_SIZE, NULL);
        printf("Received Msg: %s", evtReceived);
        if (strcmp(evtReceived, "memCreatedForSeller")!=0)
        {
            // if the event received is not the correct one, requeue it
            ret = mq_send(responseEvts, evtReceived, MSG_BUFFER_SIZE, 0);
            printf("Frontend Error: recieved incorrect request '%s', requeing it.\n", evtReceived);
        }
        else
        {
            break;
        }
    }
	printf("Seller: Done waiting for data allocation.\n");
    sellerID = *pCreationData;

    int length = snprintf(NULL, 0, "seller%d", sellerID);
	if (length < 0) {
		printf("Frontend Error: snprintf failed\n");
		return nullptr;
	}

	char* name = (char*)malloc(length + 1);
	if (name == NULL) {
		printf("Frontend Error: Memory allocation failed\n");
		return nullptr;
	}

	int result = sprintf(name, "seller%d", sellerID);
	if (result < 0) {
		printf("Frontend Error: sprintf_s failed with error %d\n", result);
		free(name);
		return nullptr;
	}
    printf("Trying to open memory for %s\n", name);
    int requestData_fd = shm_open(name, O_RDWR, 0666);
    if (requestData_fd == -1)
    {
        perror("Frontend Error: Error opening requestData mem.\n");
        return nullptr;
    }
    params->requestData = (int*)mmap(0, BYTES_IN_REQUEST, PROT_READ|PROT_WRITE, MAP_SHARED, requestData_fd, 0);

    return params;
}

void PrintCurrentSeatMap(int* seats)
{
	int count = 1;
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
}

void BuildRequest(int count, int* seatsToBook, int* request)
{
	int* start = request;

	*request = count;
	request++;
	for (int i = 1; i < 7; i++)
	{
		*request = *seatsToBook;
		request++;
		seatsToBook++;
	}
    request = 0;
	request = start;
	return;
}

void ResetRequestData(int* request)
{
	printf("Reseting requestData.\n");

	memset(request, 0, 32);

	printf("Reset requestData.\n");
	return;
}

void* BookingThread(void* args)
{
    bookingParams* params = (bookingParams*) args;
    
    while(true)
    {
        printf("Current Seats Available: \n");
		PrintCurrentSeatMap(params->sharedSeatData);
		printf("---------------------------------\n");
		int count = 0;
		printf("How many seats would you like to book?(Enter number up to 6)\n");
		scanf("%d", &count);

		int seatsToBook[6] = { 0 };
		for (int i = 0; i < count; i++)
		{
			printf("Please enter next seat number: ");
			scanf("%d", &seatsToBook[i]);
		}
		printf("Booking %d seats....\n", count);

        printf("waiting for sharedMemLock\n");
        int ret = sem_wait(sharedMemLock);
        if(ret == -1)
        {
            printf("wait failed while waiting for sharedMemLock");
            return nullptr;
        }

        BuildRequest(count, seatsToBook, params->requestData);
        printf("Request body: ");
		for (int i = 0; i < 8; i++)
		{
			printf("%d ", params->requestData[i]);
		}
		printf("\n");

        // create the message and notify backend
        int length = snprintf(NULL, 0, "%d", sellerID);
	    if (length < 0) 
        {
		    printf("Frontend Error: snprintf failed\n");
		    return nullptr;
	    }

	    char* idAsStr = (char*)malloc(length + 1);
	    if (idAsStr == NULL) 
        {
		    printf("Frontend Error: Memory allocation failed\n");
		    return nullptr;
	    }

	    int result = sprintf(idAsStr, "%d", sellerID);
	    if (result < 0) 
        {
		    printf("Frontend Error: sprintf_s failed with error %d\n", result);
		    free(idAsStr);
		    return nullptr;
	    }
        
        ret = mq_send(requestEvts, idAsStr, strlen(idAsStr)+1, 0);
        printf("\tsent event %s  with length %ld\n", idAsStr, strlen(idAsStr));
        if (ret == -1)
        {
            printf("Seller Error: failed to send request");
            return nullptr;
        }
        ret = sem_post(sharedMemLock);
        printf("Lock released\n");
		printf("waiting on response event\n");

        // check incoming responses, and wait for the correct one
        char msgBuffer[MSG_BUFFER_SIZE];
        while (true)
        {
            ssize_t res = mq_receive(responseEvts, msgBuffer, MSG_BUFFER_SIZE, 0);
            printf("Message Received: %s\n", msgBuffer);
            if (res == -1)
            {
                printf("Seller: Error occured while waiting for response evt %d\n", errno);
                return nullptr;
            }
            if (strcmp(msgBuffer, idAsStr) != 0)
            {
                res = mq_send(backendEvts, msgBuffer, strlen(msgBuffer), 0);
                if (res == -1)
                {
                    printf("Seller: Error occured while requeuing evt: %d\n", errno);
                    return nullptr;
                }
                continue;
            }
            else 
            {
                break;
            }
        }

        // got apropriate response
        printf("Waiting for shared mem lock to check the response data\n");
        ret = sem_wait(sharedMemLock);
        if(ret == -1)
        {
            printf("wait failed while waiting for sharedMemLock");
            return nullptr;
        }

        if (params->requestData[7] == 1)
		{
			printf("Successfully booked all seats.\n");
		}
		else if (params->requestData[7] == -1)
		{
			printf("Some seats were already booked, please try again.\n");
		}
		else
		{
			printf("Unkown error occured while booking seats.\n");
		}
		ResetRequestData(params->requestData);

        sem_post(sharedMemLock);
    }

    return nullptr;
}
