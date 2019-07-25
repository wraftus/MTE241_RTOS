#ifndef __RTOS_H
#define __RTOS_H

typedef enum { HIGHEST_PRIORITY = 0, DEFAULT_PRIORITY = 3, LOWEST_PRIORITY = 6, NO_PRIORITY, NUM_PRIORITIES = NO_PRIORITY } taskPriority_t;

typedef enum { RUNNING, READY, WAITING, SUSPENDED } taskState_t;

typedef enum {RTOS_OK, RTOS_NOT_INIT, RTOS_MAX_TASKS, RTOS_MUTEX_NOT_OWNED} rtosStatus_t;

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

rtosStatus_t rtosThreadNew(rtosTaskFunc_t func, void *arg, taskPriority_t taskPriority);

rtosStatus_t rtosSemaphoreInit(semaphore_t *sem, uint8_t count);
rtosStatus_t rtosWaitOnSemaphore(semaphore_t *sem);
rtosStatus_t rtosSignalSemaphore(semaphore_t *sem);

rtosStatus_t rtosMutexInit(mutex_t *mutex);
rtosStatus_t rtosAcquireMutex(mutex_t *mutex);
rtosStatus_t rtosReleaseMutex(mutex_t *mutex);

rtosStatus_t rtosWait(uint32_t ticks);

void rtosEnterFunction(void);
void rtosExitFunction(void);

void rtosEnterCriticalSection(void);
void rtosExitCriticalSection(void);
#endif /* __RTOS_H */
