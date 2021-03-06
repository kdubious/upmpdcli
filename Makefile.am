AM_CPPFLAGS = -DDEBUG -g -Wall \
	    -DMDU_INCLUDE_LOG='<libupnpp/log.h>' \
            $(upnpp_CFLAGS) \
            $(libmpdclient_CFLAGS) \
            $(libmicrohttpd_CFLAGS) \
            $(jsoncpp_CFLAGS) \
            -I$(top_srcdir)/src \
            -I$(top_srcdir)/src/mediaserver/cdplugins \
            -DDATADIR=\"${pkgdatadir}\" -DCONFIGDIR=\"${sysconfdir}\"

AM_CXXFLAGS = -std=c++11 

bin_PROGRAMS = upmpdcli scctl

if MAKECONFGUI
all-local: qtconfgui
qtconfgui:
	test -d cfgui || mkdir cfgui
	(cd cfgui; ${QMAKE} PREFIX=${prefix} upmpdcli-config.pro)
	$(MAKE) -C cfgui LFLAGS="$(LDFLAGS)" prefix=$(prefix) \
                exec_prefix=$(exec_prefix) libdir=$(libdir)
clean-local: qtconfgui-clean
qtconfgui-clean:
	-$(MAKE) -C cfgui clean
install-exec-local: qtconfgui-install
qtconfgui-install:
	$(MAKE) -C cfgui LFLAGS="$(LDFLAGS)" INSTALL_ROOT=$(DESTDIR) \
                prefix=$(prefix) exec_prefix=$(exec_prefix) libdir=$(libdir) \
                install
endif

CLEANFILES = $(BUILT_SOURCES)

upmpdcli_SOURCES = \
     src/avtransport.cxx \
     src/avtransport.hxx \
     src/chrono.cpp \
     src/chrono.h \
     src/closefrom.cpp \
     src/closefrom.h \
     src/conf_post.h \
     src/conftree.cpp \
     src/conftree.h \
     src/conman.cxx \
     src/conman.hxx \
     src/execmd.cpp \
     src/execmd.h \
     src/main.cxx \
     src/main.hxx \
     src/mediaserver/cdplugins/abuffer.h \
     src/mediaserver/cdplugins/bufxchange.h \
     src/mediaserver/cdplugins/cdplugin.hxx \
     src/mediaserver/cdplugins/cmdtalk.cpp \
     src/mediaserver/cdplugins/cmdtalk.h \
     src/mediaserver/cdplugins/curlfetch.cpp \
     src/mediaserver/cdplugins/curlfetch.h \
     src/mediaserver/cdplugins/netfetch.cpp \
     src/mediaserver/cdplugins/netfetch.h \
     src/mediaserver/cdplugins/plgwithslave.cxx \
     src/mediaserver/cdplugins/plgwithslave.hxx \
     src/mediaserver/cdplugins/streamproxy.cpp \
     src/mediaserver/cdplugins/streamproxy.h \
     src/mediaserver/contentdirectory.cxx \
     src/mediaserver/contentdirectory.hxx \
     src/mediaserver/mediaserver.cxx \
     src/mediaserver/mediaserver.hxx \
     src/mpdcli.cxx \
     src/mpdcli.hxx \
     src/netcon.cpp \
     src/netcon.h \
     src/ohcredentials.cxx \
     src/ohcredentials.hxx \
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
     src/protocolinfo.cxx \
     src/protocolinfo.hxx \
     src/pathut.cpp \
     src/pathut.h \
     src/readfile.cpp \
     src/readfile.h \
     src/renderctl.cxx \
     src/renderctl.hxx \
     src/smallut.cpp \
     src/smallut.h \
     src/sysvshm.cpp \
     src/sysvshm.h \
     src/upmpd.cxx \
     src/upmpd.hxx \
     src/upmpdutils.cxx \
     src/upmpdutils.hxx \
     src/workqueue.h

upmpdcli_LDADD = $(UPMPDCLI_LIBS)

if ENABLESPOTIFY
upmpdcli_SOURCES += src/mediaserver/cdplugins/spotify/spotiproxy.cpp \
     src/mediaserver/cdplugins/spotify/spotiproxy.h 
AM_CXXFLAGS += -DENABLE_SPOTIFY
endif

scctl_SOURCES = \
    scctl_src/scctl.cpp \
    src/netcon.cpp \
    src/smallut.cpp \
    src/upmpdutils.cxx

