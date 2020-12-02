//***********************  main.c  ***********************
// Program written by:
// - Steven Prickett  steven.prickett@gmail.com
//
// Brief desicription of program:
// - Initializes an ESP8266 module to act as a WiFi client or server
//
//*********************************************************
/* Modified by Jonathan Valvano March 28, 2017
   Modified by Andreas Gerstlauer April 9, 2020
   
 Out of the box: to make this work you must
 Step 1) Set parameters of your AP in WifiSettings.h
 Step 2) Change line 39 with directions in lines 40-42
 Step 3) Run a terminal emulator like Putty or TExasDisplay at
         115200 bits/sec, 8 bit, 1 stop, no flow control
 Step 4) Set BAUDRATE in WifiSettings.h to match baud rate of your ESP8266 (9600 or 115200)
 Step 5) Some ESP8266 respond with "ok", others with "ready"
         esp8266.c ESP8266_Init/Reset function, try different strings like "ready" and "ok"
 Step 6) Some ESP8266 respond with "ok", others with "no change"
         esp8266.c ESP8266_SetWifiMode function, try different strings like "no change" and "ok"
 Example
 AT+GMR version 0018000902 uses "ready" and "no change"
 AT+GMR version:0.60.0.0 uses "ready" and "ok"
 
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/pll.h"
#include "../inc/UART0int.h"
#include "../inc/LED.h"
#include "../inc/CortexM.h"
#include "../inc/esp8266.h"
// prototypes for functions defined in startup.s


// Client or Server ESP8266 Initialization
// 0 means client, != 0 means server at specified port
// #define SERVER       80   // port 80 is for http

// Transparently forwarding debug mode
// #define TRANSPARENT  1


#if ! TRANSPARENT && ! SERVER

const char Fetch[] = "GET /data/2.5/weather?q=Austin&APPID=1bc54f645c5f1c75e681c102ed4bbca4 HTTP/1.1\r\nHost:api.openweathermap.org\r\n\r\n";
//char Fetch[] = "GET /data/2.5/weather?q=Austin%20Texas&APPID=1234567890abcdef1234567890abcdef HTTP/1.1\r\nHost:api.openweathermap.org\r\n\r\n";
// 1) go to http://openweathermap.org/appid#use 
// 2) Register on the Sign up page
// 3) get an API key (APPID) replace the 1234567890abcdef1234567890abcdef with your APPID

char Response[512];

int main(void){
  DisableInterrupts();
  PLL_Init(Bus80MHz);
  LED_Init();  
  Output_Init();       // UART0 only used for debugging
  EnableInterrupts();
  if(!ESP8266_Init(true,false)) {  // initialize with rx echo
    printf("\r\n---No ESP detected\r\n");
    while(1) {}
  }
  printf("\r\n-----------System starting...\r\n");
  ESP8266_GetVersionNumber();
  if(!ESP8266_Connect(true)) {  // connect to access point
    printf("\r\n---Failure connecting to access point\r\n");
    while(1) {}
  }
  LED_BlueOn();
  while(1){
    ESP8266_GetStatus();
    if(ESP8266_MakeTCPConnection("api.openweathermap.org", 80, 0)){ // open socket to web server on port 80
      if(ESP8266_Send(Fetch)){  // send request 
        if(ESP8266_Receive(Response, 512)){  // receive response
          if(strncmp(Response, "HTTP", 4) == 0) { // received HTTP response?
            LED_BlueOff();
            LED_GreenOn();
          }
        }
      }      
      ESP8266_CloseTCPConnection();  // close connection   
    }          
    while(Board_Input()==0){// wait for touch
    }; 
    LED_GreenOff();
    LED_BlueOn();
    LED_RedToggle();
  }
}

#elif SERVER

/*
======================================================================================================================
==========                                     Simple HTTP SERVER                                           ==========
======================================================================================================================
*/

