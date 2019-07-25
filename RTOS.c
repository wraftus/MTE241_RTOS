#include "RTOS.h"
#include "context.h"
#include <LPC17xx.h>
#include <core_cm3.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NUM_TASKS 6
#define MAIN_TASK_ID 0

#define TASK_STACK_SIZE 1024
#define MAIN_TASK_SIZE 2048

// Position of RO (task parameter) in context "array"
#define R0_OFFSET 8
// Position of PC (Program Counter) in context "array"
#define PC_OFFSET 14
// Position of PSR (Process Status Register) in context "array"
#define PSR_OFFSET 15
#define PSR_DEFAULT 0x01000000

uint8_t numTasks = 0;

uint32_t RTOS_TICK_FREQ = 1000;
uint32_t TIME_SLICE_TICKS = 5;

uint32_t rtosTickCounter;
uint32_t nextTimeSlice;

TCB_t TCBList[MAX_NUM_TASKS];
TCB_t *runningTCB;
tcbQueue_t readyTaskPriorityQueue[NUM_PRIORITIES];
tcbQueue_t waitingTaskPriorityQueue[NUM_PRIORITIES];

const int8_t NO_OWNER = -1;

uint8_t inCriticalSection;

// This should only be called atomically
void forceContextSwitch() {
  rtosTickCounter = nextTimeSlice;
  nextTimeSlice += TIME_SLICE_TICKS;

  // check if there is a ready task to switch to
  for (taskPriority_t priority = HIGHEST_PRIORITY; priority < NUM_PRIORITIES; priority++) {
    if (readyTaskPriorityQueue[priority].head != NULL) {
      // notify PendSV_Handler we are ready to switch
      SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
      break;
    }
  }
}

void addToList(TCB_t *toAdd, tcbQueue_t *queue) {
  if (queue[toAdd->taskPriority].head == NULL) { // empty priority list
    queue[toAdd->taskPriority].head = toAdd;
    queue[toAdd->taskPriority].tail = toAdd;
  } else {
    queue[toAdd->taskPriority].tail->next = toAdd;
    queue[toAdd->taskPriority].tail = toAdd;
  }
  toAdd->currentQueue = queue;
}

void SysTick_Handler(void) {
  // check if any waiting tasks are done

  // iterate through all the task in order of priority and then in fifo
  for (taskPriority_t priority = HIGHEST_PRIORITY; priority < NUM_PRIORITIES; priority++) {
    if (waitingTaskPriorityQueue[priority].head != NULL) {
      TCB_t *TCB_ptr = waitingTaskPriorityQueue[priority].head;
      TCB_t *TCB_prev_ptr = NULL;
      do {
        // decrement time until wait ends
        TCB_ptr->waitTicks--;

        // if task is done waiting
        if (TCB_ptr->waitTicks == 0) {
          // set state to ready and add to ready queue
          TCB_ptr->state = READY;
          addToList(TCB_ptr, readyTaskPriorityQueue);

          // remove task from waiting queue
          if (TCB_ptr == waitingTaskPriorityQueue[priority].head) { // if task is the head
            if (TCB_ptr->next == NULL) {                            // if its the only task in the queue
              waitingTaskPriorityQueue[priority].tail = NULL;
            }
            waitingTaskPriorityQueue[priority].head = TCB_ptr->next;
            TCB_ptr->next = NULL;
            TCB_ptr = waitingTaskPriorityQueue[priority].head;
          } else if (TCB_ptr == waitingTaskPriorityQueue[priority].tail) { // if task is that tail
            TCB_prev_ptr->next = NULL;
            waitingTaskPriorityQueue[priority].tail = TCB_prev_ptr;
            TCB_ptr->next = NULL;
            TCB_ptr = NULL;
          } else { // neither head or tail
            TCB_prev_ptr->next = TCB_ptr->next;
            TCB_ptr->next = NULL;
            TCB_ptr = TCB_prev_ptr->next;
          }
        } else {
          TCB_prev_ptr = TCB_ptr;
          TCB_ptr = TCB_ptr->next;
        }
      } while (TCB_ptr != NULL);
    }
  }

  // check for timeslices
  rtosTickCounter++;
  if (rtosTickCounter - nextTimeSlice >= TIME_SLICE_TICKS && !inCriticalSection) {
    // we are ready to switch to the next task
    nextTimeSlice += TIME_SLICE_TICKS;
    // check if there is a ready task to switch to
    for (taskPriority_t priority = HIGHEST_PRIORITY; priority < NUM_PRIORITIES; priority++) {
      if (readyTaskPriorityQueue[priority].head != NULL) {
        // notify PendSV_Handler we are ready to switch
        SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
        break;
      }
    }
  } else if (inCriticalSection) {
    if (rtosTickCounter - nextTimeSlice >= TIME_SLICE_TICKS) {
      nextTimeSlice++;
    }
  }
}

