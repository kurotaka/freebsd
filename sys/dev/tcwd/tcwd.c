/*-
 * Copyright (c) 2012 KUROSAWA Takahiro <takahiro.kurosawa@gmai.com>
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
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <sys/watchdog.h>

#include <isa/isavar.h>
#include <dev/pci/pcivar.h>

#include "tcwd.h"

#define	tcwd_read_4(sc, off)		bus_read_4((sc)->wd_res, (off))
#define	tcwd_read_2(sc, off)		bus_read_2((sc)->wd_res, (off))
#define	tcwd_read_1(sc, off)		bus_read_1((sc)->wd_res, (off))
#define	tcwd_write_4(sc, off, val)	\
	bus_write_4((sc)->wd_res, (off), (val))
#define	tcwd_write_2(sc, off, val)	\
	bus_write_4((sc)->wd_res, (off), (val))
#define	tcwd_write_1(sc, off, val)	\
	bus_write_4((sc)->wd_res, (off), (val))

static struct tcwd_device tcwd_devices[] = {
	{ DEVICEID_TC_LPC,	"Intel Atom E6xx watchdog timer" },
	{ 0, NULL },
};

static devclass_t tcwd_devclass;

static inline void
tcwd_unlock_reg(struct tcwd_softc *sc)
{
	/* Register unlocking sequence */
	tcwd_write_1(sc, TC_WD_RR0, 0x80);
	tcwd_write_1(sc, TC_WD_RR0, 0x86);
}

static void
tcwd_timer_reload(struct tcwd_softc *sc)
{
	tcwd_unlock_reg(sc);
	tcwd_write_1(sc, TC_WD_RR1, TC_WD_RR1_RELOAD);
}

static void
tcwd_timer_init(struct tcwd_softc *sc, unsigned int cmd)
{
	u_int32_t preload;

	/* Convert from power-of-two-ns to approx. 1kHz ticks. */
	preload = ((1ULL << cmd) * 33 / 1000) >> 15;
	preload--;

printf("%s preload=0x%x\n", __func__, preload);
	/*
	 * Set watchdog to perform a cold reset toggling the GPIO pin and the
	 * prescaler set to 1ms-10m resolution.
	 */
#if 0
	tcwd_write_1(sc, TC_WD_WDTCR, TC_WD_WDTCR_RESET_EN);
#else
	tcwd_write_1(sc, TC_WD_WDTCR, TC_WD_WDTCR_TOUT_EN|TC_WD_WDTCR_RESET_EN|TC_WD_WDTCR_RESET_SEL);
#endif
	tcwd_unlock_reg(sc);
	tcwd_write_4(sc, TC_WD_PV1, 0);
	tcwd_unlock_reg(sc);
	tcwd_write_4(sc, TC_WD_PV2, preload);
	tcwd_timer_reload(sc);
	sc->timeout = cmd;
}

static void
tcwd_timer_start(struct tcwd_softc *sc)
{
printf("%s\n", __func__);
	tcwd_write_1(sc, TC_WD_WDTLR, TC_WD_WDTLR_WDT_ENABLE);
	sc->active = 1;
}

static void
tcwd_timer_stop(struct tcwd_softc *sc)
{
printf("%s\n", __func__);
	/* Disable watchdog, with a reload before for safety */
	tcwd_unlock_reg(sc);
	tcwd_write_1(sc, TC_WD_RR1, TC_WD_RR1_RELOAD);
	tcwd_write_1(sc, TC_WD_WDTLR, 0);
	sc->active = 0;
}

static void
tcwd_event(void *arg, unsigned int cmd, int *error)
{
	struct tcwd_softc *sc = arg;

	cmd &= WD_INTERVAL;
printf("%s: cmd 0x%x\n", __func__, cmd);
	if (cmd) {
		if (cmd != sc->timeout)
			tcwd_timer_init(sc, cmd);
		if (!sc->active)
			tcwd_timer_start(sc);
		tcwd_timer_reload(sc);
		*error = 0;
	} else {
		if (sc->active)
			tcwd_timer_stop(sc);
	}
}

static struct tcwd_device *
tcwd_find(device_t isab_dev)
{
	device_t pci_dev;
	struct tcwd_device *id;

	if (device_get_devclass(isab_dev) != devclass_find("isab"))
		return NULL;
	pci_dev = device_get_parent(isab_dev);
	if (pci_dev == NULL)
		return NULL;
	if (device_get_devclass(pci_dev) != devclass_find("pci"))
		return NULL;

	if (pci_get_vendor(isab_dev) != VENDORID_INTEL)
		return NULL;
	for (id = tcwd_devices; id->desc != NULL; id++)
		if (pci_get_device(isab_dev) == id->device)
			return id;

	return NULL;
}

static void
tcwd_identify(driver_t *driver, device_t parent)
{
	device_t isab_dev;

	isab_dev = device_get_parent(parent);
	if (isab_dev == NULL)
		return;
	if (tcwd_find(isab_dev) == NULL)
		return;

	if (device_find_child(parent, driver->name, -1) == NULL)
		BUS_ADD_CHILD(parent, 0, driver->name, 0);
}

