//// C++ version that I originally made
//
//#include <windows.h>
//#include <iostream>
//
//
//
//class Log {
//	const char* identifier;
//public:
//	Log(const char* identifier) {
//		this->identifier = identifier;
//		printf("%s: started\n---------\n", identifier);
//	}
//
//	void print_local(int& x) {
//		printf("local from %s:\tValue = %d\tAddress = %p\n", identifier, x, &x);
//	}
//
//	~Log()
//	{
//		printf("%s: ended\n---------\n", identifier);
//	}
//};
//
//typedef struct {
//	int id;
//	char message[100];
//} ThreadParams;
////
////DWORD WINAPI thread_start_1(LPVOID param) {
////	ThreadParams* params = (ThreadParams*)param;
////	printf("Thread id is %d, and the message is: %s.\n", params->id, params->message);
////	return 0;
////}
////
////int main() {
////	HANDLE thread;
////	DWORD thread_id;
////	ThreadParams params_to_pass;
////
////	params_to_pass.id = 5;
////	strcpy_s(params_to_pass.message, "Hello, World!");
////	
////	thread = CreateThread(NULL, 0, thread_start_1, &params_to_pass, NULL, &thread_id);
////
////	WaitForSingleObject(thread, INFINITE);
////}
//
//
// //
// //Saw that each time we create a thread, it is treated like a seperate variable, 
// //even when we create it and set its start funtion as the same function. in both instances, 
// //loc1 = 6 but they had different addresses
//
//DWORD WINAPI thread_start_1(LPVOID param) {
//	const char* func_name = "thread_start_1";
//	int loc1 = 6;
//	Log b(func_name);
//	b.print_local(loc1);
//	return 0;
//}
//
//DWORD WINAPI thread_start_2(LPVOID param) {
//	const char* func_name = "thread_start_2";
//	int loc1 = 7;
//	Log b(func_name);
//	b.print_local(loc1);
//	return 0;
//}
//
//
//
//
//int main() {
//	int loc1 = 5;
//	const char* func_name = "main";
//	Log a(func_name);
//	a.print_local(loc1);
//
//	DWORD threadIDs[3];
//	HANDLE threads[3];
//
//	LPVOID v_ptr = new LPVOID;
//	printf("Called from main!\n");
//	thread_start_1(v_ptr);
//	printf("*****************\n");
//	printf("Called from main!\n");
//	thread_start_1(v_ptr);
//	printf("*****************\n");
//	printf("Called from main!\n");
//	thread_start_1(v_ptr);
//	printf("*****************\n");
//	printf("Called from main!\n");
//	thread_start_1(v_ptr);
//	printf("*****************\n");
//
//	threads[0] = CreateThread(NULL, 0, thread_start_1, NULL, 0, &threadIDs[0]);
//	threads[1] = CreateThread(NULL, 0, thread_start_1, NULL, 0, &threadIDs[1]);
//	//threads[2] = CreateThread(NULL, 0, thread_start_2, NULL, 0, &threadIDs[2]);
//
//
//
//
//	WaitForMultipleObjects(2, threads, true, INFINITE);
//}
//
//
//
////Basic Thread
////
////DWORD ThreadStart(LPVOID param) {
////	Log b("Thread");
////	std::cout << "This is a thread doing something.\n";
////	return 0;
////}
////
////int main() {
////	Log a("main");
////	DWORD dwThreadID;
////	HANDLE thread1_handle = CreateThread(NULL, 0, ThreadStart, NULL, 0, &dwThreadID);
////	std::cout << "Thread ID: "<< dwThreadID << ", Handle:" << thread1_handle <<std::endl;
////	WaitForSingleObject(thread1_handle, INFINITE);
////	return 0;
////}