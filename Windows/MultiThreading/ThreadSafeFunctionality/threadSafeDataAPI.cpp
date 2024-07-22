#include "threadSafeDataAPI.h"

CData::CData() 
{
	mReadIndex = 0;
	mWriteIndex = 0;
	mAllDataCleared = false;
	mAllDataWritten = false;
	mHEvtDataAvail = CreateEventA(NULL, false, false, NULL);
	mHEvtSpaceAvail = CreateEventA(NULL, false, true, NULL);

	for (int i = 0; i < 5; i++)
	{
		mData[i] = NULL;
		mFileNames[i] = NULL;
	}
	InitializeCriticalSection(&mCS);
	if (DEBUG)
		printf("Critical Section Data: lockCount = %d, recursionCount = %d\n", mCS.LockCount, mCS.RecursionCount);
}

CData::~CData()
{
	if (DEBUG)
		printf("Critical Section Data: lockCount = %d, recursionCount = %d\n", mCS.LockCount, mCS.RecursionCount);
	DeleteCriticalSection(&mCS);
}


void CData::writeData(char* name, char* data, int id)
{
	bool doneWriting = false;
	while (true) 
	{
		if (DEBUG)
		{
			printf("Producer %d: entering critical section.\n", id);
			printf("%d: Critical Section Data: lockCount = %d, recursionCount = %d\n", id, mCS.LockCount, mCS.RecursionCount);
		}
		
		WaitForSingleObject(mHEvtSpaceAvail, INFINITE);
		
		if (DEBUG)
		{
			printf("Producer %d: entered critical section.\n", id);
			printf("%d: Critical Section Data: lockCount = %d, recursionCount = %d\n", id, mCS.LockCount, mCS.RecursionCount);
		}
		
		if (mWriteIndex == TOTAL_FILE_BUFFERS)
		{
			mWriteIndex = 0;
		}

		if (mData[mWriteIndex] == NULL)
		{
			if (mWriteIndex == 5)
			{
				MessageBox(0, 0, 0, 0);
			}
			mFileNames[mWriteIndex] = (char*)malloc(strlen(name) + 1);
			mData[mWriteIndex] = (char*)malloc(strlen(data) + 1);

			strcpy_s(mFileNames[mWriteIndex], strlen(name) + 1, name);
			strcpy_s(mData[mWriteIndex], strlen(data) + 1, data);

			doneWriting = true;
			mWriteIndex++;
		}
		SetEvent(mHEvtDataAvail);
		
		if (DEBUG)
		{
			printf("Producer %d: leaving critical section.\n", id);
			printf("%d: Critical Section Data: lockCount = %d, recursionCount = %d\n", id, mCS.LockCount, mCS.RecursionCount);
		}
		
		if (doneWriting)
		{
			break;
		}
	}
	return;
}

void CData::readAndDeleteData(int id) {
	char* tempData = nullptr;
	char* tempFileName = nullptr;

	while (true)
	{	
		if (DEBUG)
		{
			printf("Consumer %d: entering event section.\n", id);
			//printf("Critical Section Data: lockCount = %d, recursionCount = %d\n", mCS.LockCount, mCS.RecursionCount);
		}
		
		WaitForSingleObject(mHEvtDataAvail, INFINITE);
		
		if (mAllDataCleared || mAllDataWritten)
		{
			if (DEBUG)
				printf("Consumer %d: ALL DATA PRINTED/WRITTEN, FINISHING NOW\n", id);
			SetEvent(mHEvtDataAvail);
			break;
		}

		if (DEBUG)
		{
			printf("Consumer %d: entered event section.\n", id);
			//printf("Critical Section Data: lockCount = %d, recursionCount = %d\n", mCS.LockCount, mCS.RecursionCount);
		}
		
		if (mReadIndex == TOTAL_FILE_BUFFERS)
		{
			mReadIndex = 0;
		}

		if (mData[mReadIndex] != nullptr && mFileNames[mReadIndex] != nullptr && !mAllDataCleared)
		{
			tempFileName = (char*)malloc(strlen(mFileNames[mReadIndex]) + 1);
			tempData = (char*)malloc(strlen(mData[mReadIndex]) + 1);

			strcpy_s(tempFileName, strlen(mFileNames[mReadIndex])+1, mFileNames[mReadIndex]);
			strcpy_s(tempData, strlen(mData[mReadIndex])+1, mData[mReadIndex]);

			free(mData[mReadIndex]);
			free(mFileNames[mReadIndex]);

			mData[mReadIndex] = nullptr;
			mFileNames[mReadIndex] = nullptr;
			mReadIndex++;
		}

		SetEvent(mHEvtSpaceAvail);

		if (DEBUG)
		{
			printf("Consumer %d: leaving event section.\n", id);
			// printf("Critical Section Data: lockCount = %d, recursionCount = %d\n", mCS.LockCount, mCS.RecursionCount);
		}

		if (tempData != nullptr)
		{
			print(tempFileName, tempData, id);
			free(tempData);
			free(tempFileName);
			printedFiles++;
			break;
		}
	}
	return;
}

void CData::printRemaining(int id)
{
	if (DEBUG) 
	{
		printf("Consumer %d: entering printRemaining critical section.\n", id);
		//printf("Critical Section Data: lockCount = %d, recursionCount = %d\n", mCS.LockCount, mCS.RecursionCount);
	}

	EnterCriticalSection(&mCS);

	if (DEBUG)
	{
		printf("Consumer %d: entered printRemaining critical section.\n", id);
		printf("Critical Section Data: lockCount = %d, recursionCount = %d\n", mCS.LockCount, mCS.RecursionCount);
	}
	
	if (!mAllDataCleared)
	{
		if (DEBUG)
		{
			printf("Final print from consumer %d.\n", id);
		}
		for (int i = 0; i < TOTAL_FILE_BUFFERS; i++)
		{
			if (mData[i] != nullptr && mFileNames[i] != nullptr)
			{
				print(i);
				free(mData[i]);
				free(mFileNames[i]);
				printedFiles++;
			}
		}
		mAllDataCleared = true;
	}

	LeaveCriticalSection(&mCS);

	if (DEBUG)
	{
		printf("Consumer %d: leaving printRemaining critical section.\n", id);
		printf("Critical Section Data: lockCount = %d, recursionCount = %d\n", mCS.LockCount, mCS.RecursionCount);
	}
	
	return;
}

void CData::print(int index)
{
	printf("\n---------------------------------------------------------------------------\n");
	printf("%s:\n", mFileNames[index]);
	printf("%s\n", mData[index]);
	printf("\n---------------------------------------------------------------------------\n");
}

void CData::print(char* filename, char* data, int id)
{
	printf("\n---------------------------------------------------------------------------\n");
	if (DEBUG)
	{
		printf("Printed by consumer %d.\n", id);
	}
	printf("%s:\n", filename);
	printf("%s\n", data);
	printf("\n---------------------------------------------------------------------------\n");
}

void CData::setAllDataWritten()
{
	mAllDataWritten = true;
	SetEvent(mHEvtDataAvail);
}