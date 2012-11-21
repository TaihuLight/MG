#include "QueueJobs.h"
#ifndef STDIO_INCLUDED
#define STDIO_INCLUDED
#include <stdio.h>
#endif
#ifndef STDLIB_INCLUDED
#define STDLIB_INCLUDED
#include <stdlib.h>
#endif
#ifndef PTHREAD_INCLUDED
#define PTHREAD_INCLUDED
#include <pthread.h>
#endif
#include <unistd.h>
static void check_err(cl_int);
static void check_err2(cl_int);

//Constructor and Destructor


void CreateQueues(int MaxElements, cl_context context, cl_command_queue command_queue) {
  Queue Q = (Queue) malloc (sizeof(struct QueueRecord));
  Q->Capacity = MaxElements;
  Q->Front = 1;
  Q->Rear = 0;
  Q->ReadLock = 0;
  
  cl_int ERR;
  d_newJobs = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(struct QueueRecord), 
                             NULL, &ERR);
  check_err(ERR);
  d_newJobs_array = clCreateBuffer(context, CL_MEM_READ_ONLY, 
                                   sizeof(struct JobDescription) * MaxElements, 
                                   NULL, &ERR);
  check_err(ERR);
  d_finishedJobs = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(struct QueueRecord), 
                                  NULL, &ERR);
  check_err(ERR);
  d_finishedJobs_array = clCreateBuffer(context, CL_MEM_WRITE_ONLY, 
                                        sizeof(struct JobDescription) * MaxElements, 
                                        NULL, &ERR);
  check_err(ERR);

  
  clEnqueueWriteBuffer(command_queue, d_newJobs, CL_TRUE, 0, sizeof(Q), &Q, 0, NULL, NULL);
  clEnqueueWriteBuffer(command_queue, d_finishedJobs, CL_TRUE, 0, sizeof(Q), &Q, 0, NULL, NULL);
  free(Q);

}

static void check_err(cl_int err)
{
  if(err != CL_SUCCESS)
  {
    printf("#Error relating to buffer create or desotry!!\n");
    printf("#PLEASE ASK THE CODE WRITTER# if you see this.\n");
    switch(err)
    {
      case CL_INVALID_MEM_OBJECT:
        printf("Memroy Object Invalid\n");
      case CL_OUT_OF_RESOURCES:
        printf("failure to allocate OpenCL resources on the device\n");
      case CL_OUT_OF_HOST_MEMORY:
        printf("Out Of Host Memory\n");
      default:
        printf("Impossible Error.\n");
    }
    
  }
}

static void check_err2(cl_int err)
{
  if(err != CL_SUCCESS)
  {
    printf("#Error relating to buffer read or write!!\n");
    printf("#PLEASE ASK THE CODE WRITTER# if you see this.\n");
    switch(err)
    {
      case CL_INVALID_MEM_OBJECT:
        printf("Memroy Object Invalid\n");
      case CL_OUT_OF_RESOURCES:
        printf("failure to allocate OpenCL resources on the device\n");
      case CL_OUT_OF_HOST_MEMORY:
        printf("Out Of Host Memory\n");
      default:
        printf("Impossible Error.\n");
    }
    
    
  }//else printf("SUCCESS");
}


void DisposeQueues() {
   cl_int ERR;
   ERR = clReleaseMemObject(d_newJobs);
   check_err(ERR);
   ERR = clReleaseMemObject(d_finishedJobs);
   check_err(ERR);
   ERR = clReleaseMemObject(d_newJobs_array);
   check_err(ERR);
   ERR = clReleaseMemObject(d_finishedJobs_array);
   check_err(ERR);
   
}


