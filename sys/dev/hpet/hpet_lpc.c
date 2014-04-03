/*-
 * Copyright (c) 2013 KUROSAWA Takahiro <takahiro.kurosawa@gmail.com>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/timeet.h>
#include <sys/timetc.h>
#include <machine/resource.h>

#include <isa/isavar.h>
#include <dev/pci/pcivar.h>

#include <dev/hpet/hpetreg.h>
#include <dev/hpet/hpetvar.h>

#define HPET_LPC_VENDOR_INTEL	0x8086
#define HPET_LPC_INTEL_LPC	0x8186

static struct hpet_lpc_device {
	uint16_t vendor;
	uint16_t device;
	u_long membase;
	char *desc;
} hpet_lpc_devices[] = {
	{
		HPET_LPC_VENDOR_INTEL,
		HPET_LPC_INTEL_LPC,
		0xfed00000UL,
		"Intel LPC High Precision Event Timer",
	},
	{ 0, 0, 0, NULL }
};

static struct hpet_lpc_device *
hpet_lpc_find(device_t isab_dev)
{
	device_t pci_dev;
	struct hpet_lpc_device *id;

	if (device_get_devclass(isab_dev) != devclass_find("isab"))
		return NULL;
	pci_dev = device_get_parent(isab_dev);
	if (pci_dev == NULL)
		return NULL;
	if (device_get_devclass(pci_dev) != devclass_find("pci"))
		return NULL;

	for (id = hpet_lpc_devices; id->desc != NULL; id++)
		if (pci_get_vendor(isab_dev) == id->vendor &&
		    pci_get_device(isab_dev) == id->device)
			return id;

	return NULL;
}

static void
hpet_lpc_identify(driver_t *driver, device_t parent)
{
	device_t isab_dev;
	device_t child;
	struct hpet_lpc_device *id;

	/* Only one HPET device can be added. */
	if (devclass_get_device(hpet_devclass, 0))
		return;

	isab_dev = device_get_parent(parent);
	if (isab_dev == NULL)
		return;

	id = hpet_lpc_find(isab_dev);
	if (id == NULL)
		return;

	child = BUS_ADD_CHILD(parent, 0, driver->name, 0);
	if (child == NULL) {
		printf("hpet_lpc: can not add a child device.\n");
		return;
	}

	device_set_desc(child, id->desc);
	bus_set_resource(child, SYS_RES_MEMORY, 0, id->membase, HPET_MEM_WIDTH);
}

static int
hpet_lpc_probe(device_t dev)
{
	device_t parent;
	device_t isab_dev;
	struct hpet_lpc_device *id;

	parent = device_get_parent(dev);
	if (parent == NULL)
		return ENXIO;
	isab_dev = device_get_parent(parent);
	if (isab_dev == NULL)
		return ENXIO;

	id = hpet_lpc_find(isab_dev);
	if (id == NULL)
		return ENXIO;

	return 0;
}

static int
hpet_lpc_attach(device_t dev)
{
	struct hpet_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	return hpet_attach(sc);
}

static int
hpet_lpc_detach(device_t dev)
{
	return EBUSY;
}

static device_method_t hpet_lpc_methods[] = {
	DEVMETHOD(device_identify, hpet_lpc_identify),
	DEVMETHOD(device_probe, hpet_lpc_probe),
	DEVMETHOD(device_attach, hpet_lpc_attach),
	DEVMETHOD(device_detach, hpet_lpc_detach),
	DEVMETHOD(device_suspend, hpet_suspend),
	DEVMETHOD(device_resume, hpet_resume),

#ifdef DEV_APIC
	DEVMETHOD(bus_remap_intr, hpet_remap_intr),
#endif

	{0, 0}
};

static driver_t hpet_lpc_driver = {
	"hpet",
	hpet_lpc_methods,
	sizeof(struct hpet_softc),
};

DRIVER_MODULE(hpet, isa, hpet_lpc_driver, hpet_devclass, 0, 0);
