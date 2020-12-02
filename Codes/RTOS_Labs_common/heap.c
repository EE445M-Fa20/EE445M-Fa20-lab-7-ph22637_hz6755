// filename *************************heap.c ************************
// Implements memory heap for dynamic memory allocation.
// Follows standard malloc/calloc/realloc/free interface
// for allocating/unallocating memory.

// Jacob Egner 2008-07-31
// modified 8/31/08 Jonathan Valvano for style
// modified 12/16/11 Jonathan Valvano for 32-bit machine
// modified August 10, 2014 for C99 syntax

/* This example accompanies the book
   "Embedded Systems: Real Time Operating Systems for ARM Cortex M Microcontrollers",
   ISBN: 978-1466468863, Jonathan Valvano, copyright (c) 2015

 Copyright 2015 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains

 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */


#include <stdint.h>
#include <string.h>
#include "../RTOS_Labs_common/heap.h"

#define MAX_HEAP_SIZE 66


struct Heap_Dir{
	unsigned char Name[8];
	uint32_t Start;
};
int32_t Heap[MAX_HEAP_SIZE];
struct Heap_Dir Heap_Dir[MAX_HEAP_SIZE];

//******** Heap_Init *************** 
// Initialize the Heap
// input: none
// output: always 0
// notes: Initializes/resets the heap to a clean state where no memory
//  is allocated.
int32_t Heap_Init(void){
	
	for(int i=0;i<MAX_HEAP_SIZE;i++){
		Heap[i]=0;												//Init Heap
		Heap_Dir[i].Start = 0;						//Init Heap_Dir
		for (int j=0;j<8;j++){
			Heap_Dir[i].Name[j] = NULL;
		}
	}
	Heap[0] = -MAX_HEAP_SIZE+2;
	Heap[MAX_HEAP_SIZE-1] = -MAX_HEAP_SIZE+2;
  return 0;   // replace
}

//Used by Heap_Malloc and Heap_Calloc
int Judge_Free_Space (int32_t desiredBytes,int32_t emptyBytes){
	if (desiredBytes<=emptyBytes)
		return 1;
	else
		return 0;
}
//******** Heap_Malloc *************** 
// Allocate memory, data not initialized
// input: 
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory or will return NULL
//   if there isn't sufficient space to satisfy allocation request
void* Heap_Malloc(int32_t desiredBytes){
	if (desiredBytes%4 == 0)
		desiredBytes = desiredBytes/4;
	else
		desiredBytes = desiredBytes/4+1;
	
	for (int32_t i=0;i<MAX_HEAP_SIZE;i++){
		if ( Judge_Free_Space(desiredBytes,-Heap[i]) ){
			
			Heap[i+2+desiredBytes] = Heap[i]+desiredBytes+2;
			Heap[i+1-Heap[i]] = Heap[i]+desiredBytes+2;
			Heap[i] = desiredBytes;
			Heap[i+1+desiredBytes] = desiredBytes;
			
			return &Heap[i+1];
		}
		else
			if (Heap[i]>0)
				i=i+Heap[i]+1;
			else
				i=i-Heap[i]+1;
	}
  return NULL;				//if not found, return NULL
}


//******** Heap_Calloc *************** 
// Allocate memory, data are initialized to 0
// input:
//   desiredBytes: desired number of bytes to allocate
// output: void* pointing to the allocated memory block or will return NULL
//   if there isn't sufficient space to satisfy allocation request
//notes: the allocated memory block will be zeroed out
void* Heap_Calloc(int32_t desiredBytes){
	if (desiredBytes%4 == 0)
		desiredBytes = desiredBytes/4;
	else
		desiredBytes = desiredBytes/4+1;
	
	for (int i=0;i<MAX_HEAP_SIZE;i++){
		if ( Judge_Free_Space(desiredBytes,-Heap[i]) ){
			
			Heap[i+2+desiredBytes] = Heap[i]+desiredBytes+2;
			Heap[i+1-Heap[i]] = Heap[i]+desiredBytes+2;
			Heap[i] = desiredBytes;
			Heap[i+1+desiredBytes] = desiredBytes;
			
			for(int j=1;j<=desiredBytes;j++){			//Zero out the allocated block
				Heap[i+j]=0;
			}
			return &Heap[i+1];
		}
		else
			if (Heap[i]>0)
				i=i+Heap[i]+1;
			else
				i=i-Heap[i]+1;
	}
  return NULL;				//if not found, return NULL
}


