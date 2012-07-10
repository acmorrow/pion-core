# -----------------------------------------
# pion-platform autoconf configuration file
# -----------------------------------------

# Check for libxml2 library (REQUIRED)
AC_MSG_CHECKING([for libxml library (required)])
AC_ARG_WITH([libxml],
    AC_HELP_STRING([--with-libxml@<:@=DIR@:>@],[location of libxml (v2) library]),
    [ xml_location=$withval ], [ without_xml=true ])
# Check for libxml2 headers
if test "x$xml_location" == "xyes"; then
	AC_MSG_RESULT(checking default locations)
	TRY_LOCATION="/usr/local /usr"
elif test "x$xml_location" == "x"; then
	AC_MSG_RESULT(checking default locations)
	TRY_LOCATION="/usr/local /usr"
else
	AC_MSG_RESULT(checking $xml_location)
	TRY_LOCATION="$xml_location"
	LDFLAGS="$LDFLAGS -L$xml_location/lib"
fi
AC_MSG_CHECKING(for libxml2 headers)
FOUND_XML_HEADERS="false"
for xml_location in $TRY_LOCATION ;
do
	if test -r "$xml_location/include/libxml2/libxml/tree.h"; then
		CPPFLAGS="$CPPFLAGS -I$xml_location/include/libxml2"
		FOUND_XML_HEADERS="true"
		break;
	elif test -r "$xml_location/include/libxml/tree.h"; then
		CPPFLAGS="$CPPFLAGS -I$xml_location/include"
		FOUND_XML_HEADERS="true"
		break;
	fi
done
if test "$FOUND_XML_HEADERS" == "true"; then
	AC_MSG_RESULT([found in $xml_location])
else
	AC_MSG_RESULT(not found)
	AC_MSG_ERROR([Unable to find the libxml2 headers])
fi
# Check for libxml2 library
LIBS_SAVED="$LIBS"
LIBS="$LIBS_SAVED -lxml2"
AC_MSG_CHECKING(linking with libxml2)
AC_TRY_LINK([#include <libxml/tree.h>],[ LIBXML_TEST_VERSION; return(0); ],
	[ AC_MSG_RESULT(ok) ],
	[ AC_MSG_RESULT(failed)
	  AC_MSG_ERROR(Unable to link with the libxml2 library)
	])
# Found the libxml2 library
LIBS="$LIBS_SAVED"
PION_XML_LIB="-lxml2"
AC_SUBST(PION_XML_LIB)


# Check for "Yet Another" JSON Library (YAJL)
AC_MSG_CHECKING([for JSON support (yajl)])
AC_ARG_WITH([yajl],
    AC_HELP_STRING([--with-yajl@<:@=DIR@:>@],[location of YAJL Library (enables JSON support); use --without-yajl to disable]),
    [ yajl_location=$withval ], [ yajl_location=yes ])
if test "x$yajl_location" == "xno"; then
	# YAJL is disabled
	AC_MSG_RESULT(no)
else
	# Add paths if location is defined
	if test "x$yajl_location" != "xyes"; then
		CPPFLAGS="$CPPFLAGS -I$yajl_location/include"
		LDFLAGS="$LDFLAGS -L$yajl_location/lib"
	fi
	# Check for YAJL library
	LIBS_SAVED="$LIBS"
	LIBS="$LIBS_SAVED -lyajl"
	AC_TRY_LINK([#include <yajl/yajl_gen.h>],[ yajl_gen handle = yajl_gen_alloc(0, 0); yajl_gen_free(handle); return(0); ],
		[ PION_JSON_LIB="-lyajl"
		  PION_JSON_CODEC="JSONCodec.la"
		  PION_HAVE_JSON="-DPION_HAVE_JSON"
		  AC_MSG_RESULT(ok) ],
		[ AC_MSG_RESULT(no)
		  AC_MSG_WARN(Unable to link with the YAJL library - disabling JSONCodec) ])
	LIBS="$LIBS_SAVED"
fi
AC_SUBST(PION_HAVE_JSON)
AC_SUBST(PION_JSON_LIB)
AC_SUBST(PION_JSON_CODEC)


# Check for Python development libraries (parts copied/adapted from az_python.m4)
AC_MSG_CHECKING([for Python support])
AC_ARG_WITH([python],
    AC_HELP_STRING([--with-python@<:@=PYTHON@:>@],
    [absolute path name of Python executable; use --without-python to disable]),
    [ python_location=$withval ], [ python_location=yes ])
if test "x$python_location" == "xno"; then
	# Python support is disabled
	AC_MSG_RESULT(no)
	az_python_use=false
	AM_CONDITIONAL(PYTHON_USE, test x"$az_python_use" = x"true")
else
	if test "x$python_location" != "xyes"; then
		# path to python executable specified
		AC_SUBST([PYTHON], ["${withval}"])
	else
		# look for python executable
		AC_PATH_PROG([PYTHON], python, [])
	fi
	if test -x "$PYTHON"; then
		AZ_PYTHON_VERSION_CHECK([2.4],
			[ AC_MSG_RESULT(yes)
			  az_python_use=true
			],
			[ AC_MSG_RESULT(no)
			  az_python_use=false
			])
	else
		AC_MSG_RESULT(no)
		AC_MSG_WARN([Unable to find path to python: $PYTHON])
		az_python_use=false
	fi
	AM_CONDITIONAL(PYTHON_USE, test x"$az_python_use" = x"true")
	if test "x$az_python_use" == "xtrue"; then
		# hard code value for Mac since detection in m4 file is severely broken
		# and doesn't work at all on 10.6+
		case "$build_os" in
		*darwin*)
			## strip out i386 archive support so that it links OK
			## unfortunately this doesn't work for some reason....
			#PYTHON_LSPEC="`echo $PYTHON_LSPEC | sed 's#-arch i386##g'`"
			#AC_SUBST(PYTHON_LSPEC)
			# force use of Python 2.5 so it works on both 10.5 and 10.6
			PYTHON_CSPEC="-I/usr/include/python2.7"
			PYTHON_LSPEC="-u PyMac_Error -lpython2.7"
			;;
		*)
			AZ_PYTHON_PREFIX( )
			AZ_PYTHON_CSPEC()
			AZ_PYTHON_LSPEC()
			;;
		esac
		PION_HAVE_PYTHON="-DPION_HAVE_PYTHON"
		PION_PYTHON_REACTOR="PythonReactor.la"
		AC_MSG_NOTICE([Python support successfully enabled])
	else
		AC_MSG_WARN([Python support disabled])
	fi
fi
AC_SUBST(PION_HAVE_PYTHON)
AC_SUBST(PION_PYTHON_REACTOR)


# Only build the XMLLogService plugin if we have log4cplus
if test "$PION_LOG_LIB" == "-llog4cplus"; then
	PION_XMLLOG_SERVICE="XMLLogService.la"
fi
AC_SUBST(PION_XMLLOG_SERVICE)


# Define platform libraries
PION_PLATFORM_LIBS="$PION_XML_LIB $PION_JSON_LIB"
AC_SUBST(PION_PLATFORM_LIBS)
