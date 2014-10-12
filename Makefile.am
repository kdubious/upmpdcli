
AM_CPPFLAGS = -DDEBUG -g -Wall \
            -DDATADIR=\"${pkgdatadir}\" -DCONFIGDIR=\"${sysconfdir}\" \
            -DPACKAGE_VERSION=\"${PACKAGE_VERSION}\"

AM_CXXFLAGS = -std=c++0x 

bin_PROGRAMS = upmpdcli

upmpdcli_SOURCES = \
     src/conftree.cxx \
     src/conftree.hxx \
     src/ohmetacache.cxx \
     src/ohmetacache.hxx \
     src/mpdcli.cxx \
     src/mpdcli.hxx \
     src/upmpd.hxx \
     src/upmpd.cxx \
     src/upmpdutils.cxx \
     src/upmpdutils.hxx \
     src/renderctl.hxx \
     src/renderctl.cxx \
     src/avtransport.hxx \
     src/avtransport.cxx \
     src/conman.hxx \
     src/conman.cxx \
     src/ohproduct.hxx \
     src/ohproduct.cxx \
     src/ohinfo.hxx \
     src/ohinfo.cxx \
     src/ohtime.hxx \
     src/ohtime.cxx \
     src/ohvolume.hxx \
     src/ohvolume.cxx \
     src/ohplaylist.hxx \
     src/ohplaylist.cxx \
     src/ptmutex.hxx \
     src/workqueue.hxx
                           
upmpdcli_LDADD = -lupnpp -lmpdclient -lpthread

dist_pkgdata_DATA = src/description.xml src/AVTransport.xml \
                  src/RenderingControl.xml src/ConnectionManager.xml \
                  src/OHProduct.xml src/OHInfo.xml src/OHTime.xml \
                  src/OHVolume.xml src/OHPlaylist.xml src/protocolinfo.txt

dist_sysconf_DATA = src/upmpdcli.conf

dist_man1_MANS = man/upmpdcli.1

EXTRA_DIST = systemd/upmpdcli.service
dist-hook:
	test -z "`git status -s | grep -v upmpdcli-$(VERSION)`"
	git tag -f -a upmpdcli-v$(VERSION) -m 'version $(VERSION)'
