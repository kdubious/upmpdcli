
AM_CPPFLAGS = -DDEBUG -g -Wall -I${prefix}/include/libupnpp \
            -DDATADIR=\"${pkgdatadir}\" -DCONFIGDIR=\"${sysconfdir}\"

AM_CXXFLAGS = -std=c++11 

bin_PROGRAMS = upmpdcli scctl

upmpdcli_SOURCES = \
     src/avtransport.cxx \
     src/avtransport.hxx \
     src/closefrom.cpp \
     src/closefrom.h \
     src/conf_post.h \
     src/conftree.cpp \
     src/conftree.h \
     src/conman.cxx \
     src/conman.hxx \
     src/execmd.cpp \
     src/execmd.h \
     src/httpfs.cxx \
     src/httpfs.hxx \
     src/main.cxx \
     src/mpdcli.cxx \
     src/mpdcli.hxx \
     src/netcon.cpp \
     src/netcon.h \
     src/ohinfo.cxx \
     src/ohinfo.hxx \
     src/ohmetacache.cxx \
     src/ohmetacache.hxx \
     src/ohplaylist.cxx \
     src/ohplaylist.hxx \
     src/ohproduct.cxx \
     src/ohproduct.hxx \
     src/ohradio.cxx \
     src/ohradio.hxx \
     src/ohreceiver.cxx \
     src/ohreceiver.hxx \
     src/ohservice.hxx \
     src/ohsndrcv.cxx \
     src/ohsndrcv.hxx \
     src/ohtime.cxx \
     src/ohtime.hxx \
     src/ohvolume.cxx \
     src/ohvolume.hxx \
     src/pathut.cpp \
     src/pathut.h \
     src/smallut.cpp \
     src/smallut.h \
     src/readfile.cpp \
     src/readfile.h \
     src/renderctl.cxx \
     src/renderctl.hxx \
     src/upmpd.cxx \
     src/upmpd.hxx \
     src/upmpdutils.cxx \
     src/upmpdutils.hxx
                           
upmpdcli_LDADD = $(UPMPDCLI_LIBS)

scctl_SOURCES = \
    scctl_src/scctl.cpp \
    src/netcon.cpp \
    src/smallut.cpp \
    src/upmpdutils.cxx

scctl_LDADD = $(UPMPDCLI_LIBS)
              
dist_pkgdata_DATA = src/description.xml src/AVTransport.xml \
                  src/RenderingControl.xml src/ConnectionManager.xml \
                  src/OHProduct.xml src/OHInfo.xml src/OHTime.xml \
                  src/OHVolume.xml src/OHPlaylist.xml src/OHReceiver.xml \
                  src/OHRadio.xml \
                  src/protocolinfo.txt src/presentation.html

EXTRA_DIST = web.tar.gz rdpl2stream.tar.gz systemd/upmpdcli.service

web.tar.gz: bogus
	if test -d $(srcdir)/web; then \
           (cd $(srcdir); tar cvzf - web) > web.tar.gz; fi

rdpl2stream.tar.gz: bogus
	if test -d $(srcdir)/rdpl2stream; then \
           (cd $(srcdir); rm -rf rdpl2stream/__pycache__/; tar cvzf - rdpl2stream) > \
                        rdpl2stream.tar.gz; fi

bogus:

install-data-hook: web.tar.gz rdpl2stream.tar.gz
	(cd $(DESTDIR)$(sysconfdir); test -f upmpdcli.conf || \
        	mv upmpdcli.conf-dist upmpdcli.conf)
	(cd $(DESTDIR)$(pkgdatadir); tar xzf -) < web.tar.gz 
	(cd $(DESTDIR)$(pkgdatadir); tar xzf -) < rdpl2stream.tar.gz
	(cd $(DESTDIR)$(pkgdatadir); test -d src_scripts || mkdir src_scripts)

dist_pkgdata_SCRIPTS = samplescripts/Analog-Input

dist_sysconf_DATA = src/upmpdcli.conf-dist
            
dist_man1_MANS = man/upmpdcli.1

dist-hook:
	rm -rf rdpl2stream/__pycache__/
	test -z "`git status -s | grep -v upmpdcli-$(VERSION)| grep -v rdpl2stream.tar.gz`"
	git tag -f -a upmpdcli-v$(VERSION) -m 'version $(VERSION)'