//******** Heap_Realloc *************** 
// Reallocate buffer to a new size
//input: 
//  oldBlock: pointer to a block
//  desiredBytes: a desired number of bytes for a new block
// output: void* pointing to the new block or will return NULL
//   if there is any reason the reallocation can't be completed
// notes: the given block may be unallocated and its contents
//   are copied to a new block if growing/shrinking not possible
void* Heap_Realloc(void* oldBlock, int32_t desiredBytes){
	if (desiredBytes%4 == 0)
		desiredBytes = desiredBytes/4;
	else
		desiredBytes = desiredBytes/4+1;
	
	int32_t temp;
	void* temp2 = NULL;
	memcpy(&temp,(int32_t*)oldBlock-1,4);
	temp=-temp;
	
	if (temp>=0)				//Block is not allocated.
		return NULL;
	
	temp2 = Heap_Malloc(desiredBytes);
	if ( temp2!=NULL ){
		if (desiredBytes>=temp)
			memcpy(temp2,oldBlock,(-temp));
		else
			memcpy(temp2,oldBlock,(desiredBytes));
		Heap_Free(oldBlock);
		return temp2;
	}
	else
		return NULL;

}


//******** Heap_Free *************** 
// return a block to the heap
// input: pointer to memory to unallocate
// output: 0 if everything is ok, non-zero in case of error (e.g. invalid pointer
//     or trying to unallocate memory that has already been unallocated
int32_t Heap_Free(void* pointer){
	int32_t temp;
	
	if ((int32_t*)pointer<&Heap[0] || (int32_t*)pointer>&Heap[MAX_HEAP_SIZE])		//invalid pointer
		return 1;
	
	memcpy(&temp,(int32_t*)pointer-1,4);	
	temp=-temp;
	if (temp>=0)
		return 2;									//Block is not allocated.
	
	int32_t temp2=0;
	for (int i=0;i<-temp;i++){
		memcpy((int32_t*)pointer+i,&temp2,4);
	}

//	memcpy((int32_t*)pointer-1,&temp,4);			//do not merge
//	memcpy((int32_t*)pointer-temp,&temp,4);
	
	memcpy(&temp2,(int32_t*)pointer-temp+1,4);		//lower block allocation number
	temp2=-temp2;
	
	if (temp2>0){								//can be merged downwards 
		int32_t temp3=0;
		memcpy((int32_t*)pointer-temp,&temp3,4);
		memcpy((int32_t*)pointer-temp+1,&temp3,4);
		temp3 = -temp+temp2+2;
		temp3 = -temp3;
		memcpy((int32_t*)pointer-temp3,&temp3,4);	//the beginning of new empty heap
		memcpy((int32_t*)pointer-1,&temp3,4);			//the end of new empty heap
		return 0;
	}	
	
	memcpy(&temp2,(int32_t*)pointer-2,4);		//upper block allocation number
	temp2=-temp2;
	
	if (temp2>0){								//can be merged upwards
		int32_t temp3=0;
		memcpy((int32_t*)pointer-1,&temp3,4);
		memcpy((int32_t*)pointer-2,&temp3,4);
		temp3 = -temp+temp2+2;
		temp3 = -temp3;
		memcpy((int32_t*)pointer-3-temp2,&temp3,4);	//the beginning of new empty heap
		memcpy((int32_t*)pointer-temp,&temp3,4);			//the end of new empty heap
		return 0;
	}

		//can not be merged
		memcpy((int32_t*)pointer-1,&temp,4);
		memcpy((int32_t*)pointer-temp,&temp,4);
		return 0;

	return 0;
}


//******** Heap_Stats *************** 
// return the current status of the heap
// input: reference to a heap_stats_t that returns the current usage of the heap
// output: 0 in case of success, non-zeror in case of error (e.g. corrupted heap)
int32_t Heap_Stats(heap_stats_t *stats){
	
	stats->size = MAX_HEAP_SIZE*4;
	stats->free = stats->size;		
	stats->used = 0;
	
	for (int i=0;i<MAX_HEAP_SIZE;i++){
		if (Heap[i]<0){
			stats->used = stats->used + 8;									//8 is overhead of heap management
			stats->free = stats->free - 8;									//8 is overhead of heap management
			i=i-Heap[i]+1;																	//point to next block head
		}
		else{
			stats->used = stats->used + 8 + Heap[i]*4;									//8 is overhead of heap management
			stats->free = stats->free - 8 - Heap[i]*4;									//8 is overhead of heap management
			i=i+Heap[i]+1;																	//point to next block head
		}
	}
	
  return 0;   // replace
}
