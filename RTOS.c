#include "RTOS.h"
#include <LPC17xx.h>
#include "context.h"

uint8_t numTasks = 0;

uint32_t TIME_SLICE_FREQ = 200;

TCB_t controlBlocks[MAX_NUM_TASKS];

void SysTick_Handler(void) {
    //TODO preform context switch to next tasks
}

void rtosInit(void){
	for(uint8_t i = 0; i < MAX_NUM_TASKS; i++){
		//initialize each TCB with their stack number and base stack adress
		controlBlocks.stackNum = i;
		controlBlocks.stackPointer = __initial_sp - TASK_STACK_SIZE * (MAX_NUM_TASKS - i);
	}

	//copy over main stack to first task's stack
	//TODO not sure what to do if main stack is > 1KiB
	numTasks = 1;
	memcpy(controlBlocks[0].stackPointer, __initial_sp, TASK_STACK_SIZE);

	//set MSP to start of Main stack
	__set_MSP(__initial_sp);
	//set PSP to start of main task stack
	__set_PSP(controlBlocks[0].stackPointer);
	//set SPSEL bit (bit 1) in control register
	__set_CONTROL(__get_CONTROL & (1 << 1));

	//Set systick interupt to fire at the time slice frequency
	SysTick_Config(SystemCoreClock/TIME_SLICE_FREQ);
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
	TCB_t *newTCB = &(controlBlocks[numTasks]);

	//set P0 for this task's to arg
	*(newTCB->stackPointer + P0_OFFSET) = (uint32_t)arg;
	//set PC to adress of the taks's function
	*(newTCB->stackPointer + PC_OFFSET) = func;
	//set PSR to default value (0x01000000)
	*(newTCB->stackPointer + PSR_OFFSET) = PSR_DEFAULT;
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