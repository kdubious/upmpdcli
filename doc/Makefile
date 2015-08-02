.SUFFIXES: .txt .html

.txt.html:
	asciidoc $<

all: upmpdcli.html scmulti.html upmpdcli-or-mpdupnp.html \
     release-0.8.html release-0.9.html \
     release-0.11.html

clean:
	rm -f *.html
