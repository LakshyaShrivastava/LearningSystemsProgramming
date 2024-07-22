#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>

#define FILE_NAME_LENGTH 100
#define DIRECTORY_NAME_LENGTH 100
#define FILE_BUFFER_SIZE 1024
#define TOTAL_FILES 100

typedef struct {
   WCHAR filename[FILE_NAME_LENGTH];
   WCHAR directory[DIRECTORY_NAME_LENGTH];
   char* fileData;
} ThreadParams;

// functions prototypes
DWORD WINAPI file_processing_thread(LPVOID param);
DWORD WINAPI file_printing_thread(LPVOID param);
char* read_file(FILE* file);
FILE* search_file(const WCHAR* directory, const WCHAR* filename);

// global vars
char* fileBuffersToPrint[TOTAL_FILES];

int main() {
   WCHAR* directory = L"C:\\Users\\LakshCPlusPLus\\OneDrive - UC Irvine\\Desktop";
   WCHAR* filename = L"ford-fulkerson.py";
   ThreadParams* params = (ThreadParams*)malloc(sizeof(ThreadParams));
   DWORD thread1Id;

   wcscpy(params->directory, directory);
   wcscpy(params->filename, filename);

   HANDLE hThread1 = CreateThread(NULL, 0, file_processing_thread, params, 0, &thread1Id);
   int result = MessageBox(NULL, L"Press OK to stop the search and exit.", L"File Search", MB_OKCANCEL | MB_ICONINFORMATION);

   if (result == IDOK) {
       return 0;
   }

   WaitForSingleObject(hThread1, INFINITE);
   free(params);
   return 0;
}



FILE* search_file(const WCHAR* directory, const WCHAR* filename) {
   WIN32_FIND_DATA findFileData;
   HANDLE hFind = INVALID_HANDLE_VALUE;
   WCHAR dirSpec[MAX_PATH];

   swprintf(dirSpec, MAX_PATH, L"%s\\*", directory);
   hFind = FindFirstFile(dirSpec, &findFileData);

   if (hFind == INVALID_HANDLE_VALUE) {
       return NULL;
   }

   do {
       if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
           if (wcscmp(findFileData.cFileName, L".") == 0 || wcscmp(findFileData.cFileName, L"..") == 0) {
               continue;
           }

           WCHAR path[MAX_PATH];
           swprintf(path, MAX_PATH, L"%s\\%s", directory, findFileData.cFileName);
           printf("");
           FILE* file = search_file(path, filename);
           if (file != NULL) {
               FindClose(hFind);
               return file;
           }
       }
       else {
           if (wcscmp(findFileData.cFileName, filename) == 0) {
               WCHAR fullPath[MAX_PATH];
               swprintf(fullPath, MAX_PATH, L"%s\\%s", directory, filename);
               char fullPathChar[MAX_PATH];
               WideCharToMultiByte(CP_ACP, 0, fullPath, -1, fullPathChar, MAX_PATH, NULL, NULL);
               FILE* file = fopen(fullPathChar, "rb");
               if (file != NULL) {
                   FindClose(hFind);
                   return file;
               }
               else {
                   wprintf(L"Failed to open file: %s\n", fullPath);
               }
           }
       }
   } while (FindNextFile(hFind, &findFileData) != 0);

   FindClose(hFind);
   return NULL;
}

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

DWORD WINAPI file_processing_thread(LPVOID param) {
   ThreadParams* params = (ThreadParams*)param;
   FILE* fileToRead = search_file(params->directory, params->filename);
   if (fileToRead != NULL) {
       params->fileData = read_file(fileToRead);
       if (params->fileData!= NULL) {
           DWORD threadId;
           HANDLE print_thread = CreateThread(NULL, 0, file_printing_thread, params, 0, &threadId);
           WaitForSingleObject(print_thread, INFINITE);
           CloseHandle(print_thread);
       }
       else {
           printf("Error reading the file\n");
       }
   }
   else {
       printf("Error finding the file\n");
   }

   return 0;
}

DWORD WINAPI file_printing_thread(LPVOID param) {
   ThreadParams* params = (ThreadParams*)param;
   printf("%s\n", params->fileData);
   free(params->fileData);
   return 0;
}