#include <Windows.h>
#include <stdio.h>
#include <vector>

#define BYTES_IN_SEAT_ARR 144 // 6*6*4 for 2d int array
#define BYTES_IN_REQUEST 32
#define BYTES_IN_SELLER_CREATION_DATA 4

int seats[6][6] = { 0 };
int sellerCount = 1;
std::vector<LPVOID> sellerRequestBuffers = {};
std::vector<HANDLE> sellerRequestEvts = {};
std::vector<HANDLE> sellerResponseEvts = {};

HANDLE hSharedMemLock;
HANDLE hEvtSellerStarted;
HANDLE hEvtMemoryCreatedForSeller;
HANDLE hEvtStartProcessing;
HANDLE hEvtRefreshData;

int CreateSharedMemory(LPVOID& pDataBuf, LPVOID& pSellerCreationData);
int CreateSharedEvents(HANDLE& evtRequest, HANDLE& evtResponse);
int UpdateSharedMemory(LPVOID seatsBuf, LPVOID pReqBuf, int bookingResult);
int bookSeats(int* bookingRequest);
int bookSeat(int seatNumber);

DWORD WINAPI ProcessingThread(LPVOID param);
DWORD WINAPI SellerInitializationThread(LPVOID param);

int main()
{
	hSharedMemLock = CreateMutexA(NULL, false, "sharedMemLock");
	if (hSharedMemLock == NULL)
	{
		perror("Backend Error: could not create sharedMemLock mutex.\n");
		return EXIT_FAILURE;
	}

	LPVOID pDataBuf, pSellerCreationBuf;
	int res = CreateSharedMemory(pDataBuf, pSellerCreationBuf);
	if (res != 0)
	{
		return res;
	}

	int* seatData = (int*)pDataBuf;
	int* sellerCreationData = (int*)pSellerCreationBuf;

	HANDLE hEvtRequest, hEvtResponse;
	res = CreateSharedEvents(hEvtRequest, hEvtResponse);
	if (res != 0)
	{
		return res;
	}

	/*processingParams* params = (processingParams*)malloc(sizeof(processingParams));
	if (params == NULL)
	{
		perror("Backend Error: error allocation memory for thread params.\n");
		return EXIT_FAILURE;
	}*/


	/*params->sharedSeatData = (int*)pDataBuf;
	params->hEvtIncomingRequest = hEvtRequest;
	params->hEvtOutgoingResponse = hEvtResponse;*/

	HANDLE sellerInitThread = CreateThread(NULL, 0, SellerInitializationThread, sellerCreationData, NULL, NULL);
	if (sellerInitThread == 0)
	{
		perror("Backend Error: Failed to launch sellerInitThread.\n");
	}

	HANDLE processingThread = CreateThread(NULL, 0, ProcessingThread, pDataBuf, NULL, NULL);
	if (processingThread == 0)
	{
		perror("Backend Error: could not launch processingThread.\n");
		return EXIT_FAILURE;
	}
	HANDLE threads[] = { sellerInitThread , processingThread };
	WaitForMultipleObjects(2, threads, TRUE, INFINITE);
	//free(params);
	return 0;
}

