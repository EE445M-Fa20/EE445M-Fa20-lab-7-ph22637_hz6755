// *************os.c**************
// EE445M/EE380L.6 Labs 1, 2, 3, and 4 
// High-level OS functions
// Students will implement these functions as part of Lab
// Runs on LM4F120/TM4C123
// Jonathan W. Valvano 
// Jan 12, 2020, valvano@mail.utexas.edu


#include <stdint.h>
#include <stdio.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/CortexM.h"
#include "../inc/PLL.h"
#include "../inc/LaunchPad.h"
#include "../inc/Timer4A.h"
#include "../inc/WTimer0A.h"
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../inc/ADCT0ATrigger.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../inc/Timer5A.h"
#include "../RTOS_Labs_common/heap.h"

#define PD3                    (*((volatile uint32_t *)0x40007020))
#define NVIC_SYSPRI14_R        (*((volatile uint32_t *)0xE000ED22))  //PendSV priority register (position 14).
#define NVIC_SYSPRI15_R        (*((volatile uint32_t *)0xE000ED23))  //Systick priority register (position 15).
#define NVIC_LEVEL14               0xEF                              //Systick priority value (second lowest).
#define NVIC_LEVEL15               0xFF                              //PendSV priority value (lowest).
#define NVIC_PENDSVSET             0x10000000                        //Value to trigger PendSV exception.

uint32_t BackCreated;

// Performance Measurements 
int32_t MaxJitter;             // largest time jitter between interrupts in usec
#define JITTERSIZE 64
uint32_t const JitterSize=JITTERSIZE;
uint32_t JitterHistogram[JITTERSIZE]={0,};

int32_t MaxJitter2;             // largest time jitter between interrupts in usec
#define JITTERSIZE 64
uint32_t const JitterSize2=JITTERSIZE;
uint32_t JitterHistogram2[JITTERSIZE]={0,};

uint32_t OStime_round;												//variables defined for Lab 1, used to count rounds for timer5A,can be reset
uint32_t OStime_Task1;                        //variables defined for Lab 1, used to count Jitters,cannot be reset
uint32_t OStime_ms;														//variables defined for Lab 1, used to remember time in ms

Sema4Type LCDFree;
Sema4Type TxRoomLeft;
Sema4Type RxDataAvailable;
Sema4Type Buck;

void(*SW_task1)(void);				//hook for SW1 interrupt task
void(*SW_task2)(void);		   //hook for SW2 interrupt task
void(*PeriodicTask4)(void);	//hook for TIMER4, used for add period task
void(*PeriodicTask3)(void);
void RemovefromReady(TCBtype *Pt);
void AddtoReady(TCBtype *Pt);
void AddtoSema4(Sema4Type *Sema4);
TCBtype *RemovefromSema4(Sema4Type *Sema4);

/*	FIFO variables	*/
struct FIFO{
	long queue[MAX_FIFO_SIZE];
	Sema4Type RoomRemain;
	Sema4Type DataAvailable;
	long max;
};
struct FIFO OS_FIFO;

/*	FIFO variables	*/
struct MailBox{
	long data;
	Sema4Type BoxFree;
	Sema4Type DataValid;
};
struct MailBox OS_MailBox;

/*	PCB variables	*/
struct PCB{
	int id;
	//TCBtype TCBs[MAX_TCB_SIZE];
	long * code;
	long * data;
	int threadNum; // main thread pointer
	int priority;
	struct PCB * next;
	struct PCB * pre;
};
typedef struct PCB PCBType;
PCBType * currentPCB = NULL; // current process, temporarily unchanged after first assignment ( should be changed in contextswitch)
int PCBNum = 0;

/*	TCB variables	*/
struct TCB{
	long *SavedSP;
	struct TCB *Next;
	struct TCB *Previous;
	int32_t Id;
	uint32_t Sleep;
	uint32_t Priority;
	uint32_t Block;
	uint32_t Kill;
	Sema4Type * blockPt;
	PCBType * PCBPtr;
};



TCBtype TCB_pool[MAX_TCB_SIZE];
TCBtype *Ready_pool[MAX_TCB_SIZE];
int ReadyCreated;
TCBtype Block_pool[MAX_TCB_SIZE];

TCBtype *RunPt;
TCBtype *SchPt;
long STACK_pool[MAX_TCB_SIZE][MAX_STACK_SIZE];




/*------------------------------------------------------------------------------
  Systick Interrupt Handler
  SysTick interrupt happens every 2 ms
  used for preemptive thread switch
 *------------------------------------------------------------------------------*/
void SysTick_Handler(void) {
	SchPt = RunPt->Next;
	OS_Suspend();
} // end SysTick_Handler

unsigned long OS_LockScheduler(void){
  // lab 4 might need this for disk formating
  return 0;// replace with solution
}
void OS_UnLockScheduler(unsigned long previous){
  // lab 4 might need this for disk formating
}


void SysTick_Init(unsigned long period){
  NVIC_ST_CTRL_R = 0;                   // disable SysTick during setup
  NVIC_ST_RELOAD_R = period-1;  				// maximum reload value
  NVIC_ST_CURRENT_R = 0;                // any write to current clears it
                                        // enable SysTick with core clock
	NVIC_SYSPRI15_R = NVIC_LEVEL14;       // second lowest priorty 0xEF
	NVIC_SYSPRI14_R = NVIC_LEVEL15;    		// Set PengSV priority 0xFF
  NVIC_ST_CTRL_R = NVIC_ST_CTRL_ENABLE+NVIC_ST_CTRL_CLK_SRC+NVIC_ST_CTRL_INTEN;
}

/**
 * @details  Initialize operating system, disable interrupts until OS_Launch.
 * Initialize OS controlled I/O: serial, ADC, systick, LaunchPad I/O and timers.
 * Interrupts not yet enabled.
 * @param  none
 * @return none
 * @brief  Initialize OS
 */

void WDTimer0A_task(){
	long temp = StartCritical();
	for (int i = 0;i<MAX_TCB_SIZE;i++){
		if (TCB_pool[i].Sleep>0){
			TCB_pool[i].Sleep--;
			if (TCB_pool[i].Sleep==0){
				AddtoReady(&TCB_pool[i]);
			}
		}
	}
	EndCritical(temp);
}
void OSTtask(void){  // runs at 1Hz in background
  OStime_round += 1;
	OStime_Task1 += 1;
}

void OS_Init(void){
  // put Lab 2 (and beyond) solution here
	PLL_Init(Bus80MHz);
	UART_Init();
	for (int i=0;i<MAX_TCB_SIZE;i++){								//initiate TCB_pool to satisfy kill operation and priority operation
		TCB_pool[i].Kill = 1;
	}
	ST7735_InitR(INITR_REDTAB); // LCD initialization
	ST7735_Message(0, 0, "TOP default:", 1);
	ST7735_Message(1, 0, "DOWN default:", 42);
	ST7735_Message(1, 1, "LCDFree:", LCDFree.Value);
	
	WideTimer0A_Init(&WDTimer0A_task,80000000/1000,5);
	Timer5A_Init(&OSTtask,80000000/1,6); // 1 Hz sampling, priority=7, used for OS time recording in ms
	
	OS_InitSemaphore(&LCDFree, 1);
	OS_InitSemaphore(&TxRoomLeft, 1024);
	OS_InitSemaphore(&RxDataAvailable, 0);
	OS_InitSemaphore(&Buck, 1);
}; 

void AddtoSema4(Sema4Type *Sema4){
	if (Sema4->BlockCreated==0){
		Sema4->Block_pool[Sema4->BlockCreated] = RunPt;
		Sema4->BlockCreated++;
	}
	else{
		for (int i=0;i<Sema4->BlockCreated;i++){
			if ( Sema4->Block_pool[i]->Priority > RunPt->Priority){
				for (int j=Sema4->BlockCreated;j>i;j--){
					Sema4->Block_pool[j] = Sema4->Block_pool[j-1];
				}
				Sema4->Block_pool[i] = RunPt;
				break;
			}
			if( i == (Sema4->BlockCreated-1) ){
				Sema4->Block_pool[i] = RunPt;
			}
		}
		Sema4->BlockCreated++;
	}
}
TCBtype *RemovefromSema4(Sema4Type *Sema4){
	TCBtype *Temp;
	Temp = Sema4->Block_pool[0];
	for(int i=0;i<((Sema4->BlockCreated)-1);i++){
		Sema4->Block_pool[i] = Sema4->Block_pool[i+1];
	}
	Sema4->Block_pool[Sema4->BlockCreated-1] = NULL;
	Sema4->BlockCreated--;
	return Temp;
}
// ******** OS_InitSemaphore ************
// initialize semaphore 
// input:  pointer to a semaphore
// output: none
void OS_InitSemaphore(Sema4Type *semaPt, int32_t value){
  // put Lab 2 (and beyond) solution here
	semaPt->Value = value;
}; 

