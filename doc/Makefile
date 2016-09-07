.SUFFIXES: .txt .html

.txt.html:
	asciidoc $<

all: upmpdcli.html sc2mpd.html scmulti.html upmpdcli-or-mpdupnp.html \
     releases.html mpdas-upnp.html upmpdcli-cloud.html

upmpdcli.html: upmpdcli.txt upmpdcli-config.txt

clean:
	rm -f *.html
