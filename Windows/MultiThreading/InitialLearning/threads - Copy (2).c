#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>

#define FILE_NAME_LENGTH 100
#define DIRECTORY_NAME_LENGTH 100
#define FILE_BUFFER_SIZE 1024
#define TOTAL_FILES 1000
#define DEBUG 0

typedef struct {
    WCHAR filename[FILE_NAME_LENGTH];
    WCHAR directory[DIRECTORY_NAME_LENGTH];
    char* fileData;
} ThreadParams;


// thread prototypes
DWORD WINAPI multiple_file_searching_thread(LPVOID param);
DWORD WINAPI multiple_file_printing_thread(LPVOID param);

// helper function prototypes
char* read_file(FILE* file);
void search_multiple_files(const WCHAR* directory, const WCHAR* filename);

// global vars
char fileNamesToPrint[TOTAL_FILES][MAX_PATH] = {0};
char* fileBuffersToPrint[TOTAL_FILES] = {0};

int fileCount = 0;
int doneSearching = 0;


/**
* Runs program to look for first n files that contain the given string 
* at the start of their file name.
*/
int main() {
    WCHAR* directory = L"C:\\Users\\LakshCPlusPLus\\OneDrive - UC Irvine\\Desktop\\CompSci132";
    WCHAR* prefix = L"test";
    ThreadParams* params = (ThreadParams*)malloc(sizeof(ThreadParams));
    DWORD search_thread_id, print_thread_id;

    wcscpy(params->directory, directory);
    wcscpy(params->filename, prefix);

    HANDLE search_thread = CreateThread(NULL, 0, multiple_file_searching_thread, params, 0, &search_thread_id);
    HANDLE print_thread = CreateThread(NULL, 0, multiple_file_printing_thread, NULL, 0, &print_thread_id);

    WaitForSingleObject(search_thread, INFINITE);
    WaitForSingleObject(print_thread, INFINITE);

    CloseHandle(search_thread);
    CloseHandle(print_thread);
    free(params);

    return 0;
}

/**
* This thread start function uses search_multiple_files helper function to search for the
* first n files that match up to TOTAL_FILES.
*/
DWORD WINAPI multiple_file_searching_thread(LPVOID param) {
    ThreadParams* params = (ThreadParams*)param;
    search_multiple_files(params->directory, params->filename);
    return 0;
}

/**
* 
*/
DWORD WINAPI multiple_file_printing_thread(LPVOID param) {
    while (1) {
        int found = 0; // 0 is false, 1 is true

        for (int i = 0; i < TOTAL_FILES; i++) {
            if (fileBuffersToPrint[i] != NULL) {
                printf("\n---------------------------------------------------------------------------\n");
                printf("%s:\n", fileNamesToPrint[i]);
                printf("%s\n", fileBuffersToPrint[i]);
                printf("\n---------------------------------------------------------------------------\n");
                free(fileBuffersToPrint[i]);
                fileBuffersToPrint[i] = NULL;
                found = 1;
            }
        }

        if (found == 0) {
            //Sleep(250); // Sleep for 1 second if no data is found
        }
        else if (doneSearching == 1) {
            for (int i = 0; i < TOTAL_FILES; i++) {
                if (fileBuffersToPrint[i] != NULL) {
                    printf("%s\n", fileBuffersToPrint[i]);
                    printf("\n---------------------------------------------------------------------------\n");
                    free(fileBuffersToPrint[i]);
                    fileBuffersToPrint[i] = NULL;
                    found = 1;
                }
            }
            break;
        }
    }

    return 0;
}

/**
* 
*/
void search_multiple_files(const WCHAR* directory, const WCHAR* prefix) {
    WIN32_FIND_DATA findFileData;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    WCHAR dirSpec[MAX_PATH];

    swprintf(dirSpec, MAX_PATH, L"%s\\*", directory);
    hFind = FindFirstFile(dirSpec, &findFileData);

    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (wcscmp(findFileData.cFileName, L".") == 0 || wcscmp(findFileData.cFileName, L"..") == 0) {
                continue;
            }
            WCHAR path[MAX_PATH];
            swprintf(path, MAX_PATH, L"%s\\%s", directory, findFileData.cFileName);
            search_multiple_files(path, prefix);
        }
        else {
            if (wcsncmp(findFileData.cFileName, prefix, wcslen(prefix)) == 0) {
                if (fileCount < TOTAL_FILES) {
                    if (DEBUG) {
                        printf("Found matching file: %ls\n", findFileData.cFileName);
                    }
                    
                    WCHAR fullPath[MAX_PATH];
                    swprintf(fullPath, MAX_PATH, L"%s\\%s", directory, findFileData.cFileName);
                    char fullPathChar[MAX_PATH];
                    WideCharToMultiByte(CP_ACP, 0, fullPath, -1, fullPathChar, MAX_PATH, NULL, NULL);

                    FILE* file = fopen(fullPathChar, "rb");
                    if (file != NULL) {
                        char fileNameChar[MAX_PATH];
                        WideCharToMultiByte(CP_ACP, 0, findFileData.cFileName, -1, fileNameChar, MAX_PATH, NULL, NULL);
                        strcpy(fileNamesToPrint[fileCount], fileNameChar);
                        fileBuffersToPrint[fileCount] = read_file(file);
                        fileCount++;
                    }
                }
                else {
                    FindClose(hFind);
                    doneSearching = 1;
                    return;
                }
            }
        }
    } while (FindNextFile(hFind, &findFileData) != 0);
    FindClose(hFind);
    doneSearching = 1;
    return;
}

/**
* 
*/
char* read_file(FILE* file) {
    char* buffer;
    int file_size;
    size_t result;

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    rewind(file);

    buffer = (char*)malloc(sizeof(char) * (file_size + 1)); // Add +1 for null-terminator
    if (buffer == NULL) {
        perror("Memory error");
        fclose(file);
        return NULL;
    }

    result = fread(buffer, 1, file_size, file);
     
    if (result != file_size) {
        perror("Reading error");
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[file_size] = '\0'; // Null-terminate the buffer
    fclose(file);
    return buffer;
}