//// filename ************** eFile.c *****************************
//// High-level routines to implement a solid-state disk 
//// Wrapper around FAT filesystem for Lab 5
//// Andreas Gerstlauer 2/27/20
//#include <stdint.h>
//#include <string.h>
//#include "../RTOS_Labs_common/OS.h"
//#include "../RTOS_Labs_common/eFile.h"
//#include "ff.h"
//#include <stdio.h>


//// Display semaphore
//extern Sema4Type LCDFree;  // this should really be handled in eDisk.c, not eFile.c

//// Static file system objects
//static FATFS g_sFatFs;
//static DIR d; 
//static FIL f; 
//static FILINFO fi;


////---------- eFile_Init-----------------
//// Activate the file system, without formating
//// Input: none
//// Output: 0 if successful and 1 on failure (already initialized)
//int eFile_Init(void){ // initialize file system
//  // do nothing for FAT
//  return 0;
//}

////---------- eFile_Format-----------------
//// Erase all files, create blank directory, initialize free space manager
//// Input: none
//// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
//int eFile_Format(void){ // erase disk, add format
//	OS_bWait(&LCDFree);
//  if(f_mkfs("", 0, 0)){
//		OS_bSignal(&LCDFree);
//    return 1;
//  }
//	OS_bSignal(&LCDFree);  
//  return 0;
//}

////---------- eFile_Mount-----------------
//// Mount the file system and load metadata
//// Input: none
//// Output: 0 if successful and 1 on failure (already initialized)
//int eFile_Mount(void){ // mount disk
//	OS_bWait(&LCDFree);
//  if(f_mount(&g_sFatFs, "", 0)){
//		OS_bSignal(&LCDFree);
//    return 1;
//  }
//	OS_bSignal(&LCDFree);  
//  return 0;
//}

////---------- eFile_Create-----------------
//// Create a new, empty file with one allocated block
//// Input: file name is an ASCII string up to seven characters 
//// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
//int eFile_Create( const char name[]){  // create new file, make it empty 
//	OS_bWait(&LCDFree);
//  if(f_open(&f, name, FA_CREATE_NEW)){
//		OS_bSignal(&LCDFree);
//    return 1;
//  }
//  OS_bSignal(&LCDFree);
//  return 0;   
//}

////---------- eFile_WOpen-----------------
//// Open the file, read into RAM last block
//// Input: file name is an ASCII string up to seven characters
//// Output: 0 if successful and 1 on failure (e.g., trouble reading from flash)
//int eFile_WOpen( const char name[]){      // open a file for writing 
//	OS_bWait(&LCDFree);
//  if(f_open(&f, name, FA_WRITE)){
//		OS_bSignal(&LCDFree);
//    return 1;
//  }
//  OS_bSignal(&LCDFree);
//  return 0;   
//}

////---------- eFile_Write-----------------
//// Save at end of the open file
//// Input: data to be saved
//// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
//int eFile_Write( char data){
//  unsigned written;
//  OS_bWait(&LCDFree);
//  if(f_write(&f, &data, 1, &written) || (written != 1)){
//    OS_bSignal(&LCDFree);
//    return 1;
//  }
//  OS_bSignal(&LCDFree);
//  return 0;  
//}

////---------- eFile_WClose-----------------
//// Close the file, left disk in a state power can be removed
//// Input: none
//// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
//int eFile_WClose(void){ // close the file for writing
//	OS_bWait(&LCDFree);
//  if(f_close(&f)){
//		OS_bSignal(&LCDFree);
//    return 1;
//  }
//  OS_bSignal(&LCDFree);
//  return 0;  
//}

