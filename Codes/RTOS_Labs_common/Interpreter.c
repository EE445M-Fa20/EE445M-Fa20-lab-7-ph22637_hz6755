// *************Interpreter.c**************
// Students implement this as part of EE445M/EE380L.12 Lab 1,2,3,4 
// High-level OS user interface
// 
// Runs on LM4F120/TM4C123
// Jonathan W. Valvano 1/18/20, valvano@mail.utexas.edu
#include <stdint.h>
#include <string.h> 
#include <stdbool.h> 
#include <stdio.h>
#include <stdlib.h>
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../inc/ADCT0ATrigger.h"
#include "../inc/ADCSWTrigger.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../RTOS_Labs_common/ADC.h"
#include "../RTOS_Lab5_ProcessLoader/loader.h"
#include "../RTOS_Labs_common/heap.h"
#include "../RTOS_Labs_common/esp8266.h"
#include "../RTOS_Labs_common/Interpreter.h"
#include "../inc/tm4c123gh6pm.h"


//#include "../RTOS_Lab5_ProcessLoader/loader_config.h"
extern short Actuator;
extern uint32_t DASoutput;
//extern uint32_t thisTime;
extern int32_t MaxJitter;
extern uint32_t JitterHistogram[];
extern uint32_t IdleCount; 
extern uint32_t CPUUtil;

// write this
int max_length = 32;															//define max length of input command
int command_counter = 0;

char input_temp[32];											//string of input command from PC
char *pt_input_temp = &input_temp[0];							//pointer points to input command string

char *output_temp;
char *command[4];

char Client_input_temp[32];																			//string of input command from PC
char *Client_pt_input_temp = &Client_input_temp[0];							//pointer points to input command string

char ClientTX[32];
char ClientRX[32];

static const ELFSymbol_t symtab[] = {
  { "ST7735_Message", ST7735_Message}
};
// Print jitter histogram
void Jitter(int32_t MaxJitter, uint32_t const JitterSize, uint32_t JitterHistogram[]){
  int32_t i;
		ST7735_Message(1, 6 ,"Jitter=",MaxJitter);
	if(MaxJitter<=63){
	  ST7735_Message(1, 7,"Number=",JitterHistogram[MaxJitter]);
	}
	else{
	  ST7735_Message(1, 7,"Number=",JitterHistogram[63]);
	}
	if(MaxJitter<=63){
	for(i = 0;i <= MaxJitter;i++){
		if(JitterHistogram[i]>0){
			UART_OutString("\n\r");
		  UART_OutString("Jitter=");UART_OutUDec(i);
			UART_OutString(" Number=");UART_OutUDec(JitterHistogram[i]);
		  }
	  }
	  //UART_OutString("MaxJitter=");UART_OutUDec(MaxJitter);
	}
	if(MaxJitter>63){
	for(i = 0;i <= 63;i++){
		if(JitterHistogram[i]>0){
			UART_OutString("\n\r");
		  UART_OutString("Jitter=");UART_OutUDec(i);
			UART_OutString(" Number=");UART_OutUDec(JitterHistogram[i]);
		  }
	  }
	  UART_OutString("\n\r");
		UART_OutString("MaxJitter=");UART_OutUDec(MaxJitter);
	}
	
	// write this for Lab 3 (the latest)
	
}

void ClientSend(void){
	if(!ESP8266_Send(ClientTX)){
		ST7735_DrawString(0,2,"Send failed",ST7735_YELLOW);
		ESP8266_CloseTCPConnection();
		for (int i=0;i<32;i++){
			ClientTX[i]=0;
		}
		OS_Kill();
	}
	if(!ESP8266_Receive(ClientRX, 32)) {
		ST7735_DrawString(0,2,"No response",ST7735_YELLOW); 
		ESP8266_CloseTCPConnection();
		OS_Kill();
	}
	for (int i=0;i<32;i++){
		ClientTX[i]=0;
	}
}

