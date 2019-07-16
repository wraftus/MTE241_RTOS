
#define MAX_NUM_TASKS 6;

#define MAIN_STACK_SIZE 2048;
#define TASK_STACK_SIZE 1024;

//Position of RO (task paramater) in context "array"
#define R0_OFFSET 8
//Position of PC (Program Counter) in context "array"
#define PC_OFFSET 14
//Position of PSR (Process Status Register) in context "array"
#define PSR_OFFSET 15
#define PSR_DEFAULT 0x01000000

typedef struct {
	uint8_t stackNum;
	uint32_t *stackPointer;
} TCB_t;

typedef void (*rtosTaskFunc_t)(void *args);

typedef uint32_t semaphore_t;
typedef uint32_t mutex_t;

void rtosInit(void);

void rtosThreadNew(rtosTaskFunc_t func, void *arg);

void semaphoreInit(semaphor_t *sem, uint32_t val);
void waitOnSemaphore(semaphor_t *sem);
void signalSemaphore(semaphor_t *sem);

void mutextInit(mutex_t *mutex);
void waitOnMutex(mutex_t *mutex);
void signalMutex(mutex_t *mutex);
