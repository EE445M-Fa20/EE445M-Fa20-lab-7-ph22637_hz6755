// filename ************** eFile.c *****************************
// High-level routines to implement a solid-state disk 
// Students implement these functions in Lab 4
// Jonathan W. Valvano 1/12/20
#include <stdint.h>
#include <string.h>
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"
#include <stdio.h>

long FileCreated;
#define DirectBlockNum 5

uint32_t FAT[128];
unsigned char WBlock;
unsigned char RBlock;
int WDir;
int RDir;
int WNum;
int RNum;
int WFinal;
int RFinal;
unsigned char Wbuffer[512];
unsigned char Rbuffer[512];

struct Direct{
	unsigned char Name[8];
	uint32_t Start;
	uint32_t Active;
};
struct Direct Dir[128];

//---------- eFile_Init-----------------
// Activate the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
int eFile_Init(void){ // initialize file system
	eDisk_Init(0);
  return 0;   // replace
}

//---------- eFile_Format-----------------
// Erase all files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Format(void){ // erase disk, add format
	unsigned char name[8];
	for (int i=0;i<8;i++){
		name[i] = NULL;
	}
	for (int i=0;i<128;i++){
		FAT[i] = i+1;
		memcpy(&Dir[i],name,8);
		Dir[i].Start = 0;
	}
	int k;
	k=sizeof(struct Direct);
	FAT[127] = 0;
	name[0] = '*';
	memcpy(Dir[0].Name,name,8);
	Dir[0].Start = 5;
	
	unsigned char buffer[512];
	memcpy(buffer,&FAT,512);
	eDisk_WriteBlock(buffer,0);
	memcpy(buffer,&Dir[0],512);
	eDisk_WriteBlock(buffer,1);
	memcpy(buffer,&Dir[32],512);
	eDisk_WriteBlock(buffer,2);
	memcpy(buffer,&Dir[64],512);
	eDisk_WriteBlock(buffer,3);
	memcpy(buffer,&Dir[96],512);
	eDisk_WriteBlock(buffer,4);
	
	FileCreated = 0;
  return 0;   // replace
}

//---------- eFile_Mount-----------------
// Mount the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure
int eFile_Mount(void){ // initialize file system
	BYTE buffer[512];
	eDisk_ReadBlock(buffer,0);
	memcpy(&FAT,buffer,512);
	
	eDisk_ReadBlock(buffer,1);
	memcpy(&Dir[0],buffer,512);
	eDisk_ReadBlock(buffer,2);
	memcpy(&Dir[32],buffer,512);
	eDisk_ReadBlock(buffer,3);
	memcpy(&Dir[64],buffer,512);
	eDisk_ReadBlock(buffer,4);
	memcpy(&Dir[96],buffer,512);
	
	int i=0;
	unsigned char Mou[8];
	for (int j=0;j<8;j++){
		Mou[j] = NULL;
	}
	while ( memcmp(&Dir[i+1],Mou,8) !=0 ){
		i=i+1;
	}
	
	FileCreated = i;
	
  return 0;   // replace
}


//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create( const char name[]){  // create new file, make it empty 
	if (FAT[Dir[0].Start]!=0){
		Dir[FileCreated+1].Start = Dir[0].Start;
		Dir[0].Start = FAT[Dir[0].Start];
		FAT[Dir[FileCreated+1].Start] = 0;
		memcpy(&Dir[FileCreated+1],name,8);
	}
	else
		return 1;

	FileCreated++;
	
  return 0;   // replace
}


//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is an ASCII string up to seven characters
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WOpen( const char name[]){      // open a file for writing 
	int i=0;
	
	if (FileCreated == 0)
		return 1;
	
	while (i<FileCreated){
		if ( memcmp (&Dir[i],name,8) )
			break;
		i++;
	}
	if (i>=FileCreated)
		return 1;
	
	int temp;
	unsigned char next;
	next = Dir[i+1].Start;
	temp = next;
	while( FAT[next] != 0){
		temp = next;
		next = FAT[temp];
	}
	
	WFinal = temp;
	WDir = i+1;
  return 0;   // replace  
}

//---------- eFile_Write-----------------
// save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write( const char data){
	if (WNum<512){
		Wbuffer[WNum] = data;
		WNum++;
	}
	else{		
		WBlock = Dir[0].Start;
		FAT[WFinal] = WBlock;
		Dir[0].Start = FAT[Dir[0].Start];
		eDisk_WriteBlock(Wbuffer,WBlock);

		WFinal = WBlock;
		FAT[WFinal] = 0;
		
		WNum = 0;
		unsigned char zero[512];
		memcpy(Wbuffer,zero,512);
	}
    return 0;   // replace
}

