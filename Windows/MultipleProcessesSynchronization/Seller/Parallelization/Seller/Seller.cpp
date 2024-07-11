#include <stdio.h>
#include <Windows.h>

#define BYTES_IN_SEAT_ARR 144 // 6*6*4 for 2d int array
#define BYTES_IN_REQUEST 32
#define BYTES_IN_SELLER_CREATION_DATA 4

HANDLE hSharedMemLock;
HANDLE hEvtSellerStarted;
HANDLE hEvtMemoryCreatedForSeller;

typedef struct {
	int* sharedSeatData;
	int* requestData;
	HANDLE hEvtOutgoingRequest;
	HANDLE hEvtIncomingResponse;
}bookingParams;

bookingParams* GetSharedMemoryAndEvents();
void PrintCurrentSeatMap(int* seats);
void BuildRequest(int count, int* seatsToBook, int* request);
void ResetRequestData(int* request);
DWORD WINAPI BookingThread(LPVOID param);


int main()
{
	printf("main started\n");
	hSharedMemLock = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, "sharedMemLock");



	bookingParams* params = GetSharedMemoryAndEvents();
	if (params == NULL)
	{
		return EXIT_FAILURE;
	}

	printf("start thread\n");
	HANDLE bookingThread = CreateThread(NULL, 0, BookingThread, params, NULL, NULL);

	WaitForSingleObject(bookingThread, INFINITE);

	free(params);
	return 0;
}

