#include <LPC17xx.h>
#include <core_cm3.h>
#include "RTOS.h"
#include "context.h"
#include <stdlib.h>
#include <string.h>

uint8_t numTasks = 0;

uint32_t RTOS_TICK_FREQ = 1000;
uint32_t TIME_SLICE_TICKS = 5;

uint32_t rtosTickCounter;
uint32_t nextTimeSlice;

TCB_t TCBList[MAX_NUM_TASKS];
TCB_t *runningTCB;
TCB_t *readyListHead;
TCB_t *waitListHead;

// This should only be called atomically
void forceContextSwitch(){
	rtosTickCounter = nextTimeSlice;
	nextTimeSlice += TIME_SLICE_TICKS;
	if(readyListHead != NULL){
			//notify PendSV_Handler we are ready to switch
		SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
	}
}

void addToList(TCB_t *toAdd, TCB_t **listHead){
	if(*listHead == NULL){
		*listHead = toAdd;
	}
	else{
		TCB_t *temp = *listHead;
		while(temp->next != NULL){
			temp = temp->next;
		}
		temp->next = toAdd;
	}
}

void SysTick_Handler(void) {
	//check if any waiting tasks are done
	TCB_t *prev = NULL;
	TCB_t *cur = waitListHead;
	while(cur != NULL){
		cur->waitTicks--;
		if(cur->waitTicks == 0){
			//task is done waiting!
			cur->state = READY;
			addToList(cur, &readyListHead);
			//remove task
			if(prev == NULL){
				//first task is done
				waitListHead = cur->next;
				cur->next = NULL;
				cur = waitListHead;
			}
			else{
				prev->next = cur->next;
				cur->next = NULL;
				cur = prev->next;
			}
		}
		else{
			prev = cur;
			cur = cur->next;
		}
	}

	//check for timeslices
	rtosTickCounter++;
	if(rtosTickCounter - nextTimeSlice >= TIME_SLICE_TICKS){
		//we are ready to move to the next task
		//TODO unsure what to do if all tasks are waiting
		if(readyListHead != NULL){
			//notify PendSV_Handler we are ready to switch
		SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
		}
	}
}

void PendSV_Handler(void){
	//Preform context switch if we are ready to switch tasks
	//software store context of current running task
	runningTCB->stackPointer = storeContext();

	//queue the current running task, pop next task
	//TODO change state of running task to ready
	if(runningTCB->state != WAITING){
		addToList(runningTCB, &readyListHead);
	}
	runningTCB = readyListHead;
	readyListHead = readyListHead->next;
	runningTCB->next = NULL;

	//software restore context of next task
	__set_PSP(runningTCB->stackPointer);
	restoreContext(runningTCB->stackPointer);

}

void rtosInit(void){
	for(uint8_t i = 0; i < MAX_NUM_TASKS; i++){
		//initialize each TCB with their stack number and base stack adress
		TCBList[i].id = i;
		TCBList[i].stackPointer = TCBList[i].baseOfStack = *((uint32_t *)SCB->VTOR) - MAIN_TASK_SIZE - TASK_STACK_SIZE * (MAX_NUM_TASKS - 1 - i);
		TCBList[i].next = NULL;
		TCBList[i].state = SUSPENDED;
		TCBList[i].waitTicks = 0;
	}

	//copy over main stack to first task's stack
	//TODO not sure what to do if main stack is > 1KiB
	numTasks = 1;
	memcpy((void *)(TCBList[MAIN_TASK_ID].stackPointer - TASK_STACK_SIZE), (void *)((*((uint32_t *)SCB->VTOR)) - TASK_STACK_SIZE), TASK_STACK_SIZE);

	//change main stack pointer to be inside init
	TCBList[MAIN_TASK_ID].stackPointer = TCBList[MAIN_TASK_ID].baseOfStack - ((*((uint32_t *)SCB->VTOR)) - __get_MSP());

	//set MSP to start of Main stack
	__set_MSP(*((uint32_t*)SCB->VTOR));
	//set SPSEL bit (bit 1) in control register
	__set_CONTROL(__get_CONTROL() | CONTROL_SPSEL_Msk);
	//set PSP to start of main task stack
	__set_PSP(TCBList[MAIN_TASK_ID].stackPointer);

	//set main task to running
	TCBList[MAIN_TASK_ID].state = RUNNING;
	runningTCB = &(TCBList[MAIN_TASK_ID]);

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
	*((uint32_t *)newTCB->stackPointer + R0_OFFSET) = (uint32_t)arg;
	//set PC to adress of the taks's function
	*((uint32_t *)newTCB->stackPointer + PC_OFFSET) = (uint32_t)func;
	//set PSR to default value (0x01000000)
	*((uint32_t *)newTCB->stackPointer + PSR_OFFSET) = PSR_DEFAULT;

	//set current task to ready and put it in the list
	newTCB->state = READY;
	addToList(newTCB, &readyListHead);

	//bump up num tasks
	numTasks++;
}

void semaphorInit(semaphore_t *sem, uint8_t count){
	sem->count = count;
	sem->waitListHead = NULL;
}

void waitOnSemaphor(semaphore_t *sem){
	__disable_irq();
	if(sem->count > 0){
		//semaphore is open
		sem->count--;
	}
	else{
		//semaphore is closed, wait until it is signalled
		runningTCB->state = WAITING;
		addToList(runningTCB, &(sem->waitListHead));
		forceContextSwitch();
	}
	__enable_irq();
}

void signalSemaphor(semaphore_t *sem){
	__disable_irq();
	sem->count++;
	if(sem->waitListHead != NULL){
		semaphore_t *temp = sem->waitListHead->next;
		sem->waitListHead->next = NULL;
		sem->waitListHead->state = READY;
		addToList(sem->waitListHead, &readyListHead);
		sem->waitListHead = temp;
	}
	__enable_irq();
}

void mutextInit(mutex_t *mutex){
	mutex->owner = -1;
	mutex->waitListHead = NULL;
}

void aquireMutex(mutex_t *mutex){
	__disable_irq();
	if(mutex->owner == -1){
		//mutex is not yet owned
		mutex->owner = runningTCB->id;
	} else {
		//mutex is already owned
		runningTCB->state = WAITING;
		addToList(runningTCB, &(mutex->waitListHead));
		forceContextSwitch();
	}
	__enable_irq();
}

void releaseMutex(mutex_t *mutex){
	__disable_irq();
	if(mutex->owner != runningTCB->id){
		//cannot release a mutex you do not own
		return;
	}
	if(mutex->waitListHead == NULL){
		//nothing is waiting on this mutex
		mutex = -1;
	} else{
		mutex->owner = mutex->waitListHead->id;
		mutex_t* temp = mutex->waitListHead->next;
		mutex->waitListHead->next = NULL;
		mutex->waitListHead->state = READY;
		addToList(mutex->waitListHead, &readyListHead);
		mutex->waitListHead = temp;
	}
	__enable_irq();
}

void rtosWait(uint32_t ticks){
	rtosEnterFunction();
	__disable_irq();
	runningTCB->waitTicks = ticks;
	runningTCB->state = WAITING;
	addToList(runningTCB, &waitListHead);
	forceContextSwitch();
	__enable_irq();
	rtosExitFunction();
}

__asm void rtosEnterFunction(void){
	PUSH	{R4}
	BX	  LR
}

__asm void rtosExitFunction(void){
	POP	{R4}
	BX	LR
}
