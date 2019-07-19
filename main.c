#include <LPC17xx.h>
#include "RTOS.h"
#include <stdio.h>

void task1(void *args){
	uint32_t counter1 = 0;
	while(1){
		counter1++;
	}
}

int main(void){
	rtosInit();
	uint32_t counter = 0;
	
	rtosThreadNew(task1, NULL);
	while(1){
		counter++;
	}
}
