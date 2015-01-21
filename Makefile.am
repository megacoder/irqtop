SUBDIRS=src

DIST_EXTRA=README.md README.html README

README.html:: README.md
	rpm -q --quiet python-markdown2 || ( echo Need python-markdown2; exit 1 )
	markdown2 <README.md >$@

README:: README.html
	rpm -q --quiet lynx || ( echo Need lynx; exit 1 )
	lynx -dump README.html >$@
