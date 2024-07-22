#pragma once
#include <Windows.h>
#include <stdio.h>

#define TOTAL_FILE_BUFFERS 5
#define DEBUG false


class CData
{
public:
	int printedFiles = 0;

	CData();
	void writeData(char* name, char* data, int id);
	void readAndDeleteData(int id);
	void printRemaining(int id);
	void setAllDataWritten();
	~CData();
private:
	char* mData[TOTAL_FILE_BUFFERS];
	char* mFileNames[TOTAL_FILE_BUFFERS];
	CRITICAL_SECTION mCS;
	int mReadIndex;
	int mWriteIndex;
	bool mAllDataCleared;
	bool mAllDataWritten;
	HANDLE mHEvtDataAvail;
	HANDLE mHEvtSpaceAvail;

	void print(int index);
	void print(char* filename, char* data, int id);

};