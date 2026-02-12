#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    
    /*
    * Changes done by Likhita Jonnakuti
    * 1) Sleep for wait_obtain_ms
    * 2) Lock mutex
    * 3) Hold Mutex for wait_to_release_ms
    * 4) Unlock mutext
    * 5) Return thread_data pointer 
    */
    
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    thread_func_args->thread_complete_success = false;
    
    //wait vefore attempting to obtain the mutex
    usleep(thread_func_args->wait_to_obtain_ms * 1000);
    
    //Aquire mutex 
    if(pthread_mutex_lock(thread_func_args->mutex)!=0)
    {
    	return thread_func_args;
    }
    
    //Hold the mutex for requested time
    usleep(thread_func_args->wait_to_release_ms * 1000);
    
    //Release the mutex
    if(pthread_mutex_unlock(thread_func_args->mutex)!=0)
    {
    	return thread_func_args;
    }
    
    thread_func_args->thread_complete_success = true;  
      
    return thread_func_args;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
     
     /*
     * Updated by Likhita Jonnakuti
     * update the thread arguments  
     * create thread using pthread_create and the function that runs 
     * is function thread and the thread is create using thread_func_args
     */
     
    struct thread_data* thread_func_args = malloc(sizeof(struct thread_data));
    if(thread_func_args == NULL)
    {
       return false;
    }
    
    thread_func_args->mutex = mutex;
    thread_func_args->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_func_args->wait_to_release_ms = wait_to_release_ms;
    thread_func_args->thread_complete_success = false;
    
    int rc = pthread_create(thread, NULL, threadfunc, thread_func_args);
    if(rc != 0)
    {
    // clear up to avoid memory leak
    	free(thread_func_args);
    	return false;
    }
    
    return true;
    
}

