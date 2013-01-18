/*-
 * Copyright (c) 2012 KUROSAWA Takahiro <takahiro.kurosawa@gmail.com>
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
 *
 * $FreeBSD$
 */

#ifndef	_TCWD_H_
#define	_TCWD_H_

struct tcwd_device {
	uint16_t		device;
	char			*desc;
};

struct tcwd_softc {
	device_t		dev;

	int			wd_rid;
	struct resource		*wd_res;
	int			gpio_rid;
	struct resource		*gpio_res;

	eventhandler_tag	ev_tag;
	unsigned int		timeout;
	int			active;
};

#define	VENDORID_INTEL		0x8086
#define	DEVICEID_TC_LPC		0x8186

/*
 * E6xx LPC Interface Registers
 * from Table 275 in Intel(R) Atom(TM) Processor E6xx Series Datasheet:
 *   http://download.intel.com/embedded/processor/datasheet/324208.pdf
 */
#define	TC_LPC_SMBA		0x40	/* SMBus Base Address */
#define	TC_LPC_GBA		0x44	/* GPIO Base Address */
#define	TC_LPC_WDTBA		0x84	/* WDT Base Address */
#define	TC_WDTBA_EN		(1U << 31)	/* WDT IO Decode Enable */
#define	TC_WDTBA_MASK		0xffff		/* WDT Base Address Mask */
#define	TC_GPIO_EN		(1U << 31)	/* GPIO IO Decode Enable */
#define	TC_GPIO_MASK		0xffff		/* GPIO Base Address Mask */

#define TC_WD_REG_LEN		64
#define TC_GPIO_REG_LEN		64

/*
 * Watchdog Timer Registers from Table 387-400.
 */
#define	TC_WD_PV1		0x00	/* Preload Value 1 Register */
#define	TC_WD_PV2		0x04	/* Preload Value 2 Register */
#define	TC_WD_RR0		0x0c	/* Reload Register 0 */
#define	TC_WD_RR1		0x0d	/* Reload Register 1 */
#define	TC_WD_RR1_TOUT		(1U << 1)	/* WDT Timeout Flag */
#define	TC_WD_RR1_RELOAD	(1U << 0)	/* WDT Reload Flag */
#define	TC_WD_WDTCR		0x10	/* WDT Configuration Register */
#define	TC_WD_WDTCR_TOUT_EN	(1U << 5)	/* WDT Timeout Output Enable */
#define	TC_WD_WDTCR_RESET_EN	(1U << 4)	/* WDT Reset Enable */
#define	TC_WD_WDTCR_RESET_SEL	(1U << 3)	/* WDT Reset Select */
#define	TC_WD_WDTCR_PRE_SEL	(1U << 2)	/* WDT Prescaler Select */
#define	TC_WD_DCR		0x14	/* Down Counter Register */
#define	TC_WD_WDTLR		0x18	/* WDT Lock Register */
#define	TC_WD_WDTLR_TOUT_CNF	(1U << 2)	/* WDT Timeout Configuration */
#define	TC_WD_WDTLR_WDT_ENABLE	(1U << 1)	/* Watchdog Timer Enable */
#define	TC_WD_WDTLR_WDT_LOCK	(1U << 0)	/* Watchdog Timer Lock */

/*
 * GPIO Registers from Table 340.
 */
#define	TC_GPIO_CGEN		0x00	/* GPIO Enable */
#define	TC_GPIO_CGEN_GPIO4	(1U << 4)	/* GPIO[4] Enable */

#endif	/* _TCWD_H_ */
