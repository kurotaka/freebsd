/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/pciio.h>
#include <sys/ioctl.h>

#include <dev/io/iodev.h>
#include <dev/pci/pcireg.h>

#include <machine/iodev.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <machine/vmm.h>
#include <vmmapi.h>
#include "pci_emul.h"
#include "mem.h"

#ifndef _PATH_DEVPCI
#define	_PATH_DEVPCI	"/dev/pci"
#endif

#ifndef	_PATH_DEVIO
#define	_PATH_DEVIO	"/dev/io"
#endif

#define	LEGACY_SUPPORT	1

#define MSIX_TABLE_COUNT(ctrl) (((ctrl) & PCIM_MSIXCTRL_TABLE_SIZE) + 1)
#define MSIX_CAPLEN 12

static int pcifd = -1;
static int iofd = -1;

struct passthru_softc {
	struct pci_devinst *psc_pi;
	struct pcibar psc_bar[PCI_BARMAX + 1];
	struct {
		int		capoff;
		int		msgctrl;
		int		emulated;
	} psc_msi;
	struct {
		int		capoff;
	} psc_msix;
	struct pcisel psc_sel;
};

static int
msi_caplen(int msgctrl)
{
	int len;
	
	len = 10;		/* minimum length of msi capability */

	if (msgctrl & PCIM_MSICTRL_64BIT)
		len += 4;

#if 0
	/*
	 * Ignore the 'mask' and 'pending' bits in the MSI capability.
	 * We'll let the guest manipulate them directly.
	 */
	if (msgctrl & PCIM_MSICTRL_VECTOR)
		len += 10;
#endif

	return (len);
}

static uint32_t
read_config(const struct pcisel *sel, long reg, int width)
{
	struct pci_io pi;

	bzero(&pi, sizeof(pi));
	pi.pi_sel = *sel;
	pi.pi_reg = reg;
	pi.pi_width = width;

	if (ioctl(pcifd, PCIOCREAD, &pi) < 0)
		return (0);				/* XXX */
	else
		return (pi.pi_data);
}

static void
write_config(const struct pcisel *sel, long reg, int width, uint32_t data)
{
	struct pci_io pi;

	bzero(&pi, sizeof(pi));
	pi.pi_sel = *sel;
	pi.pi_reg = reg;
	pi.pi_width = width;
	pi.pi_data = data;

	(void)ioctl(pcifd, PCIOCWRITE, &pi);		/* XXX */
}

#ifdef LEGACY_SUPPORT
static int
passthru_add_msicap(struct pci_devinst *pi, int msgnum, int nextptr)
{
	int capoff, i;
	struct msicap msicap;
	u_char *capdata;

	pci_populate_msicap(&msicap, msgnum, nextptr);

	/*
	 * XXX
	 * Copy the msi capability structure in the last 16 bytes of the
	 * config space. This is wrong because it could shadow something
	 * useful to the device.
	 */
	capoff = 256 - roundup(sizeof(msicap), 4);
	capdata = (u_char *)&msicap;
	for (i = 0; i < sizeof(msicap); i++)
		pci_set_cfgdata8(pi, capoff + i, capdata[i]);

	return (capoff);
}
#endif	/* LEGACY_SUPPORT */

