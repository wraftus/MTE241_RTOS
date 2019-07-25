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


typedef enum {RUNNING, READY, WAITING, SUSPENDED} taskState_t;

typedef struct TCB_t TCB_t;
struct TCB_t{
	uint8_t id;
	uint32_t baseOfStack;
	uint32_t stackPointer;
	uint32_t waitTicks;
	taskState_t state;
	TCB_t *next;
};

typedef void (*rtosTaskFunc_t)(void *args);

typedef struct{
	uint8_t count;
	TCB_t *waitListHead;
} semaphore_t;
typedef struct{
	int8_t owner;
	TCB_t *waitListHead;
} mutex_t;

void rtosInit(void);

void rtosThreadNew(rtosTaskFunc_t func, void *arg);

void semaphoreInit(semaphore_t *sem, uint32_t count);
void waitOnSemaphore(semaphore_t *sem);
void signalSemaphore(semaphore_t *sem);

void mutexInit(mutex_t *mutex);
void acquireMutex(mutex_t *mutex);
void releaseMutex(mutex_t *mutex);

void rtosWait(uint32_t ticks);

void rtosEnterFunction(void);
void rtosExitFunction(void);