////---------- eFile_ROpen-----------------
//// Open the file, read first block into RAM 
//// Input: file name is an ASCII string up to seven characters
//// Output: 0 if successful and 1 on failure (e.g., trouble reading from flash)
//int eFile_ROpen( const char name[]){      // open a file for reading 
//	OS_bWait(&LCDFree);
//  if(f_open(&f, name, FA_READ)){
//		OS_bSignal(&LCDFree);
//    return 1;
//  }
//  OS_bSignal(&LCDFree);
//  return 0;   
//}
// 
////---------- eFile_ReadNext-----------------
//// Retreive data from open file
//// Input: none
//// Output: return by reference data
////         0 if successful and 1 on failure (e.g., end of file)
//int eFile_ReadNext( char *pt){       // get next byte 
//  unsigned read;
//  OS_bWait(&LCDFree);
//  if(f_read(&f, pt, 1, &read) || (read != 1)){
//    OS_bSignal(&LCDFree);
//    return 1;
//  }
//  OS_bSignal(&LCDFree);
//  return 0; 
//}

////---------- eFile_RClose-----------------
//// Close the reading file
//// Input: none
//// Output: 0 if successful and 1 on failure (e.g., wasn't open)
//int eFile_RClose(void){ // close the file for writing
//	OS_bWait(&LCDFree);
//  if(f_close(&f)){
//		OS_bSignal(&LCDFree);
//    return 1;
//  }
//  OS_bSignal(&LCDFree);
//  return 0;
//}

////---------- eFile_Delete-----------------
//// delete this file
//// Input: file name is a single ASCII letter
//// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
//int eFile_Delete( const char name[]){  // remove this file 
//	OS_bWait(&LCDFree);
//  if(f_unlink(name)){
//    OS_bSignal(&LCDFree);
//    return 1;
//  }
//  OS_bSignal(&LCDFree);
//  return 0;
//}                             

////---------- eFile_DOpen-----------------
//// Open a (sub)directory, read into RAM
//// Input: directory name is an ASCII string up to seven characters
////        (empty/NULL for root directory)
//// Output: 0 if successful and 1 on failure (e.g., trouble reading from flash)
//int eFile_DOpen( const char name[]){ // open directory
//	OS_bWait(&LCDFree);
//  if(f_opendir(&d, name)) {
//		OS_bSignal(&LCDFree);
//		return 1;
//  }
//	OS_bSignal(&LCDFree);
//  return 0;
//}
//  
////---------- eFile_DirNext-----------------
//// Retreive directory entry from open directory
//// Input: none
//// Output: return file name and size by reference
////         0 if successful and 1 on failure (e.g., end of file)
//int eFile_DirNext( char *name[], unsigned long *size){  // get next entry 
//	OS_bWait(&LCDFree);
//	if(f_readdir(&d, &fi) || !fi.fname[0]) {
//		OS_bSignal(&LCDFree);
//		return 1;
//  }
//  *name = fi.fname;
//  *size = fi.fsize;
//	OS_bSignal(&LCDFree);
//  return 0;
//}

////---------- eFile_DClose-----------------
//// Close the directory
//// Input: none
//// Output: 0 if successful and 1 on failure (e.g., wasn't open)
//int eFile_DClose(void){ // close the directory
//	OS_bWait(&LCDFree);
//  if(f_closedir(&d)){
//		OS_bSignal(&LCDFree);
//    return 1;
//  }
//  OS_bSignal(&LCDFree);
//  return 0;
//}

////---------- eFile_Unmount-----------------
//// Unmount and deactivate the file system
//// Input: none
//// Output: 0 if successful and 1 on failure (not currently mounted)
//int eFile_Unmount(void){ 
//	OS_bWait(&LCDFree);
//  if(f_mount(NULL, "", 0)){
//		OS_bSignal(&LCDFree);
//    return 1;
//  }
//	OS_bSignal(&LCDFree);  
//  return 0;   
//}



// filename ************** eFile.c *****************************
// High-level routines to implement a solid-state disk 
// Students implement these functions in Lab 4
// Jonathan W. Valvano 1/12/20
#include <stdint.h>
#include <string.h>
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../RTOS_Labs_common/UART0int.h"
#include <stdio.h>

long FileCreated;
#define DirectBlockNum 5