// ******** OS_Wait ************
// decrement semaphore 
// Lab2 spinlock
// Lab3 block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
  DisableInterrupts();
	(semaPt->Value)--;
	if(semaPt->Value < 0){
		RunPt->blockPt = semaPt;
		SchPt = RunPt->Next;
		RemovefromReady(RunPt);
		OS_Suspend();
		AddtoSema4(semaPt);
	}
	
	EnableInterrupts();
};

// ******** OS_Signal ************
// increment semaphore 
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a counting semaphore
// output: none
void OS_Signal(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
	long temp = StartCritical();
	(semaPt->Value)++;
	if(semaPt->Value <= 0){
		TCBtype *TCBTemp;
		TCBTemp = RemovefromSema4(semaPt);
		TCBTemp->blockPt = NULL;
		AddtoReady(TCBTemp);
//		if (TCBTemp->Priority<RunPt->Priority){
//			SchPt = RunPt->Next;
//			OS_Suspend();
//		}
	}
	EndCritical(temp);
}; 

// ******** OS_bWait ************
// Lab2 spinlock, set to 0
// Lab3 block if less than zero
// input:  pointer to a binary semaphore
// output: none
void OS_bWait(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
	DisableInterrupts();
	while(semaPt->Value == 0){
		EnableInterrupts();
		DisableInterrupts();
	}
	semaPt->Value = 0;
	EnableInterrupts();
}; 

// ******** OS_bSignal ************
// Lab2 spinlock, set to 1
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a binary semaphore
// output: none
void OS_bSignal(Sema4Type *semaPt){
  // put Lab 2 (and beyond) solution here
	semaPt->Value = 1;
}; 



//******** OS_AddThread *************** 
// add a foregound thread to the scheduler
// Inputs: pointer to a void/void foreground task
//         number of bytes allocated for its stack
//         priority, 0 is highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// stack size must be divisable by 8 (aligned to double word boundary)
// In Lab 2, you can ignore both the stackSize and priority fields
// In Lab 3, you can ignore the stackSize fields

int OS_AddThread(void(*task)(void), 
   uint32_t stackSize, uint32_t priority){
  // put Lab 2 (and beyond) solution here
		 if (NumCreated < MAX_TCB_SIZE){
			 for(int k=0;k<=NumCreated;k++){
				 if(TCB_pool[k].Kill == 1){
					 TCB_pool[k].SavedSP = &STACK_pool[NumCreated][MAX_STACK_SIZE-1];			//initiate SP information in TCB			 
          *(TCB_pool[k].SavedSP) = 0x01000000L;
          *(--TCB_pool[k].SavedSP) = (long) task;    //initiate stack
					*(--TCB_pool[k].SavedSP) = (long) task;
					*(--TCB_pool[k].SavedSP) = 0x12121212L;
					*(--TCB_pool[k].SavedSP) = 0x11111111L;
					*(--TCB_pool[k].SavedSP) = 0x10101010L;
					*(--TCB_pool[k].SavedSP) = 0x09090909L;
					*(--TCB_pool[k].SavedSP) = 0x08080808L;
					*(--TCB_pool[k].SavedSP) = 0x07070707L;
					*(--TCB_pool[k].SavedSP) = 0x06060606L;
					
					TCB_pool[k].PCBPtr = currentPCB;
					if(currentPCB != NULL){
					NumCreated++;	
					currentPCB->threadNum += 1;
					 *(--TCB_pool[k].SavedSP) = (long)currentPCB->data;
					}else{
					 *(--TCB_pool[k].SavedSP) = 0x05050505L; //r9

					}
					
					*(--TCB_pool[k].SavedSP) = 0x04040404L; //r8
					*(--TCB_pool[k].SavedSP) = 0x03030303L; //r7
					*(--TCB_pool[k].SavedSP) = 0x02020202L; //r6
					*(--TCB_pool[k].SavedSP) = 0x01010101L; //r5
					*(--TCB_pool[k].SavedSP) = 0x00000000L; //r4
					 long temp = StartCritical();

					 TCB_pool[k].Priority = priority;
					 TCB_pool[k].Kill = 0;
					 TCB_pool[k].blockPt = NULL;

					 AddtoReady(&TCB_pool[k]);
					 /*if (NumCreated == 0){
						 Ready_pool[NumCreated] = &TCB_pool[k];
						 ReadyCreated++;
					 }
					 else{
						 for (int i=0;i<NumCreated;i++){
							 if(Ready_pool[i]->Priority > priority){
								 for (int j=NumCreated;j>i;j--){
									 Ready_pool[j] = Ready_pool[j-1];
								 }
								 Ready_pool[i] = &TCB_pool[k];
								 break;
							 }
							 if( i==(NumCreated-1) ){
								 Ready_pool[NumCreated] = &TCB_pool[k];
								 break;
							 }
						 }
						 
						 for (int i=0;i<NumCreated;i++){
							 Ready_pool[i]->Next = Ready_pool[i+1];
							 Ready_pool[i]->Id = i;
						 }
						 Ready_pool[NumCreated]->Next = Ready_pool[0];
						 Ready_pool[NumCreated]->Id = NumCreated;
						 ReadyCreated++;
					 }*/
					 
					 EndCritical(temp);
					 return 1; 
				 }
			 }
		 }
		 else
		 return 0;
};

//******** OS_AddProcess *************** 
// add a process with foregound thread to the scheduler
// Inputs: pointer to a void/void entry point
//         pointer to process text (code) segment
//         pointer to process data segment
//         number of bytes allocated for its stack
//         priority (0 is highest)
// Outputs: 1 if successful, 0 if this process can not be added
// This function will be needed for Lab 5
// In Labs 2-4, this function can be ignored
PCBType PCB_pool[16];
int processNum = 0;

int OS_AddProcess(void(*entry)(void), void *text, void *data, 
  unsigned long stackSize, unsigned long priority){
  // put Lab 5 solution here
	
	PCBType * pcb = &PCB_pool[processNum];
	processNum ++;
		
	pcb->code = text;
	pcb->data = data;
	pcb->id = ++PCBNum;
		
	// no process 
	if(currentPCB == NULL){
		currentPCB = pcb;
		pcb->next = pcb;
		pcb->pre = pcb;
	}else{
		//at least 1 process
		PCBType * pre = currentPCB->pre;
		PCBType * next = currentPCB->next;
		
		pre->next = pcb;
		pcb->pre = pre;
		pcb->next = next;
		next->pre = pcb;
	}

  // this thread belongs to this process
	OS_AddThread(entry, stackSize, priority);
	NumCreated++;
  return 1; // replace this line with Lab 5 solution
}


//******** OS_Id *************** 
// returns the thread ID for the currently running thread
// Inputs: none
// Outputs: Thread ID, number greater than zero 
uint32_t OS_Id(void){
  // put Lab 2 (and beyond) solution here
  
  return 71; // replace this line with solution
};

