#incldue "RTOS.h"

uint8_t MAX_NUM_TASKS 6;

TCB_t controlBlocks[MAX_NUM_TASKS];

void rtosInit(){

}

void rtosThreadNew(rtosTaskFunc_t func, void *arg){

}

void semaphorInit(semaphor_t *sem, uint32_t val){

}

void waitOnSemaphor(semaphor_t *sem){

}

void signalSemaphor(semaphor_t *sem){

}

void mutextInit(mutex_t *mutex){

}

void waitOnMutex(mutex_t *mutex){

}

void signalMutex(mutex_t *mutex){

}