//Client function for TCP/IP
void Client(void){
	UART_OutString("\n\r");
  // Initialize and bring up Wifi adapter  
  if(!ESP8266_Init(true,false)) {  // verbose rx echo on UART for debugging
    ST7735_DrawString(0,1,"No Wifi adapter",ST7735_YELLOW); 
    OS_Kill();
  }
  // Get Wifi adapter version (for debugging)
  ESP8266_GetVersionNumber(); 
  // Connect to access point
  if(!ESP8266_Connect(true)) {  
    ST7735_DrawString(0,1,"No Wifi network",ST7735_YELLOW); 
    OS_Kill();
  }
//  ST7735_DrawString(0,1,"Wifi connected",ST7735_GREEN);
	
	if(!ESP8266_MakeTCPConnection("192.168.1.101",1848,0)){ // open socket to sever TM4C on port 1849
//    ST7735_DrawString(0,2,"Connection failed",ST7735_YELLOW);
		UART_OutString("Connection failed\n\r");
    OS_Kill();
  }
	else{
//    ST7735_DrawString(0,2,"Connected",ST7735_YELLOW);
		UART_OutString("Connected\n\r");
	}

		while (1){
			UART_InString (Client_pt_input_temp , max_length);
			command[command_counter] = strtok(Client_input_temp , " ");   //first instruction
			while (Client_pt_input_temp!=NULL){
				command_counter++;
				command[command_counter] = strtok(NULL , " ");       //second instruction
				if ( !strcmp(command[0],"LED") ){
					command[2] = strtok(NULL , "\n");                  //third instruction
					break;
				}
				if ( ( !strcmp(command[0],"ST7735") ) || ( !strcmp(command[0],"eFile") ) ){
					command[2] = strtok(NULL , " ");       //second instruction
					command[3] = strtok(NULL , "\n");                  //fourth instruction
					break;
				}
				break;
			}
			if (!strcmp(command[0],"ADC_In")){											//ADC_In for client
				for (int i=0;i<32;i++){
					ClientRX[i]=0;
				}
				ClientTX[0] = '1';
				ClientTX[1] = '\n';
				ClientSend();
				UART_OutString("\n");
				UART_OutString(ClientRX);
				UART_OutString("\n\r");
				for (int i=0;i<32;i++){
					ClientRX[i]=0;
				}
			}																												//end
			else if (!strcmp(command[0],"OS_Time")){											//OS_Time for client
				for (int i=0;i<32;i++){
					ClientRX[i]=0;
				}
				ClientTX[0] = '2';
				ClientTX[1] = '\n';
				ClientSend();
				UART_OutString("\n");
				UART_OutString(ClientRX);
				UART_OutString("\n\r");
			}																												//end
			else if (!strcmp(command[0],"ST7735")){											//ST7735 for client
				for (int i=0;i<32;i++){
					ClientRX[i]=0;
				}
				ClientTX[0] = '3';
				ClientTX[1] = ' ';
				
				char command_temp[32];
				strcpy(command_temp,command[1]);
				strcat(command_temp," ");
				strcat(command_temp,command[2]);
				strcat(command_temp," ");
				strcat(command_temp,command[3]);
				strcat(command_temp,"\n");
				
				strcpy(&ClientTX[2],command_temp);
				ClientSend();
			}																												//end
			else if (!strcmp(command[0],"LED")){													//LED for client
				for (int i=0;i<32;i++){
					ClientRX[i]=0;
				}
				ClientTX[0] = '4';
				ClientTX[1] = ' ';
				ClientTX[2] = (*command[1]);
				ClientTX[3] = ' ';
				ClientTX[4] = (*command[2]);
				ClientTX[5] = '\n';
				ClientSend();
			}																												//end
			else if (!strcmp(command[0],"eFile")){												//File system for client
				for (int i=0;i<32;i++){
					ClientRX[i]=0;
				}
				ClientTX[0] = '5';
				ClientTX[1]=' ';
				
				if (!strcmp(command[1],"Format")){
					ClientTX[2] = '1';
					ClientTX[3]=' ';
					ClientSend();
					UART_OutString("\n");
					UART_OutString(ClientRX);
					UART_OutString("\n\r");
				}
				if (!strcmp(command[1],"Create")){
					ClientTX[2] = '2';
					ClientTX[3]=' ';
					command[2]=strcat(command[2],"\n");
					strcpy(&ClientTX[4],command[2]);
					ClientSend();
					UART_OutString("\n");
					UART_OutString(ClientRX);
					UART_OutString("\n\r");
				}
				if (!strcmp(command[1],"Read")){
					ClientTX[2] = '3';
					ClientTX[3]=' ';
					command[2]=strcat(command[2],"\n");
					strcpy(&ClientTX[4],command[2]);
					ClientSend();
					UART_OutString("\n");
					UART_OutString(ClientRX);
					UART_OutString("\n\r");
				}
				if (!strcmp(command[1],"Write")){
					ClientTX[2] = '4';
					ClientTX[3]=' ';
					
					char command_temp[32];
					strcpy(command_temp,command[2]);
					strcat(command_temp," ");
					strcat(command_temp,command[3]);
					strcat(command_temp,"\n");
					strcpy(&ClientTX[4],command_temp);
					ClientSend();
					UART_OutString("\n");
					UART_OutString(ClientRX);
					UART_OutString("\n\r");
				}

			}																												//end
			else if (!strcmp(command[0],"Elf")){													//user process for client
				for (int i=0;i<32;i++){
					ClientRX[i]=0;
				}
				ClientTX[0] = '6';
				ClientTX[1] = '\n';
				ClientSend();
			}																												//end
			else if (!strcmp(command[0],"DisConnect")){									//Disconnect Client
				for (int i=0;i<32;i++){
					ClientRX[i]=0;
				}
				ClientTX[0] = '7';
				ClientTX[1] = '\n';
				ClientSend();
				ESP8266_CloseTCPConnection();
				OS_Kill();
			}																												//end
			else if (!strcmp(command[0],"Toggle")){									//Disconnect Client
				for (int i=0;i<32;i++){
					ClientRX[i]=0;
				}
				ClientTX[0] = '8';
				ClientTX[1] = '\n';
				ClientSend();
			}																												//end
			else if(!strcmp(command[0],"Send")){
				for (int i=0;i<32;i++){
					ClientRX[i]=0;
				}
				if (Error_Type!=0){
					ClientTX[0] = '9';
					ClientTX[1] = ' ';
					ClientTX[2] = Error_Type;
					ClientTX[3] = ' ';
					
					char command_temp[32];
					memcpy(command_temp,&Error_Num,4);
					strcat(command_temp,"\n");
					strcpy(&ClientTX[4],command_temp);
					
					Error_Type = 0;
					Error_Num = 0;
					Error_Send ++;

					ClientSend();
					if (Error_Type<3){
						Error_Type = 0;
						Error_Num = 0;
						NumCreated += OS_AddThread(&Buck_Fun,128,1);
					}
				}
			}
			else if (!strcmp(command[0],"Help")){												//Client help messages
				UART_OutString("\n\r");
				UART_OutString("ADC_In:Return ADC value on server board\n\r");
				UART_OutString("OS_Time:Return OS running time on server board\n\r");
				UART_OutString("ST7735 Char: Output Char on server ST7735\n\r");
				UART_OutString("eFile Func name: Execute efile_Func on server\n\r");
				UART_OutString("Elf: Execute user.axf on server side\n\r");
				UART_OutString("DisConnect: Disconnect Client from server\n\r");
			}																												//end
			else{
				UART_OutString("\n\rError\n\r");
			}
			command_counter = 0;
				
		}
}

