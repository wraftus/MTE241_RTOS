#include <LPC17xx.h>
#include <core_cm3.h>
#include "RTOS.h"
#include "context.h"
#include <stdlib.h>
#include <string.h>

#define MAX_NUM_TASKS 6
#define MAIN_TASK_ID 0

#define TASK_STACK_SIZE 1024
#define MAIN_TASK_SIZE 2048

//Position of RO (task parameter) in context "array"
#define R0_OFFSET 8
//Position of PC (Program Counter) in context "array"
#define PC_OFFSET 14
//Position of PSR (Process Status Register) in context "array"
#define PSR_OFFSET 15
#define PSR_DEFAULT 0x01000000

uint8_t numTasks = 0;

uint32_t RTOS_TICK_FREQ = 1000;
uint32_t TIME_SLICE_TICKS = 5;

uint32_t rtosTickCounter;
uint32_t nextTimeSlice;

TCB_t TCBList[MAX_NUM_TASKS];
TCB_t *runningTCB;
tcbQueue_t *readyPriorityQueue[NUM_PRIORITIES];
tcbQueue_t *waitPriorityQueue[NUM_PRIORITIES];

// This should only be called atomically
void forceContextSwitch(){
	rtosTickCounter = nextTimeSlice;
	nextTimeSlice += TIME_SLICE_TICKS;
	if(readyListHead != NULL){
			//notify PendSV_Handler we are ready to switch
		SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
	}
}

void addToList(TCB_t *toAdd, tcbQueue_t *queue[]){
	if(queue[toAdd->taskPriority]->head == NULL){ //empty priority list
          queue[toAdd->taskPriority]->head = toAdd;
          queue[toAdd->taskPriority]->tail = toAdd;
        }
	else{
		queue[toAdd->taskPriority]->tail->next = toAdd;
	  queue[toAdd->taskPriority]->tail = queue[toAdd->taskPriority]->tail->next;
	}
}

void SysTick_Handler(void) {
	//check if any waiting tasks are done
	TCB_t *prev = NULL;
	tcbQueue_t *cur = waitPriorityQueue;
	while(cur != NULL){
		cur->waitTicks--;
		if(cur->waitTicks == 0){
			//task is done waiting!
			cur->state = READY;
			addToList(cur, readyListHead);
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
		addToList(runningTCB, readyPriorityQueue);
	}

  for (taskPriority_t priority = HIGHEST_PRIORITY; priority < NUM_PRIORITIES; priority++){
		if (readyPriorityQueue[priority]->head!=NULL){
			runningTCB = readyPriorityQueue[priority]->head;
			if(readyPriorityQueue[priority]->head->next != NULL){
				readyPriorityQueue[priority]->head = readyPriorityQueue[priority]->head->next;
			} else { //priority level is now empty
				readyPriorityQueue[priority]->tail = NULL;
			}
			runningTCB->next = NULL;
			break;
		}
	}

	//software restore context of next task
	__set_PSP(runningTCB->stackPointer);
	restoreContext(runningTCB->stackPointer);

}

void rtosInit(void){
	for(uint8_t i = 0; i < MAX_NUM_TASKS; i++){
		//initialize each TCB with their stack number and base stack address
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

	//initialize Priority Queues
	for (taskPriority_t priority = HIGHEST_PRIORITY; priority < NUM_PRIORITIES; priority++){
		readyPriorityQueue[priority] = NULL;
		waitPriorityQueue[priority] = NULL;
	}


	//set up timer variables
	rtosTickCounter = 0;
	nextTimeSlice = TIME_SLICE_TICKS;

	//Set systick interrupt to fire at the time slice frequency
	SysTick_Config(SystemCoreClock/RTOS_TICK_FREQ);
}

//TODO will probably need to make this atomic
void rtosThreadNew(rtosTaskFunc_t func, void *arg, taskPriority_t taskPriority){
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

  //set R0 for this task's to arg
  *((uint32_t *)newTCB->stackPointer + R0_OFFSET) = (uint32_t)arg;
  //set PC to address of the tasks's function
  *((uint32_t *)newTCB->stackPointer + PC_OFFSET) = (uint32_t)func;
  //set PSR to default value (0x01000000)
  *((uint32_t *)newTCB->stackPointer + PSR_OFFSET) = PSR_DEFAULT;

  //set current task to ready and put it in the list
  newTCB->taskPriority = taskPriority;
  newTCB->state = READY;
  addToList(newTCB, readyPriorityQueue);

  //bump up num tasks
  numTasks++;
}

void semaphoreInit(semaphore_t *sem, uint8_t count){
	sem->count = count;
	sem->waitListHead = NULL;
}

void waitOnSemaphore(semaphore_t *sem){
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

void signalSemaphore(semaphore_t *sem){
	__disable_irq();
	sem->count++;
	if(sem->waitListHead != NULL){
		TCB_t *temp = sem->waitListHead->next;
		sem->waitListHead->next = NULL;
		sem->waitListHead->state = READY;
		addToList(sem->waitListHead, &readyListHead);
		sem->waitListHead = temp;
	}
	__enable_irq();
}

void mutexInit(mutex_t *mutex){
	mutex->owner = -1;
	mutex->waitListHead = NULL;
}

void acquireMutex(mutex_t *mutex){
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
		mutex->owner = -1;
	} else{
		mutex->owner = mutex->waitListHead->id;
		TCB_t* temp = mutex->waitListHead->next;
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
	addToList(runningTCB, waitPriorityQueue);
	forceContextSwitch();
	__enable_irq();
	rtosExitFunction();
}

__asm void rtosEnterFunction(void){
	PUSH	{R4-R11}
	BX	  LR
}

__asm void rtosExitFunction(void){
	POP	{R4-R11}
	BX	LR
}
