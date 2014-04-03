/*-
 * Copyright (c) 2005 Poul-Henning Kamp
 * Copyright (c) 2010 Alexander Motin <mav@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $FreeBSD$ */

#ifndef __HPETVAR_H__
#define	__HPETVAR_H__

struct hpet_softc {
	device_t		dev;
	int			mem_rid;
	int			intr_rid;
	int			irq;
	int			useirq;
	int			legacy_route;
	int			per_cpu;
	uint32_t		allowed_irqs;
	struct resource		*mem_res;
	struct resource		*intr_res;
	void			*intr_handle;
	ACPI_HANDLE		handle;
	uint64_t		freq;
	uint32_t		caps;
	struct timecounter	tc;
	struct hpet_timer {
		struct eventtimer	et;
		struct hpet_softc	*sc;
		int			num;
		int			mode;
		int			intr_rid;
		int			irq;
		int			pcpu_cpu;
		int			pcpu_misrouted;
		int			pcpu_master;
		int			pcpu_slaves[MAXCPU];
		struct resource		*intr_res;
		void			*intr_handle;
		uint32_t		caps;
		uint32_t		vectors;
		uint32_t		div;
		uint32_t		next;
		char			name[8];
	} 			t[32];
	int			num_timers;
};

#endif /* !__HPETVAR_H__ */