//******** Timer4 Interrupt *************** 
//used for add period background thread
void Timer4A_Handler(void){
  TIMER4_ICR_R = TIMER_ICR_TATOCINT;// acknowledge timer4A timeout
  (*PeriodicTask4)();                // execute user task
}
void Timer3A_Handler(void){
  TIMER3_ICR_R = TIMER_ICR_TATOCINT;// acknowledge TIMER3A timeout
  (*PeriodicTask3)();                // execute user task
}
//******** OS_AddPeriodicThread *************** 
// add a background periodic task
// typically this function receives the highest priority
// Inputs: pointer to a void/void background function
//         period given in system time units (12.5ns)
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// You are free to select the time resolution for this function
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal   OS_AddThread
// This task does not have a Thread ID
// In lab 1, this command will be called 1 time
// In lab 2, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, this command will be called 0 1 or 2 times
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddPeriodicThread(void(*task)(void), 
   uint32_t period, uint32_t priority){
  if(BackCreated == 0){
    long temp = StartCritical();
		
    SYSCTL_RCGCTIMER_R |= 0x10;      // 0) activate timer4
    PeriodicTask4 = task;            // user function (this line also allows time to finish activating)
    TIMER4_CTL_R &= 0x00000000;     // 1) disable timer4A during setup
    TIMER4_CFG_R = 0x00000000;       // 2) configure for 32-bit timer mode
    TIMER4_TAMR_R = 0x00000002;      // 3) configure for periodic mode, default down-count settings
    TIMER4_TAILR_R = period-1;       // 4) reload value
    TIMER4_TAPR_R = 0;               // 5) 12.5ns timer4A
    TIMER4_ICR_R = 0x00000001;       // 6) clear timer4A timeout flag
    TIMER4_IMR_R = 0x00000001;      // 7) arm timeout interrupt
    NVIC_PRI17_R = (NVIC_PRI17_R&0xFF00FFFF)|(priority<<21); 
    NVIC_EN2_R = 0x0000040;     // 9) enable interrupt 70 in NVIC
    TIMER4_CTL_R = 0x00000001;      // 10) enable timer4A
	  
		EndCritical(temp);
		BackCreated++;
    return 1; // replace this line with solution
	}
	if(BackCreated == 1){
    long temp = StartCritical();
		
    SYSCTL_RCGCTIMER_R |= 0x08;   // 0) activate TIMER3
    PeriodicTask3 = task;         // user function
    TIMER3_CTL_R = 0x00000000;    // 1) disable TIMER3A during setup
    TIMER3_CFG_R = 0x00000000;    // 2) configure for 32-bit mode
    TIMER3_TAMR_R = 0x00000002;   // 3) configure for periodic mode, default down-count settings
    TIMER3_TAILR_R = period-1;    // 4) reload value
    TIMER3_TAPR_R = 0;            // 5) bus clock resolution
    TIMER3_ICR_R = 0x00000001;    // 6) clear TIMER3A timeout flag
    TIMER3_IMR_R = 0x00000001;    // 7) arm timeout interrupt
    NVIC_PRI8_R = (NVIC_PRI8_R&0x00FFFFFF)|(priority<<29); // priority  
                                                           // interrupts enabled in the main program after all devices initialized
                                                           // vector number 51, interrupt number 35
    NVIC_EN1_R = 1<<(35-32);      // 9) enable IRQ 35 in NVIC
    TIMER3_CTL_R = 0x00000001;    // 10) enable TIMER3A
		
	  EndCritical(temp);
		BackCreated++;
    return 1; // replace this line with solution
	}
};


/*----------------------------------------------------------------------------
  PF1 Interrupt Handler
 *----------------------------------------------------------------------------*/
void GPIOPortF_Handler(void){
  if(GPIO_PORTF_RIS_R&0x10){  // poll PF4
    GPIO_PORTF_ICR_R = 0x10;  // acknowledge flag4
    (*SW_task1)();            // validate SW task1
  }
  if(GPIO_PORTF_RIS_R&0x01){  // poll PF0
    GPIO_PORTF_ICR_R = 0x01;  // acknowledge flag4
    (*SW_task2)();            // validate SW task2
  }
}

//******** OS_AddSW1Task *************** 
// add a background task to run whenever the SW1 (PF4) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal   OS_AddThread
// This task does not have a Thread ID
// In labs 2 and 3, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddSW1Task(void(*task)(void), uint32_t priority){
  // put Lab 2 (and beyond) solution here
	long temp = StartCritical();
  SYSCTL_RCGCGPIO_R |= 0x00000020; // (a) activate clock for port F
  SW_task1 = task ;             // (b) initialize task
  GPIO_PORTF_DIR_R &= ~0x10;    // (c) make PF4 in (built-in button)
  GPIO_PORTF_AFSEL_R &= ~0x10;  //     disable alt funct on PF4
  GPIO_PORTF_DEN_R |= 0x10;     //     enable digital I/O on PF4   
  GPIO_PORTF_PCTL_R &= ~0x000F0000; // configure PF4 as GPIO
  GPIO_PORTF_AMSEL_R = 0;       //     disable analog functionality on PF
  GPIO_PORTF_PUR_R |= 0x10;     //     enable weak pull-up on PF4
  GPIO_PORTF_IS_R &= ~0x10;     // (d) PF4 is edge-sensitive
  GPIO_PORTF_IBE_R &= ~0x10;    //     PF4 is not both edges
  GPIO_PORTF_IEV_R &= ~0x10;    //     PF4 falling edge event
  GPIO_PORTF_ICR_R = 0x10;      // (e) clear flag4
  GPIO_PORTF_IM_R |= 0x10;      // (f) arm interrupt on PF4 *** No IME bit as mentioned in Book ***
  NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|(priority<<21); // (g) priority set
  NVIC_EN0_R = 0x40000000;      // (h) enable interrupt 30 in NVIC
	EndCritical(temp);
  return 1; // replace this line with solution
};

//******** OS_AddSW2Task *************** 
// add a background task to run whenever the SW2 (PF0) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is highest, 5 is lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed user task will run to completion and return
// This task can not spin block loop sleep or kill
// This task can call issue OS_Signal, it can call OS_AddThread
// This task does not have a Thread ID
// In lab 2, this function can be ignored
// In lab 3, this command will be called will be called 0 or 1 times
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddSW2Task(void(*task)(void), uint32_t priority){
  // put Lab 2 (and beyond) solution here
	long temp = StartCritical();
  SYSCTL_RCGCGPIO_R |= 0x00000020; // (a) activate clock for port F
	GPIO_PORTF_LOCK_R = 0x4C4F434B;   // 2) unlock GPIO Port F
  GPIO_PORTF_CR_R = 0x1F;           // allow changes to PF4-0
  SW_task2 = task ;             // (b) initialize task
  GPIO_PORTF_DIR_R &= ~0x01;    // (c) make PF4 in (built-in button)
  GPIO_PORTF_AFSEL_R &= ~0x01;  //     disable alt funct on PF4
  GPIO_PORTF_DEN_R |= 0x01;     //     enable digital I/O on PF4   
  GPIO_PORTF_PCTL_R &= ~0x0000000F; // configure PF4 as GPIO
  GPIO_PORTF_AMSEL_R = 0;       //     disable analog functionality on PF
  GPIO_PORTF_PUR_R |= 0x01;     //     enable weak pull-up on PF4
  GPIO_PORTF_IS_R &= ~0x01;     // (d) PF4 is edge-sensitive
  GPIO_PORTF_IBE_R &= ~0x01;    //     PF4 is not both edges
  GPIO_PORTF_IEV_R &= ~0x01;    //     PF4 falling edge event
  GPIO_PORTF_ICR_R = 0x01;      // (e) clear flag4
  GPIO_PORTF_IM_R |= 0x01;      // (f) arm interrupt on PF4 *** No IME bit as mentioned in Book ***
  NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|(priority<<21); // (g) priority set
  NVIC_EN0_R = 0x40000000;      // (h) enable interrupt 30 in NVIC
	EndCritical(temp);
  return 1; // replace this line with solution  
};


// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// You are free to select the time resolution for this function
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(uint32_t sleepTime){
  // put Lab 2 (and beyond) solution here
	long temp = StartCritical();
  RunPt->Sleep = sleepTime;
	SchPt = RunPt->Next;
	RemovefromReady(RunPt);
	OS_Suspend();
	EndCritical(temp);
};  