// *********** Command line interpreter (shell) ************
char FileName[8]="Buck0";
void Interpreter(void){ 
		while (1){
			UART_InString (pt_input_temp , max_length);
			command[command_counter] = strtok(input_temp , " ");   //first instruction
			while (pt_input_temp){
				command_counter++;
				command[command_counter] = strtok(NULL , " ");       //second instruction
				if ((!strcmp(command[0],"LCD_T")) || (strcmp(command[0],"LCD_Down"))){
					command[2] = strtok(NULL , "\0");                  //third instruction
					break;
				}
			}
			if (!strcmp(command[0],"Connect")){
				NumCreated += OS_AddThread(&Client,128,2);
			}
			else if (!strcmp(command[0],"ADC_In")){
				UART_OutString("\n");
				UART_OutUDec(ADC_In());
				UART_OutString("\n\r");
			}
			else if (!strcmp(command[0],"OS_Time")){
				UART_OutString("\n");
				UART_OutUDec(OS_MsTime());
				UART_OutString("ms\n\r");
			}
			else if (!strcmp(command[0],"OS_Cleartime")){
				OS_ClearMsTime();
				UART_OutString("\n");
				UART_OutString("  Cleared\n\r");
			}
			else if(!strcmp(command[0],"LCD_T")){
				uint32_t l_temp;
				l_temp = strtol(command[1],NULL,10);
				if(l_temp > 7){
				  UART_OutString("  Invalid Row\n\r");
				}
				else{
				ST7735_Message(0, l_temp ,command[2],42);
				UART_OutString("  Done\n\r");
				}
			}
			else if (!strcmp(command[0],"LCD_D")){
				uint32_t l_temp;
				l_temp = strtol(command[1],NULL,10);
				if(l_temp > 7){
				  UART_OutString("  Invalid Row\n\r");
				}
				else{
				ST7735_Message(1, l_temp ,command[2],42);
				UART_OutString("  Done\n\r");
				}
			}
			else if (!strcmp(command[0],"PID")){
				UART_OutString("\n");
//				ST7735_Message(1, 2 ,"Actuator= ",Actuator);
				UART_OutString("PID shown\n\r");
			}
			else if (!strcmp(command[0],"Jitter")){
				UART_OutString("\n\r");
				Jitter(MaxJitter, 64, JitterHistogram);
				UART_OutString("\n\r");
//				UART_OutUDec(CPUUtil);UART_OutString("\n\r");
//				UART_OutUDec(IdleCount);
			}
			else if (!strcmp(command[0],"DAS")){
				UART_OutString("\n");
//				ST7735_Message(1, 5 ,"DASOutput=  ",DASoutput);
				//ST7735_Message(1, 6 ,"thistime=  ",thisTime);
				//ST7735_Message(1, 7 ,"MaxJitter=  ",MaxJitter);
				UART_OutString("Done\n\r");
			}
			else if (!strcmp(command[0],"Format")){
				UART_OutString("\n");
        int i = eFile_Format();
				if(i){UART_OutString("Format Failed\n\r");}
				else{
				UART_OutString("Format Successed\n\r");}
			}
			else if (!strcmp(command[0],"CreateFile")){
				int i = eFile_Mount();
				if(i){UART_OutString("\n\rCreate Failed\n\r");}
				i = eFile_Create(command[1]);
				if(i){UART_OutString("\n\rCreate Failed\n\r");}
				else{
				UART_OutString("\n\rCreate Successed\n\r");
			}
		}
			else if (!strcmp(command[0],"DeleteFile")){
				int i = eFile_Delete(command[1]);
				if(i){UART_OutString("\n\rDelete Failed\n\r");}
				else{
				UART_OutString("\n\rDelete Successed\n\r");
			  }
			}
			else if (!strcmp(command[0],"Mount")){
        int i = eFile_Mount();
				if(i==0) UART_OutString("\n\rMount Completed\n\r");
				else UART_OutString("\n\rMount Failed\n\r");
		}
      else if (!strcmp(command[0],"Unmount")){
        int i = eFile_Unmount();
				if(i==0) UART_OutString("\n\rUnMount Completed\n\r");
				else UART_OutString("\n\rUnMount Failed\n\r");
		}
			else if (!strcmp(command[0],"Write")){

				int i = eFile_WOpen(command[1]);
				if(i){UART_OutString("\n\rWOpen Failed\n\r");}
					else{char *pt=command[2];
					while(*pt){
						eFile_Write(*pt);
						pt++;
					}
					UART_OutString("\n\rWrite Completed\n\r");
					int k = eFile_WClose();
					if(k){UART_OutString("\n\rWClose Failed\n\r");}
					else{UART_OutString("\n\rWClose Successed\n\r");}
				}
		}
			else if (!strcmp(command[0],"Read")){
				int i;
				i = eFile_Mount();
				if(i){UART_OutString("\n\rMount Failed\n\r");}
				i = eFile_ROpen(command[1]);
				if(i){UART_OutString("\n\rROpen Failed\n\r");}
				else{
					UART_OutString("\n\r");
					char pt;
					int k = 0;
					while(k == 0){	
						k = eFile_ReadNext(&pt);
						if(k==1)break;
						UART_OutChar(pt);
					}
					UART_OutString("\n\rRead Completed\n\r");
					int j = eFile_RClose();
					if(j){UART_OutString("\n\rRClose Failed\n\r");}
					else{UART_OutString("\n\rRClose Successed\n\r");}
			}
		}
			else if (!strcmp(command[0],"LoadELF")){
				int k = eFile_Mount();
				if(k==0) UART_OutString("\n\rMount Completed\n\r");
				UART_OutString("\n\r");
			  ELFEnv_t env = { symtab , 1};
				int i = exec_elf("User.axf", &env);
		    if(i == -1){UART_OutString("\n\rLoadELF failed\n\r");}
				else {UART_OutString("\n\rLoadELF done\n\r"); }
		}
			else if (!strcmp(command[0],"Heapinit")){
				UART_OutString("\n\r");
			  int i = Heap_Init();
				UART_OutString("\n\rHeapinit done\n\r"); 
		}
			else if (!strcmp(command[0],"Buck")){
				if (Error_Type !=0){
					eFile_Create(FileName);
					eFile_WOpen(FileName);
					StreamToDevice = 1;
					if (Error_Type == 1)
						printf("\n\rError:Over_Voltage\n\r");
					if (Error_Type == 2)
						printf("Error:Low_Voltage\n\r");
					if (Error_Type==3)
						printf("Error:HardFault\n\r");
					printf("%d\n\r",Error_Num);	
					StreamToDevice = 0;
					eFile_WClose();
					FileName[5] = (FileName[5]+1)&0xF7;
					UART_OutString("\n\rError Saved\n\r");
				}
				else
				{
					UART_OutString("\n\rNo Error\n\r");
				}
			}
			else if (!strcmp(command[0],"Restart")){
				if (Error_Type == 0)
					UART_OutString("\n\r Running \n\r");
				else{
					NumCreated += OS_AddThread(&Buck_Fun,128,3);
					UART_OutString("\n\rRestarted\n\r");
				}
			}
			else if (!strcmp(command[0],"Help")){
				UART_OutString("\n");UART_OutString("\r");
				UART_OutString("ADC_In:Return ADC value\n\r");
				UART_OutString("OS_Time:Return OS running time\n\r");
				UART_OutString("OS_Cleartime: Reset OS running time\n\r");
				UART_OutString("LCD_T X Chars: Print Chars on TOP Xth row of LCD\n\r");
				UART_OutString("LCD_D X Chars: Print Chars on DOWN Xth row of LCD\n\r");
			}
			else{
				output_temp = "Error";
				UART_OutString(output_temp);
				UART_OutString("\n\r");
			}
			command_counter = 0;
		}
}


