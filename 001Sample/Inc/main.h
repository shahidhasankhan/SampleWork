#ifndef MAIN_H_
#define MAIN_H_

#define MAX_TASKS			 	3
#define SIZE_TASK_STACK  	 	1024U
#define SIZE_SCHEDULER_STACK 	1024U

#define SRAM_SIZE 			 	((128)*(1024))
#define SRAM_START 			 	0x20000000U
#define SRAM_END 			 	((SRAM_START)+(SRAM_SIZE))

#define TASK1_STACK_START 	 	SRAM_END
#define TASK2_STACK_START 	 	((TASK1_STACK_START)-(SIZE_TASK_STACK))
#define TASK3_STACK_START 	 	((TASK2_STACK_START)-(SIZE_TASK_STACK))
#define SCHEDULER_STACK_START 	((TASK3_STACK_START)-(SIZE_TASK_STACK))

#define HSI_CLK 				16000000U
#define SYSTICK_TIMER_CLK 		HSI_CLK
#define TICK_HZ 				1000U

#define DUMMY_XPSR				0x01000000U

#endif /* MAIN_H_ */
