#include <stdint.h>
#include "sched.h"
#include "job.h"


#define portSTACK_TYPE    uint32_t
typedef portSTACK_TYPE StackType_t;
typedef void (* TaskFunction_t)( void * );

/* Constants required to set up the initial stack. */
#define portINITIAL_XPSR			( 0x01000000 )
#define portINITIAL_EXC_RETURN			( 0xfffffffd )

/* For strict compliance with the Cortex-M spec the task start address should
 * have bit-0 clear, as it is loaded into the PC on exit from an ISR. */
#define portSTART_ADDRESS_MASK			( ( StackType_t ) 0xfffffffeUL )

#define portTASK_RETURN_ADDRESS		prvTaskExitError

#define STACK_SIZE	512

/* Task control block.  A task control block (TCB) is allocated for each task,
 * and stores task state information, including a pointer to the task's context
 * (the task's run time environment, including register values) */
typedef struct tskTaskControlBlock /* The old naming convention is used to prevent breaking kernel aware debuggers. */
{
	/* Points to the location of the last item placed on the tasks stack.
	THIS MUST BE THE FIRST MEMBER OF THE TCB STRUCT. */
	volatile StackType_t *pxTopOfStack;
	StackType_t Stack[STACK_SIZE] __attribute__((aligned(8)));
} tskTCB;

/* The old tskTCB name is maintained above then typedefed to the new TCB_t name
 * below to enable the use of older kernel aware debuggers. */
typedef tskTCB TCB_t;


static void prvTaskExitError(void)
{
	for ( ; ; ) { }
}


static StackType_t * pxPortInitialiseStack(
	StackType_t * pxTopOfStack,
	TaskFunction_t pxCode,
	void * pvParameters)
{
	/* Simulate the stack frame as it would be created by a context switch
	* interrupt. */

	/* Offset added to account for the way the MCU uses the stack on entry/exit
	* of interrupts, and to ensure alignment. */
	pxTopOfStack--;

	*pxTopOfStack = portINITIAL_XPSR;                                    /* xPSR */
	pxTopOfStack--;
	*pxTopOfStack = ( ( StackType_t ) pxCode ) & portSTART_ADDRESS_MASK; /* PC */
	pxTopOfStack--;
	*pxTopOfStack = ( StackType_t ) portTASK_RETURN_ADDRESS;             /* LR */

	/* Save code space by skipping register initialisation. */
	pxTopOfStack -= 5;                            /* R12, R3, R2 and R1. */
	*pxTopOfStack = ( StackType_t ) pvParameters; /* R0 */

	/* A save method is being used that requires each task to maintain its
	* own exec return value. */
	pxTopOfStack--;
	*pxTopOfStack = portINITIAL_EXC_RETURN;

	pxTopOfStack -= 8; /* R11, R10, R9, R8, R7, R6, R5 and R4. */

	return pxTopOfStack;
}

static void prvPortStartFirstTask(void)
{
	/* Start the first task.  This also clears the bit that indicates the FPU is
	 * in use in case the FPU was used before the scheduler was started - which
	 * would otherwise result in the unnecessary leaving of space in the SVC stack
	 * for lazy saving of FPU registers. */
	__asm volatile (
	" ldr r0, =0xE000ED08	\n" /* Use the NVIC offset register to locate the stack. */
	" ldr r0, [r0]		\n"
	" ldr r0, [r0]		\n"
	" msr msp, r0		\n" /* Set the msp back to the start of the stack. */
	" mov r0, #0		\n" /* Clear the bit that indicates the FPU is in use, see comment above. */
	" msr control, r0	\n"
	" cpsie i		\n" /* Globally enable interrupts. */
	" cpsie f		\n"
	" dsb			\n"
	" isb			\n"
	" svc 0			\n" /* System call to start first task. */
	" nop			\n"
	" .ltorg		\n"
	);
}

TCB_t *volatile pxCurrentTCB;

void start_scheduler(void)
{
	static TCB_t tcb_one = { 0 };
	static TCB_t tcb_two = { 0 };
	tcb_one.pxTopOfStack = pxPortInitialiseStack(&tcb_one.Stack[STACK_SIZE - 1], &job1, 0);
	tcb_two.pxTopOfStack = pxPortInitialiseStack(&tcb_two.Stack[STACK_SIZE - 1], &job2, 0);
	pxCurrentTCB = &tcb_two;
	prvPortStartFirstTask();
}

__attribute__((naked)) void vPortSVCHandler(void)
{
	__asm volatile (
	"ldr r3, pxCurrentTCBConst2	\n" /* Restore the context. */
	"ldr r1, [r3]			\n" /* Use pxCurrentTCBConst to get the pxCurrentTCB address. */
	"ldr r0, [r1]			\n" /* The first item in pxCurrentTCB is the task top of stack. */
	"ldmia r0!, {r4-r11, r14}	\n" /* Pop the registers that are not automatically saved on exception entry and the critical nesting count. */
	"msr psp, r0			\n" /* Restore the task stack pointer. */
	"isb				\n"
	"mov r0, #0			\n"
	"msr basepri, r0		\n"
	"bx r14				\n"
	"				\n"
	".align 4			\n"
	"pxCurrentTCBConst2: .word pxCurrentTCB	\n"
	);
}