void PendSV_Handler(void) {
  // Preform context switch if we are ready to switch tasks
  // software store context of current running task
  runningTCB->stackPointer = storeContext();

  // queue the current running task
  if (runningTCB->state != WAITING) {
    addToList(runningTCB, readyTaskPriorityQueue);
    runningTCB->state = RUNNING;
  }

  // pop next task
  for (taskPriority_t priority = HIGHEST_PRIORITY; priority < NUM_PRIORITIES; priority++) {
    if (readyTaskPriorityQueue[priority].head != NULL) {
      runningTCB = readyTaskPriorityQueue[priority].head;
      readyTaskPriorityQueue[priority].head = runningTCB->next;
      if (runningTCB->next == NULL) {
        readyTaskPriorityQueue[priority].tail = NULL;
      }
      runningTCB->next = NULL;
      break;
    }
  }

  // software restore context of next task
  __set_PSP(runningTCB->stackPointer);
  restoreContext(runningTCB->stackPointer);
}

void rtosInit(void) {
  for (uint8_t i = 0; i < MAX_NUM_TASKS; i++) {
    // initialize each TCB with their stack number and base stack address
    TCBList[i].id = i;
    TCBList[i].stackPointer = TCBList[i].baseOfStack =
        *((uint32_t *)SCB->VTOR) - MAIN_TASK_SIZE - TASK_STACK_SIZE * (MAX_NUM_TASKS - 1 - i);
    TCBList[i].next = NULL;
    TCBList[i].state = SUSPENDED;
    TCBList[i].waitTicks = 0;
    TCBList[i].taskPriority = DEFAULT_PRIORITY;
  }

  // copy over main stack to first task's stack
  // TODO not sure what to do if main stack is > 1KiB
  numTasks = 1;
  memcpy((void *)(TCBList[MAIN_TASK_ID].stackPointer - TASK_STACK_SIZE),
         (void *)((*((uint32_t *)SCB->VTOR)) - TASK_STACK_SIZE), TASK_STACK_SIZE);

  // set Main Task to lowest Priority to act as idle thread
  TCBList[MAIN_TASK_ID].taskPriority = LOWEST_PRIORITY;

  // change main stack pointer to be inside init
  TCBList[MAIN_TASK_ID].stackPointer = TCBList[MAIN_TASK_ID].baseOfStack - ((*((uint32_t *)SCB->VTOR)) - __get_MSP());

  // set MSP to start of Main stack
  __set_MSP(*((uint32_t *)SCB->VTOR));
  // set SPSEL bit (bit 1) in control register
  __set_CONTROL(__get_CONTROL() | CONTROL_SPSEL_Msk);
  // set PSP to start of main task stack
  __set_PSP(TCBList[MAIN_TASK_ID].stackPointer);

  // set main task to running
  TCBList[MAIN_TASK_ID].state = RUNNING;
  runningTCB = &(TCBList[MAIN_TASK_ID]);

  // initialize Priority Queues
  for (taskPriority_t priority = HIGHEST_PRIORITY; priority < NUM_PRIORITIES; priority++) {
    readyTaskPriorityQueue[priority].head = NULL;
    readyTaskPriorityQueue[priority].tail = NULL;
    waitingTaskPriorityQueue[priority].head = NULL;
    waitingTaskPriorityQueue[priority].tail = NULL;
  }

  // set up timer variables
  rtosTickCounter = 0;
  nextTimeSlice = TIME_SLICE_TICKS;

  // initialize inCriticalSection
  inCriticalSection = 0;

  // Set systick interrupt to fire at the time slice frequency
  SysTick_Config(SystemCoreClock / RTOS_TICK_FREQ);
}

