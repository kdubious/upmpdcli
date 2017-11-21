AC_DEFUN([AC_FUNC_LIBUPNPP_LINNSONGCAST],[
AC_SINGLE_CXX_CHECK([ac_cv_func_setsourceindex_in_linn],
                    [setSourceIndex], [<libupnpp/control/linnsongcast.hxx>],
                    [const std::string s; int i=0; UPnPClient::Songcast::setSourceIndex(s,i);])
if test "$ac_cv_func_setsourceindex_in_linn" = "yes" ; then
  AC_DEFINE([HAVE_SETSOURCEINDEX_IN_LINN],1,[Set to 1 if the setSourceIndex function is found in libupnpp])
fi

AC_SINGLE_CXX_CHECK([ac_cv_func_setreceiversplaying_in_linn],
                    [setReceiversPlaying], [<libupnpp/control/linnsongcast.hxx>],
                    [const std::vector<std::string> v; UPnPClient::Songcast::setReceiversPlaying(v);])
if test "$ac_cv_func_setreceiversplaying_in_linn" = "yes" ; then
  AC_DEFINE([HAVE_SETRECEIVERSPLAYING_IN_LINN],1,[Set to 1 if the setReceiversPlaying function is found in libupnpp])
fi

AC_SINGLE_CXX_CHECK([ac_cv_func_withstatus_in_linn],
                    [stopReceiversWithStatus], [<libupnpp/control/linnsongcast.hxx>],
                    [const std::vector<std::string> args; std::vector<std::string> reasons; UPnPClient::Songcast::stopReceiversWithStatus(args, reasons);])
if test "$ac_cv_func_withstatus_in_linn" = "yes" ; then
  AC_DEFINE([HAVE_WITHSTATUS_IN_LINN],1,[Set to 1 if the stopReceiversWithStatus function is found in libupnpp])
fi
])