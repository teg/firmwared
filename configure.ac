# ------------------------------------------------------------------------------
# versions

AC_PREREQ([2.64])
AC_INIT([firmwared],
        [1],
        [http://www.github.com/teg/firmwared/issues],
        [firmwared],
        [http://www.github.com/teg/firmwared])

# ------------------------------------------------------------------------------
# autotools

AC_CONFIG_SRCDIR([src/firmwared.c])
AC_CONFIG_MACRO_DIR([build/m4])
AC_CONFIG_HEADERS([build/config.h])
AC_CONFIG_AUX_DIR([build/build-aux])

# suppress conflicting defaults
CFLAGS=${CFLAGS:-""}
CXXFLAGS=${CXXFLAGS:-""}

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AM_MAINTAINER_MODE([enable])
AM_INIT_AUTOMAKE([foreign 1.11 -Wall -Wno-portability silent-rules tar-pax no-dist-gzip dist-xz subdir-objects parallel-tests])
AM_SILENT_RULES([yes])
AC_CANONICAL_HOST
AC_DEFINE_UNQUOTED([CANONICAL_HOST], "$host", [Canonical host string.])
AC_PROG_CC_C99
AC_PROG_RANLIB
AC_PROG_SED
AC_PROG_LN_S

m4_pattern_forbid([^_?PKG_[A-Z_]+$],[*** pkg.m4 missing, please install pkg-config])

# ------------------------------------------------------------------------------
have_libudev=no
PKG_CHECK_MODULES(LIBUDEV, [ libudev ],
        [AC_DEFINE(HAVE_LIBUDEV, 1, [Define if libudev is available]) have_libudev=yes], have_libudev=no)
if test "x$have_libudev" = xno; then
        AC_MSG_ERROR([*** libudev support required but libraries not found])
fi
AM_CONDITIONAL(HAVE_LIBUDEV, [test "$have_libmount" = "yes"])

# ------------------------------------------------------------------------------
AC_ARG_ENABLE(test-runner,
        AC_HELP_STRING([--disable-test-runner], [build test-runner for testing]),
        [enable_test_runner=${enableval}])
if test "x$enable_test_runner" != xno; then
        have_glib=no
        PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.28,
                [AC_DEFINE(HAVE_GLIB, 1, [Define if libglib is available]) have_glib=yes], have_glib=no)
        if test "x$have_glib" = xno; then
                AC_MSG_ERROR([*** libglib support required but libraries not found])
        fi
fi
AM_CONDITIONAL(TEST_RUNNER, [test "x$enable_test_runner" != "xno"])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

# ------------------------------------------------------------------------------
AC_ARG_WITH(firmware-path,
        AS_HELP_STRING([--with-firmware-path=DIR[[[:DIR[...]]]]],
           [Firmware search path (default="/lib/firmware/updates:/lib/firmware")]),
        [], [with_firmware_path="/lib/firmware/updates:/lib/firmware"])
OLD_IFS=$IFS
IFS=:
for i in $with_firmware_path; do
       if test "x${FIRMWARE_PATH}" = "x"; then
              FIRMWARE_PATH="\\\"${i}/\\\""
       else
              FIRMWARE_PATH="${FIRMWARE_PATH}, \\\"${i}/\\\""
       fi
done
IFS=$OLD_IFS
AC_SUBST(FIRMWARE_PATH)

# ------------------------------------------------------------------------------
# report

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
AC_MSG_RESULT([
        $PACKAGE_NAME $VERSION

	firmware_path:          ${FIRMWARE_PATH}

        libudev:                ${have_libudev}

        prefix:                 ${prefix}
        exec_prefix:            ${exec_prefix}
        includedir:             ${includedir}
        libdir:                 ${libdir}

        CFLAGS:                 ${CFLAGS}
        CPPFLAGS:               ${CPPFLAGS}
        LDFLAGS:                ${LDFLAGS}
])
