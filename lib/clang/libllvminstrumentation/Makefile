# $FreeBSD$

LIB=	llvminstrumentation

SRCDIR=	lib/Transforms/Instrumentation
SRCS=	AddressSanitizer.cpp \
	BlackList.cpp \
	BoundsChecking.cpp \
	EdgeProfiling.cpp \
	GCOVProfiling.cpp \
	Instrumentation.cpp \
	OptimalEdgeProfiling.cpp \
	PathProfiling.cpp \
	ProfilingUtils.cpp \
	ThreadSanitizer.cpp

TGHDRS=	Intrinsics

.include "../clang.lib.mk"
