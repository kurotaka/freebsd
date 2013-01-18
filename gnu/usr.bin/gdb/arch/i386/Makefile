# $FreeBSD$

GENSRCS+= xm.h
.if !defined(GDB_CROSS_DEBUGGER)
LIBSRCS+= fbsd-proc.c fbsd-threads.c gcore.c
LIBSRCS+= i386-nat.c i386bsd-nat.c i386fbsd-nat.c
.endif
LIBSRCS+= solib.c solib-svr4.c
LIBSRCS+= i386-tdep.c i386bsd-tdep.c i386fbsd-tdep-fixed.c i387-tdep.c

nm.h:
	echo '#ifndef GENSRCS_NM_H' > ${.TARGET}
	echo '#define GENSRCS_NM_H' >> ${.TARGET}
	echo '#include "i386/nm-fbsd.h"' >> ${.TARGET}
	echo '#undef I386_DR_LOW_SET_CONTROL' >> ${.TARGET}
	echo '#undef I386_DR_LOW_SET_ADDR' >> ${.TARGET}
	echo '#undef I386_DR_LOW_RESET_ADDR' >> ${.TARGET}
	echo '#undef I386_DR_LOW_GET_STATUS' >> ${.TARGET}
	echo '#define I386_DR_LOW_SET_CONTROL(c) \' >> ${.TARGET}
	echo '  fbsd_thread_dr_set_control (c)' >> ${.TARGET}
	echo 'void i386bsd_dr_set_control (unsigned long);' >> ${.TARGET}
	echo '#define I386_DR_LOW_SET_ADDR(r, v) \' >> ${.TARGET}
	echo '  fbsd_thread_dr_set_addr (r, v)' >> ${.TARGET}
	echo 'void fbsd_thread_dr_set_addr (int, CORE_ADDR);' >> ${.TARGET}
	echo '#define I386_DR_LOW_RESET_ADDR(r) \' >> ${.TARGET}
	echo '  fbsd_thread_dr_reset_addr (r)' >> ${.TARGET}
	echo 'void fbsd_thread_dr_reset_addr (int);' >> ${.TARGET}
	echo '#define I386_DR_LOW_GET_STATUS() \' >> ${.TARGET}
	echo '  fbsd_thread_dr_get_status ()' >> ${.TARGET}
	echo 'unsigned long fbsd_thread_dr_get_status (void);' >> ${.TARGET}
	echo '#endif /* GENSRCS_NM_H */' >> ${.TARGET}

tm.h:
	echo '#include "i386/tm-fbsd.h"' > ${.TARGET}

xm.h:
	echo '#include "i386/xm-i386.h"' > ${.TARGET}

# Fix source static/extern mismatch nits that GCC 4.2 warns about.
CLEANFILES += i386fbsd-tdep-fixed.c
i386fbsd-tdep-fixed.c: i386fbsd-tdep.c
	sed -e '48s/^static\ //' ${.ALLSRC} > ${.TARGET}
