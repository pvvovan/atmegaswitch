#include <stdint.h>
#include "sched.h"
#include "job.h"


#define portSTACK_TYPE	uint32_t
typedef portSTACK_TYPE	StackType_t;
typedef void (* TaskFunction_t)( void * );

/* configMAX_SYSCALL_INTERRUPT_PRIORITY sets the interrupt priority above which
 * FreeRTOS API calls must not be made.  Interrupts above this priority are never
 * disabled, so never delayed by RTOS activity.  The default value is set to the
 * highest interrupt priority (0).  Not supported by all FreeRTOS ports.
 * See https://www.freertos.org/RTOS-Cortex-M3-M4.html for information specific to
 * ARM Cortex-M devices. */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY	0

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
static TCB_t tcb_one = { 0 };
static TCB_t tcb_two = { 0 };

void start_scheduler(void)
{
	__asm volatile ("cpsid i" : : : "memory");
	__asm volatile ("dsb" : : : "memory");
	__asm volatile ("isb" : : : "memory");
	tcb_one.pxTopOfStack = pxPortInitialiseStack(&tcb_one.Stack[STACK_SIZE - 1], &job1, 0);
	tcb_two.pxTopOfStack = pxPortInitialiseStack(&tcb_two.Stack[STACK_SIZE - 1], &job2, 0);
	pxCurrentTCB = &tcb_two;
	prvPortStartFirstTask();
}

void vTaskSwitchContext(void)
{
	static uint8_t i = 0;
	i++;
	if ((i % 2) == 1) {
		pxCurrentTCB = &tcb_one;
	} else {
		pxCurrentTCB = &tcb_two;
	}
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

__attribute__((naked)) void xPortPendSVHandler(void)
{
	/* This is a naked function. */
	__asm volatile (
	"mrs r0, psp                         \n"
	"isb                                 \n"
	"                                    \n"
	"ldr r3, pxCurrentTCBConst           \n" /* Get the location of the current TCB. */
	"ldr r2, [r3]                        \n"
	"                                    \n"
	"tst r14, #0x10                      \n" /* Is the task using the FPU context?  If so, push high vfp registers. */
	"it eq                               \n"
	"vstmdbeq r0!, {s16-s31}             \n"
	"                                    \n"
	"stmdb r0!, {r4-r11, r14}            \n" /* Save the core registers. */
	"str r0, [r2]                        \n" /* Save the new top of stack into the first member of the TCB. */
	"                                    \n"
	"stmdb sp!, {r0, r3}                 \n"
	"mov r0, %0                          \n"
	"cpsid i                             \n" /* ARM Cortex-M7 r0p1 Errata 837070 workaround. */
	"msr basepri, r0                     \n"
	"dsb                                 \n"
	"isb                                 \n"
	"cpsie i                             \n" /* ARM Cortex-M7 r0p1 Errata 837070 workaround. */
	"bl vTaskSwitchContext               \n"
	"mov r0, #0                          \n"
	"msr basepri, r0                     \n"
	"ldmia sp!, {r0, r3}                 \n"
	"                                    \n"
	"ldr r1, [r3]                        \n" /* The first item in pxCurrentTCB is the task top of stack. */
	"ldr r0, [r1]                        \n"
	"                                    \n"
	"ldmia r0!, {r4-r11, r14}            \n" /* Pop the core registers. */
	"                                    \n"
	"tst r14, #0x10                      \n" /* Is the task using the FPU context?  If so, pop the high vfp registers too. */
	"it eq                               \n"
	"vldmiaeq r0!, {s16-s31}             \n"
	"                                    \n"
	"msr psp, r0                         \n"
	"isb                                 \n"
	"                                    \n"
	#ifdef WORKAROUND_PMU_CM001 /* XMC4000 specific errata workaround. */
	#if WORKAROUND_PMU_CM001 == 1
	"	push { r14 }                \n"
	"	pop { pc }                  \n"
	#endif
	#endif
	"                                    \n"
	"bx r14                              \n"
	"                                    \n"
	".align 4                            \n"
	"pxCurrentTCBConst: .word pxCurrentTCB  \n"
	::"i" (configMAX_SYSCALL_INTERRUPT_PRIORITY)
	);
}
