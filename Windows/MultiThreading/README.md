# ThreadSafeFunctionalty

### <em>threadSafeDataAPI.h</em>

<p>Gives CData class that has threadsafe functions for writing to the data array and reading from the data array. These functions now use events to wait 
on each other to do work so that neither is wasting its time by checking every time the sleep timer expires. printRemaining uses a Critical Section
to make sure only one thread prints out the remaining files once the allDataWritten flag is set.
</p>

### <em>threadSafeDataAPI.cpp</em>

<p>Defines all the functions in CData.</p>

### <em>fileProcessing.cpp</em>

<p>Uses the threadSafeDataAPi to provide functionality to read data from a given directory and have it printed to the console. Supports multiple
data producers and multiple data consumers.</p>

## **Initial Learning**

### <em>threads.cpp</em>

<p>First attempt at making a thread and having main wait until it is complete to terminate.</p>

### <em>threads-Copy.c</em>

<p>2 threads, one for finding a file, another for printing it. file_processing_thread finds the specified file, stores the value in a buffer on the head and passes it to file_print_thread to print it.added messagebox giving the user an option to exit the search whenever they want. Shows the usefulness of having multiple threads, we can keep main free to listen to user inputs while other threads work. </p>

### <em>threads-Copy(2).c</em>

<p>Added a call to Sleep in the printing thread. This way it isnt always just running even when there is no data to print.</p>

### <em>threads.c</em>

<p>Working version with 2 searching threads and 1 printing thread. Print thread still has a sleep.</p>
