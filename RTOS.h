typedef struct {
	
} TCB_t;

typedef void (*rtosTaskFunc_t)(void *args);

typedef uint32_t semaphor_t;
typedef uint32_t mutex_t;

void rtosInit();

void rtosThreadNew(rtosTaskFunc_t func, void *arg);

void semaphorInit(semaphor_t *sem, uint32_t val);
void waitOnSemaphor(semaphor_t *sem);
void signalSemaphor(semaphor_t *sem);

void mutextInit(mutex_t *mutex);
void waitOnMutex(mutex_t *mutex);
void signalMutex(mutex_t *mutex);