// ******** OS_Kill ************
// kill the currently running thread, release its TCB and stack
// input:  none
// output: none
void OS_Kill(void){
  // put Lab 2 (and beyond) solution here
	DisableInterrupts();
	RunPt->Kill = 1;
	SchPt = RunPt->Next;
	RemovefromReady(RunPt);
	OS_Suspend();
	NumCreated --;
	
	// deal with PCB
	// if there is no thread under this PCB, release the heap
	PCBType * PCBPtr = RunPt->PCBPtr;
	if(PCBPtr != NULL){
		int num = -- PCBPtr->threadNum;
		if(num == 0){			
			Heap_Free(PCBPtr->data);
			Heap_Free(PCBPtr->code);

			
			// only 1 process left
			if(currentPCB->next == currentPCB){
				currentPCB = NULL;
				processNum--;
			}else{
				// remove this PCB
				if(currentPCB == PCBPtr){
					currentPCB = PCBPtr -> next;
				}
				PCBPtr->pre->next = PCBPtr->next;
				PCBPtr->next->pre = PCBPtr->pre;
				processNum--;
			}
		}
	}
	
  EnableInterrupts();   // end of atomic section 
//  for(;;){};        // can not return
    
}; 

void AddtoReady(TCBtype *Pt){
	if (ReadyCreated == 0){
		Ready_pool[0] = Pt;
		Ready_pool[0]->Next = Ready_pool[0];
		ReadyCreated++;
	}
	else{
		for (int i=0;i<ReadyCreated;i++){
			if ( Ready_pool[i]->Priority > Pt->Priority){
				for (int j=ReadyCreated;j>i;j--){
					Ready_pool[j] = Ready_pool[j-1];
				}
				Ready_pool[i] = Pt;
				break;
			}
			if( i == (ReadyCreated-1) ){
				Ready_pool[ReadyCreated] = Pt;
			}
		}
		for (int i=0;i<ReadyCreated;i++){
		 Ready_pool[i]->Next = Ready_pool[i+1];
		 Ready_pool[i]->Id = i;
		}
		Ready_pool[ReadyCreated]->Next = Ready_pool[0];
		Ready_pool[ReadyCreated]->Id = ReadyCreated;
		ReadyCreated++;
	}
};

void RemovefromReady(TCBtype *Pt){
	for (int i=Pt->Id;i<(ReadyCreated-1);i++){
		Ready_pool[i]=Ready_pool[i+1];
		Ready_pool[i]->Id = i;
	}
	ReadyCreated --;
	Ready_pool[ReadyCreated] = NULL;
	if ( Pt->Id != 0)
		Ready_pool[Pt->Id-1]->Next = Ready_pool[Pt->Id];
	Ready_pool[ReadyCreated-1]->Next = Ready_pool[0];
		
};

// ******** OS_Suspend ************
// suspend execution of currently running thread
// scheduler will choose another thread to execute
// Can be used to implement cooperative multitasking 
// Same function as OS_Sleep(0)
// input:  none
// output: none
void OS_Suspend(void){
  // put Lab 2 (and beyond) solution here
	if( SchPt->Priority > RunPt->Priority || Ready_pool[0]->Priority < RunPt->Priority )
		SchPt = Ready_pool[0];
	else
		;
  ContextSwitch();
};
  
// ******** OS_Fifo_Init ************
// Initialize the Fifo to be empty
// Inputs: size
// Outputs: none 
// In Lab 2, you can ignore the size field
// In Lab 3, you should implement the user-defined fifo size
// In Lab 3, you can put whatever restrictions you want on size
//    e.g., 4 to 64 elements
//    e.g., must be a power of 2,4,8,16,32,64,128
void OS_Fifo_Init(uint32_t size){
  // put Lab 2 (and beyond) solution here
	int i;
	for (i=size;i>=1;i--){
		OS_FIFO.queue[size-1] = 15;
	}
  OS_FIFO.RoomRemain.Value = size;
	OS_FIFO.DataAvailable.Value = 0;
	OS_FIFO.max = size;
};

// ******** OS_Fifo_Put ************
// Enter one data sample into the Fifo
// Called from the background, so no waiting 
// Inputs:  data
// Outputs: true if data is properly saved,
//          false if data not saved, because it was full
// Since this is called by interrupt handlers 
//  this function can not disable or enable interrupts
int OS_Fifo_Put(uint32_t data){
  // put Lab 2 (and beyond) solution here
	if (OS_FIFO.RoomRemain.Value <= 0){
		return 0;
	}
	OS_Wait(&OS_FIFO.RoomRemain);
	DisableInterrupts();
	OS_FIFO.queue[OS_FIFO.RoomRemain.Value] = data;
	EnableInterrupts();
	OS_Signal(&OS_FIFO.DataAvailable);
	return 1; // replace this line with solution
};  

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data 
uint32_t OS_Fifo_Get(void){
  // put Lab 2 (and beyond) solution here
	int data_temp;int i;
	OS_Wait(&OS_FIFO.DataAvailable);
	DisableInterrupts();
	data_temp = OS_FIFO.queue[OS_FIFO.max-1];
	for (i=OS_FIFO.max;i>OS_FIFO.RoomRemain.Value;i--){
		OS_FIFO.queue[i-1] = OS_FIFO.queue[i-2];
	}
	EnableInterrupts();
	OS_Signal(&OS_FIFO.RoomRemain);

  return data_temp; // replace this line with solution
};

// ******** OS_Fifo_Size ************
// Check the status of the Fifo
// Inputs: none
// Outputs: returns the number of elements in the Fifo
//          greater than zero if a call to OS_Fifo_Get will return right away
//          zero or less than zero if the Fifo is empty 
//          zero or less than zero if a call to OS_Fifo_Get will spin or block
int32_t OS_Fifo_Size(void){
  // put Lab 2 (and beyond) solution here
   
  return 0; // replace this line with solution
};


// ******** OS_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
void OS_MailBox_Init(void){
  // put Lab 2 (and beyond) solution here
  OS_MailBox.data = 0;
	OS_MailBox.BoxFree.Value = 1;
	OS_MailBox.DataValid.Value = 0;
  // put solution here
};

// ******** OS_MailBox_Send ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received 
void OS_MailBox_Send(uint32_t data){
  // put Lab 2 (and beyond) solution here
  // put solution here
	OS_Wait(&OS_MailBox.BoxFree);
	OS_MailBox.data = data;
	OS_Signal(&OS_MailBox.DataValid);
};

// ******** OS_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty 
uint32_t OS_MailBox_Recv(void){
  // put Lab 2 (and beyond) solution here
	int data_temp;
	OS_Wait(&OS_MailBox.DataValid);
	data_temp = OS_MailBox.data;
	OS_Signal(&OS_MailBox.BoxFree);
  return data_temp; // replace this line with solution
};

// ******** OS_Time ************
// return the system time 
// Inputs:  none
// Outputs: time in 12.5ns units, 0 to 4294967295
// The time resolution should be less than or equal to 1us, and the precision 32 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_TimeDifference have the same resolution and precision 
uint32_t OS_Time(void){
  // put Lab 2 (and beyond) solution here
	uint32_t OStime_us = (80000000 - 1 - TIMER5_TAR_R)/80 + 1000000 * OStime_Task1;
  return OStime_us; // replace this line with solution
};

// ******** OS_TimeDifference ************
// Calculates difference between two times
// Inputs:  two times measured with OS_Time
// Outputs: time difference in 12.5ns units 
// The time resolution should be less than or equal to 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_Time have the same resolution and precision 
uint32_t OS_TimeDifference(uint32_t start, uint32_t stop){
  // put Lab 2 (and beyond) solution here
  uint32_t diff_ns = (stop - start)*80 ;   //time in 12.5ns units, 0 to 4294967295
  return diff_ns; // output difference in 12.5ns units
                  // replace this line with solution
};


// ******** OS_ClearMsTime ************
// sets the system time to zero (solve for Lab 1), and start a periodic interrupt
// Inputs:  none
// Outputs: none
// You are free to change how this works
void OS_ClearMsTime(void){
  // put Lab 1 solution here
	NVIC_DIS2_R = 1<<28;          			 // 1) disable interrupt 92 in NVIC
  TIMER5_CTL_R = 0x00000000;    			 // 2) disable timer5A
	OStime_round = 0;										 // 3) clear the variable for time record
	OStime_ms = 0;
  TIMER5_TAILR_R = 80000000/1-1;    	// 4) reload value, period = 1Hz
  TIMER5_ICR_R = 0x00000001;    			 // 5) clear TIMER5A timeout flag
  TIMER5_IMR_R = 0x00000001;    			 // 6) arm timeout interrupt
	// vector number 108, interrupt number 92
  NVIC_EN2_R = 1<<28;          			 	 // 7) enable IRQ 92 in NVIC
  TIMER5_CTL_R = 0x00000001;    			 // 8) enable TIMER5A
};

