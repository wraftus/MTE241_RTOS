#include <LPC17xx.h>
#include "RTOS.h"
#include <stdio.h>


const uint8_t led_pos1[3] = {31, 29, 28};
const uint8_t led_pos2[5] = {6, 5, 4, 3, 2};

mutex_t print_mutex;
const char name1[] = "Walter";
const char name2[] = "Melvin";

const uint8_t testTaskCount = 4;
typedef struct { 
	semaphore_t turnstile;
	mutex_t mutex;
	uint32_t count, n;
} barrier_t;
barrier_t barrier;

//non reusable barrier
void syncOnBarrier(barrier_t *barrier){
	//increase barrier's count by acquiring the mutex
	aquireMutex(&(barrier->mutex));
	barrier->count++;
	releaseMutex(&(barrier->mutex));
	
	//if count = n, then we are the last to sync, so signal semaphore
	if(barrier->count == barrier->n){
		signalSemaphore(&(barrier->turnstile));
	}
	
	//wait on the turnstile, and signal it when you go through
	waitOnSemaphore(&(barrier->turnstile));
	signalSemaphore(&(barrier->turnstile));
}

//Task that creates a clock on the LEDs on the board
void ledTimerTask(void *args){
	//set direction for leds to output
	LPC_GPIO1->FIODIR |= (1UL << led_pos1[0]) | (1UL << led_pos1[1]) | (1UL << led_pos1[2]);
	LPC_GPIO2->FIODIR |= (1UL << led_pos2[0]) | (1UL << led_pos2[1]) | (1UL << led_pos2[2]) | (1UL << led_pos2[3]) | (1UL << led_pos2[4]);
	//clear all leds
	LPC_GPIO1->FIOCLR = (1UL << led_pos1[0]) | (1UL << led_pos1[1]) | (1UL << led_pos1[2]);
	LPC_GPIO2->FIOCLR = (1UL << led_pos2[0]) | (1UL << led_pos2[1]) | (1UL << led_pos2[2]) | (1UL << led_pos2[3]) | (1UL << led_pos2[4]);
	
	uint32_t timer = 0;
	
	//wait until all other tasks are ready to go
	syncOnBarrier(&barrier);
	while(1){
		//write count to leds, going through each bit
		for(uint8_t bit = 0; bit < 8; bit++){
			if(timer & (1 << bit)){
				if(bit <= 4){
					LPC_GPIO2->FIOSET = (1UL << led_pos2[bit]);
				} else {
					LPC_GPIO1->FIOSET = (1UL << led_pos1[bit - 5]);
				}
			} else {
				if(bit <= 4){
					LPC_GPIO2->FIOCLR = (1UL << led_pos2[bit]);
				} else {
					LPC_GPIO1->FIOCLR = (1UL << led_pos1[bit - 5]);
				}
			}
		}
		//wait for a second
		rtosWait(1000);
		timer++;
	}
}

//task takes in a name, and prints a message to the console,
//using mutexes to make sure only one person is printing at a time
void printTask(void *args){
	//get name from args
	char *name = (char *)args;
	
	//wait until all other tasks are ready to go
	syncOnBarrier(&barrier);
	while(1){
		//aquire the mutex
		aquireMutex(&print_mutex);
		//print out the message
		printf("Hi! My name is %s, and I currently have the mutex.\n", name);
		//wait for 0.5s
		rtosWait(500);
		releaseMutex(&print_mutex);
	}
}

void lazyGLCDTask(void *args){
	//wait until all other tasks are ready to go
	syncOnBarrier(&barrier);
	while(1);
}

int main(void){
 	rtosInit();
	printf("Main Task!\n");

	//intialize printing mutex
	mutextInit(&print_mutex);
	
	//initialize barrier type
	mutextInit(&(barrier.mutex));
	semaphoreInit(&(barrier.turnstile), 0);
	barrier.count = 0;
	barrier.n = 4;

	//start timer task
	rtosThreadNew(ledTimerTask, NULL);
	
	//start both print tasks
	rtosThreadNew(printTask, (void *)name1);
	rtosThreadNew(printTask, (void *)name2);
	
	//start lazy GLCD task
	
	while(1);
}