static int
cfginitmsi(struct passthru_softc *sc)
{
	int i, ptr, capptr, cap, sts, caplen, table_size;
	uint32_t u32;
	struct pcisel sel;
	struct pci_devinst *pi;
	struct msixcap msixcap;
	uint32_t *msixcap_ptr;

	pi = sc->psc_pi;
	sel = sc->psc_sel;

	/*
	 * Parse the capabilities and cache the location of the MSI
	 * and MSI-X capabilities.
	 */
	sts = read_config(&sel, PCIR_STATUS, 2);
	if (sts & PCIM_STATUS_CAPPRESENT) {
		ptr = read_config(&sel, PCIR_CAP_PTR, 1);
		while (ptr != 0 && ptr != 0xff) {
			cap = read_config(&sel, ptr + PCICAP_ID, 1);
			if (cap == PCIY_MSI) {
				/*
				 * Copy the MSI capability into the config
				 * space of the emulated pci device
				 */
				sc->psc_msi.capoff = ptr;
				sc->psc_msi.msgctrl = read_config(&sel,
								  ptr + 2, 2);
				sc->psc_msi.emulated = 0;
				caplen = msi_caplen(sc->psc_msi.msgctrl);
				capptr = ptr;
				while (caplen > 0) {
					u32 = read_config(&sel, capptr, 4);
					pci_set_cfgdata32(pi, capptr, u32);
					caplen -= 4;
					capptr += 4;
				}
			} else if (cap == PCIY_MSIX) {
				/*
				 * Copy the MSI-X capability 
				 */
				sc->psc_msix.capoff = ptr;
				caplen = 12;
				msixcap_ptr = (uint32_t*) &msixcap;
				capptr = ptr;
				while (caplen > 0) {
					u32 = read_config(&sel, capptr, 4);
					*msixcap_ptr = u32;
					pci_set_cfgdata32(pi, capptr, u32);
					caplen -= 4;
					capptr += 4;
					msixcap_ptr++;
				}
			}
			ptr = read_config(&sel, ptr + PCICAP_NEXTPTR, 1);
		}
	}

	if (sc->psc_msix.capoff != 0) {
		pi->pi_msix.pba_bar =
		    msixcap.pba_info & PCIM_MSIX_BIR_MASK;
		pi->pi_msix.pba_offset =
		    msixcap.pba_info & ~PCIM_MSIX_BIR_MASK;
		pi->pi_msix.table_bar =
		    msixcap.table_info & PCIM_MSIX_BIR_MASK;
		pi->pi_msix.table_offset =
		    msixcap.table_info & ~PCIM_MSIX_BIR_MASK;
		pi->pi_msix.table_count = MSIX_TABLE_COUNT(msixcap.msgctrl);

		/* Allocate the emulated MSI-X table array */
		table_size = pi->pi_msix.table_count * MSIX_TABLE_ENTRY_SIZE;
		pi->pi_msix.table = malloc(table_size);
		bzero(pi->pi_msix.table, table_size);

		/* Mask all table entries */
		for (i = 0; i < pi->pi_msix.table_count; i++) {
			pi->pi_msix.table[i].vector_control |=
						PCIM_MSIX_VCTRL_MASK;
		}
	}

#ifdef LEGACY_SUPPORT
	/*
	 * If the passthrough device does not support MSI then craft a
	 * MSI capability for it. We link the new MSI capability at the
	 * head of the list of capabilities.
	 */
	if ((sts & PCIM_STATUS_CAPPRESENT) != 0 && sc->psc_msi.capoff == 0) {
		int origptr, msiptr;
		origptr = read_config(&sel, PCIR_CAP_PTR, 1);
		msiptr = passthru_add_msicap(pi, 1, origptr);
		sc->psc_msi.capoff = msiptr;
		sc->psc_msi.msgctrl = pci_get_cfgdata16(pi, msiptr + 2);
		sc->psc_msi.emulated = 1;
		pci_set_cfgdata8(pi, PCIR_CAP_PTR, msiptr);
	}
#endif

	/* Make sure one of the capabilities is present */
	if (sc->psc_msi.capoff == 0 && sc->psc_msix.capoff == 0) 
		return (-1);
	else
		return (0);
}

static uint64_t
msix_table_read(struct passthru_softc *sc, uint64_t offset, int size)
{
	struct pci_devinst *pi;
	struct msix_table_entry *entry;
	uint8_t *src8;
	uint16_t *src16;
	uint32_t *src32;
	uint64_t *src64;
	uint64_t data;
	size_t entry_offset;
	int index;

	pi = sc->psc_pi;

	index = offset / MSIX_TABLE_ENTRY_SIZE;
	if (index >= pi->pi_msix.table_count)
		return (-1);

	entry = &pi->pi_msix.table[index];
	entry_offset = offset % MSIX_TABLE_ENTRY_SIZE;

	switch(size) {
	case 1:
		src8 = (uint8_t *)((void *)entry + entry_offset);
		data = *src8;
		break;
	case 2:
		src16 = (uint16_t *)((void *)entry + entry_offset);
		data = *src16;
		break;
	case 4:
		src32 = (uint32_t *)((void *)entry + entry_offset);
		data = *src32;
		break;
	case 8:
		src64 = (uint64_t *)((void *)entry + entry_offset);
		data = *src64;
		break;
	default:
		return (-1);
	}

	return (data);
}

