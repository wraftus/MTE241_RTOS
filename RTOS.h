#ifndef __RTOS_H
#define __RTOS_H

typedef enum {HIGHEST_PRIORITY = 0, DEFAULT_PRIORITY = 3 , LOWEST_PRIORITY = 6, NUM_PRIORITIES} taskPriority_t;

typedef enum {RUNNING, READY, WAITING, SUSPENDED} taskState_t;

typedef struct TCB_t TCB_t;
struct TCB_t{
	uint8_t id;
	uint32_t baseOfStack;
	uint32_t stackPointer;
	taskPriority_t taskPriority;
	uint32_t waitTicks;
	taskState_t state;
	TCB_t *next;
};

typedef struct tcbQueue {
    TCB_t* head;
    TCB_t* tail;
}tcbQueue_t;

typedef void (*rtosTaskFunc_t)(void *args);

typedef struct {
	uint8_t count;
	tcbQueue_t *waitingPriorityQueue[NUM_PRIORITIES];
} semaphore_t;

typedef struct{
	int8_t owner;
        tcbQueue_t *waitingPriorityQueue[NUM_PRIORITIES];
} mutex_t;

void rtosInit(void);

void rtosThreadNew(rtosTaskFunc_t func, void *arg, taskPriority_t taskPriority);

void semaphoreInit(semaphore_t *sem, uint8_t count);
void waitOnSemaphore(semaphore_t *sem);
void signalSemaphore(semaphore_t *sem);

void mutexInit(mutex_t *mutex);
void acquireMutex(mutex_t *mutex);
void releaseMutex(mutex_t *mutex);

void rtosWait(uint32_t ticks);

void rtosEnterFunction(void);
void rtosExitFunction(void);
#endif /* __RTOS_H */
