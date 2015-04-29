.SUFFIXES: .txt .html

.txt.html:
	asciidoc $<

all: upmpdcli.html scmulti.html release-0.8.html release-0.9.html

clean:
	rm -f *.html
