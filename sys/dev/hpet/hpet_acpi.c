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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/timeet.h>
#include <sys/timetc.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>

#include <dev/hpet/hpetreg.h>
#include <dev/hpet/hpetvar.h>

ACPI_SERIAL_DECL(hpet, "ACPI HPET support");

/* ACPI CA debugging */
#define _COMPONENT	ACPI_TIMER
ACPI_MODULE_NAME("HPET")

static char *hpet_ids[] = { "PNP0103", NULL };

static ACPI_STATUS
hpet_acpi_find(ACPI_HANDLE handle, UINT32 level, void *context,
    void **status)
{
	char 		**ids;
	uint32_t	id = (uint32_t)(uintptr_t)context;
	uint32_t	uid = 0;

	for (ids = hpet_ids; *ids != NULL; ids++) {
		if (acpi_MatchHid(handle, *ids))
		        break;
	}
	if (*ids == NULL)
		return (AE_OK);
	if (ACPI_FAILURE(acpi_GetInteger(handle, "_UID", &uid)) ||
	    id == uid)
		*((int *)status) = 1;
	return (AE_OK);
}

/* Discover the HPET via the ACPI table of the same name. */
static void 
hpet_acpi_identify(driver_t *driver, device_t parent)
{
	ACPI_TABLE_HPET *hpet;
	ACPI_STATUS	status;
	device_t	child;
	int 		i, found;

	/* Only one HPET device can be added. */
	if (devclass_get_device(hpet_devclass, 0))
		return;
	for (i = 1; ; i++) {
		/* Search for HPET table. */
		status = AcpiGetTable(ACPI_SIG_HPET, i, (ACPI_TABLE_HEADER **)&hpet);
		if (ACPI_FAILURE(status))
			return;
		/* Search for HPET device with same ID. */
		found = 0;
		AcpiWalkNamespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
		    100, hpet_acpi_find, NULL, (void *)(uintptr_t)hpet->Sequence, (void *)&found);
		/* If found - let it be probed in normal way. */
		if (found)
			continue;
		/* If not - create it from table info. */
		child = BUS_ADD_CHILD(parent, 2, "hpet", 0);
		if (child == NULL) {
			printf("%s: can't add child\n", __func__);
			continue;
		}
		bus_set_resource(child, SYS_RES_MEMORY, 0, hpet->Address.Address,
		    HPET_MEM_WIDTH);
	}
}

static int
hpet_acpi_probe(device_t dev)
{
	ACPI_FUNCTION_TRACE((char *)(uintptr_t) __func__);

	if (acpi_disabled("hpet"))
		return (ENXIO);
	if (acpi_get_handle(dev) != NULL &&
	    ACPI_ID_PROBE(device_get_parent(dev), dev, hpet_ids) == NULL)
		return (ENXIO);

	device_set_desc(dev, "High Precision Event Timer");
	return (0);
}

static int
hpet_acpi_attach(device_t dev)
{
	struct hpet_softc *sc;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t) __func__);

	sc = device_get_softc(dev);
	sc->dev = dev;

	return hpet_attach(sc);
}

static int
hpet_acpi_detach(device_t dev)
{
	ACPI_FUNCTION_TRACE((char *)(uintptr_t) __func__);

	/* XXX Without a tc_remove() function, we can't detach. */
	return (EBUSY);
}

static device_method_t hpet_acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify, hpet_acpi_identify),
	DEVMETHOD(device_probe, hpet_acpi_probe),
	DEVMETHOD(device_attach, hpet_acpi_attach),
	DEVMETHOD(device_detach, hpet_acpi_detach),
	DEVMETHOD(device_suspend, hpet_suspend),
	DEVMETHOD(device_resume, hpet_resume),

#ifdef DEV_APIC
	DEVMETHOD(bus_remap_intr, hpet_remap_intr),
#endif

	{0, 0}
};

static driver_t	hpet_acpi_driver = {
	"hpet",
	hpet_acpi_methods,
	sizeof(struct hpet_softc),
};

DRIVER_MODULE(hpet, acpi, hpet_acpi_driver, hpet_devclass, 0, 0);
MODULE_DEPEND(hpet, acpi, 1, 1, 1);
