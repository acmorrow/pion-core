# -----------------------------------------
# Pion Platform autoconf configuration file
# -----------------------------------------

# Set Pion version information
AC_INIT([pion-platform], [4.0.14], [support@atomiclabs.com])

# Initialize some other things
AC_PREREQ([2.59])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([m4])
AC_CONFIG_SRCDIR(platform/src/Vocabulary.cpp)

# package and version args are now taken from AC_INIT()
AM_INIT_AUTOMAKE

# Disable static libraries to speed up build (they are not used)
AM_DISABLE_STATIC

# Needed to set per-target compilation flags
AM_PROG_CC_C_O

# Setup libtool
AC_LIBTOOL_WIN32_DLL
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL

# Setup doxygen support
DX_DOXYGEN_FEATURE(ON)
DX_HTML_FEATURE(ON)
DX_INIT_DOXYGEN(pion-platform, platform/doc/Doxyfile, platform/doc)

# Define CXXFLAGS before AC_PROG_CXX to suppress the default autoconf
# compiler options
CFLAGS="-DPION_PLATFORM"
CXXFLAGS="-DPION_PLATFORM"

# Currently only the embedded common library can be used
PION_COMMON_HOME=`pwd`/common
PION_COMMON_LIB="$PION_COMMON_HOME/src/libpion-common.la"
PION_COMMON_MAKEDIRS=""
AC_SUBST(PION_COMMON_HOME)
AC_SUBST(PION_COMMON_LIB)
AC_SUBST(PION_COMMON_MAKEDIRS)
AC_MSG_NOTICE([Using the embedded pion-common library])

# Currently only the embedded net library can be used
PION_NET_HOME=`pwd`/net
PION_NET_LIB="$PION_NET_HOME/src/libpion-net.la"
AC_SUBST(PION_NET_HOME)
AC_SUBST(PION_NET_LIB)
AC_MSG_NOTICE([Using the embedded pion-net library])

# Include pion-config.inc
m4_include([common/build/pion-setup.inc])
m4_include([common/build/pion-boost.inc])
m4_include([common/build/pion-config.inc])
m4_include([platform/build/pion-config.inc])

# Output Makefiles
AC_OUTPUT(Makefile pion-platform.pc common/pion-common.pc net/pion-net.pc
	common/Makefile common/include/Makefile common/include/pion/Makefile
	common/src/Makefile common/tests/Makefile common/tests/PluginsUsedByUnitTests/Makefile
	net/Makefile net/include/Makefile net/include/pion/Makefile net/include/pion/net/Makefile
	net/src/Makefile net/services/Makefile net/utils/Makefile net/tests/Makefile
	platform/Makefile platform/include/Makefile
	platform/include/pion/Makefile platform/include/pion/platform/Makefile
	platform/src/Makefile platform/server/Makefile platform/tests/Makefile
	platform/codecs/Makefile platform/protocols/Makefile
	platform/reactors/Makefile platform/databases/Makefile
	platform/services/Makefile sqlite/Makefile)
