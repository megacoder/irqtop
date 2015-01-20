SUBDIRS=src

DIST_EXTRA=README.md README.html README

README.html:: README.md
	markdown2 <README.md >$@

README:: README.html
	lynx -dump README.html >$@