rtosStatus_t rtosThreadNew(rtosTaskFunc_t func, void *arg, taskPriority_t taskPriority) {
  rtosEnterFunction();
  __disable_irq();
  if (numTasks == 0) {
    // rtos has not yet, return and notify somehow???
    return RTOS_NOT_INIT;
  }
  if (numTasks == MAX_NUM_TASKS) {
    // Max number of tasks reached, return and notify somehow???
    return RTOS_MAX_TASKS;
  }

  // Get next task block
  TCB_t *newTCB = &(TCBList[numTasks]);

  // set R0 for this task's to arg
  *((uint32_t *)newTCB->stackPointer + R0_OFFSET) = (uint32_t)arg;
  // set PC to address of the tasks's function
  *((uint32_t *)newTCB->stackPointer + PC_OFFSET) = (uint32_t)func;
  // set PSR to default value (0x01000000)
  *((uint32_t *)newTCB->stackPointer + PSR_OFFSET) = PSR_DEFAULT;

  // set current task to ready and put it in the list
  newTCB->taskPriority = taskPriority;
  newTCB->state = READY;
  addToList(newTCB, readyTaskPriorityQueue);

  // bump up num tasks
  numTasks++;
  __enable_irq();
  rtosExitFunction();
  return RTOS_OK;
}

rtosStatus_t rtosSemaphoreInit(semaphore_t *sem, uint8_t count) {
  rtosEnterFunction();
  sem->count = count;
  for (taskPriority_t priority = HIGHEST_PRIORITY; priority < NUM_PRIORITIES; priority++) {
    sem->waitingPriorityQueue[priority].head = NULL;
    sem->waitingPriorityQueue[priority].tail = NULL;
  }
  rtosExitFunction();
  return RTOS_OK;
}

rtosStatus_t rtosWaitOnSemaphore(semaphore_t *sem) {
  rtosEnterFunction();
  __disable_irq();
  if (sem->count > 0) {
    // semaphore is open
    sem->count--;
  } else {
    // semaphore is closed, wait until it is signalled
    runningTCB->state = WAITING;
    addToList(runningTCB, sem->waitingPriorityQueue);
    forceContextSwitch();
  }
  __enable_irq();
  rtosExitFunction();
  return RTOS_OK;
}

rtosStatus_t rtosSignalSemaphore(semaphore_t *sem) {
  rtosEnterFunction();
  __disable_irq();
  sem->count++;

  // check if there is a task waiting for semaphore
  for (taskPriority_t priority = HIGHEST_PRIORITY; priority < NUM_PRIORITIES; priority++) {
    if (sem->waitingPriorityQueue[priority].head != NULL) {
      TCB_t *unblockedTask = sem->waitingPriorityQueue[priority].head;

      sem->waitingPriorityQueue[priority].head = unblockedTask->next;
      if (unblockedTask->next == NULL) { // if the only task in list
        sem->waitingPriorityQueue[priority].tail = NULL;
      }

      // set task to ready state and queue in ready task queue
      unblockedTask->next = NULL;
      unblockedTask->state = READY;
      addToList(unblockedTask, readyTaskPriorityQueue);
      break;
    }
  }
  __enable_irq();
  rtosExitFunction();
  return RTOS_OK;
}

rtosStatus_t rtosMutexInit(mutex_t *mutex) {
  rtosEnterFunction();
  mutex->owner = NO_OWNER;
  for (taskPriority_t priority = HIGHEST_PRIORITY; priority < NUM_PRIORITIES; priority++) {
    mutex->waitingPriorityQueue[priority].head = NULL;
    mutex->waitingPriorityQueue[priority].tail = NULL;
    mutex->storedPriority = NO_PRIORITY;
  }
  rtosExitFunction();
  return RTOS_OK;
}

