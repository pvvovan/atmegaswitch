#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stddef.h>
#include <avr/interrupt.h>

static constexpr uint8_t PB5_MASK {0b0010'0000};


/* ================================================================================================================ */

uint8_t ctx[1024];

typedef uint8_t                     StackType_t;
typedef void (* TaskFunction_t)( void * );
#define portFLAGS_INT_ENABLED           ( (StackType_t) 0x80 )


typedef struct tskTaskControlBlock       /* The old naming convention is used to prevent breaking kernel aware debuggers. */
{
    volatile StackType_t* pxTopOfStack; /*< Points to the location of the last item placed on the tasks stack.  THIS MUST BE THE FIRST MEMBER OF THE TCB STRUCT. */
    StackType_t* pxStack;                      /*< Points to the start of the stack. */
} TCB_t;

volatile TCB_t* volatile pxCurrentTCB;

StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters )
{
	uint16_t usAddress;
    /* Simulate how the stack would look after a call to vPortYield() generated by
    the compiler. */

    /* The start of the task code will be popped off the stack last, so place
    it on first. */
    usAddress = ( uint16_t ) pxCode;
    *pxTopOfStack = ( StackType_t ) ( usAddress & ( uint16_t ) 0x00ff );
    pxTopOfStack--;

    usAddress >>= 8;
    *pxTopOfStack = ( StackType_t ) ( usAddress & ( uint16_t ) 0x00ff );
    pxTopOfStack--;

    /* Next simulate the stack as if after a call to portSAVE_CONTEXT().
    portSAVE_CONTEXT places the flags on the stack immediately after r0
    to ensure the interrupts get disabled as soon as possible, and so ensuring
    the stack use is minimal should a context switch interrupt occur. */
    *pxTopOfStack = ( StackType_t ) 0x00;    /* R0 */
    pxTopOfStack--;
    *pxTopOfStack = portFLAGS_INT_ENABLED;
    pxTopOfStack--;

    /* Now the remaining registers. The compiler expects R1 to be 0. */
    *pxTopOfStack = ( StackType_t ) 0x00;    /* R1 */

    /* Leave R2 - R23 untouched */
    pxTopOfStack -= 23;

    /* Place the parameter on the stack in the expected location. */
    usAddress = ( uint16_t ) pvParameters;
    *pxTopOfStack = ( StackType_t ) ( usAddress & ( uint16_t ) 0x00ff );
    pxTopOfStack--;

    usAddress >>= 8;
    *pxTopOfStack = ( StackType_t ) ( usAddress & ( uint16_t ) 0x00ff );

    /* Leave register R26 - R31 untouched */
    pxTopOfStack -= 7;

    return pxTopOfStack;
}

static void xTaskCreate2(TaskFunction_t pxTaskCode, void *const pvParameters)
{
	constexpr int N{80};
	static int pos{N};
	TCB_t* pxNewTCB = (TCB_t*)&ctx[pos+N];
	pxNewTCB->pxStack = (StackType_t*)&ctx[pos];
	pos += N;
	if (pos >= 1024-N-N) {
		pos = N;
	}

	pxNewTCB->pxTopOfStack = pxPortInitialiseStack(pxNewTCB->pxStack, pxTaskCode, pvParameters);
	pxCurrentTCB = pxNewTCB;
}

#define portRESTORE_CONTEXT()                                                           \
        __asm__ __volatile__ (  "lds    r26, pxCurrentTCB                       \n\t"   \
                                "lds    r27, pxCurrentTCB + 1                   \n\t"   \
                                "ld     r28, x+                                 \n\t"   \
                                "out    __SP_L__, r28                           \n\t"   \
                                "ld     r29, x+                                 \n\t"   \
                                "out    __SP_H__, r29                           \n\t"   \
                                "pop    r31                                     \n\t"   \
                                "pop    r30                                     \n\t"   \
                                "pop    r29                                     \n\t"   \
                                "pop    r28                                     \n\t"   \
                                "pop    r27                                     \n\t"   \
                                "pop    r26                                     \n\t"   \
                                "pop    r25                                     \n\t"   \
                                "pop    r24                                     \n\t"   \
                                "pop    r23                                     \n\t"   \
                                "pop    r22                                     \n\t"   \
                                "pop    r21                                     \n\t"   \
                                "pop    r20                                     \n\t"   \
                                "pop    r19                                     \n\t"   \
                                "pop    r18                                     \n\t"   \
                                "pop    r17                                     \n\t"   \
                                "pop    r16                                     \n\t"   \
                                "pop    r15                                     \n\t"   \
                                "pop    r14                                     \n\t"   \
                                "pop    r13                                     \n\t"   \
                                "pop    r12                                     \n\t"   \
                                "pop    r11                                     \n\t"   \
                                "pop    r10                                     \n\t"   \
                                "pop    r9                                      \n\t"   \
                                "pop    r8                                      \n\t"   \
                                "pop    r7                                      \n\t"   \
                                "pop    r6                                      \n\t"   \
                                "pop    r5                                      \n\t"   \
                                "pop    r4                                      \n\t"   \
                                "pop    r3                                      \n\t"   \
                                "pop    r2                                      \n\t"   \
                                "pop    __zero_reg__                            \n\t"   \
                                "pop    __tmp_reg__                             \n\t"   \
                                "out    __SREG__, __tmp_reg__                   \n\t"   \
                                "pop    __tmp_reg__                             \n\t"   \
                             );

