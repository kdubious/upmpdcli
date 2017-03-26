.SUFFIXES: .txt .html

# volnitsky or flask
.txt.html:
	asciidoc -b html5 -a toc2 -a theme=volnitsky $<

all: upmpdcli-manual.html upmpdcli-or-mpdupnp.html \
     releases.html mpdas-upnp.html upmpdcli-cloud.html

upmpdcli-manual.html: upmpdcli-manual.txt upmpdcli-config.txt

clean:
	rm -f *.html