int CreateSharedMemory(LPVOID& pDataBuf, LPVOID& pSellerCreationBuf)
{
	// making seatMap memory
	HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, BYTES_IN_SEAT_ARR, "seatMap");
	if (hMap == NULL)
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

	// making sellerCreation memory
	HANDLE hCreationMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, BYTES_IN_SELLER_CREATION_DATA, "sellerCreationData");
	if (hCreationMap == NULL)
	{
		perror("Backend Error: Could not create shared sellerCreationData memory.\n");
		return EXIT_FAILURE;
	}

	pSellerCreationBuf = MapViewOfFile(hCreationMap, FILE_MAP_ALL_ACCESS, 0, 0, BYTES_IN_SELLER_CREATION_DATA);
	if (pSellerCreationBuf == NULL)
	{
		perror("Backend Error: Could not create map view of sellerCreationData memory.\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int CreateSharedEvents(HANDLE& evtRequest, HANDLE& evtResponse)
{
	evtRequest = CreateEventA(NULL, FALSE, FALSE, "requestEvent");
	if (evtRequest == 0)
	{
		return EXIT_FAILURE;
	}
	evtResponse = CreateEventA(NULL, FALSE, FALSE, "responseEvent");
	if (evtResponse == 0)
	{
		return EXIT_FAILURE;
	}
	// create the global events
	hEvtSellerStarted = CreateEventA(NULL, FALSE, FALSE, "sellerStarted");
	if (hEvtSellerStarted == 0)
	{
		printf("failed to create sellerStartedEvent.\n");
		return EXIT_FAILURE;
	}
	else
	{
		printf("created sellerStartedEvent.\n");
		printf("Handle value before SetEvent: %p\n", hEvtSellerStarted);
	}

	hEvtMemoryCreatedForSeller = CreateEventA(NULL, FALSE, FALSE, "memCreatedForSeller");
	if (hEvtMemoryCreatedForSeller == 0)
	{
		printf("failed to create memCreatedForSeller.\n");
		return EXIT_FAILURE;
	}
	else
	{
		printf("created memCreatedForSeller.\n");
		printf("Handle value before SetEvent: %p\n", hEvtMemoryCreatedForSeller);
	}

	hEvtStartProcessing = CreateEventA(NULL, FALSE, FALSE, "startProcessing");
	hEvtRefreshData = CreateEventA(NULL, FALSE, FALSE, "refreshData");

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
	// Request Format = [totalSeats,_,_,_,_,_,_, returnVal]
	int totalSeats = request[0];
	int res = 0;

	printf("Booking %d seats.\n", totalSeats);
	for (int i = 1; i <= totalSeats + 1; i++)
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
		return 2; // success keeping it diff from initialization
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
	WaitForSingleObject(hEvtStartProcessing, INFINITE);
	int* sharedSeatData = (int*)param;

	/*while (sellerRequestEvts.empty()) {
		Sleep(100);
	}*/


	while (true)
	{

		HANDLE* sellerRequestEvtsArr = sellerRequestEvts.data();
		DWORD size = sellerRequestEvts.size();
		printf("Backend: Waiting for incoming request.\n");
		DWORD result = WaitForMultipleObjects(size, sellerRequestEvtsArr, FALSE, INFINITE);
		if (result == WAIT_FAILED)
		{
			printf("wait failed with error %d.\n", GetLastError());
		}

		int requestIndex = result - WAIT_OBJECT_0;
		if (requestIndex == 0)
		{
			printf("New seller added, refreshing data.\n");
			continue;
		}

		printf("Recieved request from seller %d.\n", requestIndex); // +1 bc we started seller numbers at 1 

		printf("Getting the request from its buffer.\n");
		LPVOID pSellerRequestBuf = sellerRequestBuffers[requestIndex-1];
		int* request = (int*)pSellerRequestBuf;

		printf("Request body: ");
		for (int i = 0; i < 8; i++)
		{
			printf("%d ", request[i]);
		}
		printf("\n");

		printf("Backend: Waiting for lock.\n");
		if (WaitForSingleObject(hSharedMemLock, INFINITE) == WAIT_FAILED)
		{
			perror("Error waiting for hSharedMemLock.\n");
		}
		printf("Backend: Acquired lock, booking seats.\n");
		int res = bookSeats(request);
		UpdateSharedMemory(sharedSeatData, request, res);
		printf("Backend: Setting response event.\n");
		SetEvent(sellerResponseEvts[requestIndex-1]);

		ReleaseMutex(hSharedMemLock);
		printf("Backend: Released lock.\n");
	}
}

DWORD WINAPI SellerInitializationThread(LPVOID param)
{
	int* returnMemory = (int*)param; 	// define this memory in main

	sellerRequestEvts.push_back(hEvtRefreshData);

	while (true)
	{
		printf("Seller Initialization: Waiting for event to be triggered.\n");
		WaitForSingleObject(hEvtSellerStarted, INFINITE);
		printf("Seller Initialization: seller started!\n");


		int length = snprintf(NULL, 0, "seller%d", sellerCount);
		if (length < 0) {
			printf("snprintf failed\n");
			return EXIT_FAILURE;
		}

		char* name = (char*)malloc(length + 1);
		if (name == NULL) {
			printf("Memory allocation failed\n");
			return EXIT_FAILURE;
		}

		int result = sprintf_s(name, length + 1, "seller%d", sellerCount);
		if (result < 0) {
			printf("sprintf_s failed with error %d\n", result);
			free(name); // Don't forget to free the allocated memory
			return EXIT_FAILURE;
		}

		HANDLE hRequestMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, BYTES_IN_REQUEST, name);
		if (hRequestMap == NULL)
		{
			printf("Failed To Create Mapping for seller %d.\n", sellerCount);
			return EXIT_FAILURE;
		}

		LPVOID pMapView = MapViewOfFile(hRequestMap, FILE_MAP_ALL_ACCESS, 0, 0, BYTES_IN_REQUEST);
		if (pMapView == NULL)
		{
			printf("Failed to create mapping for seller %d.\n", sellerCount);
			return EXIT_FAILURE;
		}

		sellerRequestBuffers.push_back(pMapView);

		length = snprintf(NULL, 0, "sellerResponseEvt%d", sellerCount);
		char* responseEvtName = (char*)malloc(length + 1);

		if (responseEvtName != NULL) {
			sprintf_s(responseEvtName, length + 1, "sellerResponseEvt%d", sellerCount);
		}
		else {
			printf("Memory allocation failed\n");
			return EXIT_FAILURE;
		}

		HANDLE sellerRequestEvent = CreateEventA(NULL, FALSE, FALSE, responseEvtName);

		sellerResponseEvts.push_back(sellerRequestEvent);

		length = snprintf(NULL, 0, "sellerRequestEvt%d", sellerCount);
		char* requestEvtName = (char*)malloc(length + 1);
		if (requestEvtName != NULL)
		{
			sprintf_s(requestEvtName, length + 1, "sellerRequestEvt%d", sellerCount);
			printf("seller requestEvtName: %s\n", requestEvtName);
		}
		else
		{
			printf("Memory allocation failed\n");
			return EXIT_FAILURE;
		}

		HANDLE sellerResponseEvent = CreateEventA(NULL, FALSE, FALSE, requestEvtName);
		sellerRequestEvts.push_back(sellerResponseEvent);

		*returnMemory = sellerCount;
		sellerCount++;
		SetEvent(hEvtMemoryCreatedForSeller);
		SetEvent(hEvtStartProcessing);
		SetEvent(hEvtRefreshData);
	}
}