#include <Windows.h>
#include <stdio.h>

#define BYTES_IN_SEAT_ARR 144 // 6*6*4 for 2d int array
#define BYTES_IN_REQUEST 32

int seats[6][6] = { 0 };
HANDLE hSharedMemLock;

typedef struct {
	int* sharedSeatData;
	int* incRequestData;
	HANDLE hEvtIncomingRequest;
	HANDLE hEvtOutgoingResponse;
}processingParams;

int CreateSharedMemory(LPVOID& pDataBuf, LPVOID& pReqBuf);
int CreateSharedEvents(HANDLE& evtRequest, HANDLE& evtResponse);
int UpdateSharedMemory(LPVOID seatsBuf, LPVOID pReqBuf, int bookingResult);
int bookSeats(int* bookingRequest);
int bookSeat(int seatNumber);

DWORD WINAPI ProcessingThread(LPVOID param);

int main()
{	
	hSharedMemLock = CreateSemaphoreA(NULL, 1, 1, "sharedMemLock");
	//hSharedMemLock = CreateMutexA(NULL, false, "sharedMemLock");
	if (hSharedMemLock == NULL)
	{
		perror("Backend Error: could not create seatArrLock semaphore.\n");
		return EXIT_FAILURE;
	}

	LPVOID pDataBuf, pReqBuf;
	int res = CreateSharedMemory(pDataBuf, pReqBuf);
	if (res != 0)
	{
		return res;
	}

	int* data = (int*) pDataBuf;
	
	int* request = (int*)pReqBuf;

	printf("Request buffer on creation is at %p.\n", &pReqBuf);

	HANDLE hEvtRequest, hEvtResponse;
	res = CreateSharedEvents(hEvtRequest, hEvtResponse);
	if (res != 0)
	{
		return res;
	}

	processingParams* params = (processingParams*)malloc(sizeof(processingParams));
	if (params == NULL)
	{
		perror("Backend Error: error allocation memory for thread params.\n");
		return EXIT_FAILURE;
	}


	params->sharedSeatData = (int*)pDataBuf;
	params->incRequestData = (int*)pReqBuf;
	params->hEvtIncomingRequest = hEvtRequest;
	params->hEvtOutgoingResponse = hEvtResponse;


	HANDLE processingThread = CreateThread(NULL, 0, ProcessingThread, params, NULL, NULL);
	if (processingThread == 0)
	{
		perror("Backend Error: could not launch array.\n");
		return EXIT_FAILURE;
	}

	WaitForSingleObject(processingThread, INFINITE);
	free(params);
	return 0;
}

int CreateSharedMemory(LPVOID& pDataBuf, LPVOID& pReqBuf)
{
	// making seatMap memory
	HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, BYTES_IN_SEAT_ARR, "seatMap");
	if (hMap == 0)
	{
		perror("Backend Error: Could not create shared seatMap memory.\n");
		return EXIT_FAILURE;
	}

	pDataBuf = MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, BYTES_IN_SEAT_ARR);
	if (pDataBuf == NULL)
	{
		perror("Backend Error: could not make view of seatMap memory.\n");
		return EXIT_FAILURE;
	}

	HANDLE incRequestMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, BYTES_IN_REQUEST, "requestDataMap");
	if (incRequestMap == 0)
	{
		perror("Backend Error: Could not created shared requestDataMap memory.\n");
		return EXIT_FAILURE;
	}

	pReqBuf = MapViewOfFile(incRequestMap, FILE_MAP_ALL_ACCESS, 0, 0, BYTES_IN_REQUEST);
	if (pDataBuf == NULL)
	{
		perror("Backend Error: could not make view of requestDataMap memory.\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

int CreateSharedEvents(HANDLE& evtRequest, HANDLE& evtResponse)
{
	evtRequest = CreateEventA(NULL, false, false, "requestEvent");
	if (evtRequest == 0)
	{
		return EXIT_FAILURE;
	}
	evtResponse = CreateEventA(NULL, false, false, "responseEvent");
	if (evtResponse == 0)
	{
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

int UpdateSharedMemory(LPVOID seatsBuf, LPVOID pReqBuf, int bookingResult)
{	
	printf("Updating Shared Memory.\n");
	memcpy(seatsBuf, seats, BYTES_IN_SEAT_ARR);
	printf("Updated seats array. New vals:\n");
	
	int count = 1;
	int* seats = (int*)seatsBuf;
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

int bookSeats(int* request)
{
	int totalSeats = request[0];
	int res = 0;

	printf("Booking %d seats.\n", totalSeats);
	for (int i = 1; i <= totalSeats; i++)
	{
		res = bookSeat(request[i]);
		if (res != 0)
		{
			printf("Backend Error: could not book seat %d, already booked.\n", request[i]);
			return EXIT_FAILURE;
		}
	}
	if (totalSeats != 0)
	{
		return 2;
	}
	else
	{
		return 0;
	}
}

int bookSeat(int seatNumber)
{
	int* curr = &seats[0][0];
	curr += seatNumber - 1;
	if (*curr != 0)
	{
		return EXIT_FAILURE;
	}
	*curr = 1;
	return EXIT_SUCCESS;
}

DWORD WINAPI ProcessingThread(LPVOID param)
{
	processingParams* params = (processingParams*)param;

	while (true)
	{
		printf("Backend: Waiting for incoming request.\n");
		if (WaitForSingleObject(params->hEvtIncomingRequest, INFINITE) == WAIT_FAILED)
		{
			perror("Error waiting for incoming request event");
			continue;
		}
		printf("Backend: Recieved incoming request.\n");
		printf("Incoming Request body: ");
		
		for (int i = 0; i < 8; i++)
		{
			printf("%d ", params->incRequestData[i]);
		}
		printf("\n");

		printf("Backend: Waiting for lock.\n");
		if (WaitForSingleObject(hSharedMemLock, INFINITE) == WAIT_FAILED)
		{
			perror("Error waiting for semaphore");
			continue;
		}
		printf("Backend: Acquired lock, booking seats.\n");
		
		int res = bookSeats(params->incRequestData);

		UpdateSharedMemory(params->sharedSeatData, params->incRequestData, res);

		printf("Backend: Setting response event.\n");
		SetEvent(params->hEvtOutgoingResponse);
		printf("Backend: Releasing lock.\n");

		LONG previousCount = 0;
		ReleaseSemaphore(hSharedMemLock, 1, &previousCount);

		printf("Semaphore count before releasing: %ld\n", previousCount);
		printf("Semaphore count after releasing: %ld\n", previousCount+1);

		printf("Backend: Released lock.\n");
		//ReleaseMutex(hSharedMemLock);
	}
}