#define portSAVE_CONTEXT()                                                              \
        __asm__ __volatile__ (  "push   __tmp_reg__                             \n\t"   \
                                "in     __tmp_reg__, __SREG__                   \n\t"   \
                                "cli                                            \n\t"   \
                                "push   __tmp_reg__                             \n\t"   \
                                "push   __zero_reg__                            \n\t"   \
                                "clr    __zero_reg__                            \n\t"   \
                                "push   r2                                      \n\t"   \
                                "push   r3                                      \n\t"   \
                                "push   r4                                      \n\t"   \
                                "push   r5                                      \n\t"   \
                                "push   r6                                      \n\t"   \
                                "push   r7                                      \n\t"   \
                                "push   r8                                      \n\t"   \
                                "push   r9                                      \n\t"   \
                                "push   r10                                     \n\t"   \
                                "push   r11                                     \n\t"   \
                                "push   r12                                     \n\t"   \
                                "push   r13                                     \n\t"   \
                                "push   r14                                     \n\t"   \
                                "push   r15                                     \n\t"   \
                                "push   r16                                     \n\t"   \
                                "push   r17                                     \n\t"   \
                                "push   r18                                     \n\t"   \
                                "push   r19                                     \n\t"   \
                                "push   r20                                     \n\t"   \
                                "push   r21                                     \n\t"   \
                                "push   r22                                     \n\t"   \
                                "push   r23                                     \n\t"   \
                                "push   r24                                     \n\t"   \
                                "push   r25                                     \n\t"   \
                                "push   r26                                     \n\t"   \
                                "push   r27                                     \n\t"   \
                                "push   r28                                     \n\t"   \
                                "push   r29                                     \n\t"   \
                                "push   r30                                     \n\t"   \
                                "push   r31                                     \n\t"   \
                                "lds    r26, pxCurrentTCB                       \n\t"   \
                                "lds    r27, pxCurrentTCB + 1                   \n\t"   \
                                "in     __tmp_reg__, __SP_L__                   \n\t"   \
                                "st     x+, __tmp_reg__                         \n\t"   \
                                "in     __tmp_reg__, __SP_H__                   \n\t"   \
                                "st     x+, __tmp_reg__                         \n\t"   \
                             );

size_t fs[2];

static void slow(void* mask)
{
	uint8_t PB5_MASK = *(uint8_t*)mask;

	for ( ; ; ) {
		_delay_ms(1000);
		PORTB ^= PB5_MASK;
		static int c{0};
		if (c++ > 3) {
			break;
		}
		portSAVE_CONTEXT();
		portRESTORE_CONTEXT();
	}

	static uint8_t s_mask = 0b0010'0000;
	xTaskCreate2((void (*)(void*))fs[0], &s_mask);
	portRESTORE_CONTEXT();
	__asm__ __volatile__ ( "ret" );
}

static void fast(void* mask)
{
	uint8_t PB5_MASK = *(uint8_t*)mask;

	for ( ; ; ) {
		_delay_ms(100);
		PORTB ^= PB5_MASK;
		static int c{};
		if (++c > 50) {
			break;
		}
		portSAVE_CONTEXT();
		portRESTORE_CONTEXT();
	}

	static uint8_t s_mask = 0b0010'0000;
	xTaskCreate2((void (*)(void*))fs[1], &s_mask);
	portRESTORE_CONTEXT();
	__asm__ __volatile__ ( "ret" );
}

/* ================================================================================================================ */


int main()
{
	cli();
	PORTB = PB5_MASK;
	DDRB  = PB5_MASK;
	_delay_ms(200);

	fs[0] = (size_t)&fast;
	fs[1] = (size_t)&slow;

	static uint8_t s_mask = 0b0010'0000;
	xTaskCreate2((void (*)(void*))fs[1], &s_mask);
	portRESTORE_CONTEXT();
	__asm__ __volatile__ ( "ret" );

	while (true) {
		_delay_ms(2500);
		PORTB ^= PB5_MASK;
	}

	return 0;
}