// ******** OS_MsTime ************
// reads the current time in msec (solve for Lab 1)
// Inputs:  none
// Outputs: time in ms units
// You are free to select the time resolution for this function
// For Labs 2 and beyond, it is ok to make the resolution to match the first call to OS_AddPeriodicThread
uint32_t OS_MsTime(void){
  // put Lab 1 solution here
	
  OStime_ms = (80000000 - 1 - TIMER5_TAR_R) / 80000 + 1000 * OStime_round;				// counter * 12.5 / 10^6 = counter / 80000 ; 1 round = 1 (s) = 1000 (ms)
  
  return OStime_ms; // replace this line with solution
};


//******** OS_Launch *************** 
// start the scheduler, enable interrupts
// Inputs: number of 12.5ns clock cycles for each time slice
//         you may select the units of this parameter
// Outputs: none (does not return)
// In Lab 2, you can ignore the theTimeSlice field
// In Lab 3, you should implement the user-defined TimeSlice field
// It is ok to limit the range of theTimeSlice to match the 24-bit SysTick
void OS_Launch(uint32_t theTimeSlice){
  // put Lab 2 (and beyond) solution here
	SysTick_Init(theTimeSlice);
	RunPt = Ready_pool[0];
	StartOS(RunPt->SavedSP);
};

//************** I/O Redirection *************** 
// redirect terminal I/O to UART or file (Lab 4)

int StreamToDevice=0;                // 0=UART, 1=stream to file (Lab 4)

int fputc (int ch, FILE *f) { 
  if(StreamToDevice==1){  // Lab 4
    if(eFile_Write(ch)){          // close file on error
       OS_EndRedirectToFile(); // cannot write to file
       return 1;                  // failure
    }
    return 0; // success writing
  }
  
  // default UART output
  UART_OutChar(ch);
  return ch; 
}

int fgetc (FILE *f){
  char ch = UART_InChar();  // receive from keyboard
  UART_OutChar(ch);         // echo
  return ch;
}

int OS_RedirectToFile(const char *name){  // Lab 4
  eFile_Create(name);              // ignore error if file already exists
  if(eFile_WOpen(name)) return 1;  // cannot open file
  StreamToDevice = 1;
  return 0;
}

int OS_EndRedirectToFile(void){  // Lab 4
  StreamToDevice = 0;
  if(eFile_WClose()) return 1;    // cannot close file
  return 0;
}

int OS_RedirectToUART(void){
  StreamToDevice = 0;
  return 0;
}

int OS_RedirectToST7735(void){
  
  return 1;
}




//// *************os.c**************
//// EE445M/EE380L.6 Labs 1, 2, 3, and 4 
//// High-level OS functions
//// Students will implement these functions as part of Lab
//// Runs on LM4F120/TM4C123
//// Jonathan W. Valvano 
//// Jan 12, 2020, valvano@mail.utexas.edu


//#include <stdint.h>
//#include <stdio.h>
//#include "../inc/tm4c123gh6pm.h"
//#include "../inc/CortexM.h"
//#include "../inc/PLL.h"
//#include "../inc/LaunchPad.h"
//#include "../inc/Timer4A.h"
//#include "../inc/WTimer0A.h"
//#include "../RTOS_Labs_common/OS.h"
//#include "../RTOS_Labs_common/ST7735.h"
//#include "../inc/ADCT0ATrigger.h"
//#include "../RTOS_Labs_common/UART0int.h"
//#include "../RTOS_Labs_common/eFile.h"
//#include "../inc/Timer5A.h"

//#define PD3                    (*((volatile uint32_t *)0x40007020))
//#define NVIC_SYSPRI14_R        (*((volatile uint32_t *)0xE000ED22))  //PendSV priority register (position 14).
//#define NVIC_SYSPRI15_R        (*((volatile uint32_t *)0xE000ED23))  //Systick priority register (position 15).
//#define NVIC_LEVEL14               0xEF                              //Systick priority value (second lowest).
//#define NVIC_LEVEL15               0xFF                              //PendSV priority value (lowest).
//#define NVIC_PENDSVSET             0x10000000                        //Value to trigger PendSV exception.

//// Performance Measurements 
//int32_t MaxJitter;             // largest time jitter between interrupts in usec
//#define JITTERSIZE 64
//uint32_t const JitterSize=JITTERSIZE;
//uint32_t JitterHistogram[JITTERSIZE]={0,};

//#define MAX_TCB_SIZE 13
//#define MAX_STACK_SIZE 128
//#define MAX_FIFO_SIZE 64

//uint32_t OStime_round;												//variables defined for Lab 1, used to count rounds for timer5A,can be reset
//uint32_t OStime_Task1;                        //variables defined for Lab 1, used to count Jitters,cannot be reset
//uint32_t OStime_ms;														//variables defined for Lab 1, used to remember time in ms

//Sema4Type LCDFree;
//Sema4Type TxRoomLeft;
//Sema4Type RxDataAvailable;

//void(*SW_task)(void);				//hook for SW interrupt task
//void(*PeriodicTask4)(void);	//hook for TIMER0, used for add period task

///*	FIFO variables	*/
//struct FIFO{
//	long queue[MAX_FIFO_SIZE];
//	Sema4Type RoomRemain;
//	Sema4Type DataAvailable;
//	long max;
//};
//struct FIFO OS_FIFO;

///*	FIFO variables	*/
//struct MailBox{
//	long data;
//	Sema4Type BoxFree;
//	Sema4Type DataValid;
//};
//struct MailBox OS_MailBox;

///*	TCB variables	*/
//struct TCB{
//	long *SavedSP;
//	struct TCB *Next;
//	struct TCB *Previous;
//	int32_t Id;
//	uint32_t Sleep;
//	uint32_t Priority;
//	uint32_t Block;
//};
//typedef struct TCB TCBtype;

//TCBtype TCB_pool[MAX_TCB_SIZE];
//TCBtype *RunPt;
//TCBtype *SchPt;
//long STACK_pool[MAX_TCB_SIZE][MAX_STACK_SIZE];

///*------------------------------------------------------------------------------
//  Systick Interrupt Handler
//  SysTick interrupt happens every 2 ms
//  used for preemptive thread switch
// *------------------------------------------------------------------------------*/
//void SysTick_Handler(void) {
//	SchPt = RunPt->Next;
//	while(SchPt->Sleep != 0){
//		SchPt = SchPt->Next;
//	}
//	NVIC_INT_CTRL_R = NVIC_PENDSVSET; //NVIC_PENDSVSET = 0x10000000
//  
//} // end SysTick_Handler

//unsigned long OS_LockScheduler(void){
//  // lab 4 might need this for disk formating
//  return 0;// replace with solution
//}
//void OS_UnLockScheduler(unsigned long previous){
//  // lab 4 might need this for disk formating
//}


//void SysTick_Init(unsigned long period){
//  NVIC_ST_CTRL_R = 0;                   // disable SysTick during setup
//  NVIC_ST_RELOAD_R = period-1;  				// maximum reload value
//  NVIC_ST_CURRENT_R = 0;                // any write to current clears it
//                                        // enable SysTick with core clock
//	NVIC_SYSPRI15_R = NVIC_LEVEL14;       // second lowest priorty 0xEF
//	NVIC_SYSPRI14_R = NVIC_LEVEL15;    		// Set PengSV priority 0xFF
//  NVIC_ST_CTRL_R = NVIC_ST_CTRL_ENABLE+NVIC_ST_CTRL_CLK_SRC+NVIC_ST_CTRL_INTEN;
//}

///**
// * @details  Initialize operating system, disable interrupts until OS_Launch.
// * Initialize OS controlled I/O: serial, ADC, systick, LaunchPad I/O and timers.
// * Interrupts not yet enabled.
// * @param  none
// * @return none
// * @brief  Initialize OS
// */

//void WDTimer0A_task(){
//	long temp = StartCritical();
//	for (int i = NumCreated;i>=0;i--){
//		if (TCB_pool[i].Sleep>0)
//			TCB_pool[i].Sleep--;
//	}
//	EndCritical(temp);
//}
//void OSTtask(void){  // runs at 1Hz in background
//  OStime_round += 1;
//	OStime_Task1 += 1;
//}

