AC_DEFUN([AC_FUNC_LIBUPNPP_LINNSONGCAST],[
AC_SINGLE_CXX_CHECK([ac_cv_func_setsourceindex_in_linn],
                    [setSourceIndex], [<libupnpp/control/linnsongcast.hxx>],
                    [const std::string s; int i=0; UPnPClient::Songcast::setSourceIndex(s,i);])
if test "$ac_cv_func_setsourceindex_in_linn" = "yes" ; then
  AC_DEFINE([HAVE_SETSOURCEINDEX_IN_LINN],1,[Set to 1 if the setSourceIndex function is found in libupnpp])
fi
])