int MAX_FILE = 128;

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
	uint32_t size;
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
void PrintDir(void){
  int i = 1;
	int k;
	UART_OutString("\n\r");
	for(i=1;i<128;i++){
	  if(Dir[i].Name[0] == NULL) break;
		for(k=0;k<8;k++){
			if(Dir[i].Name[k] == NULL) break;
		  UART_OutChar(Dir[i].Name[k]);
		}
		UART_OutString("\n\r");
	}
}
//---------- eFile_Mount-----------------
// Mount the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure
int eFile_Mount(void){ // initialize file system
	unsigned char buffer[512];
	eDisk_ReadBlock(buffer,0);
	memcpy(&FAT,buffer,512);
	
	eDisk_ReadBlock(buffer,1);
	for(int i = 0; i < 32; i++){
		memcpy(&Dir[i],buffer + i * 16, 16);
	}
	//memcpy(&Dir[0],buffer,512);
	eDisk_ReadBlock(buffer,2);
	for(int i = 0; i < 32; i++){
		memcpy(&Dir[i + 32],buffer + i * 16, 16);
	}
	
	eDisk_ReadBlock(buffer,3);
	for(int i = 0; i < 32; i++){
		memcpy(&Dir[i + 64],buffer + i * 16, 16);
	}
		
	eDisk_ReadBlock(buffer,4);
	for(int i = 0; i < 32; i++){
		memcpy(&Dir[i + 96],buffer + i * 16, 16);
	}
	
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
	// if extra space for block exists
	if (FAT[Dir[0].Start]!=0){
		// init the dir(file)
		memcpy(&Dir[FileCreated+1],name,8);
//		Dir[FileCreated+1].Start = Dir[0].Start;
		Dir[FileCreated+1].size = 0;
		Dir[FileCreated+1].Start = 0;
		
		// relink the linkedlist 
//		Dir[0].Start = FAT[Dir[0].Start];
//		FAT[Dir[FileCreated+1].Start] = 0;
	}
	else
		return 1;

	FileCreated++;
	eFile_Unmount();
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
	
	// find file with given name
	while (i < MAX_FILE){
		if ( strcmp ((char*)&Dir[i].Name,name) == 0)
			break;
		i++;
	}
	if (i>=MAX_FILE)
		return 1;
	
	int temp;
	unsigned char next;
	next = Dir[i].Start;
	temp = next;
	
	if (next == 0){
		WDir = i;
		return 0;
	}
	
	while( FAT[next] != 0){
		temp = next;
		next = FAT[temp];
	}
	
	WFinal = next;	// final block id of file with this name
	WDir = i;
	WNum = 0;
	WBlock = 0;
	
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
		
		if (Dir[WDir].Start == 0){					//New File
			int newBlock; newBlock = Dir[0].Start;
			Dir[WDir].Start = newBlock;				//allocate block
			WFinal = newBlock;
						
			eDisk_WriteBlock(Wbuffer,WFinal);	//write to block
			
			Dir[0].Start = FAT[Dir[0].Start];		//relinke the FAT
			FAT[newBlock] = 0;
		}
		else{
		// write to disk
		eDisk_WriteBlock(Wbuffer,WFinal);
		
		// get new block for this file
		int newBlock; newBlock = Dir[0].Start;
		FAT[WFinal] = newBlock;
		Dir[0].Start = FAT[Dir[0].Start];		//relinke the FAT
		
		FAT[newBlock] = 0;
		WFinal = newBlock;
		}
		
		Dir[WDir].size += 1;
		
		unsigned char zero[512];
		for (int j=0;j<512;j++){
			zero[j] = NULL;
		}
		memcpy(Wbuffer,zero,512);
		
		Wbuffer[0] = data;
		WNum = 1;
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
	
	
	// add EOF to buffer
	Wbuffer[WNum] = -1;
	
	if (Dir[WDir].Start == 0){					//New File
		int newBlock; newBlock = Dir[0].Start;
		Dir[WDir].Start = newBlock;				//allocate block
		WFinal = newBlock;
					
		eDisk_WriteBlock(Wbuffer,WFinal);	//write to block
		
		Dir[0].Start = FAT[Dir[0].Start];		//relinke the FAT
		FAT[newBlock] = 0;
	}
	else{
	int newBlock; newBlock = Dir[0].Start;
	FAT[WFinal] = newBlock;
	Dir[0].Start = FAT[Dir[0].Start];
	
	FAT[newBlock] = 0;
	WFinal = newBlock;
	
	// write to disk
	eDisk_WriteBlock(Wbuffer,WFinal);
	}
	Dir[WDir].size += 1;
	
	
	WNum = 0;
	WFinal = 0;
	WBlock = 0;
	WDir = 0;

	unsigned char zero[512];
	for (int j=0;j<512;j++){
		zero[j] = NULL;
	}
	memcpy(Wbuffer,zero,512);
	
	eFile_Unmount();
	
  return 0;   // replace
}


