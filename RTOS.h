#ifndef __RTOS_H
#define __RTOS_H

typedef enum { HIGHEST_PRIORITY = 0, DEFAULT_PRIORITY = 3, LOWEST_PRIORITY = 6, NO_PRIORITY, NUM_PRIORITIES = NO_PRIORITY } taskPriority_t;

typedef enum { RUNNING, READY, WAITING, SUSPENDED } taskState_t;

typedef struct TCB TCB_t;
typedef struct tcbQueue tcbQueue_t;

struct tcbQueue {
  TCB_t *head;
  TCB_t *tail;
};

struct TCB {
  uint8_t id;
  uint32_t baseOfStack;
  uint32_t stackPointer;
  taskPriority_t taskPriority;
  uint32_t waitTicks;
  taskState_t state;
  tcbQueue_t *currentQueue;
  TCB_t *next;
};


typedef void (*rtosTaskFunc_t)(void *args);

typedef struct {
  uint8_t count;
  tcbQueue_t waitingPriorityQueue[NUM_PRIORITIES];
} semaphore_t;

typedef struct {
  int8_t owner;
  taskPriority_t storedPriority;
  tcbQueue_t waitingPriorityQueue[NUM_PRIORITIES];
} mutex_t;

void rtosInit(void);

void rtosThreadNew(rtosTaskFunc_t func, void *arg, taskPriority_t taskPriority);

void rtosSemaphoreInit(semaphore_t *sem, uint8_t count);
void rtosWaitOnSemaphore(semaphore_t *sem);
void rtosSignalSemaphore(semaphore_t *sem);

void rtosMutexInit(mutex_t *mutex);
void rtosAcquireMutex(mutex_t *mutex);
void rtosReleaseMutex(mutex_t *mutex);

void rtosWait(uint32_t ticks);

void rtosEnterFunction(void);
void rtosExitFunction(void);

void rtosEnterCriticalSection(void);
void rtosExitCriticalSection(void);
#endif /* __RTOS_H */