scctl_LDADD = $(SCCTL_LIBS)
              
dist_pkgdata_DATA = src/description.xml src/AVTransport.xml \
                  src/RenderingControl.xml src/ConnectionManager.xml \
                  src/OHProduct.xml src/OHCredentials.xml src/OHInfo.xml \
                  src/OHTime.xml \
                  src/OHVolume.xml src/OHPlaylist.xml src/OHReceiver.xml \
                  src/OHRadio.xml src/mediaserver/ContentDirectory.xml \
                  src/mediaserver/MS-description.xml \
                  src/protocolinfo.txt src/presentation.html \
                  src/upmpdcli.conf-dist 

radioscriptsdir = $(pkgdatadir)/radio_scripts
dist_radioscripts_DATA = \
                    src/radio_scripts/radio-paradise-get-art.sh \
                    src/radio_scripts/radio-paradise-get-flac.py \
                    src/radio_scripts/fip-meta.py 

cdpluginspycomdir = $(pkgdatadir)/cdplugins/pycommon
dist_cdpluginspycom_DATA = \
                    src/mediaserver/cdplugins/pycommon/cmdtalk.py \
                    src/mediaserver/cdplugins/pycommon/cmdtalkplugin.py \
                    src/mediaserver/cdplugins/pycommon/conftree.py \
                    src/mediaserver/cdplugins/pycommon/routing.py \
                    src/mediaserver/cdplugins/pycommon/upmplgmodels.py \
                    src/mediaserver/cdplugins/pycommon/upmplgutils.py
cdpluginstidaldir = $(pkgdatadir)/cdplugins/tidal
dist_cdpluginstidal_DATA = \
                  src/mediaserver/cdplugins/tidal/tidal-app.py 
cdpluginstidallibdir = $(pkgdatadir)/cdplugins/tidal/tidalapi
dist_cdpluginstidallib_DATA = \
                  src/mediaserver/cdplugins/tidal/tidalapi/__init__.py \
                  src/mediaserver/cdplugins/tidal/tidalapi/models.py

cdpluginsqobuzdir = $(pkgdatadir)/cdplugins/qobuz
dist_cdpluginsqobuz_DATA = \
                  src/mediaserver/cdplugins/qobuz/debug.py  \
                  src/mediaserver/cdplugins/qobuz/exception.py \
                  src/mediaserver/cdplugins/qobuz/__init__.py \
                  src/mediaserver/cdplugins/qobuz/qobuz-app.py \
                  src/mediaserver/cdplugins/qobuz/session.py 
cdpluginsqobuzlibdir = $(pkgdatadir)/cdplugins/qobuz/api
dist_cdpluginsqobuzlib_DATA = \
                  src/mediaserver/cdplugins/qobuz/api/__init__.py \
                  src/mediaserver/cdplugins/qobuz/api/raw.py

cdpluginsgmusicdir = $(pkgdatadir)/cdplugins/gmusic
dist_cdpluginsgmusic_DATA = \
                  src/mediaserver/cdplugins/gmusic/gmusic-app.py \
                  src/mediaserver/cdplugins/gmusic/session.py 

cdpluginsspotifydir = $(pkgdatadir)/cdplugins/spotify
dist_cdpluginsspotify_DATA = \
                  src/mediaserver/cdplugins/spotify/spotify-app.py \
                  src/mediaserver/cdplugins/spotify/session.py \
                  src/mediaserver/cdplugins/spotify/upmspotid.py


cdpluginsspotifylibdir = $(pkgdatadir)/cdplugins/spotify/spotipy
dist_cdpluginsspotifylib_DATA = \
                  src/mediaserver/cdplugins/spotify/spotipy/__init__.py \
                  src/mediaserver/cdplugins/spotify/spotipy/client.py \
                  src/mediaserver/cdplugins/spotify/spotipy/oauth2.py \
                  src/mediaserver/cdplugins/spotify/spotipy/util.py