//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is an ASCII string up to seven characters
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)

int eFile_ROpen( const char name[]){      // open a file for reading 

	int i=0;DRESULT Rtest;
	
	//if (FileCreated == 0)
	//	return 1;
	
	while (i < MAX_FILE){
		if ( strcmp ((char*)&Dir[i].Name,name) == 0)
			break;
		i++;
	}
	if (i>=MAX_FILE)
		return 1;
	
	RDir = i;
	RBlock = Dir[i].Start;
	Rtest = eDisk_ReadBlock(Rbuffer,RBlock);
	RNum = 0;
	
	if (Dir[i].Start==0)
		return 1;
	
  return 0;   // replace   
}
 
//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
//char ch;

int eFile_ReadNext( char *pt){       // get next byte 
	
	
  DRESULT Rtest;
	char ch;
		
//	if (RNum < 512){
//		ch = Rbuffer[RNum];
//	}else{
//		RBlock = FAT[RBlock];
//		if(RBlock == 0) return 1;
//		
//		Rtest = eDisk_ReadBlock(Rbuffer,RBlock);
//		RNum = 0;
//		
//		ch = Rbuffer[RNum];
//		
//	}
//	RNum++;
//	if( ch == -1 ){
//		return 1;
//	}
//	else if( ch==-1 ){
//		RNum = 512;
//	}
//		
//	*pt = ch;

	if (RNum < 512){
		ch = Rbuffer[RNum];
		if (ch==-1&&FAT[RBlock]==0){
			return 1;
		}
		else if (ch==-1){
			RBlock = FAT[RBlock];
			
			Rtest = eDisk_ReadBlock(Rbuffer,RBlock);
			RNum = 0;
			
			ch = Rbuffer[RNum];
		}
	}
	else{
		RBlock = FAT[RBlock];
		if(RBlock == 0) return 1;
		
		Rtest = eDisk_ReadBlock(Rbuffer,RBlock);
		RNum = 0;
		
		ch = Rbuffer[RNum];
		if (ch==-1&&FAT[RBlock]==0){
			return 1;
		}
	}
	RNum++;
	*pt = ch;
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
	
	while (i < MAX_FILE){
		if ( strcmp ((char*)&Dir[i].Name,name) == 0)
			break;
		i++;
	}
	if (i>=MAX_FILE)
		return 1;
	
	int temp = Dir[0].Start;
	while (FAT[temp] != 0){
		temp = FAT[temp];
	}
	FAT[temp] = 	Dir[i].Start;
	
	for (int j=i;j<127;j++){
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
	
	FileCreated--;
	
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
   
	unsigned char UMou[512];
	
	memcpy(UMou,&FAT,512);
	eDisk_WriteBlock(UMou,0);
	
	// dir 0 is sepecial for dectecting free block
	for(int i = 0; i < 32; i++){
		memcpy(UMou + i * 16, &Dir[i], 16);
	}
	eDisk_WriteBlock(UMou,1);
	
	for(int i = 0; i < 32; i++){
		memcpy(UMou + i * 16, &Dir[i + 32], 16);
	}
	eDisk_WriteBlock(UMou,2);
	
	for(int i = 0; i < 32; i++){
		memcpy(UMou + i * 16, &Dir[i + 64], 16);
	}
	eDisk_WriteBlock(UMou,3);
	
	for(int i = 0; i < 32; i++){
		memcpy(UMou + i * 16, &Dir[i + 96], 16);
	}
	eDisk_WriteBlock(UMou,4);
	
  return 0;   // replace
}
