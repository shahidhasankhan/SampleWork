#include <stdio.h>
#include <stdint.h>
#include "main.h"

void task1_handler(void);
void task2_handler(void);
void task3_handler(void);
void default_task(void);

__attribute__((naked)) void init_scheduler_stack(uint32_t scheduler_stack_top);
void init_task_handlers(void);
void init_task_stacks(void);
void init_systick_timer(uint32_t tick_hz);

void enable_processor_faults(void);
uint32_t get_PSP_value(void);
void set_PSP_value(uint32_t current_task_stack_top);
__attribute__((naked)) void switch_SP_to_PSP(void);
void switch_to_next_Task(void);

void block_task(uint32_t tick_count);
void update_global_tick_count(void);
void unblock_tasks(void);
void trigger_PendSV(void);

uint8_t  current_task = 0;
uint32_t g_tick_count = 0;

typedef struct
{
	uint32_t task_PSP;
	uint32_t block_count;
	uint8_t current_state;
	void (*task_handler)(void);
}TCB_t;

TCB_t application_tasks[MAX_TASKS];

int main(void)
{
	enable_processor_faults();

	init_scheduler_stack(SCHEDULER_STACK_START);
	init_task_handlers();
	init_task_stacks();
	init_systick_timer(TICK_HZ);

	switch_SP_to_PSP();

	task1_handler();
    /* Loop forever */
	for(;;);
}

void task1_handler(void)
{
	while(1)
	{
		printf("TASK1\n");
		block_task(1000);
	}
}

void task2_handler(void)
{
	while(1)
	{
		printf("TASK2\n");
		block_task(1000);
	}
}

void task3_handler(void)
{
	while(1)
	{
		printf("TASK3\n");
		block_task(1000);
	}
}

void default_task(void)
{
	//Put MCU in Sleep Mode
	while(1)
	{
		//printf("Default\n");
	}
}

__attribute__((naked)) void init_scheduler_stack(uint32_t scheduler_stack_top)
{
	//Load Value of scheduler stack top into MSP
	__asm volatile("MSR MSP,%0"::"r"(scheduler_stack_top):);

	//Branch Back to Main
	__asm volatile("BX LR");
}

void init_task_handlers(void)
{
	application_tasks[0].current_state = TASK_RUNNING;
	application_tasks[0].task_PSP = DEFAULT_TASK_STACK_START;
	application_tasks[0].task_handler = default_task;

	application_tasks[1].current_state = TASK_RUNNING;
	application_tasks[1].task_PSP = TASK1_STACK_START;
	application_tasks[1].task_handler = task1_handler;

	application_tasks[2].current_state = TASK_RUNNING;
	application_tasks[2].task_PSP = TASK2_STACK_START;
	application_tasks[2].task_handler = task2_handler;

	application_tasks[3].current_state = TASK_RUNNING;
	application_tasks[3].task_PSP = TASK3_STACK_START;
	application_tasks[3].task_handler = task3_handler;

	//Task 0 will be default/idle task. Since all tasks are in Running State now so default task is not the current task
	current_task = 1;
}

void init_task_stacks(void)
{
	uint32_t *pPSP;
	for(int i=0;i<MAX_TASKS;i++)
	{
		pPSP = (uint32_t*)application_tasks[i].task_PSP;

		//Stack Implementation is of Full Descending Type so Decrement by 1 before Pushing New Data
		pPSP--;
		//Put Dummy XPSR
		*pPSP = 0x01000000U;  //Set T Bit (24th Bit) as 1 for Thumb Instruction Set Rest Are Ignored in the PSR

		//Put PC
		pPSP--;
		*pPSP = (uint32_t)application_tasks[i].task_handler;

		//Put LR
		pPSP--;
		*pPSP = 0xFFFFFFFD;  //EXC RETURN to Thread Mode with PSP as SP

		//Put R0 to R13
		for(int j=0;j<13;j++)
		{
			pPSP--;
			*pPSP = 0;
		}
		application_tasks[i].task_PSP = (uint32_t)pPSP;
	}
}

void init_systick_timer(uint32_t tick_hz)
{
	uint32_t *pSRVR = (uint32_t*)0xE000E014;             //SYSTICK RELOAD VALUE REGISTER ADDRESS
	uint32_t *pSCSR = (uint32_t*)0xE000E010;			 //SYSTICK CONTROL AND STATUS REGISTER ADDRESS

	uint32_t count_value = SYSTICK_TIMER_CLK /tick_hz -1;

	//Reset Any Preloaded Values but leave the last 8 reserved bits untouched
	*pSRVR &= ~(0x00FFFFFF);

	//Load Count Value into SRVR
	*pSRVR |= count_value;

	//Enable Systick Exception at 1st Bit and Set Clock Source as Processor at 2nd Bit by setting them to 1
	*pSCSR |= (3 << 1);

	//Enable Systick Counter by Setting 0th Bit of SCSR to 1
	*pSCSR |= (1 << 0);

}

