#define MAX_NUM_TASKS 6

#define MAIN_STACK_SIZE 2048
#define TASK_STACK_SIZE 1024

//Position of RO (task paramater) in context "array"
#define R0_OFFSET 8
//Position of PC (Program Counter) in context "array"
#define PC_OFFSET 14
//Position of PSR (Process Status Register) in context "array"
#define PSR_OFFSET 15
#define PSR_DEFAULT 0x01000000

//bit mask to set PendSV to pending
#define PEND_SV_SET (1 << 28)

typedef enum taskState_t {RUNNING, READY, WAITING, DONE} taskState_t;

typedef struct TCB_t TCB_t;
struct TCB_t{
	uint8_t stackNum;
	uint32_t stackPointer;
	taskState_t state;
	TCB_t *next;
};

typedef void (*rtosTaskFunc_t)(void *args);

typedef uint32_t semaphore_t;
typedef uint32_t mutex_t;

void rtosInit(void);

void rtosThreadNew(rtosTaskFunc_t func, void *arg);

void semaphoreInit(semaphore_t *sem, uint32_t val);
void waitOnSemaphore(semaphore_t *sem);
void signalSemaphore(semaphore_t *sem);

void mutextInit(mutex_t *mutex);
void waitOnMutex(mutex_t *mutex);
void signalMutex(mutex_t *mutex);
