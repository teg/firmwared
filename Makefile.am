# ------------------------------------------------------------------------------
# autotools

SHELL = bash
ACLOCAL_AMFLAGS = -I build/m4 ${ACLOCAL_FLAGS}
AUTOMAKE_OPTIONS = color-tests parallel-tests
AM_MAKEFLAGS = --no-print-directory
AM_TESTS_ENVIRONMENT = LD_LIBRARY_PATH=$(abs_builddir)
check_PROGRAMS = $(default_tests)
TESTS = $(default_tests)
CLEANFILES =
EXTRA_DIST =

# remove targets if the command fails
.DELETE_ON_ERROR:

# keep intermediate files
.SECONDARY:

# keep the test-suite.log and Makefile around at all times
.PRECIOUS: $(TEST_SUITE_LOG) Makefile

AM_CPPFLAGS = \
	-include $(top_builddir)/build/config.h \
	-I $(top_srcdir)/src \
	-I $(top_builddir)/src \
	-DFIRMWARE_PATH="$(FIRMWARE_PATH)"

AM_CFLAGS = \
	-fdata-sections \
	-ffunction-sections \
	-fvisibility=hidden \
	-fno-common \
	-fPIC \
	-g \
	-O2 \
	-Wall \
	-Wundef \
	-Wextra \
	-Wshadow \
	-Winit-self \
	-Wformat=2 \
	-Wformat-security \
	-Wformat-nonliteral \
	-Wmissing-prototypes \
	-Wmissing-declarations \
	-Wdeclaration-after-statement \
	-Wredundant-decls \
	-Wno-missing-field-initializers \
	-Wno-unused-parameter \
	-Wno-inline

AM_LDFLAGS = \
	-Wl,--as-needed \
	-Wl,--no-undefined \
	-Wl,--gc-sections \
	-Wl,-z,relro \
	-Wl,-z,now \
	-pie

# ------------------------------------------------------------------------------
# libfirmware.a

libfirmware_a_SOURCES = \
	src/firmware.h \
	src/firmware.c

# ------------------------------------------------------------------------------
# firmwared

firmwared_SOURCES = \
		src/firmwared.c \
                src/firmwared.h \
		src/manager.h \
		src/manager.c \
		src/log-util.h
firmwared_LDADD = \
		libfirmware.a \
		$(LIBUDEV_LIBS)
firmwared_CFLAGS = \
		$(LIBUDEV_CFLAGS) \
		$(AM_CFLAGS)

# ------------------------------------------------------------------------------
# test-basic

test_basic_SOURCES = src/test-basic.c
test_basic_LDADD = libfirmware.a

# ------------------------------------------------------------------------------
# test-runner

if TEST_RUNNER
test_runner_SOURCES = \
	tools/test-runner.c

libtester_a_SOURCES = \
	tools/tester.c \
	tools/tester.h \
	tools/util.h \
	tools/util.c
libtester_a_CFLAGS = \
	$(GLIB_CFLAGS) \
        $(AM_CFLAGS)
libtester_a_LDLAGS = \
	$(GLIB_LDLAGS)

firmware_tester_SOURCES = \
	tools/firmware-tester.c
firmware_tester_CFLAGS = \
	$(GLIB_CFLAGS) \
        $(AM_CFLAGS)
firmware_tester_LDADD = \
        libtester.a \
	$(GLIB_LIBS)
endif

# ------------------------------------------------------------------------------
# targets

noinst_LIBRARIES = libfirmware.a

if TEST_RUNNER
noinst_LIBRARIES += \
        libtester.a
noinst_PROGRAMS = \
	test-runner \
	firmware_tester
endif

bin_PROGRAMS = firmwared
default_tests = \
	test-basic

EXTRA_DIST += src/test-build.sh
TESTS += src/test-build.sh