//void OS_Init(void){
//  // put Lab 2 (and beyond) solution here
//	PLL_Init(Bus80MHz);
//	UART_Init();
//	ST7735_InitR(INITR_REDTAB); // LCD initialization
//	ST7735_Message(0, 0, "TOP default:", 12345);
//	ST7735_Message(1, 0, "DOWN default:", 42);
//	ST7735_Message(1, 1, "LCDFree:", LCDFree.Value);
//	
//	WideTimer0A_Init(&WDTimer0A_task,80000000/1000,1);
//	Timer5A_Init(&OSTtask,80000000/1,2); // 1 Hz sampling, priority=2, used for OS time recording in ms
//	
//	OS_InitSemaphore(&LCDFree, 1);
//	OS_InitSemaphore(&TxRoomLeft, 1024);
//	OS_InitSemaphore(&RxDataAvailable, 0);
//}; 


//// ******** OS_InitSemaphore ************
//// initialize semaphore 
//// input:  pointer to a semaphore
//// output: none
//void OS_InitSemaphore(Sema4Type *semaPt, int32_t value){
//  // put Lab 2 (and beyond) solution here
//	semaPt->Value = value;
//}; 

//// ******** OS_Wait ************
//// decrement semaphore 
//// Lab2 spinlock
//// Lab3 block if less than zero
//// input:  pointer to a counting semaphore
//// output: none
//void OS_Wait(Sema4Type *semaPt){
//  // put Lab 2 (and beyond) solution here
//  DisableInterrupts();
//	while(semaPt->Value <= 0){
//		EnableInterrupts();
//		DisableInterrupts();
//	}
//	semaPt->Value--;
//	EnableInterrupts();
//}; 

//// ******** OS_Signal ************
//// increment semaphore 
//// Lab2 spinlock
//// Lab3 wakeup blocked thread if appropriate 
//// input:  pointer to a counting semaphore
//// output: none
//void OS_Signal(Sema4Type *semaPt){
//  // put Lab 2 (and beyond) solution here
//	long temp = StartCritical();
//	semaPt->Value++;
//	EndCritical(temp);
//}; 

//// ******** OS_bWait ************
//// Lab2 spinlock, set to 0
//// Lab3 block if less than zero
//// input:  pointer to a binary semaphore
//// output: none
//void OS_bWait(Sema4Type *semaPt){
//  // put Lab 2 (and beyond) solution here
//	DisableInterrupts();
//	while(semaPt->Value == 0){
//		EnableInterrupts();
//		DisableInterrupts();
//	}
//	semaPt->Value = 0;
//	EnableInterrupts();
//}; 

//// ******** OS_bSignal ************
//// Lab2 spinlock, set to 1
//// Lab3 wakeup blocked thread if appropriate 
//// input:  pointer to a binary semaphore
//// output: none
//void OS_bSignal(Sema4Type *semaPt){
//  // put Lab 2 (and beyond) solution here
//	semaPt->Value = 1;
//}; 



////******** OS_AddThread *************** 
//// add a foregound thread to the scheduler
//// Inputs: pointer to a void/void foreground task
////         number of bytes allocated for its stack
////         priority, 0 is highest, 5 is the lowest
//// Outputs: 1 if successful, 0 if this thread can not be added
//// stack size must be divisable by 8 (aligned to double word boundary)
//// In Lab 2, you can ignore both the stackSize and priority fields
//// In Lab 3, you can ignore the stackSize fields
//int OS_AddThread(void(*task)(void), 
//   uint32_t stackSize, uint32_t priority){
//  // put Lab 2 (and beyond) solution here
//		 if (NumCreated < MAX_TCB_SIZE){

//			 TCB_pool[NumCreated].SavedSP = &STACK_pool[NumCreated][MAX_STACK_SIZE-1];			//initiate SP information in TCB
//			 
//			 *(TCB_pool[NumCreated].SavedSP) = 0x01000000L;
//			 *(--TCB_pool[NumCreated].SavedSP) = (long) task;				//initiate stack
//			 *(--TCB_pool[NumCreated].SavedSP) = 0x13131313L;
//			 *(--TCB_pool[NumCreated].SavedSP) = 0x12121212L;
//			 *(--TCB_pool[NumCreated].SavedSP) = 0x11111111L;
//			 *(--TCB_pool[NumCreated].SavedSP) = 0x10101010L;
//			 *(--TCB_pool[NumCreated].SavedSP) = 0x09090909L;
//			 *(--TCB_pool[NumCreated].SavedSP) = 0x08080808L;
//			 *(--TCB_pool[NumCreated].SavedSP) = 0x07070707L;
//			 *(--TCB_pool[NumCreated].SavedSP) = 0x06060606L;
//			 *(--TCB_pool[NumCreated].SavedSP) = 0x05050505L;
//			 *(--TCB_pool[NumCreated].SavedSP) = 0x04040404L;
//			 *(--TCB_pool[NumCreated].SavedSP) = 0x03030303L;
//			 *(--TCB_pool[NumCreated].SavedSP) = 0x02020202L;
//			 *(--TCB_pool[NumCreated].SavedSP) = 0x01010101L;
//			 *(--TCB_pool[NumCreated].SavedSP) = 0x00000000L;
//			 
//			 long temp = StartCritical();
//			 if (NumCreated == 0){
//				 TCB_pool[NumCreated].Next = &TCB_pool[NumCreated];
//				 TCB_pool[NumCreated].Previous = &TCB_pool[NumCreated];
//			 }
//			 else{
//				 TCB_pool[NumCreated].Next = &TCB_pool[0];
//				 TCB_pool[NumCreated].Previous = &TCB_pool[NumCreated - 1];
//				 TCB_pool[NumCreated].Previous->Next = &TCB_pool[NumCreated];
//				 TCB_pool[0].Previous = &TCB_pool[NumCreated];
//			 }
//			 EndCritical(temp);
//			 return 1;
///*				 
//			 switch (NumCreated){
//				 case 0:{
//					 TCB_pool[NumCreated].Previous = &TCB_pool[2];
//					 TCB_pool[NumCreated].Next = &TCB_pool[1];
//					 return 1;
//				 }
//				 case 1:{
//					 TCB_pool[NumCreated].Previous = &TCB_pool[0];
//					 TCB_pool[NumCreated].Next = &TCB_pool[2];
//					 return 1;
//				 }
//				 case 2:{
//					 TCB_pool[NumCreated].Previous = &TCB_pool[1];
//					 TCB_pool[NumCreated].Next = &TCB_pool[0];
//					 return 1;
//				 }
//				 default:
//					 return 0;
//			 };
//*/		 
//		 }
//		 else
//		 return 0;

//};

////******** OS_AddProcess *************** 
//// add a process with foregound thread to the scheduler
//// Inputs: pointer to a void/void entry point
////         pointer to process text (code) segment
////         pointer to process data segment
////         number of bytes allocated for its stack
////         priority (0 is highest)
//// Outputs: 1 if successful, 0 if this process can not be added
//// This function will be needed for Lab 5
//// In Labs 2-4, this function can be ignored
//int OS_AddProcess(void(*entry)(void), void *text, void *data, 
//  unsigned long stackSize, unsigned long priority){
//  // put Lab 5 solution here

//     
//  return 0; // replace this line with Lab 5 solution
//}


////******** OS_Id *************** 
//// returns the thread ID for the currently running thread
//// Inputs: none
//// Outputs: Thread ID, number greater than zero 
//uint32_t OS_Id(void){
//  // put Lab 2 (and beyond) solution here
//  
//  return 0; // replace this line with solution
//};