cdpluginsuprcldir = $(pkgdatadir)/cdplugins/uprcl
dist_cdpluginsuprcl_DATA = \
                 src/mediaserver/cdplugins/uprcl/bottle.py \
                 src/mediaserver/cdplugins/uprcl/minimconfig.py \
                 src/mediaserver/cdplugins/uprcl/minimtagfixer.py \
                 src/mediaserver/cdplugins/uprcl/rclconfig-fields \
                 src/mediaserver/cdplugins/uprcl/rclconfig-mimeconf \
                 src/mediaserver/cdplugins/uprcl/rclconfig-mimemap \
                 src/mediaserver/cdplugins/uprcl/rwlock.py \
                 src/mediaserver/cdplugins/uprcl/uprcl-app.py \
                 src/mediaserver/cdplugins/uprcl/uprclfolders.py \
                 src/mediaserver/cdplugins/uprcl/uprclhttp.py \
                 src/mediaserver/cdplugins/uprcl/uprclindex.py \
                 src/mediaserver/cdplugins/uprcl/uprclinit.py \
                 src/mediaserver/cdplugins/uprcl/uprclplaylists.py \
                 src/mediaserver/cdplugins/uprcl/uprclsearch.py \
                 src/mediaserver/cdplugins/uprcl/uprcltags.py \
                 src/mediaserver/cdplugins/uprcl/uprcltagscreate.py \
                 src/mediaserver/cdplugins/uprcl/uprcluntagged.py \
                 src/mediaserver/cdplugins/uprcl/uprclutils.py

EXTRA_DIST = \
    autogen.sh \
    cfgui/Makefile \
    cfgui/confgui.cpp \
    cfgui/confgui.h \
    cfgui/confmain.cpp \
    cfgui/mainwindow.h \
    cfgui/picoxml.h \
    cfgui/upmpdcli-config.pro \
    rdpl2stream.tar.gz \
    autogen.sh \
    src/mediaserver/cdplugins/spotify/spotiproxy.cpp \
    src/mediaserver/cdplugins/spotify/spotiproxy.h \
    systemd/upmpdcli.service \
    uprclbottle.tar.gz \
    web.tar.gz


web.tar.gz: bogus
	if test -d $(srcdir)/web; then \
           (cd $(srcdir); tar cvzf - web) > web.tar.gz; \
        fi

uprclbottle.tar.gz: bogus
	if test -d $(srcdir)/src/mediaserver/cdplugins/uprcl/bottle; then \
	(cd $(srcdir)/src/mediaserver; tar cvzf - cdplugins/uprcl/bottle) > \
            uprclbottle.tar.gz; \
        fi

rdpl2stream.tar.gz: bogus
	if test -d $(srcdir)/rdpl2stream; then \
	    (cd $(srcdir); rm -rf rdpl2stream/__pycache__/; \
               tar cvzf - rdpl2stream) > rdpl2stream.tar.gz; \
        fi

bogus:

install-data-hook: web.tar.gz rdpl2stream.tar.gz uprclbottle.tar.gz
	whoami
	(cd $(DESTDIR)$(sysconfdir); test -f upmpdcli.conf || \
		mv upmpdcli.conf-dist upmpdcli.conf; chmod 600 upmpdcli.conf)
	(cd $(DESTDIR)$(pkgdatadir); tar xzf -) < web.tar.gz 
	(cd $(DESTDIR)$(pkgdatadir); tar xzf -) < rdpl2stream.tar.gz
	(cd $(DESTDIR)$(pkgdatadir); tar xzf -) < uprclbottle.tar.gz
	@(cd $(DESTDIR)$(pkgdatadir); \
         chown -R root:root rdpl2stream web cdplugins) || true
	(cd $(DESTDIR)$(pkgdatadir); test -d src_scripts || mkdir src_scripts)
	(cd $(DESTDIR)/$(cdpluginstidaldir); chmod a+x tidal-app.py) 
	(cd $(DESTDIR)/$(cdpluginsqobuzdir); chmod a+x qobuz-app.py) 
	(cd $(DESTDIR)/$(cdpluginsgmusicdir); chmod a+x gmusic-app.py) 
	(cd $(DESTDIR)/$(cdpluginsspotifydir); chmod a+x spotify-app.py) 
	(cd $(DESTDIR)/$(cdpluginsuprcldir); chmod a+x uprcl-app.py) 
	(cd $(DESTDIR)/$(radioscriptsdir);chmod a+rx . *)
	rm -rf $(DESTDIR)/$(pkgdatadir)/cdplugins

dist_sysconf_DATA = src/upmpdcli.conf-dist

dist_man1_MANS = man/upmpdcli.1

dist-hook:
	rm -rf rdpl2stream/__pycache__/
	test -z "`git status -s | grep -v '??' | grep -v Makefile.am`" 
	git tag -a upmpdcli-v$(VERSION) -m 'version $(VERSION)'
