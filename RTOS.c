#include "RTOS.h"
#include <LPC17xx.h>
#include "context.h"

uint8_t MAX_NUM_TASKS = 6;
uint8_t numTasks = 0;

uint16_t MAIN_STACK_SIZE = 2048;
uint16_t TASK_STACK_SIZE = 1024;

uint32_t TIME_SLICE_FREQ = 200;

TCB_t controlBlocks[MAX_NUM_TASKS];

void SysTick_Handler(void) {
    //TODO preform context switch to next tasks
}

void rtosInit(void){
	for(uint8_t i = 0; i < MAX_NUM_TASKS; i++){
		//initialize each TCB with their stack number and base stack adress
		controlBlocks[i].stackNum = i;
		controlBlocks[i].baseStackAddress = __initial_sp - TASK_STACK_SIZE * (MAX_NUM_TASKS - i);
	}

	//copy over main stack to first task's stack
	//TODO not sure what to do if main stack is > 1KiB
	numTasks = 1;
	memcpy(controlBlocks[0].baseStackAddress, __initial_sp, TASK_STACK_SIZE);

	//set MSP to start of Main stack
	__set_MSP(__initial_sp);
	//set PSP to start of main task stack
	__set_PSP(controlBlocks[0].baseStackAddress);
	//set SPSEL bit (bit 1) in control register
	__set_CONTROL(__get_CONTROL & (1 << 1));

	//Set systick interupt to fire at the time slice frequency
	SysTick_Config(SystemCoreClock/TIME_SLICE_FREQ);
}

void rtosThreadNew(rtosTaskFunc_t func, void *arg){

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