static int
tcwd_probe(device_t dev)
{
	struct tcwd_device *id;
	device_t isa_dev, isab_dev;
	u_int32_t reg;
	int ret;

	isa_dev = device_get_parent(dev);
	if (isa_dev == NULL)
		return ENXIO;
	isab_dev = device_get_parent(isa_dev);
	if (isab_dev == NULL)
		return ENXIO;
	id = tcwd_find(isab_dev);
	if (id == NULL)
		return ENXIO;

	reg = pci_read_config(isab_dev, TC_LPC_WDTBA, 4);
	if ((reg == 0) || ((reg & TC_WDTBA_EN) == 0)) {
		device_printf(dev,
		    "Watchdog disabled in BIOS or hardware.\n");
		return ENXIO;
	}

	reg &= TC_WDTBA_MASK;
	ret = bus_set_resource(dev, SYS_RES_IOPORT, 0, reg, TC_WD_REG_LEN);
	if (ret != 0) {
		device_printf(dev,
		    "bus_set_resource for WDT register space failed.\n");
		return ENXIO;
	}

	reg = pci_read_config(isab_dev, TC_LPC_GBA, 4);
	if ((reg == 0) || ((reg & TC_GPIO_EN) == 0)) {
		device_printf(dev,
		    "GPIO disabled in BIOS or hardware.\n");
		return ENXIO;
	}

	reg &= TC_GPIO_MASK;
	ret = bus_set_resource(dev, SYS_RES_IOPORT, 1, reg, TC_GPIO_REG_LEN);
	if (ret != 0) {
		device_printf(dev,
		    "bus_set_resource for GPIO register space failed.\n");
		return ENXIO;
	}

	device_set_desc(dev, id->desc);

	return BUS_PROBE_GENERIC;
}

static int
tcwd_attach(device_t dev)
{
	struct tcwd_softc *sc;
	u_int32_t reg;

	sc = device_get_softc(dev);
	sc->dev = dev;

	sc->wd_rid = 0;
	sc->wd_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->wd_rid,
	    0ul, ~0ul, TC_WD_REG_LEN, RF_ACTIVE | RF_SHAREABLE);
	if (sc->wd_res == NULL) {
		device_printf(dev, "unable to reserve WDT registers.\n");
		goto fail;
	}

	sc->gpio_rid = 1;
	sc->gpio_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->gpio_rid,
	    0ul, ~0ul, TC_GPIO_REG_LEN, RF_ACTIVE | RF_SHAREABLE);
	if (sc->gpio_res == NULL) {
		device_printf(dev, "unable to reserve GPIO registers.\n");
		goto fail;
	}

#if 0
	reg = bus_read_4(sc->gpio_res, TC_GPIO_CGEN);
	if ((reg & TC_GPIO_CGEN_GPIO4) != 0) {
		reg &= ~TC_GPIO_CGEN_GPIO4;
		bus_write_4(sc->gpio_res, TC_GPIO_CGEN, reg);
		device_printf(dev, "GPIO[4] modified for the watchdog.\n");
	}
printf("CGEN 0x%x\n", bus_read_4(sc->gpio_res, TC_GPIO_CGEN));
#endif

	reg = tcwd_read_1(sc, TC_WD_RR1);
	if (reg & TC_WD_RR1_TOUT) {
		device_printf(dev, "watchdog rebooted the system.\n");
		tcwd_unlock_reg(sc);
		tcwd_write_1(sc, TC_WD_RR1, TC_WD_RR1_TOUT);
	}

	reg = tcwd_read_1(sc, TC_WD_WDTLR);
	if ((reg & TC_WD_WDTLR_WDT_LOCK) != 0) {
		device_printf(dev, "WDTLR register already locked.\n");
		goto fail;
	}

	tcwd_timer_stop(sc);
	sc->ev_tag = EVENTHANDLER_REGISTER(watchdog_list, tcwd_event, sc, 0);

	return 0;

fail:
	if (sc->gpio_res != NULL)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    sc->gpio_rid, sc->gpio_res);

	if (sc->wd_res != NULL)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    sc->wd_rid, sc->wd_res);

	return ENXIO;
}

static int
tcwd_detach_shutdown(device_t dev, int is_detach)
{
	struct tcwd_softc *sc;

	sc = device_get_softc(dev);

	if (sc->ev_tag != NULL)
		EVENTHANDLER_DEREGISTER(watchdog_list, sc->ev_tag);
	sc->ev_tag = NULL;

	if (is_detach) {
		tcwd_timer_stop(sc);
	} else {
		if (sc->active) {
			device_printf(dev,
			    "Keeping watchdog alive during shutdown"
			    " for 275 secs.\n");
			tcwd_timer_init(sc, 38);
		}
	}

	if (sc->gpio_res != NULL)
		bus_release_resource(sc->dev, SYS_RES_IOPORT,
		    sc->gpio_rid, sc->gpio_res);

	if (sc->wd_res != NULL)
		bus_release_resource(sc->dev, SYS_RES_IOPORT,
		    sc->wd_rid, sc->wd_res);

	return 0;
}

static int
tcwd_detach(device_t dev)
{
	return tcwd_detach_shutdown(dev, 1);
}

static int
tcwd_shutdown(device_t dev)
{
	return tcwd_detach_shutdown(dev, 0);
}

static device_method_t tcwd_methods[] = {
	DEVMETHOD(device_identify, tcwd_identify),
	DEVMETHOD(device_probe, tcwd_probe),
	DEVMETHOD(device_attach, tcwd_attach),
	DEVMETHOD(device_detach, tcwd_detach),
	DEVMETHOD(device_shutdown, tcwd_shutdown),
	{0,0}
};

static driver_t tcwd_driver = {
	"tcwd",
	tcwd_methods,
	sizeof(struct tcwd_softc),
};

static int
tcwd_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		printf("tcwd module loaded.\n");
		break;
	case MOD_UNLOAD:
		printf("tcwd module uloaded.\n");
		break;
	case MOD_SHUTDOWN:
		printf("tcwd module shutting down.\n");
		break;
	}

	return error;
}

DRIVER_MODULE(tcwd, isa, tcwd_driver, tcwd_devclass, tcwd_modevent, NULL);
