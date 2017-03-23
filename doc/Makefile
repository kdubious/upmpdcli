.SUFFIXES: .txt .html

# volnitsky or flask
.txt.html:
	asciidoc -b html5 -a toc2 -a theme=volnitsky $<

all: upmpdcli-manual.html upmpdcli-or-mpdupnp.html \
     releases.html mpdas-upnp.html upmpdcli-cloud.html

upmpdcli.html: upmpdcli.txt upmpdcli-config.txt

clean:
	rm -f *.html
