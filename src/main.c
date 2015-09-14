/*
 *  Copyright (C) 2008-2015 Marvell International Ltd.
 *  All Rights Reserved.
 */
/*
 * Hello World Application
 *
 * Summary:
 *
 * Prints Hello World every 5 seconds on the serial console.
 * The serial console is set on UART-0.
 *
 * A serial terminal program like HyperTerminal, putty, or
 * minicom can be used to see the program output.
 */
#include <wmstdio.h>
#include <wm_os.h>


/***************************************************************************************
* set nvic to the lowest execution level, and use nvic to do context switch
****************************************************************************************/
#define NVIC_PendSV_PRIO    ((1 << __NVIC_PRIO_BITS) - 1)

#define preempt_disable()   __set_BASEPRI(((NVIC_PendSV_PRIO) << (8 - __NVIC_PRIO_BITS)) & 0xff)
#define preempt_enable()    __set_BASEPRI(0)
    

/**********************************************************
* use exclusive access to implement mutex, spin_lock & semhoare
**********************************************************/
typedef struct {
	int counter;
} atomic_t;

/*
 * ARMv6 UP and SMP safe atomic ops.  We use load exclusive and
 * store exclusive to ensure that these are atomic.  We may loop
 * to ensure that the update happens.
 */

extern inline void prefetchw(const void *ptr)  
{
	__builtin_prefetch(ptr, 1, 3);
}

//define memory Barry
#define dmb(option) __asm__ __volatile__ ("dmb " #option : : : "memory")
#define smp_mb()	dmb(ish)


#define ATOMIC_OP(op, c_op, asm_op)					\
static inline void atomic_##op(int i, atomic_t *v)			\
{									\
	unsigned long tmp;						\
	int result;							\
									\
	prefetchw(&v->counter);						\
	__asm__ __volatile__("@ atomic_" #op "\n"			\
"1:	ldrex	%0, [%3]\n"						\
"	" #asm_op "	%0, %0, %4\n"					\
"	strex	%1, %0, [%3]\n"						\
"	teq	%1, #0\n"						\
"	bne	1b"							\
	: "=&r" (result), "=&r" (tmp), "+Qo" (v->counter)		\
	: "r" (&v->counter), "Ir" (i)					\
	: "cc");							\
}									\

#define ATOMIC_OP_RETURN(op, c_op, asm_op)				\
static inline int atomic_##op##_return(int i, atomic_t *v)		\
{									\
	unsigned long tmp;						\
	int result;							\
									\
	smp_mb();							\
	prefetchw(&v->counter);						\
									\
	__asm__ __volatile__("@ atomic_" #op "_return\n"		\
"1:	ldrex	%0, [%3]\n"						\
"	" #asm_op "	%0, %0, %4\n"					\
"	strex	%1, %0, [%3]\n"						\
"	teq	%1, #0\n"						\
"	bne	1b"							\
	: "=&r" (result), "=&r" (tmp), "+Qo" (v->counter)		\
	: "r" (&v->counter), "Ir" (i)					\
	: "cc");							\
									\
	smp_mb();							\
									\
	return result;							\
}

static inline int atomic_cmpxchg(atomic_t *ptr, int old, int new)
{
	int oldval;
	unsigned long res;

	smp_mb();
	prefetchw(&ptr->counter);

	do {
		__asm__ __volatile__("@ atomic_cmpxchg\n"
		"ldrex	%1, [%3]\n"
		"mov	%0, #0\n"
		"teq	%1, %4\n"
		"strexeq %0, %5, [%3]\n"
		    : "=&r" (res), "=&r" (oldval), "+Qo" (ptr->counter)
		    : "r" (&ptr->counter), "Ir" (old), "r" (new)
		    : "cc");
	} while (res);

	smp_mb();

	return oldval;
}

static inline int __atomic_add_unless(atomic_t *v, int a, int u)
{
	int oldval, newval;
	unsigned long tmp;

	smp_mb();
	prefetchw(&v->counter);

	__asm__ __volatile__ ("@ atomic_add_unless\n"
"1:	ldrex	%0, [%4]\n"
"	teq	%0, %5\n"
"	beq	2f\n"
"	add	%1, %0, %6\n"
"	strex	%2, %1, [%4]\n"
"	teq	%2, #0\n"
"	bne	1b\n"
"2:"
	: "=&r" (oldval), "=&r" (newval), "=&r" (tmp), "+Qo" (v->counter)
	: "r" (&v->counter), "r" (u), "r" (a)
	: "cc");

	if (oldval != u)
		smp_mb();

	return oldval;
}

#define ATOMIC_OPS(op, c_op, asm_op)					\
	ATOMIC_OP(op, c_op, asm_op)					\
	ATOMIC_OP_RETURN(op, c_op, asm_op)

ATOMIC_OPS(add, +=, add)
ATOMIC_OPS(sub, -=, sub)

#define atomic_inc_return(v)    (atomic_add_return(1, v))
#define atomic_dec_return(v)    (atomic_sub_return(1, v))

//the implementating of lock must associate with process schedule
void mutex_lock(atomic_t * count)
{
    if(atomic_dec_return(count) < 0) { //fail
        wmprintf("lock fail \r\n");
    } else {
        wmprintf("lock success \r\n");
    }
}


void mutex_unlock(atomic_t * count)
{
    if(atomic_inc_return(count) <= 0) { //fail 
        wmprintf("unlock fail \r\n");
    } else {
        wmprintf("unlock success \r\n");
    }
}


/*
 * The application entry point is main(). At this point, minimal
 * initialization of the hardware is done, and also the RTOS is
 * initialized.
 */
int main(void)
{
	int count = 0;

    atomic_t mutex;
    mutex.counter = 1;

	/* Initialize console on uart0 */
	wmstdio_init(UART0_ID, 0);

	wmprintf("Hello World application Started\r\n");
    
    mutex_lock(&mutex);
    wmprintf("the value of counter after lock %d \r\n",mutex.counter);
    
    wmprintf("mutex will try to lock again \r\n");
    mutex_lock(&mutex);
    wmprintf("value after relock is %d \r\n",mutex.counter);

    mutex_unlock(&mutex);
    wmprintf("the value of counter after unlock %d \r\n",mutex.counter);
	
    while (1) {
		count++;
		wmprintf("Hello World: iteration %d\r\n", count);

		/* Sleep  5 seconds */
		os_thread_sleep(os_msec_to_ticks(5000));
	}
	return 0;
}
