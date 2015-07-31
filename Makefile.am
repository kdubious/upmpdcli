
AM_CPPFLAGS = -DDEBUG -g -Wall \
            -DDATADIR=\"${pkgdatadir}\" -DCONFIGDIR=\"${sysconfdir}\" \
            -DPACKAGE_VERSION=\"${PACKAGE_VERSION}\"

AM_CXXFLAGS = -std=c++0x 

bin_PROGRAMS = upmpdcli scctl

upmpdcli_SOURCES = \
     src/avtransport.cxx \
     src/avtransport.hxx \
     src/execmd.cpp \
     src/execmd.h \
     src/httpfs.cxx \
     src/httpfs.hxx \
     src/netcon.cpp \
     src/netcon.h \
     src/closefrom.cpp \
     src/closefrom.h \
     src/conftree.cxx \
     src/conftree.hxx \
     src/conman.cxx \
     src/conman.hxx \
     src/mpdcli.cxx \
     src/mpdcli.hxx \
     src/ohinfo.cxx \
     src/ohinfo.hxx \
     src/ohmetacache.cxx \
     src/ohmetacache.hxx \
     src/ohplaylist.cxx \
     src/ohplaylist.hxx \
     src/ohproduct.cxx \
     src/ohreceiver.hxx \
     src/ohreceiver.cxx \
     src/ohproduct.hxx \
     src/ohtime.cxx \
     src/ohtime.hxx \
     src/ohvolume.cxx \
     src/ohvolume.hxx \
     src/ptmutex.hxx \
     src/renderctl.cxx \
     src/renderctl.hxx \
     src/upmpd.cxx \
     src/upmpd.hxx \
     src/upmpdutils.cxx \
     src/upmpdutils.hxx \
     src/workqueue.hxx
                           
upmpdcli_LDADD = $(UPMPDCLI_LIBS)

scctl_SOURCES = \
    scctl_src/scctl.cpp \
    src/netcon.cpp \
    src/upmpdutils.cxx

scctl_LDADD = -lupnpp -lpthread
              
dist_pkgdata_DATA = src/description.xml src/AVTransport.xml \
                  src/RenderingControl.xml src/ConnectionManager.xml \
                  src/OHProduct.xml src/OHInfo.xml src/OHTime.xml \
                  src/OHVolume.xml src/OHPlaylist.xml src/OHReceiver.xml \
                  src/protocolinfo.txt src/presentation.html

EXTRA_DIST = web.tar.gz systemd/upmpdcli.service

web.tar.gz: bogus
	if test -d $(srcdir)/web; then \
           (cd $(srcdir); tar cvzf - web) > web.tar.gz; fi

bogus:

install-data-hook: web.tar.gz
	(cd $(DESTDIR)$(pkgdatadir); tar xzf -) < web.tar.gz 

dist_sysconf_DATA = src/upmpdcli.conf

dist_man1_MANS = man/upmpdcli.1

dist-hook:
	test -z "`git status -s | grep -v upmpdcli-$(VERSION)`"
	git tag -f -a upmpdcli-v$(VERSION) -m 'version $(VERSION)'
