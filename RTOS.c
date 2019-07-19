#include <LPC17xx.h>
#include "RTOS.h"
#include "context.h"
#include <stdlib.h>
#include <string.h>

uint8_t numTasks = 0;

uint32_t RTOS_TICK_FREQ = 1000;
uint32_t TIME_SLICE_TICKS = 5;

uint32_t rtosTickCounter;
uint32_t nextTimeSlice;

TCB_t TCBList[MAX_NUM_TASKS + 1];
TCB_t *runningTCB;
TCB_t *readyListHead;

void addToReadyList(TCB_t *tcb){
	if(readyListHead == NULL){
		readyListHead = tcb;
	}
	else{
		TCB_t *readyTCB = readyListHead;
		while(readyTCB->next != NULL){
			readyTCB = readyTCB->next;
		}
		readyTCB->next = tcb;
	}
}

void SysTick_Handler(void) {
    rtosTickCounter++;
    if(rtosTickCounter - nextTimeSlice >= TIME_SLICE_TICKS){
    	//we are ready to move to the next task
    	nextTimeSlice += TIME_SLICE_TICKS;
    	if(readyListHead != NULL){
    		//notify PendSV_Handler we are ready to switch
				SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
    	}
    }
}

void PendSV_Handler(void){
	//Preform context switch if we are ready to switch tasks
	//software store context of current running task
	storeContext();

	//queue the current running task, pop next task
	//TODO change state of running task to ready
	addToReadyList(runningTCB);
	runningTCB = readyListHead;
	readyListHead = readyListHead->next;
	runningTCB->next = NULL;
	__set_PSP(runningTCB->stackPointer);

	//software restore context of next task
	restoreContext(runningTCB->stackPointer);
}

void rtosInit(void){
	for(uint8_t i = 0; i <= MAX_NUM_TASKS; i++){
		//initialize each TCB with their stack number and base stack adress
		TCBList[i].stackNum = i;
		TCBList[i].stackPointer = __get_MSP() + TASK_STACK_SIZE * i;
		TCBList[i].next = NULL;
	}

	//copy over main stack to first task's stack
	//TODO not sure what to do if main stack is > 1KiB
	numTasks = 1;
	memcpy((void *)TCBList[6].stackPointer, (void *)__get_MSP(), MAIN_STACK_SIZE);

	//set MSP to start of Main stack
	__set_MSP(__get_MSP());
	//set PSP to start of main task stack
	__set_PSP(TCBList[6].stackPointer);
	//set SPSEL bit (bit 1) in control register
	__set_CONTROL(__get_CONTROL() & (1 << 1));

	//set main task to running
	TCBList[5].state = RUNNING;
	runningTCB = &(TCBList[6]);

	//initialize ready list to NULL
	readyListHead = NULL;

	//set up timer variables
	rtosTickCounter = 0;
	nextTimeSlice = TIME_SLICE_TICKS;

	//Set systick interupt to fire at the time slice frequency
	SysTick_Config(SystemCoreClock/RTOS_TICK_FREQ);
}

//TODO will probably need to make this atomic
void rtosThreadNew(rtosTaskFunc_t func, void *arg){
	if(numTasks == 0){
		//rtos has not yet, return and notify somehow???
		return;
	}
	if(numTasks -1 == MAX_NUM_TASKS){
		//Max number of tasks reached, return and notify somehow???
		return;
	}

	//Get next task block
	TCB_t *newTCB = &(TCBList[numTasks - 1]);

	//set P0 for this task's to arg
	*((uint32_t *)newTCB->stackPointer + R0_OFFSET) = (uint32_t)arg;
	//set PC to adress of the taks's function
	*((uint32_t *)newTCB->stackPointer + PC_OFFSET) = (uint32_t)func;
	//set PSR to default value (0x01000000)
	*((uint32_t *)newTCB->stackPointer + PSR_OFFSET) = PSR_DEFAULT;

	//set current task to ready and put it in the list
	newTCB->state = READY;
	addToReadyList(newTCB);

	//bump up num tasks
	numTasks++;
}

void semaphorInit(semaphore_t *sem, uint32_t val){

}

void waitOnSemaphor(semaphore_t *sem){

}

void signalSemaphor(semaphore_t *sem){

}

void mutextInit(mutex_t *mutex){

}

void waitOnMutex(mutex_t *mutex){

}

void signalMutex(mutex_t *mutex){

}
