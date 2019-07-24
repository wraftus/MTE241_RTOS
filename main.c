#include <LPC17xx.h>
#include "RTOS.h"
#include <stdio.h>

void task1(void *args){
	uint32_t counter1 = 0;
	while(1){
		//printf("1: Task 1!\n");
		counter1++;
	}
}

void task2(void *args){
	uint32_t counter2 = 0;
	while(1){
		//printf("2: Task 2!\n");
		counter2++;
	}
}

int main(void){
	uint32_t counter = 255;
	counter-=1;
	uint32_t *counter_addr = &counter; 
	rtosInit();

	printf("0: Main Task!\n");
	
	rtosThreadNew(task1, NULL);
	rtosThreadNew(task2, NULL);
	while(1){
		counter++;
	}
}