////******** Timer0 Interrupt *************** 
////used for add period background thread
//void Timer4A_Handler(void){
//  TIMER4_ICR_R = TIMER_ICR_TATOCINT;// acknowledge timer4A timeout
//  (*PeriodicTask4)();                // execute user task
//}
////******** OS_AddPeriodicThread *************** 
//// add a background periodic task
//// typically this function receives the highest priority
//// Inputs: pointer to a void/void background function
////         period given in system time units (12.5ns)
////         priority 0 is the highest, 5 is the lowest
//// Outputs: 1 if successful, 0 if this thread can not be added
//// You are free to select the time resolution for this function
//// It is assumed that the user task will run to completion and return
//// This task can not spin, block, loop, sleep, or kill
//// This task can call OS_Signal  OS_bSignal   OS_AddThread
//// This task does not have a Thread ID
//// In lab 1, this command will be called 1 time
//// In lab 2, this command will be called 0 or 1 times
//// In lab 2, the priority field can be ignored
//// In lab 3, this command will be called 0 1 or 2 times
//// In lab 3, there will be up to four background threads, and this priority field 
////           determines the relative priority of these four threads
//int OS_AddPeriodicThread(void(*task)(void), 
//   uint32_t period, uint32_t priority){
//  SYSCTL_RCGCTIMER_R |= 0x10;      // 0) activate timer4
//  PeriodicTask4 = task;            // user function (this line also allows time to finish activating)
//  TIMER4_CTL_R &= 0x00000000;     // 1) disable timer4A during setup
//  TIMER4_CFG_R = 0x00000000;       // 2) configure for 32-bit timer mode
//  TIMER4_TAMR_R = 0x00000002;      // 3) configure for periodic mode, default down-count settings
//  TIMER4_TAILR_R = period-1;       // 4) reload value
//  TIMER4_TAPR_R = 0;               // 5) 12.5ns timer4A
//  TIMER4_ICR_R = 0x00000001;       // 6) clear timer4A timeout flag
//  TIMER4_IMR_R = 0x00000001;      // 7) arm timeout interrupt
//  NVIC_PRI17_R = (NVIC_PRI17_R&0xFF00FFFF)|(priority<<21); 
//  NVIC_EN2_R = 0x0000040;     // 9) enable interrupt 70 in NVIC
//  TIMER4_CTL_R = 0x00000001;      // 10) enable timer4A
//  return 1; // replace this line with solution
//};


///*----------------------------------------------------------------------------
//  PF1 Interrupt Handler
// *----------------------------------------------------------------------------*/
//void GPIOPortF_Handler(void){
//  GPIO_PORTF_ICR_R = 0x10;      // acknowledge PF4 edge interrupt
//	(*SW_task)();									// validate SW task
//}

////******** OS_AddSW1Task *************** 
//// add a background task to run whenever the SW1 (PF4) button is pushed
//// Inputs: pointer to a void/void background function
////         priority 0 is the highest, 5 is the lowest
//// Outputs: 1 if successful, 0 if this thread can not be added
//// It is assumed that the user task will run to completion and return
//// This task can not spin, block, loop, sleep, or kill
//// This task can call OS_Signal  OS_bSignal   OS_AddThread
//// This task does not have a Thread ID
//// In labs 2 and 3, this command will be called 0 or 1 times
//// In lab 2, the priority field can be ignored
//// In lab 3, there will be up to four background threads, and this priority field 
////           determines the relative priority of these four threads
//int OS_AddSW1Task(void(*task)(void), uint32_t priority){
//  // put Lab 2 (and beyond) solution here
//  SYSCTL_RCGCGPIO_R |= 0x00000020; // (a) activate clock for port F
//  SW_task = task ;             // (b) initialize task
//  GPIO_PORTF_DIR_R &= ~0x10;    // (c) make PF4 in (built-in button)
//  GPIO_PORTF_AFSEL_R &= ~0x10;  //     disable alt funct on PF4
//  GPIO_PORTF_DEN_R |= 0x10;     //     enable digital I/O on PF4   
//  GPIO_PORTF_PCTL_R &= ~0x000F0000; // configure PF4 as GPIO
//  GPIO_PORTF_AMSEL_R = 0;       //     disable analog functionality on PF
//  GPIO_PORTF_PUR_R |= 0x10;     //     enable weak pull-up on PF4
//  GPIO_PORTF_IS_R &= ~0x10;     // (d) PF4 is edge-sensitive
//  GPIO_PORTF_IBE_R &= ~0x10;    //     PF4 is not both edges
//  GPIO_PORTF_IEV_R &= ~0x10;    //     PF4 falling edge event
//  GPIO_PORTF_ICR_R = 0x10;      // (e) clear flag4
//  GPIO_PORTF_IM_R |= 0x10;      // (f) arm interrupt on PF4 *** No IME bit as mentioned in Book ***
//  NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|0x00A00000; // (g) priority 5
//  NVIC_EN0_R = 0x40000000;      // (h) enable interrupt 30 in NVIC
//  return 0; // replace this line with solution
//};

////******** OS_AddSW2Task *************** 
//// add a background task to run whenever the SW2 (PF0) button is pushed
//// Inputs: pointer to a void/void background function
////         priority 0 is highest, 5 is lowest
//// Outputs: 1 if successful, 0 if this thread can not be added
//// It is assumed user task will run to completion and return
//// This task can not spin block loop sleep or kill
//// This task can call issue OS_Signal, it can call OS_AddThread
//// This task does not have a Thread ID
//// In lab 2, this function can be ignored
//// In lab 3, this command will be called will be called 0 or 1 times
//// In lab 3, there will be up to four background threads, and this priority field 
////           determines the relative priority of these four threads
//int OS_AddSW2Task(void(*task)(void), uint32_t priority){
//  // put Lab 2 (and beyond) solution here
//    
//  return 0; // replace this line with solution
//};


//// ******** OS_Sleep ************
//// place this thread into a dormant state
//// input:  number of msec to sleep
//// output: none
//// You are free to select the time resolution for this function
//// OS_Sleep(0) implements cooperative multitasking
//void OS_Sleep(uint32_t sleepTime){
//  // put Lab 2 (and beyond) solution here
//	long temp = StartCritical();
//  RunPt->Sleep = sleepTime;
//	SchPt = RunPt->Next;
//	while(SchPt->Sleep != 0){
//		SchPt = SchPt->Next;
//	}
//	NVIC_INT_CTRL_R = NVIC_PENDSVSET; //NVIC_PENDSVSET = 0x10000000
//	EndCritical(temp);
//};  

//// ******** OS_Kill ************
//// kill the currently running thread, release its TCB and stack
//// input:  none
//// output: none
//void OS_Kill(void){
//  // put Lab 2 (and beyond) solution here
//	DisableInterrupts();
//	RunPt->Previous->Next = RunPt->Next;
//	RunPt->Next->Previous = RunPt->Previous;
//	NumCreated --;
//	SchPt = RunPt->Next;
//	while(SchPt->Sleep != 0){
//		SchPt = SchPt->Next;
//	}
//	NVIC_INT_CTRL_R = NVIC_PENDSVSET; //NVIC_PENDSVSET = 0x10000000
//  EnableInterrupts();   // end of atomic section 
//  for(;;){};        // can not return
//    
//}; 

//// ******** OS_Suspend ************
//// suspend execution of currently running thread
//// scheduler will choose another thread to execute
//// Can be used to implement cooperative multitasking 
//// Same function as OS_Sleep(0)
//// input:  none
//// output: none
//void OS_Suspend(void){
//  // put Lab 2 (and beyond) solution here
//  ContextSwitch();
//};
//  
//// ******** OS_Fifo_Init ************
//// Initialize the Fifo to be empty
//// Inputs: size
//// Outputs: none 
//// In Lab 2, you can ignore the size field
//// In Lab 3, you should implement the user-defined fifo size
//// In Lab 3, you can put whatever restrictions you want on size
////    e.g., 4 to 64 elements
////    e.g., must be a power of 2,4,8,16,32,64,128
//void OS_Fifo_Init(uint32_t size){
//  // put Lab 2 (and beyond) solution here
//	int i;
//	for (i=size;i>=1;i--){
//		OS_FIFO.queue[size-1] = 15;
//	}
//  OS_FIFO.RoomRemain.Value = size;
//	OS_FIFO.DataAvailable.Value = 0;
//	OS_FIFO.max = size;
//};

//// ******** OS_Fifo_Put ************
//// Enter one data sample into the Fifo
//// Called from the background, so no waiting 
//// Inputs:  data
//// Outputs: true if data is properly saved,
////          false if data not saved, because it was full
//// Since this is called by interrupt handlers 
////  this function can not disable or enable interrupts
//int OS_Fifo_Put(uint32_t data){
//  // put Lab 2 (and beyond) solution here
//	if(OS_FIFO.RoomRemain.Value == 0){
//		return 0;
//	}
//	OS_Wait(&OS_FIFO.RoomRemain);
//	DisableInterrupts();
//	OS_FIFO.queue[OS_FIFO.RoomRemain.Value] = data;
//	EnableInterrupts();
//	OS_Signal(&OS_FIFO.DataAvailable);
//	return 1; // replace this line with solution
//};  