static void
msix_table_write(struct vmctx *ctx, int vcpu, struct passthru_softc *sc,
		 uint64_t offset, int size, uint64_t data)
{
	struct pci_devinst *pi;
	struct msix_table_entry *entry;
	uint32_t *dest;
	size_t entry_offset;
	uint32_t vector_control;
	int error, index;

	pi = sc->psc_pi;
	index = offset / MSIX_TABLE_ENTRY_SIZE;
	if (index >= pi->pi_msix.table_count)
		return;

	entry = &pi->pi_msix.table[index];
	entry_offset = offset % MSIX_TABLE_ENTRY_SIZE;

	/* Only 4 byte naturally-aligned writes are supported */
	assert(size == 4);
	assert(entry_offset % 4 == 0);

	vector_control = entry->vector_control;
	dest = (uint32_t *)((void *)entry + entry_offset);
	*dest = data;
	/* If MSI-X hasn't been enabled, do nothing */
	if (pi->pi_msix.enabled) {
		/* If the entry is masked, don't set it up */
		if ((entry->vector_control & PCIM_MSIX_VCTRL_MASK) == 0 ||
		    (vector_control & PCIM_MSIX_VCTRL_MASK) == 0) {
			error = vm_setup_msix(ctx, vcpu, sc->psc_sel.pc_bus,
					      sc->psc_sel.pc_dev, 
					      sc->psc_sel.pc_func,
					      index, entry->msg_data, 
					      entry->vector_control,
					      entry->addr);
		}
	}
}

static int
init_msix_table(struct vmctx *ctx, struct passthru_softc *sc, uint64_t base)
{
	int idx;
	size_t table_size;
	vm_paddr_t start;
	size_t len;
	struct pci_devinst *pi = sc->psc_pi;

	/* 
	 * If the MSI-X table BAR maps memory intended for
	 * other uses, it is at least assured that the table 
	 * either resides in its own page within the region, 
	 * or it resides in a page shared with only the PBA.
	 */
	if (pi->pi_msix.pba_bar == pi->pi_msix.table_bar && 
	    ((pi->pi_msix.pba_offset - pi->pi_msix.table_offset) < 4096)) {
		/* Need to also emulate the PBA, not supported yet */
		printf("Unsupported MSI-X table and PBA in same page\n");
		return (-1);
	}

	/* 
	 * May need to split the BAR into 3 regions:
	 * Before the MSI-X table, the MSI-X table, and after it
	 * XXX for now, assume that the table is not in the middle
	 */
	table_size = pi->pi_msix.table_count * MSIX_TABLE_ENTRY_SIZE;
	idx = pi->pi_msix.table_bar;

	/* Round up to page size */
	table_size = roundup2(table_size, 4096);
	if (pi->pi_msix.table_offset == 0) {		
		/* Map everything after the MSI-X table */
		start = pi->pi_bar[idx].addr + table_size;
		len = pi->pi_bar[idx].size - table_size;
	} else {
                /* Map everything before the MSI-X table */
		start = pi->pi_bar[idx].addr;
		len = pi->pi_msix.table_offset;
	}
	return (vm_map_pptdev_mmio(ctx, sc->psc_sel.pc_bus, 
				   sc->psc_sel.pc_dev, sc->psc_sel.pc_func, 
				   start, len, base + table_size));
}

static int
cfginitbar(struct vmctx *ctx, struct passthru_softc *sc)
{
	int i, error;
	struct pci_devinst *pi;
	struct pci_bar_io bar;
	enum pcibar_type bartype;
	uint64_t base;

	pi = sc->psc_pi;

	/*
	 * Initialize BAR registers
	 */
	for (i = 0; i <= PCI_BARMAX; i++) {
		bzero(&bar, sizeof(bar));
		bar.pbi_sel = sc->psc_sel;
		bar.pbi_reg = PCIR_BAR(i);

		if (ioctl(pcifd, PCIOCGETBAR, &bar) < 0)
			continue;

		if (PCI_BAR_IO(bar.pbi_base)) {
			bartype = PCIBAR_IO;
			base = bar.pbi_base & PCIM_BAR_IO_BASE;
		} else {
			switch (bar.pbi_base & PCIM_BAR_MEM_TYPE) {
			case PCIM_BAR_MEM_64:
				bartype = PCIBAR_MEM64;
				break;
			default:
				bartype = PCIBAR_MEM32;
				break;
			}
			base = bar.pbi_base & PCIM_BAR_MEM_BASE;
		}

		/* Cache information about the "real" BAR */
		sc->psc_bar[i].type = bartype;
		sc->psc_bar[i].size = bar.pbi_length;
		sc->psc_bar[i].addr = base;

		/* Allocate the BAR in the guest I/O or MMIO space */
		error = pci_emul_alloc_pbar(pi, i, base, bartype,
					    bar.pbi_length);
		if (error)
			return (-1);

		/* The MSI-X table needs special handling */
		if (i == pi->pi_msix.table_bar) {
			error = init_msix_table(ctx, sc, base);
			if (error) 
				return (-1);
		} else if (bartype != PCIBAR_IO) {
			/* Map the physical MMIO space in the guest MMIO space */
			error = vm_map_pptdev_mmio(ctx, sc->psc_sel.pc_bus,
				sc->psc_sel.pc_dev, sc->psc_sel.pc_func,
				pi->pi_bar[i].addr, pi->pi_bar[i].size, base);
			if (error)
				return (-1);
		}

		/*
		 * 64-bit BAR takes up two slots so skip the next one.
		 */
		if (bartype == PCIBAR_MEM64) {
			i++;
			assert(i <= PCI_BARMAX);
			sc->psc_bar[i].type = PCIBAR_MEMHI64;
		}
	}
	return (0);
}

