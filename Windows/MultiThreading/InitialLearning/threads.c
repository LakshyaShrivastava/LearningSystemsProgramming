//#define _CRT_SECURE_NO_WARNINGS
//
//#include <windows.h>
//#include <stdio.h>
//
//#define FILE_NAME_LENGTH 100
//#define DIRECTORY_NAME_LENGTH 100
//#define FILE_BUFFER_SIZE 1024
//#define TOTAL_FILES 3
//#define DEBUG TRUE
//
//typedef struct {
//    WCHAR filename[FILE_NAME_LENGTH];
//    WCHAR directory[DIRECTORY_NAME_LENGTH];
//} ThreadParams;
//
//
//// thread prototypes
//DWORD WINAPI multiple_file_searching_thread(LPVOID param);
//DWORD WINAPI multiple_file_printing_thread(LPVOID param);
//
//// helper function prototypes
//char* read_file(FILE* file);
//void search_multiple_files(const WCHAR* directory, const WCHAR* filename);
//void printFileData(char* filename, char* data);
//void cleanFileDataArray(int index);
//void waitTillArrayHasFreeSpace(WCHAR* threadOrigin);
//
//// global vars
//char fileNamesToPrint[TOTAL_FILES][MAX_PATH] = {NULL};
//char* fileBuffersToPrint[TOTAL_FILES] = {NULL};
//int writeIndex = 0;
//int readIndex = 0;
//boolean searchFinished = FALSE;
//
///**
//* Runs program to look for first n files that contain the given string 
//* at the start of their file name.
//*/
//int main() {
//    WCHAR* directory_1 = L"C:\\Users\\LakshCPlusPLus\\OneDrive - UC Irvine\\Desktop\\CompSci132";
//    WCHAR* directory_2 = L"C:\\Users\\LakshCPlusPLus\\OneDrive - UC Irvine\\Desktop\\CS112";
//    WCHAR* prefix = L"test";
//    ThreadParams* params_1 = (ThreadParams*)malloc(sizeof(ThreadParams));
//    ThreadParams* params_2 = (ThreadParams*)malloc(sizeof(ThreadParams));
//    DWORD search_thread_id_1, search_thread_id_2, print_thread_id;
//
//    wcscpy(params_1->directory, directory_1);
//    wcscpy(params_1->filename, prefix);
//    wcscpy(params_2->directory, directory_2);
//    wcscpy(params_2->filename, prefix);
//
//    HANDLE search_thread_1 = CreateThread(NULL, 0, multiple_file_searching_thread, params_1, 0, &search_thread_id_1);
//    HANDLE search_thread_2 = CreateThread(NULL, 0, multiple_file_searching_thread, params_2, 0, &search_thread_id_2);
//    HANDLE print_thread = CreateThread(NULL, 0, multiple_file_printing_thread, NULL, 0, &print_thread_id);
//
//    WaitForSingleObject(search_thread_1, INFINITE);
//    WaitForSingleObject(search_thread_2, INFINITE);
//    WaitForSingleObject(print_thread, INFINITE);
//
//    CloseHandle(search_thread_1);
//    CloseHandle(search_thread_2);
//    CloseHandle(print_thread);
//    free(params_1);
//    free(params_2);
//
//    return 0;
//}
//
///**
//* This thread start function uses search_multiple_files helper function to search for the
//* first n files that match up to TOTAL_FILES.
//*/
//DWORD WINAPI multiple_file_searching_thread(LPVOID param) {
//    ThreadParams* params = (ThreadParams*)param;
//    CRITICAL_SECTION cs;
//    InitializeCriticalSection(&cs);
//    search_multiple_files(params->directory, params->filename, &cs);
//    return 0;
//}
//
///**
//* 
//*/
//DWORD WINAPI multiple_file_printing_thread(LPVOID param) {
//    while (TRUE) {
//        if (DEBUG) {
//            printf("READ: current readIndex: %d\n", readIndex);
//            printf("READ: next readIndex: %d\n", (readIndex + 1) % TOTAL_FILES);
//        }
//        if (fileBuffersToPrint[readIndex] != NULL) {
//            if (DEBUG) {
//                printf("READ: entered reading condition\n");
//            }
//            printFileData(fileNamesToPrint[readIndex], fileBuffersToPrint[readIndex]);
//            fileBuffersToPrint[readIndex] = NULL;
//            readIndex = (readIndex + 1) % TOTAL_FILES;
//        }
//        else if (searchFinished) {
//            if (DEBUG) {
//                printf("READ: entered exit condition\n");
//            }
//            break;
//        }
//        else {
//            if (DEBUG) {
//                printf("READ: entered sleeping condition\n");
//            }
//            Sleep(2000); // Sleep if no data is available
//        }
//    }
//
//    return 0;
//}
//
///**
//* 
//*/
//void search_multiple_files(const WCHAR* directory, const WCHAR* prefix, LPCRITICAL_SECTION cs) {
//    WIN32_FIND_DATA findFileData;
//    HANDLE hFind = INVALID_HANDLE_VALUE;
//    WCHAR dirSpec[MAX_PATH];
//
//    swprintf(dirSpec, MAX_PATH, L"%s\\*", directory);
//    hFind = FindFirstFile(dirSpec, &findFileData);
//
//    if (hFind == INVALID_HANDLE_VALUE) {
//        return;
//    }
//
//    do {
//        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
//            if (wcscmp(findFileData.cFileName, L".") == 0 || wcscmp(findFileData.cFileName, L"..") == 0) {
//                continue;
//            }
//            WCHAR path[MAX_PATH];
//            swprintf(path, MAX_PATH, L"%s\\%s", directory, findFileData.cFileName);
//            search_multiple_files(path, prefix, cs);
//        }
//        else {
//            if (wcsncmp(findFileData.cFileName, prefix, wcslen(prefix)) == 0) {
//                
//                WCHAR fullPath[MAX_PATH];
//                swprintf(fullPath, MAX_PATH, L"%s\\%s", directory, findFileData.cFileName);
//                char fullPathChar[MAX_PATH];
//                WideCharToMultiByte(CP_ACP, 0, fullPath, -1, fullPathChar, MAX_PATH, NULL, NULL);
//               
//                if (DEBUG) {
//                    //printf("WRITE %ls: Found matching file: %ls\n", directory, findFileData.cFileName);
//                }
//
//                FILE* file = fopen(fullPathChar, "rb");
//                if (file != NULL) {
//                    char* temp = read_file(file);
//                    EnterCriticalSection(cs);
//                    waitTillArrayHasFreeSpace(directory);
//                    if (DEBUG) {
//                        printf("WRITE %ls: Going to write %ls at index %d\n", directory, findFileData.cFileName, writeIndex);
//                    }
//                    fileBuffersToPrint[writeIndex] = temp;
//                    strcpy(fileNamesToPrint[writeIndex], fullPathChar);
//                    if (DEBUG) {
//                        printf("WRITE %ls: Wrote %ls at index %d\n", directory, findFileData.cFileName, writeIndex);
//                    }
//                    writeIndex++;
//                    LeaveCriticalSection(cs);
//                }
//            }
//        }
//    } while (FindNextFile(hFind, &findFileData) != 0);
//    FindClose(hFind);
//    ///searchFinished = TRUE;
//    return;
//}
//
//void waitTillArrayHasFreeSpace(WCHAR* threadOrigin) {
//    if (writeIndex >= TOTAL_FILES) {
//        writeIndex = 0;
//    }
//    int original = writeIndex;
//    while (1) {
//        if (fileBuffersToPrint[writeIndex] != NULL) {
//            for (int i = 0; i < TOTAL_FILES; i++) {
//                if (fileBuffersToPrint[i] == NULL) {
//                    writeIndex = i;
//                    /*if (DEBUG) {
//                        printf("WRITE %ls: WriteIndex was %d, now it is %d\n", threadOrigin, original, writeIndex);
//                    }*/
//                    break;
//                }
//            }
//            if (original == writeIndex) {
//                if (DEBUG) {
//                    printf("WRITE %ls: No free spots, going to sleep.\n", threadOrigin);
//                }
//                Sleep(2000);
//            }
//        }
//        else {
//            if (DEBUG) {
//               // printf("WRITE: Exiting Wait Loop.\n");
//            }
//            break;
//        }
//    }
//}
//
///**
//* 
//*/
//char* read_file(FILE* file) {
//    char* buffer;
//    int file_size;
//    size_t result;
//
//    fseek(file, 0, SEEK_END);
//    file_size = ftell(file);
//    rewind(file);
//
//    buffer = (char*)malloc(sizeof(char) * (file_size + 1)); // Add +1 for null-terminator
//    if (buffer == NULL) {
//        perror("Memory error");
//        fclose(file);
//        return NULL;
//    }
//
//    result = fread(buffer, 1, file_size, file);
//     
//    if (result != file_size) {
//        perror("Reading error");
//        free(buffer);
//        fclose(file);
//        return NULL;
//    }
//
//    buffer[file_size] = '\0'; // Null-terminate the buffer
//    fclose(file);
//    return buffer;
//}
//
//void printFileData(char* filename, char* data) {
//    printf("\n---------------------------------------------------------------------------\n");
//    printf("%s:\n", filename);
//    printf("%s\n", data);
//    printf("\n---------------------------------------------------------------------------\n");
//}
//
//void cleanFileDataArray(int index) {
//    free(fileBuffersToPrint[index]);
//    fileBuffersToPrint[index] = NULL;
//}
//
