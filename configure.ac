AC_INIT([upmpdcli], [1.1.4], [jfd@lesbonscomptes.com],
             [upmpdcli], [http://www.lesbonscomptes.com/upmpdcli])
AC_PREREQ([2.53])
AC_CONFIG_SRCDIR([src/upmpd.cxx])
AC_CONFIG_HEADERS([src/config.h])
AH_BOTTOM([#include "src/conf_post.h"])

AM_INIT_AUTOMAKE([1.10 no-define subdir-objects foreign])

AC_PROG_CXX

# ! Not sure that this is needed for upmpdcli and not only libupnpp
# libupnp is configured with large file support, and we need to do the same,
# else a difference in off_t size impacts struct File_Info and prevents the
# vdir to work. This does make a difference, for exemple, for Raspbian
# on the Raspberry PI. Use the same directives as libupnp's configure.ac
AC_TYPE_SIZE_T
AC_TYPE_OFF_T
AC_DEFINE([_LARGE_FILE_SOURCE], [], [Large files support])
AC_DEFINE([_FILE_OFFSET_BITS], [64], [File Offset size])

#### Libraries
AC_CHECK_LIB([pthread], [pthread_create], [], [])

# The 3 following checks for libthreadutil libixml and libupnp are normally
# unnecessary and even problematic. upmpdcli does not use them directly,
# and they should be used automatically because libupnpp is linked with them.
# Still you may need them to include them on some systems where the
# automatic link does not work.
dnl AC_CHECK_LIB([threadutil], [TimerThreadRemove], [],
dnl     AC_MSG_ERROR([libthreadutil (part of libupnp) not found]))
dnl AC_CHECK_LIB([ixml], [ixmlDocument_getElementsByTagName], [], 
dnl    AC_MSG_ERROR([libixml (usually installed as part of libupnp) not found]))
dnl AC_CHECK_LIB([upnp], [UpnpInit], [], AC_MSG_ERROR([libupnp not found]))
dnl AC_CHECK_LIB([curl], [curl_easy_init], [],AC_MSG_ERROR([libcurl not found]))
dnl AC_CHECK_LIB([expat], [XML_ParserCreate], [],AC_MSG_ERROR([libexpat not found]))


AC_CHECK_LIB([upnpp], [getsyshwaddr], [], [AC_MSG_ERROR([libupnpp])])
AC_CHECK_LIB([mpdclient], [mpd_connection_new], [],
                          AC_MSG_ERROR([libmpdclient not found]))
SCCTL_LIBS=$LIBS

AC_CHECK_LIB([rtmp], [RTMP_Alloc], [], [])
UPMPDCLI_LIBS=$LIBS
echo "UPMPDCLI_LIBS=$LIBS"

LIBS=""

AC_SUBST(UPMPDCLI_LIBS)
AC_SUBST(SCCTL_LIBS)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
