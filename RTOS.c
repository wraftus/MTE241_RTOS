#include "RTOS.h"
#include <LPC17xx.h>
#include "context.h"

uint8_t numTasks = 0;

uint32_t RTOS_TICK_FREQ = 1000;
uint32_t TIME_SLICE_TICKS = 5;

uint32_t rtosTickCounter;
uint32_t nextTimeSlice;

TCB_t TCBList[MAX_NUM_TASKS];
TCB_t *runningTCB;
TCB_t *readyListHead;

void addToReadyList(TCB_t *tcb){
	if(readyListHead == NULL){
		readyListHead = tcb;
	}
	else{
		readyTCB* = readyListHead;
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
    	if(readyListHead != NULL){
    		//TODO store current context

    		//queue the current running task, pop next task
    		addToReadyList(runningTCB);
    		runningTCB = readyListHead;
    		readyListHead = readyListHead->next;

		    //TODO pop context of next task
    	}
    }
}

void rtosInit(void){
	for(uint8_t i = 0; i < MAX_NUM_TASKS; i++){
		//initialize each TCB with their stack number and base stack adress
		controlBlocks[i].stackNum = i;
		controlBlocks[i].baseStackAddress = __initial_sp - TASK_STACK_SIZE * (MAX_NUM_TASKS - i);
		TCBList[i].next = NULL;
	}

	//copy over main stack to first task's stack
	//TODO not sure what to do if main stack is > 1KiB
	numTasks = 1;
	memcpy(TCBList[0].stackPointer, __initial_sp, TASK_STACK_SIZE);

	//set MSP to start of Main stack
	__set_MSP(__initial_sp);
	//set PSP to start of main task stack
	__set_PSP(TCBList[0].stackPointer);
	//set SPSEL bit (bit 1) in control register
	__set_CONTROL(__get_CONTROL & (1 << 1));

	//set main task to running
	TCBList[0].state = RUNNING;
	runningTCB = &(TCBList[0]);

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
	if(numTasks == MAX_NUM_TASKS){
		//Max number of tasks reached, return and notify somehow???
		return;
	}

	//Get next task block
	TCB_t *newTCB = &(TCBList[numTasks]);

	//set P0 for this task's to arg
	*(newTCB->stackPointer + P0_OFFSET) = (uint32_t)arg;
	//set PC to adress of the taks's function
	*(newTCB->stackPointer + PC_OFFSET) = func;
	//set PSR to default value (0x01000000)
	*(newTCB->stackPointer + PSR_OFFSET) = PSR_DEFAULT;

	//set current task to ready and put it in the list
	newTCB->status = READY;
	addToReadyList(newTCB);

	//bump up num tasks
	numTasks++;
}

void semaphorInit(semaphor_t *sem, uint32_t val){

}

void waitOnSemaphor(semaphor_t *sem){

}

void signalSemaphor(semaphor_t *sem){

}

void mutextInit(mutex_t *mutex){

}

void waitOnMutex(mutex_t *mutex){

}

void signalMutex(mutex_t *mutex){

}