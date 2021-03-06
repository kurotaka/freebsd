/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 *
 * $FreeBSD$
 */
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include "assym.s"

#define _ASM

#include <sys/cpuvar_defs.h>
#include <sys/dtrace.h>

#include <machine/asm.h>
/*
#include <machine/cpu.h>
*/

/*
 * Primitives
 */

        .text

/*
void dtrace_membar_producer(void)
*/
ASENTRY_NOPROF(dtrace_membar_producer)
	blr
END(dtrace_membar_producer)

/*
void dtrace_membar_consumer(void)
*/
ASENTRY_NOPROF(dtrace_membar_consumer)
	blr
END(dtrace_membar_consumer)

/*
dtrace_icookie_t dtrace_interrupt_disable(void)
*/
ASENTRY_NOPROF(dtrace_interrupt_disable)
	mfmsr	%r3
	andi.	%r0,%r3,~PSL_EE@l
	mtmsr	%r0
	blr
END(dtrace_interrupt_disable)

/*
void dtrace_interrupt_enable(dtrace_icookie_t cookie)
*/
ASENTRY_NOPROF(dtrace_interrupt_enable)
	mtmsr	%r3
	blr
END(dtrace_interrupt_enable)

/*
uint32_t dtrace_cas32(uint32_t *target, uint32_t cmp, uint32_t new)
*/
ASENTRY_NOPROF(dtrace_cas32)
1:
	lwarx	%r0,0,%r3
	cmpw	%r4,%r0
	bne		2f
	stwcx.	%r5,0,%r3
	bne		1b
2:	mr		%r3,%r0
	blr
END(dtrace_cas32)

/*
void *
dtrace_casptr(void *target, void *cmp, void *new)
*/
ASENTRY_NOPROF(dtrace_casptr)
1:
	lwarx	%r0,0,%r3
	cmpw	%r4,%r0
	bne		2f
	stwcx.	%r5,0,%r3
	bne		1b
2:	mr		%r3,%r0
	blr
END(dtrace_casptr)


/*
uintptr_t
dtrace_fulword(void *addr)
*/
ASENTRY_NOPROF(dtrace_fulword)
END(dtrace_fulword)

/*
uint8_t
dtrace_fuword8_nocheck(void *addr)
*/
ASENTRY_NOPROF(dtrace_fuword8_nocheck)
	lbz	%r3,0(%r3)
	blr
END(dtrace_fuword8_nocheck)

/*
uint16_t
dtrace_fuword16_nocheck(void *addr)
*/
ASENTRY_NOPROF(dtrace_fuword16_nocheck)
	lhz	%r3,0(%r3)
	blr
END(dtrace_fuword16_nocheck)

/*
uint32_t
dtrace_fuword32_nocheck(void *addr)
*/
ASENTRY_NOPROF(dtrace_fuword32_nocheck)
	lwz	%r3,0(%r3)
	blr
END(dtrace_fuword32_nocheck)

/*
uint64_t
dtrace_fuword64_nocheck(void *addr)
*/
ASENTRY_NOPROF(dtrace_fuword64_nocheck)
#if defined(__powerpc64__)
	ld	%r3,0(%r3)
#else
	lwz	%r5,0(%r3)
	lwz	%r4,4(%r3)
	mr	%r3,%r5
#endif
	blr
END(dtrace_fuword64_nocheck)

/*
XXX: unoptimized
void
dtrace_copy(uintptr_t src, uintptr_t dest, size_t size)
*/
ASENTRY_NOPROF(dtrace_copy)
	addme	%r7,%r3
	addme	%r8,%r4
1:
	lbzu	%r3,1(%r7)
	stbu	%r3,1(%r8)
	addme	%r5,%r5
	beq		2f
2:
	blr
END(dtrace_copy)

/*
void
dtrace_copystr(uintptr_t uaddr, uintptr_t kaddr, size_t size,
    volatile uint16_t *flags)
*/
ASENTRY_NOPROF(dtrace_copystr)
	addme	%r7,%r3
	addme	%r8,%r4
1:
	lbzu	%r3,1(%r7)
	stbu	%r3,1(%r8)
	addme	%r5,%r5
	beq		2f
	or		%r3,%r3,%r3
	beq		2f
	andi.	%r0,%r5,0x0fff
	beq		2f
	lwz		%r0,0(%r6)
	andi.	%r0,%r0,CPU_DTRACE_BADADDR
	beq		1b
2:
	blr
END(dtrace_copystr)

/*
void dtrace_invop_init(void)
*/
ASENTRY_NOPROF(dtrace_invop_init)
	/* XXX: impement it properly -- implement dtrace_invop_start */
	li		%r0,0
	li		%r3,dtrace_invop_jump_addr@l
	addis	%r3,%r3,dtrace_invop_jump_addr@ha
	stw		%r0,0(%r3)
	blr
END(dtrace_invop_init)

/*
void dtrace_invop_uninit(void)
*/
ASENTRY_NOPROF(dtrace_invop_uninit)
	li		%r0,0
	li		%r3,dtrace_invop_jump_addr@l
	addis	%r3,%r3,dtrace_invop_jump_addr@ha
	stw		%r0,0(%r3)
	blr
END(dtrace_invop_uninit)

/*
 * The panic() and cmn_err() functions invoke vpanic() as a common entry point
 * into the panic code implemented in panicsys().  vpanic() is responsible
 * for passing through the format string and arguments, and constructing a
 * regs structure on the stack into which it saves the current register
 * values.  If we are not dying due to a fatal trap, these registers will
 * then be preserved in panicbuf as the current processor state.  Before
 * invoking panicsys(), vpanic() activates the first panic trigger (see
 * common/os/panic.c) and switches to the panic_stack if successful.  Note that
 * DTrace takes a slightly different panic path if it must panic from probe
 * context.  Instead of calling panic, it calls into dtrace_vpanic(), which
 * sets up the initial stack as vpanic does, calls dtrace_panic_trigger(), and
 * branches back into vpanic().
 */

/*
void
vpanic(const char *format, va_list alist)
*/
ASENTRY_NOPROF(vpanic)				/* Initial stack layout: */
	
vpanic_common:
	blr
END(vpanic)



/*
void
dtrace_vpanic(const char *format, va_list alist)
*/
ASENTRY_NOPROF(dtrace_vpanic)			/* Initial stack layout: */

#if 0
	bl	dtrace_panic_trigger	/* %eax = dtrace_panic_trigger() */
#endif
	b	vpanic_common
END(dtrace_vpanic)

/*
uintptr_t
dtrace_caller(int aframes)
*/
ASENTRY_NOPROF(dtrace_caller)
	li	%r3, -1
	blr
END(dtrace_caller)