static int
cfginit(struct vmctx *ctx, struct pci_devinst *pi, int bus, int slot, int func)
{
	int error;
	struct passthru_softc *sc;

	error = 1;
	sc = pi->pi_arg;

	bzero(&sc->psc_sel, sizeof(struct pcisel));
	sc->psc_sel.pc_bus = bus;
	sc->psc_sel.pc_dev = slot;
	sc->psc_sel.pc_func = func;

	if (cfginitmsi(sc) != 0)
		goto done;

	if (cfginitbar(ctx, sc) != 0)
		goto done;

	error = 0;				/* success */
done:
	return (error);
}

static int
passthru_init(struct vmctx *ctx, struct pci_devinst *pi, char *opts)
{
	int bus, slot, func, error;
	struct passthru_softc *sc;

	sc = NULL;
	error = 1;

	if (pcifd < 0) {
		pcifd = open(_PATH_DEVPCI, O_RDWR, 0);
		if (pcifd < 0)
			goto done;
	}

	if (iofd < 0) {
		iofd = open(_PATH_DEVIO, O_RDWR, 0);
		if (iofd < 0)
			goto done;
	}

	if (opts == NULL ||
	    sscanf(opts, "%d/%d/%d", &bus, &slot, &func) != 3)
		goto done;

	if (vm_assign_pptdev(ctx, bus, slot, func) != 0)
		goto done;

	sc = malloc(sizeof(struct passthru_softc));
	memset(sc, 0, sizeof(struct passthru_softc));

	pi->pi_arg = sc;
	sc->psc_pi = pi;

	/* initialize config space */
	if ((error = cfginit(ctx, pi, bus, slot, func)) != 0)
		goto done;
	
	error = 0;		/* success */
done:
	if (error) {
		free(sc);
		vm_unassign_pptdev(ctx, bus, slot, func);
	}
	return (error);
}

static int
bar_access(int coff)
{
	if (coff >= PCIR_BAR(0) && coff < PCIR_BAR(PCI_BARMAX + 1))
		return (1);
	else
		return (0);
}

static int
msicap_access(struct passthru_softc *sc, int coff)
{
	int caplen;

	if (sc->psc_msi.capoff == 0)
		return (0);

	caplen = msi_caplen(sc->psc_msi.msgctrl);

	if (coff >= sc->psc_msi.capoff && coff < sc->psc_msi.capoff + caplen)
		return (1);
	else
		return (0);
}

static int 
msixcap_access(struct passthru_softc *sc, int coff)
{
	if (sc->psc_msix.capoff == 0) 
		return (0);

	return (coff >= sc->psc_msix.capoff && 
	        coff < sc->psc_msix.capoff + MSIX_CAPLEN);
}

static int
passthru_cfgread(struct vmctx *ctx, int vcpu, struct pci_devinst *pi,
		 int coff, int bytes, uint32_t *rv)
{
	struct passthru_softc *sc;

	sc = pi->pi_arg;

	/*
	 * PCI BARs and MSI capability is emulated.
	 */
	if (bar_access(coff) || msicap_access(sc, coff))
		return (-1);

#ifdef LEGACY_SUPPORT
	/*
	 * Emulate PCIR_CAP_PTR if this device does not support MSI capability
	 * natively.
	 */
	if (sc->psc_msi.emulated) {
		if (coff >= PCIR_CAP_PTR && coff < PCIR_CAP_PTR + 4)
			return (-1);
	}
#endif

	/* Everything else just read from the device's config space */
	*rv = read_config(&sc->psc_sel, coff, bytes);

	return (0);
}

