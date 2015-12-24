.SUFFIXES: .txt .html

.txt.html:
	asciidoc $<

all: upmpdcli.html sc2mpd.html scmulti.html upmpdcli-or-mpdupnp.html \
     releases.html

clean:
	rm -f *.html