//// ******** OS_Fifo_Get ************
//// Remove one data sample from the Fifo
//// Called in foreground, will spin/block if empty
//// Inputs:  none
//// Outputs: data 
//uint32_t OS_Fifo_Get(void){
//  // put Lab 2 (and beyond) solution here
//	int data_temp;int i;
//	OS_Wait(&OS_FIFO.DataAvailable);
//	DisableInterrupts();
//	data_temp = OS_FIFO.queue[OS_FIFO.max-1];
//	for (i=OS_FIFO.max;i>OS_FIFO.RoomRemain.Value;i--){
//		OS_FIFO.queue[i-1] = OS_FIFO.queue[i-2];
//	}
//	EnableInterrupts();
//	OS_Signal(&OS_FIFO.RoomRemain);

//  return data_temp; // replace this line with solution
//};

//// ******** OS_Fifo_Size ************
//// Check the status of the Fifo
//// Inputs: none
//// Outputs: returns the number of elements in the Fifo
////          greater than zero if a call to OS_Fifo_Get will return right away
////          zero or less than zero if the Fifo is empty 
////          zero or less than zero if a call to OS_Fifo_Get will spin or block
//int32_t OS_Fifo_Size(void){
//  // put Lab 2 (and beyond) solution here
//   
//  return 0; // replace this line with solution
//};


//// ******** OS_MailBox_Init ************
//// Initialize communication channel
//// Inputs:  none
//// Outputs: none
//void OS_MailBox_Init(void){
//  // put Lab 2 (and beyond) solution here
//  OS_MailBox.data = 0;
//	OS_MailBox.BoxFree.Value = 1;
//	OS_MailBox.DataValid.Value = 0;
//  // put solution here
//};

//// ******** OS_MailBox_Send ************
//// enter mail into the MailBox
//// Inputs:  data to be sent
//// Outputs: none
//// This function will be called from a foreground thread
//// It will spin/block if the MailBox contains data not yet received 
//void OS_MailBox_Send(uint32_t data){
//  // put Lab 2 (and beyond) solution here
//  // put solution here
//	OS_bWait(&OS_MailBox.BoxFree);
//	OS_MailBox.data = data;
//	OS_bSignal(&OS_MailBox.DataValid);
//};

//// ******** OS_MailBox_Recv ************
//// remove mail from the MailBox
//// Inputs:  none
//// Outputs: data received
//// This function will be called from a foreground thread
//// It will spin/block if the MailBox is empty 
//uint32_t OS_MailBox_Recv(void){
//  // put Lab 2 (and beyond) solution here
//	int data_temp;
//	OS_bWait(&OS_MailBox.DataValid);
//	data_temp = OS_MailBox.data;
//	OS_bSignal(&OS_MailBox.BoxFree);
//  return data_temp; // replace this line with solution
//};

//// ******** OS_Time ************
//// return the system time 
//// Inputs:  none
//// Outputs: time in 12.5ns units, 0 to 4294967295
//// The time resolution should be less than or equal to 1us, and the precision 32 bits
//// It is ok to change the resolution and precision of this function as long as 
////   this function and OS_TimeDifference have the same resolution and precision 
//uint32_t OS_Time(void){
//  // put Lab 2 (and beyond) solution here
//	uint32_t OStime_us = (80000000 - 1 - TIMER5_TAR_R)/80 + 1000000 * OStime_Task1;
//  return OStime_us; // replace this line with solution
//};

//// ******** OS_TimeDifference ************
//// Calculates difference between two times
//// Inputs:  two times measured with OS_Time
//// Outputs: time difference in 12.5ns units 
//// The time resolution should be less than or equal to 1us, and the precision at least 12 bits
//// It is ok to change the resolution and precision of this function as long as 
////   this function and OS_Time have the same resolution and precision 
//uint32_t OS_TimeDifference(uint32_t start, uint32_t stop){
//  // put Lab 2 (and beyond) solution here
//  uint32_t diff_ns = (stop - start)*80 ;   //time in 12.5ns units, 0 to 4294967295
//  return diff_ns; // output difference in 12.5ns units
//                  // replace this line with solution
//};


//// ******** OS_ClearMsTime ************
//// sets the system time to zero (solve for Lab 1), and start a periodic interrupt
//// Inputs:  none
//// Outputs: none
//// You are free to change how this works
//void OS_ClearMsTime(void){
//  // put Lab 1 solution here
//	NVIC_DIS2_R = 1<<28;          			 // 1) disable interrupt 92 in NVIC
//  TIMER5_CTL_R = 0x00000000;    			 // 2) disable timer5A
//	OStime_round = 0;										 // 3) clear the variable for time record
//	OStime_ms = 0;
//  TIMER5_TAILR_R = 80000000/1-1;    	// 4) reload value, period = 1Hz
//  TIMER5_ICR_R = 0x00000001;    			 // 5) clear TIMER5A timeout flag
//  TIMER5_IMR_R = 0x00000001;    			 // 6) arm timeout interrupt
//	// vector number 108, interrupt number 92
//  NVIC_EN2_R = 1<<28;          			 	 // 7) enable IRQ 92 in NVIC
//  TIMER5_CTL_R = 0x00000001;    			 // 8) enable TIMER5A
//};

//// ******** OS_MsTime ************
//// reads the current time in msec (solve for Lab 1)
//// Inputs:  none
//// Outputs: time in ms units
//// You are free to select the time resolution for this function
//// For Labs 2 and beyond, it is ok to make the resolution to match the first call to OS_AddPeriodicThread
//uint32_t OS_MsTime(void){
//  // put Lab 1 solution here
//	
//  OStime_ms = (80000000 - 1 - TIMER5_TAR_R) / 80000 + 1000 * OStime_round;				// counter * 12.5 / 10^6 = counter / 80000 ; 1 round = 1 (s) = 1000 (ms)
//  
//  return OStime_ms; // replace this line with solution
//};


////******** OS_Launch *************** 
//// start the scheduler, enable interrupts
//// Inputs: number of 12.5ns clock cycles for each time slice
////         you may select the units of this parameter
//// Outputs: none (does not return)
//// In Lab 2, you can ignore the theTimeSlice field
//// In Lab 3, you should implement the user-defined TimeSlice field
//// It is ok to limit the range of theTimeSlice to match the 24-bit SysTick
//void OS_Launch(uint32_t theTimeSlice){
//  // put Lab 2 (and beyond) solution here
//	SysTick_Init(theTimeSlice);
//	RunPt = &TCB_pool[NumCreated-1];
//	StartOS(RunPt->SavedSP);
//};

////************** I/O Redirection *************** 
//// redirect terminal I/O to UART or file (Lab 4)

//int StreamToDevice=0;                // 0=UART, 1=stream to file (Lab 4)

//int fputc (int ch, FILE *f) { 
//  if(StreamToDevice==1){  // Lab 4
//    if(eFile_Write(ch)){          // close file on error
//       OS_EndRedirectToFile(); // cannot write to file
//       return 1;                  // failure
//    }
//    return 0; // success writing
//  }
//  
//  // default UART output
//  UART_OutChar(ch);
//  return ch; 
//}

//int fgetc (FILE *f){
//  char ch = UART_InChar();  // receive from keyboard
//  UART_OutChar(ch);         // echo
//  return ch;
//}

//int OS_RedirectToFile(const char *name){  // Lab 4
//  eFile_Create(name);              // ignore error if file already exists
//  if(eFile_WOpen(name)) return 1;  // cannot open file
//  StreamToDevice = 1;
//  return 0;
//}

//int OS_EndRedirectToFile(void){  // Lab 4
//  StreamToDevice = 0;
//  if(eFile_WClose()) return 1;    // cannot close file
//  return 0;
//}

//int OS_RedirectToUART(void){
//  StreamToDevice = 0;
//  return 0;
//}

//int OS_RedirectToST7735(void){
//  
//  return 1;
//}