const char formBody[] = 
  "<!DOCTYPE html><html><body><center> \
<h1>Enter a message to send to your microcontroller:</h1> \
  <form> \
  <input type=\"text\" name=\"message\" value=\"Hello ESP8266!\"> \
  <br><input type=\"submit\" value=\"Go!\"> \
  </form></center></body></html>";

const char statusBody[] = 
  "<!DOCTYPE html><html><body><center> \
  <h1>Message sent successfully!</h1> \
  </body></html>";

/*
===================================================================================================
  HTTP :: HTTP_ServePage  
   - constructs and sends a web page via the ESP8266 server
   - NOTE: this seems to work for sending pages to Firefox (and maybe other PC-based browsers),
           but does not seem to load properly on iPhone based Safari. May need to add some more
           data to the header.
===================================================================================================
*/
int HTTP_ServePage(const char* body){
  char header[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: ";
    
  char contentLength[16];
  sprintf(contentLength, "%d\r\n\r\n", strlen(body));

  if(!ESP8266_SendBuffered(header)) return 0;
  if(!ESP8266_SendBuffered(contentLength)) return 0;
  if(!ESP8266_SendBuffered(body)) return 0;    
  
  if(!ESP8266_SendBufferedStatus()) return 0;
  
  return 1;
}

char HTTP_Request[64];

int main(void){  
  DisableInterrupts();
  PLL_Init(Bus80MHz);
  LED_Init();  
  Output_Init();       // UART0 only used for debugging
  EnableInterrupts();
  if(!ESP8266_Init(true,false)) {  // initialize with rx echo
    printf("\r\n---No ESP detected\r\n");
    while(1) {}
  }
  printf("\r\n-----------System starting...\r\n");
  ESP8266_GetVersionNumber();
  if(!ESP8266_Connect(true)) {  // connect to access point
    printf("\r\n---Failure connecting to access point\r\n");
    while(1) {}
  }
  if(!ESP8266_StartServer(SERVER,600)) {  // 5min timeout
    printf("\r\n---Failure starting server\r\n");
    while(1) {}
  }  
  LED_BlueOn();
  while(1) {
    // Wait for connection
    ESP8266_WaitForConnection();
    
    // Receive request
    if(!ESP8266_Receive(HTTP_Request, 64)){
      ESP8266_CloseTCPConnection();
      continue;
    }
    
    // check for HTTP GET
    if(strncmp(HTTP_Request, "GET", 3) == 0) {
      char* messagePtr = strstr(HTTP_Request, "?message=");
      if(messagePtr) {
        // Process form reply
        if(HTTP_ServePage(statusBody)) {
          LED_GreenOff();
          LED_BlueOn();
        }
        // Terminate message at first separating space
        char* messageEnd = strchr(messagePtr, ' ');
        if(messageEnd) *messageEnd = 0;  // terminate with null character
        // Print message on terminal
        printf("\r\n---Message from the Internet: %s\r\n", messagePtr + 9);
      } else {
        // Serve web page
        if(HTTP_ServePage(formBody)) {
          LED_BlueOff();
          LED_GreenOn();
        }         
      }        
    } else {
      // handle data that may be sent via means other than HTTP GET
    }
    LED_RedToggle();
    ESP8266_CloseTCPConnection();
  }
}

#else  // TRANSPARENT

// transparent mode for testing
void ESP8266_SendCommand(char *);
void ESP8266_OutChar(char);
int main(void){  char data;
  DisableInterrupts();
  PLL_Init(Bus80MHz);
  LED_Init();  
  Output_Init();       // UART0 as a terminal
  EnableInterrupts();
  if(!ESP8266_Init(true,false)) {  // initialize with rx echo
    printf("\r\n---No ESP detected\r\n");
    while(1) {}
  }
  printf("\r\n-----------System starting...\r\n");
  ESP8266_Reset();
//  ESP8266_SendCommand("AT+UART=115200,8,1,0,3\r\n");
//  data = UART_InChar();
  
  while(1){
// echo data back and forth
    data = UART_InCharNonBlock();
    if(data){
      ESP8266_OutChar(data);
    }
  }
}

#endif