static int
passthru_cfgwrite(struct vmctx *ctx, int vcpu, struct pci_devinst *pi,
		  int coff, int bytes, uint32_t val)
{
	int error, msix_table_entries, i;
	struct passthru_softc *sc;

	sc = pi->pi_arg;

	/*
	 * PCI BARs are emulated
	 */
	if (bar_access(coff))
		return (-1);

	/*
	 * MSI capability is emulated
	 */
	if (msicap_access(sc, coff)) {
		msicap_cfgwrite(pi, sc->psc_msi.capoff, coff, bytes, val);

		error = vm_setup_msi(ctx, vcpu, sc->psc_sel.pc_bus,
			sc->psc_sel.pc_dev, sc->psc_sel.pc_func, pi->pi_msi.cpu,
			pi->pi_msi.vector, pi->pi_msi.msgnum);
		if (error != 0) {
			printf("vm_setup_msi returned error %d\r\n", errno);
			exit(1);
		}
		return (0);
	}

	if (msixcap_access(sc, coff)) {
		msixcap_cfgwrite(pi, sc->psc_msix.capoff, coff, bytes, val);
		if (pi->pi_msix.enabled) {
			msix_table_entries = pi->pi_msix.table_count;
			for (i = 0; i < msix_table_entries; i++) {
				error = vm_setup_msix(ctx, vcpu, sc->psc_sel.pc_bus,
						      sc->psc_sel.pc_dev, 
						      sc->psc_sel.pc_func, i, 
						      pi->pi_msix.table[i].msg_data,
						      pi->pi_msix.table[i].vector_control,
						      pi->pi_msix.table[i].addr);
		
				if (error) {
					printf("vm_setup_msix returned error %d\r\n", errno);
					exit(1);	
				}
			}
		}
		return (0);
	}

#ifdef LEGACY_SUPPORT
	/*
	 * If this device does not support MSI natively then we cannot let
	 * the guest disable legacy interrupts from the device. It is the
	 * legacy interrupt that is triggering the virtual MSI to the guest.
	 */
	if (sc->psc_msi.emulated && pci_msi_enabled(pi)) {
		if (coff == PCIR_COMMAND && bytes == 2)
			val &= ~PCIM_CMD_INTxDIS;
	}
#endif

	write_config(&sc->psc_sel, coff, bytes, val);

	return (0);
}

static void
passthru_write(struct vmctx *ctx, int vcpu, struct pci_devinst *pi, int baridx,
	       uint64_t offset, int size, uint64_t value)
{
	struct passthru_softc *sc;
	struct iodev_pio_req pio;

	sc = pi->pi_arg;

	if (pi->pi_msix.table_bar == baridx) {
		msix_table_write(ctx, vcpu, sc, offset, size, value);
	} else {
		assert(pi->pi_bar[baridx].type == PCIBAR_IO);
		bzero(&pio, sizeof(struct iodev_pio_req));
		pio.access = IODEV_PIO_WRITE;
		pio.port = sc->psc_bar[baridx].addr + offset;
		pio.width = size;
		pio.val = value;
		
		(void)ioctl(iofd, IODEV_PIO, &pio);
	}
}

static uint64_t
passthru_read(struct vmctx *ctx, int vcpu, struct pci_devinst *pi, int baridx,
	      uint64_t offset, int size)
{
	struct passthru_softc *sc;
	struct iodev_pio_req pio;
	uint64_t val;

	sc = pi->pi_arg;

	if (pi->pi_msix.table_bar == baridx) {
		val = msix_table_read(sc, offset, size);
	} else {
		assert(pi->pi_bar[baridx].type == PCIBAR_IO);
		bzero(&pio, sizeof(struct iodev_pio_req));
		pio.access = IODEV_PIO_READ;
		pio.port = sc->psc_bar[baridx].addr + offset;
		pio.width = size;
		pio.val = 0;

		(void)ioctl(iofd, IODEV_PIO, &pio);

		val = pio.val;
	}

	return (val);
}

struct pci_devemu passthru = {
	.pe_emu		= "passthru",
	.pe_init	= passthru_init,
	.pe_cfgwrite	= passthru_cfgwrite,
	.pe_cfgread	= passthru_cfgread,
	.pe_barwrite 	= passthru_write,
	.pe_barread    	= passthru_read,
};
PCI_EMUL_SET(passthru);