//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void){ // close the file for writing
	
	if (WDir==0)
		return 1;
	
  if (WNum!=0){
		WBlock = Dir[0].Start;
		FAT[WFinal] = WBlock;
		Dir[0].Start = FAT[Dir[0].Start];
		
		eDisk_WriteBlock(Wbuffer,WBlock);
		
		WFinal = WBlock;
		FAT[WFinal] = 0;
		
		WNum = 0;
		unsigned char zero[512];
		memcpy(Wbuffer,zero,512);
	}
	
	WNum = 0;
	WFinal = 0;
	WBlock = 0;
	WDir = 0;
	
  return 0;   // replace
}


//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is an ASCII string up to seven characters
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen( const char name[]){      // open a file for reading 
	int i=0;DRESULT Rtest;
	
	if (FileCreated == 0)
		return 1;
	
	while (i<FileCreated){
		if ( memcmp (&Dir[i],name,8) )
			break;
		i++;
	}
	if (i>=FileCreated)
		return 1;
	
	RDir = i;
	RBlock = Dir[i+1].Start;
	Rtest = eDisk_ReadBlock(Rbuffer,RBlock);
	
  return 0;   // replace   
}
 
//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext( char *pt){       // get next byte 
  DRESULT Rtest;
	if (RNum<512){
		pt = &Rbuffer[RNum];
		RNum++;
	}
	else if (FAT[RBlock] == 0){
		return 1;
	}
	else{
		RBlock = FAT[RBlock];
		Rtest = eDisk_ReadBlock(Rbuffer,RBlock);
		RNum = 0;
		pt = &Rbuffer[RNum];
		RNum++;
	}
  return 0;   // replace
}
    
//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void){ // close the file for writing
  
	if(RDir == 0)
		return 1;
	else{
		RBlock = 0;
		RDir = 0;
		RNum = 0;
		unsigned char zero[512];
		memcpy(Rbuffer,zero,512);
	}
	
  return 0;   // replace
}


//---------- eFile_Delete-----------------
// delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete( const char name[]){  // remove this file 
	int i=0;DRESULT Rtest;
	
	if (FileCreated == 0)
		return 1;
	
	while (i<FileCreated){
		if ( memcmp (&Dir[i],name,8) )
			break;
		i++;
	}
	if (i>=FileCreated)
		return 1;
	
	int temp = Dir[0].Start;
	while (FAT[temp] != 0){
		temp = FAT[temp];
	}
	FAT[temp] = 	Dir[i+1].Start;
	
	for (int j=i+1;j<127;j++){
		Dir[j]=Dir[j+1];
	}

	unsigned char buffer[512];
	memcpy(buffer,&FAT,512);
	eDisk_WriteBlock(buffer,0);
	memcpy(buffer,&Dir[0],512);
	eDisk_WriteBlock(buffer,1);
	memcpy(buffer,&Dir[32],512);
	eDisk_WriteBlock(buffer,2);
	memcpy(buffer,&Dir[64],512);
	eDisk_WriteBlock(buffer,3);
	memcpy(buffer,&Dir[96],512);
	eDisk_WriteBlock(buffer,4);
	
  return 0;   // replace
}                             


//---------- eFile_DOpen-----------------
// Open a (sub)directory, read into RAM
// Input: directory name is an ASCII string up to seven characters
//        (empty/NULL for root directory)
// Output: 0 if successful and 1 on failure (e.g., trouble reading from flash)
int eFile_DOpen( const char name[]){ // open directory
   
  return 0;   // replace
}
  
//---------- eFile_DirNext-----------------
// Retreive directory entry from open directory
// Input: none
// Output: return file name and size by reference
//         0 if successful and 1 on failure (e.g., end of directory)
int eFile_DirNext( char *name[], unsigned long *size){  // get next entry 
   
  return 0;   // replace
}

//---------- eFile_DClose-----------------
// Close the directory
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_DClose(void){ // close the directory
   
  return 0;   // replace
}


//---------- eFile_Unmount-----------------
// Unmount and deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently mounted)
int eFile_Unmount(void){ 
   
  return 0;   // replace
}
