AC_INIT([highlyadvanced], [0.41], [nenolod+highlyadvanced@sacredspiral.co.uk])
AC_PREREQ([2.59])

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AC_PROG_CC
AC_CHECK_HEADERS(sys/soundcard.h)
AC_PROG_CPP
AC_PROG_CXX
AC_C_BIGENDIAN
AC_C_INLINE

CFLAGS="-DLINUX -I./VBA -DVERSION_STR=\\\"0.41\\\" -DHA_VERSION_STR=\\\"0.11\\\""
LDFLAGS="-lz"

interpolation=yes
use_optimisation=no


AC_ARG_ENABLE(
		[interpolation],
		AS_HELP_STRING([--disable-interpolation],
		[Dont compile interpolation code. (Default is NO)]),
		if test "$enableval" = "no"
		then
			CFLAGS="$CFLAGS -DNO_INTERPOLATION"
			interpolation=no
		fi
)

AC_ARG_ENABLE(
		[optimisations],
		AS_HELP_STRING([--enable-optimisations],
		[Enable compiler optimisations. (Default is NO)]),
		if test "$enableval" = "yes"
		then
			use_optimisation=yes
		fi
)

if test $use_optimisation == "yes"
then
echo "Using optimisations"
CFLAGS="$CFLAGS -O3"
else
echo "Not using optimisations"
fi


CFLAGS="$CFLAGS -DC_CORE"

PKG_CHECK_MODULES(AUDACIOUS, [audacious >= 1.3.99],
	[
	 AUDACIOUS_PLUGIN_DIR=`pkg-config audacious --variable=input_plugin_dir`
	 AC_SUBST(AUDACIOUS_PLUGIN_DIR)
	], [AC_MSG_ERROR([Audacious 1.4 required.])])

BUILDSYS_SHARED_LIB
BUILDSYS_PROG_IMPLIB

AC_CONFIG_HEADER(config.h)
AC_OUTPUT(buildsys.mk extra.mk)

echo
echo "--- Configuration:"
if test $interpolation == "yes"
then
echo "Interpolation enabled"
else
echo "Interpolation disabled"
fi