void enable_processor_faults(void)
{
	uint32_t *pSHCSR = (uint32_t*)0xE000ED24;

	//Memory Management Fault at 16th Bit
	//Bus Fault at 17th Bit
	//Usage Fault at 18th Bit
	// 7 = 0b111
	*pSHCSR |= (7 << 16);
}

uint32_t get_PSP_value()
{
	return application_tasks[current_task].task_PSP;
}

__attribute__((naked)) void switch_SP_to_PSP(void)
{
	//Save Return Address to Main because we will jump to another function in next instruction and LR Value will be changed
	__asm volatile ("PUSH {LR}");
	__asm volatile ("BL get_PSP_value"); //Return Value will be in Register R0
	//Set PSP value to Task Stack of Current Task
	__asm volatile ("MSR PSP,R0");
	//Pop the Return Address to Main back into LR
	__asm volatile ("POP {LR}");

	//Set CONTROL Register 1st Bit as 1 to Set MSP as Current Stack Point
	__asm volatile ("MOV R0,#0x02");
	__asm volatile ("MSR CONTROL,R0");

	//No Epilogue for Naked functions so Go Back to Main manually
	__asm volatile ("BX LR");
}

void set_PSP_value(uint32_t current_task_stack_top)
{
	application_tasks[current_task].task_PSP = current_task_stack_top;
}

void switch_to_next_Task(void)
{
	for(int i=1;i<MAX_TASKS;i++)
	{
		current_task++;
		//Get Back to First Task in Round Robin if Last Task on Queue is Done
		current_task %= MAX_TASKS;
		if(application_tasks[i].current_state == TASK_RUNNING) //better to get current state in separate variable
			break;
	}
	//After going through all tasks, the current task is still blocked, then run default task
	if(application_tasks[current_task].current_state != TASK_RUNNING)
		current_task = 0;
}

void block_task(uint32_t tick_count)
{
	if(current_task != 0) //Default Task Should Not Be Blocked since its default
	{
		application_tasks[current_task].block_count = g_tick_count + tick_count;
		application_tasks[current_task].current_state = TASK_BLOCKED;
		trigger_PendSV();
	}
}

void unblock_tasks(void)
{
	for(int i=1;i<MAX_TASKS;i++)
	{
		if(application_tasks[i].current_state == TASK_BLOCKED && application_tasks[i].block_count == g_tick_count)
		{
			application_tasks[i].current_state = TASK_RUNNING;
		}
	}
}

void update_global_tick_count(void)
{
	g_tick_count++;
}

void trigger_PendSV(void)
{
	uint32_t *pICSR = (uint32_t*)0xE000ED04;
	*pICSR |= (1 << 28);
}

void HardFault_Handler(void)
{
	printf("Hard Fault Occurred\n");
	while(1);
}

void MemManage_Handler(void)
{
	printf("Memory Management Fault Occurred\n");
	while(1);
}

void BusFault_Handler(void)
{
	printf("Bus Fault Occurred\n");
	while(1);
}

__attribute__((naked)) void PendSV_Handler(void)
{
	//Get PSP of Current Task
	__asm volatile("MRS R0,PSP");
	//R0-R3,R12,LR (Stack Frame 1)/Caller Saved Registers will be Automatically Saved at Function Call Time
	//Store Values of R4-R11 (SF2)/Callee Saved Registers as per Procedure Call Standard of AAPCS
	//We can not use PUSH because it will change the SP (PSP since we're in Privileged Mode)
	__asm volatile("STMDB R0!,{R4-R11}");

	//Push LR i.e Return Address to Main in Main Stack
	//This is because we are going to jump to other functions and its value will be changed
	__asm volatile("PUSH {LR}");

	//Set Updated PSP Value (after pushing SF2 to Global PSP of Current Task
	//According to AAPCS the called function will take its input from R0
	__asm volatile("BL set_PSP_value");

	//Context of Current Task is Saved, Now Load Context of Next Task
	__asm volatile("BL switch_to_next_Task");
	__asm volatile("BL get_PSP_value");  //Return Value i.e PSP of Next Task is in R0

	//Load SF2/Callee Saved Registers/R4-R11 Back into the SFRs
	__asm volatile("LDMIA R0!,{R4-R11}");
	//Set PSP to Top of Next Tasks Stack Top
	__asm volatile("MSR PSP,R0");

	//Pop LR i.e Return Address to Main back from Main Stack
	__asm volatile("POP {LR}");

	//This is a Naked Function so there is no Epilogue Sequence and we have to manually return to main
	__asm volatile("BX LR");
}

void SysTick_Handler(void)
{
	update_global_tick_count();
	unblock_tasks();
	trigger_PendSV();
}
