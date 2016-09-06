
AM_CPPFLAGS = -DDEBUG -g -Wall \
            -I${prefix}/include/libupnpp \
            -I$(top_srcdir)/src \
            -I$(top_srcdir)/src/cdplugins \
            -DDATADIR=\"${pkgdatadir}\" -DCONFIGDIR=\"${sysconfdir}\"

AM_CXXFLAGS = -std=c++11 

bin_PROGRAMS = upmpdcli scctl

upmpdcli_SOURCES = \
     src/avtransport.cxx \
     src/avtransport.hxx \
     src/cdplugin.hxx \
     src/cdplugins/cmdtalk.cpp \
     src/cdplugins/cmdtalk.h \
     src/cdplugins/plgwithslave.cxx \
     src/cdplugins/plgwithslave.hxx \
     src/closefrom.cpp \
     src/closefrom.h \
     src/conf_post.h \
     src/conftree.cpp \
     src/conftree.h \
     src/conman.cxx \
     src/conman.hxx \
     src/contentdirectory.cxx \
     src/contentdirectory.hxx \
     src/execmd.cpp \
     src/execmd.h \
     src/httpfs.cxx \
     src/httpfs.hxx \
     src/json.hpp \
     src/main.hxx \
     src/main.cxx \
     src/mediaserver.cxx \
     src/mediaserver.hxx \
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
     src/readfile.cpp \
     src/readfile.h \
     src/renderctl.cxx \
     src/renderctl.hxx \
     src/smallut.cpp \
     src/smallut.h \
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

scctl_LDADD = $(SCCTL_LIBS)
              
dist_pkgdata_DATA = src/description.xml src/AVTransport.xml \
                  src/RenderingControl.xml src/ConnectionManager.xml \
                  src/OHProduct.xml src/OHInfo.xml src/OHTime.xml \
                  src/OHVolume.xml src/OHPlaylist.xml src/OHReceiver.xml \
                  src/OHRadio.xml src/ContentDirectory.xml \
                  src/protocolinfo.txt src/presentation.html

cdpluginspycomdir = $(pkgdatadir)/cdplugins/pycommon
dist_cdpluginspycom_DATA = \
                    src/cdplugins/pycommon/cmdtalk.py \
                    src/cdplugins/pycommon/cmdtalkplugin.py \
                    src/cdplugins/pycommon/conftree.py \
                    src/cdplugins/pycommon/routing.py \
                    src/cdplugins/pycommon/upmplgmodels.py \
                    src/cdplugins/pycommon/upmplgutils.py

cdpluginstidaldir = $(pkgdatadir)/cdplugins/tidal
dist_cdpluginstidal_DATA = \
                  src/cdplugins/tidal/tidal-app.py 
cdpluginstidallibdir = $(pkgdatadir)/cdplugins/tidal/tidalapi
dist_cdpluginstidallib_DATA = \
                  src/cdplugins/tidal/tidalapi/__init__.py \
                  src/cdplugins/tidal/tidalapi/models.py

cdpluginsqobuzdir = $(pkgdatadir)/cdplugins/qobuz
dist_cdpluginsqobuz_DATA = \
                  src/cdplugins/qobuz/debug.py  \
                  src/cdplugins/qobuz/exception.py \
                  src/cdplugins/qobuz/__init__.py \
                  src/cdplugins/qobuz/qobuz-app.py \
                  src/cdplugins/qobuz/session.py 
cdpluginsqobuzlibdir = $(pkgdatadir)/cdplugins/qobuz/api
dist_cdpluginsqobuzlib_DATA = \
                  src/cdplugins/qobuz/api/easy.py \
                  src/cdplugins/qobuz/api/__init__.py \
                  src/cdplugins/qobuz/api/raw.py

cdpluginsgmusicdir = $(pkgdatadir)/cdplugins/gmusic
dist_cdpluginsgmusic_DATA = \
                  src/cdplugins/gmusic/gmusic-app.py \
                  src/cdplugins/gmusic/session.py 

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
	(cd $(DESTDIR)/$(cdpluginstidaldir); chmod a+x tidal-app.py) 
	(cd $(DESTDIR)/$(cdpluginsqobuzdir); chmod a+x qobuz-app.py) 
	(cd $(DESTDIR)/$(cdpluginsgmusicdir); chmod a+x gmusic-app.py) 

dist_pkgdata_SCRIPTS = samplescripts/Analog-Input

dist_sysconf_DATA = src/upmpdcli.conf-dist
            
dist_man1_MANS = man/upmpdcli.1

dist-hook:
	rm -rf rdpl2stream/__pycache__/
	test -z "`git status -s | grep -v upmpdcli-$(VERSION)| grep -v rdpl2stream.tar.gz`"
	git tag -f -a upmpdcli-v$(VERSION) -m 'version $(VERSION)'
