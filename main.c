#include <LPC17xx.h>
#include "RTOS.h"
#include <stdio.h>

void add(uint32_t *a, uint8_t *b){
	*a += *b;
}

void task1(void *args){
	uint32_t counter1 = (uint32_t)args;
	uint8_t test = 69;
	while(1){
		//printf("1: Task 1!\n");
		counter1--;
		add(&counter1, &test);
	}
}

void task2(void *args){
	uint32_t counter2 = 0;
	while(1){
		printf("Task 2 Counter: %d!\n", counter2);
		rtosWait(1000);
		counter2++;
	}
}

int main(void){
	rtosInit();
	uint32_t counter = 255;

	printf("0: Main Task!\n");
	
	//rtosThreadNew(task1, (void *)100);
	rtosThreadNew(task2, NULL);
	while(1){
		counter++;
	}
}