bookingParams* GetSharedMemoryAndEvents()
{
	bookingParams* params = (bookingParams*)malloc(sizeof(bookingParams));
	if (params == NULL)
	{
		printf("Frontend Error: malloc failed for bookingParams.\n");
		return NULL;
	}

	HANDLE seatMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, false, "seatMap");
	if (seatMap == 0)
	{
		perror("Frontend Error: Could not open shared seatMap memory.\n");
		return NULL;
	}
	params->sharedSeatData = (int*)MapViewOfFile(seatMap, FILE_MAP_ALL_ACCESS, 0, 0, BYTES_IN_SEAT_ARR);
	if (params->sharedSeatData == NULL)
	{
		perror("Frontend Error: Could not create MapView of seatMap memory.\n");
		return NULL;
	}

	hEvtSellerStarted = OpenEventA(EVENT_ALL_ACCESS, FALSE, "sellerStarted");
	if (hEvtSellerStarted == NULL)
	{
		perror("Frontend Error: Could not open sellerStarted event.\n");
		return NULL;
	}
	else
	{
		printf("opened sellerStartedEvent.\n");
		printf("Handle value before SetEvent: %p\n", hEvtSellerStarted);
	}

	hEvtMemoryCreatedForSeller = OpenEventA(EVENT_ALL_ACCESS, FALSE, "memCreatedForSeller");
	if (hEvtMemoryCreatedForSeller == NULL)
	{
		perror("Frontend Error: Could not open memCreatedForSeller");
		return NULL;
	}
	

	HANDLE hCreationDataMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, "sellerCreationData");
	int* pCreationData = (int*)MapViewOfFile(hCreationDataMap, FILE_MAP_ALL_ACCESS, 0, 0, BYTES_IN_SELLER_CREATION_DATA);

	printf("setting sellerStarted.\n");
	if (!SetEvent(hEvtSellerStarted)) {
		printf("SetEvent failed with error %d.\n", GetLastError());
	}
	printf("waiting for backend to allocate data.\n");
	if (WaitForSingleObject(hEvtMemoryCreatedForSeller, INFINITE) == WAIT_FAILED)
	{
		printf("Wait failed while waiting for seller data allocation with error %d.\n", GetLastError());
	}
	printf("Done waiting for data allocation.\n");

	int sellerID = *pCreationData;
	int length = snprintf(NULL, 0, "seller%d", sellerID);
	char* name = (char*)malloc(length + 1);

	if (name != NULL) {
		sprintf_s(name, length+1, "seller%d", sellerID);
	}
	else {
		printf("Memory allocation failed\n");
		return NULL;
	}

	HANDLE requestMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name);
	if (requestMap == 0)
	{
		perror("Frontend Error: Could not open shared requestDataMap memory.\n");
		return NULL;
	}
	params->requestData = (int*)MapViewOfFile(requestMap, FILE_MAP_ALL_ACCESS, 0, 0, BYTES_IN_REQUEST);
	if (params->requestData == NULL)
	{
		perror("Frontend Error: Could not create MapView of requestDataMap memory.\n");
		return NULL;
	}

	length = snprintf(NULL, 0, "sellerResponseEvt%d", sellerID);
	char* responseEvtName = (char*)malloc(length + 1);

	if (responseEvtName != NULL) {
		sprintf_s(responseEvtName, length + 1, "sellerResponseEvt%d", sellerID);
	}
	else {
		printf("Memory allocation failed\n");
		return NULL;
	}

	params->hEvtIncomingResponse = OpenEventA(EVENT_ALL_ACCESS, FALSE, responseEvtName);
	if (params->hEvtIncomingResponse == NULL)
	{
		perror("Frontend Error: Could not open event responseEvent.\n");
		return NULL;
	}
	
	length = snprintf(NULL, 0, "sellerRequestEvt%d", sellerID);
	char* requestEvtName = (char*)malloc(length + 1);

	if (requestEvtName != NULL) {
		sprintf_s(requestEvtName, length + 1, "sellerRequestEvt%d", sellerID);
	}
	else {
		printf("Memory allocation failed\n");
		return NULL;
	}


	params->hEvtOutgoingRequest = OpenEventA(EVENT_ALL_ACCESS, FALSE, requestEvtName);
	if (params->hEvtOutgoingRequest == NULL)
	{
		perror("Frontend Error: Could not open event requestEvent.\n");
		return NULL;
	}
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

	DWORD WINAPI BookingThread(LPVOID param)
	{
		bookingParams* params = (bookingParams*)param;
		while (true)
		{
			printf("Current Seats Available: \n");
			PrintCurrentSeatMap(params->sharedSeatData);
			printf("---------------------------------\n");
			int count = 0;
			printf("How many seats would you like to book?(Enter number up to 6)\n");
			scanf_s("%d", &count);

			int seatsToBook[6] = { 0 };
			for (int i = 0; i < count; i++)
			{
				printf("Please enter next seat number: ");
				scanf_s("%d", &seatsToBook[i]);
			}
			printf("Booking %d seats....\n", count);
		
			/*printf("Waiting For seller lock.\n");
			
			DWORD waitResult = WaitForSingleObject(hSellerLock, INFINITE);
			if (waitResult == WAIT_OBJECT_0) 
			{
				printf("Acquired seller lock\n");
			}
			else
			{
				printf("Wait failed, res %d error %d", waitResult, GetLastError());
			}*/

			printf("waiting for sharedMemLock\n");
			DWORD res = WaitForSingleObject(hSharedMemLock, INFINITE);
			if (res == WAIT_FAILED)
			{
				printf("res = %d error = %d\n", res, GetLastError());
			}


			BuildRequest(count, seatsToBook, params->requestData);
			printf("Request body: ");
			for (int i = 0; i < 8; i++)
			{
				printf("%d ", params->requestData[i]);
			}
			printf("\n");
			SetEvent(params->hEvtOutgoingRequest);
			ReleaseMutex(hSharedMemLock);
			printf("Lock released\n");
			printf("waiting on response event\n");
			DWORD ret = WaitForSingleObject(params->hEvtIncomingResponse, INFINITE);
			if (ret == WAIT_OBJECT_0)
			{
				printf("Event signalled\n");
			}
			else
			{
				printf("wait failed ret %d error %d", ret, GetLastError());
			}

			printf("Waiting for shared mem lock to check the response data\n");
			ret = WaitForSingleObject(hSharedMemLock, INFINITE);

			if (ret == WAIT_OBJECT_0)
			{
				printf("lock signalled\n");
			}
			else
			{
				printf("wait failed ret %d error %d", ret, GetLastError());
			}

			if (params->requestData[7] == 2)
			{
				printf("Successfully booked all seats.\n");
			}
			else if (params->requestData[7] == 1)
			{
				printf("Some seats were already booked, please try again.\n");
			}
			else
			{
				printf("Unkown error occured while booking seats.\n");
			}
			ResetRequestData(params->requestData);
			ReleaseMutex(hSharedMemLock);
			//ReleaseMutex(hSellerLock);
		}
	}