void EnqueueJob(JobDescription * h_jobDescription, cl_command_queue command_queue)
{
  
  Queue h_Q = (Queue) malloc(sizeof(struct QueueRecord));
  if(h_Q == NULL)
  printf("ERROR allocating memory in EnqueueJob!\n");
  cl_int ERR1;
  ERR1 = clEnqueueReadBuffer(command_queue, d_newJobs, CL_TRUE, 0, sizeof(h_Q), &h_Q, 0, NULL, NULL);
  check_err2(ERR1);
  while(h_IsFull(h_Q))
  {
    pthread_yield();
    ERR1 = clEnqueueReadBuffer(command_queue, d_newJobs, CL_TRUE, 0, 
                        sizeof(h_Q), &h_Q, 0, NULL, NULL);
    check_err2(ERR1);
  }
  
  h_Q->Rear = (h_Q->Rear+1)%(h_Q->Capacity);
  ERR1 = clEnqueueWriteBuffer(command_queue, d_newJobs, CL_TRUE, sizeof(JobDescription) * h_Q->Rear, sizeof(JobDescription), h_jobDescription, 0, NULL, NULL);
  check_err2(ERR1);
  
  ERR1 = clEnqueueWriteBuffer(command_queue, d_newJobs, CL_TRUE, 4+sizeof(JobDescription*), 
                       sizeof(int), &h_Q->Rear, 0, NULL, NULL);
  check_err2(ERR1);
    /*
    printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
    printf("@@@@@@@@       DEBUG    @@@@@@@@@\n");
    printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
    sleep(1);
    printf("\n%d\n", h_Q->Rear);                     
    free(h_Q);
    */
}

JobDescription * FrontAndDequeueResult(cl_command_queue command_queue)
{
  Queue h_Q = (Queue) malloc(sizeof(struct QueueRecord));
  clEnqueueReadBuffer(command_queue, d_finishedJobs, CL_TRUE, 0, sizeof(h_Q), 
                      &h_Q, 0, NULL, NULL);
    printf("HERE IS RESULT: %d\n", h_Q->Rear);
  
  while(h_IsEmpty(h_Q)){
    pthread_yield();
    clEnqueueReadBuffer(command_queue, d_finishedJobs, CL_TRUE, 0, sizeof(h_Q), 
                        &h_Q, 0, NULL, NULL);
  }
  
  
  JobDescription *result = (JobDescription *) malloc(sizeof(JobDescription));
  clEnqueueReadBuffer(command_queue, d_finishedJobs, CL_TRUE, 
                      sizeof(JobDescription) * h_Q->Front, 
                      sizeof(JobDescription), result, 0, NULL, NULL);
  
  h_Q->Front = (h_Q->Front+1)%(h_Q->Capacity);
  clEnqueueWriteBuffer(command_queue, d_finishedJobs, CL_TRUE, 8+sizeof(JobDescription*), 
                       sizeof(int), &h_Q->Rear, 0, NULL, NULL);

  free(h_Q);
  return result;
  
}




//Helper Functions

int h_IsEmpty(Queue Q) {
  return (Q->Rear+1)%Q->Capacity == Q->Front;
}

int h_IsFull(Queue Q) {
  return (Q->Rear+2)%Q->Capacity == Q->Front;
}

void OpenCLSafeMemcpy(int mode, cl_mem device_mem, void *ptr, int size, int offset, cl_command_queue a_command_queue, pthread_mutex_t memcpyLock)
{ //READ=mode(0)
  //WRITE=mode(1)
  pthread_mutex_lock(&memcpyLock);
  
  if(mode == 0)
  {
    clEnqueueReadBuffer(a_command_queue, device_mem, CL_TRUE, offset, size, ptr, 0, NULL, NULL);
  }
  else if(mode == 1)
  {
    clEnqueueWriteBuffer(a_command_queue, device_mem, CL_TRUE, offset, size, ptr, 0, NULL, NULL);
  }
  else printf("Check code calling safe memeory copy!");
  
  pthread_mutex_unlock(&memcpyLock);

}

/* error handler

void printAnyErrors()
{
  cudaError e = cudaGetLastError();
  if(e!=cudaSuccess){
    printf("CUDA Error: %s\n", cudaGetErrorString( e ) );
  }
}

*/
