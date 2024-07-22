#include "threadSafeDataAPI.h"

#define MAX_NAME_LENGTH 512
#define NUM_PRODUCER_THREADS 5
#define NUM_CONSUMER_THREADS 5


typedef struct {
	char filename[MAX_NAME_LENGTH];
	char directory[MAX_NAME_LENGTH];
	CData* data;
	int id;
} producerParams;

typedef struct {
	CData* data;
	int id;
} consumerParams;

// global vars
int finishedProducerCount = 0;
int finishedConsumerCount = 0;

// threads
DWORD WINAPI producerThread(LPVOID param);
DWORD WINAPI consumerThread(LPVOID param);

// helper functions
void produceData(char* directory, char* filename, CData* data, int id);
char* getFileContent(FILE* file);

int main()
{
	char directory[] = "C:\\Users\\LakshCPlusPLus\\OneDrive - UC Irvine\\Desktop\\CompSci132";
	char filename[] = "test";

	CData data;

	HANDLE producers[NUM_PRODUCER_THREADS];
	HANDLE consumers[NUM_CONSUMER_THREADS];

	for (int i = 0; i < NUM_PRODUCER_THREADS; i++)
	{
		producerParams* params = (producerParams*)malloc(sizeof(producerParams));
		strcpy_s(params->directory, directory);
		strcpy_s(params->filename, filename);
		params->data = &data;
		params->id = i + 1;

		producers[i] = CreateThread(NULL, 0, producerThread, &(*params), NULL, NULL);

		
		if (producers[i] == 0)
		{
			printf("Error In producer thread creation.\n");
			return 2;
		}
	}

	for (int i = 0; i < NUM_CONSUMER_THREADS; i++)
	{
		consumerParams* params = (consumerParams*)malloc(sizeof(consumerParams));
		params->id = i + 1;
		params->data = &data;

		consumers[i] = CreateThread(NULL, 0, consumerThread, &(*params), NULL, NULL);
		if (consumers[i] == 0)
		{
			printf("Error In consumer thread creation.\n");
			return 2;
		}
	}

	WaitForMultipleObjects(NUM_PRODUCER_THREADS, producers, TRUE, INFINITE);
	WaitForMultipleObjects(NUM_CONSUMER_THREADS, consumers, TRUE, INFINITE);

	printf("Total Files Printed = %d.\n", data.printedFiles);

	return 0;
}


DWORD WINAPI consumerThread(LPVOID param)
{
	consumerParams* params = (consumerParams*)param;

	if (DEBUG)
		printf("Consumer Thread Started.\n");
	while (true)
	{
		params->data->readAndDeleteData(params->id);
		if (DEBUG)
			printf("Consumer: finishedProducerCount = %d\n", finishedProducerCount);
		
		if (finishedProducerCount == NUM_PRODUCER_THREADS)
		{
			if (DEBUG)
				printf("Consumer thread reached end condition.\n");
			params->data->printRemaining(params->id);
			break;
		}
	}
	finishedConsumerCount++;
	if (DEBUG)
	{
		printf("***************Consumer Thread Finished***********\n");
		printf(" Finished Consumers = %d\n", finishedConsumerCount);
	}
	free(params);
	return 0;
}

DWORD WINAPI producerThread(LPVOID param)
{
	producerParams* params = (producerParams*)param;
	if(DEBUG)
		printf("Producer %d: Thread Started.\n", params->id);

	produceData(params->directory, params->filename, params->data, params->id);
	finishedProducerCount++;
	
	if (finishedProducerCount == NUM_PRODUCER_THREADS)
	{
		params->data->setAllDataWritten();
	}

	if(DEBUG)
		printf("**************Producer %d: Thread finished.**************\n", params->id);
	
	free(params);
	return 0;
}

void produceData(char* directory, char* filename, CData* data, int id)
{	
	WIN32_FIND_DATAA findFileData;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	char dirSpec[MAX_PATH];
	
	sprintf_s(dirSpec, "%s\\*", directory);

	hFind = FindFirstFileA(dirSpec, &findFileData);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		return;
	}

	do
	{
		if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (strcmp(findFileData.cFileName, ".") == 0 || strcmp(findFileData.cFileName, "..") == 0)
			{
				continue;
			}
			if(DEBUG)
				printf("Producer %d: Found Directory %s.\n", id, findFileData.cFileName);

			char nextDirectory[MAX_PATH];
			sprintf_s(nextDirectory, "%s\\%s", directory, findFileData.cFileName);
			produceData(nextDirectory, filename, data, id);
		}
		else
		{
			if (strncmp(findFileData.cFileName, filename, strlen(filename)) == 0)
			{
				if (DEBUG)
					printf("Producer %d: found matching file %s.\n", id, findFileData.cFileName);
				char completePath[MAX_PATH];
				sprintf_s(completePath, "%s\\%s", directory, findFileData.cFileName);
				
				FILE* file;
				errno_t err = fopen_s(&file, completePath, "rb");
				if (file != NULL)
				{
					char* contents = getFileContent(file);
					data->writeData(findFileData.cFileName, contents, id);
				}
			}
		}
	} while (FindNextFileA(hFind, &findFileData) != 0);
	FindClose(hFind);
	return;
}

char* getFileContent(FILE* file)
{
	char* fileBuffer;
	int fileSize;
	size_t res;

	fseek(file, 0, SEEK_END);
	fileSize = ftell(file);
	rewind(file);

	fileBuffer = (char*)malloc(sizeof(char) * (fileSize + 1)); // Add +1 for null-terminator

	if (fileBuffer == NULL) {
		perror("Memory error");
		fclose(file);
		return NULL;
	}

	res = fread_s(fileBuffer, fileSize, 1, fileSize, file);

	if (res != fileSize) {
		perror("Reading error");
		free(fileBuffer);
		fclose(file);
		return NULL;
	}

	fileBuffer[fileSize] = '\0'; // Null-terminate the buffer
	fclose(file);
	return fileBuffer;
}