rtosStatus_t rtosAcquireMutex(mutex_t *mutex) {
  rtosEnterFunction();
  __disable_irq();
  if (mutex->owner == NO_OWNER) {
    mutex->owner = runningTCB->id;
  } else { // mutex already owned

    if (TCBList[mutex->owner].taskPriority > runningTCB->taskPriority) {
      // elevate mutex owner priority to level of running TCB
      mutex->storedPriority = TCBList[mutex->owner].taskPriority;
      TCBList[mutex->owner].taskPriority = runningTCB->taskPriority;

      tcbQueue_t *queue = TCBList[mutex->owner].currentQueue;
      taskPriority_t priority = mutex->storedPriority;

      TCB_t *TCB_ptr = queue[priority].head;
      TCB_t *TCB_prev_ptr = NULL;
      // find task in queue
      while (TCB_ptr->id != mutex->owner) {
        TCB_prev_ptr = TCB_ptr;
        TCB_ptr = TCB_ptr->next;
      }
      // remove task from queue
      if (TCB_ptr == queue[priority].head) { // if task is the head
        if (TCB_ptr->next == NULL) {         // if its the only task in the queue
          queue[priority].tail = NULL;
        }
        queue[priority].head = TCB_ptr->next;
        TCB_ptr->next = NULL;
        TCB_ptr = queue[priority].head;
      } else if (TCB_ptr == queue[priority].tail) { // if task is that tail
        TCB_prev_ptr->next = NULL;
        queue[priority].tail = TCB_prev_ptr;
        TCB_ptr->next = NULL;
        TCB_ptr = NULL;
      } else { // neither head or tail
        TCB_prev_ptr->next = TCB_ptr->next;
        TCB_ptr->next = NULL;
        TCB_ptr = TCB_prev_ptr->next;
      }
      // insert elevated mutex owner task back into same queue,but with elevated priority
      addToList(&(TCBList[mutex->owner]), queue);
    }

    runningTCB->state = WAITING;
    addToList(runningTCB, mutex->waitingPriorityQueue);
    forceContextSwitch();
  }
  __enable_irq();
  rtosExitFunction();
  return RTOS_OK;
}

rtosStatus_t rtosReleaseMutex(mutex_t *mutex) {
  rtosEnterFunction();
  __disable_irq();
  if (mutex->owner != runningTCB->id) {
    // cannot release a mutex you do not own
    rtosExitFunction();
    return RTOS_MUTEX_NOT_OWNED;
  }

  if (mutex->storedPriority != NO_PRIORITY) { // if need to restore unelevated priority
    // return elevated task to original priority
    TCBList[mutex->owner].taskPriority = mutex->storedPriority;
    // reset stored priority
    mutex->storedPriority = NO_PRIORITY;
  }

  for (taskPriority_t priority = HIGHEST_PRIORITY; priority < NUM_PRIORITIES; priority++) {
    if (mutex->waitingPriorityQueue[priority].head != NULL) {
      TCB_t *unblockedTask = mutex->waitingPriorityQueue[priority].head;

      mutex->waitingPriorityQueue[priority].head = unblockedTask->next;
      if (unblockedTask->next == NULL) { // if the only task in list
        mutex->waitingPriorityQueue[priority].tail = NULL;
      }
      mutex->owner = unblockedTask->id;

      // set task to ready state and queue in ready task queue
      unblockedTask->next = NULL;
      unblockedTask->state = READY;
      addToList(unblockedTask, readyTaskPriorityQueue);
      __enable_irq();
      rtosExitFunction();
      return RTOS_OK;
    }
  }

  // Nothing waiting on mutex
  mutex->owner = NO_OWNER;
  __enable_irq();
  rtosExitFunction();
  return RTOS_OK;
}

rtosStatus_t rtosWait(uint32_t ticks) {
  rtosEnterFunction();
  __disable_irq();
  if (numTasks == 0) {
    // rtos not initialized
    return RTOS_NOT_INIT;
  }
  runningTCB->waitTicks = ticks;
  runningTCB->state = WAITING;
  addToList(runningTCB, waitingTaskPriorityQueue);
  forceContextSwitch();
  __enable_irq();
  rtosExitFunction();
  return RTOS_OK;
}

__asm void rtosEnterFunction(void) {
		PUSH{R4 - R11}
		BX LR
}

__asm void rtosExitFunction(void) {
		POP{R4-R11}
		BX LR
}

void rtosEnterCriticalSection(void) {
  __disable_irq();
  inCriticalSection = 1;
  __enable_irq();
}
void rtosExitCriticalSection(void) {
  __disable_irq();
  inCriticalSection = 0;
  __enable_irq();
}
