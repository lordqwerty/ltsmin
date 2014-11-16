ACLOCAL_AMFLAGS = -I m4

SUBDIRS = @subdirs@ lemon examples src testsuite
if HAVE_ASCIIDOC
if HAVE_XMLTO
SUBDIRS += doc
endif
endif
EXTRA_DIST  = ltsminreconf
EXTRA_DIST += Doxyfile
EXTRA_DIST += contrib/bash-completion/ltsmin
EXTRA_DIST += CODING-STANDARDS

doc_DATA = AUTHORS COPYING README CODING-STANDARDS

if HAVE_SPINS_COMPILER
dist_pkgdata_DATA = spins/spins.jar
endif

if HAVE_PROB_COMPILER
dist_pkgdata_DATA2 = ProB-StateSpaceExtractor/ProB-StateSpaceExtractor-1.0.jar
endif

MAINTAINERCLEANFILES = spins/spins.jar ProBWrapper/ProB-StateSpaceExtractor-1.0.jar

if DX_COND_doc
mostlyclean-local: doxygen-clean
endif

.DELETE_ON_ERROR:

include m4/amdoxygen.m4

distcheck-hook:
# sample check whether manpages are present in generated tarball
	test -f $(top_distdir)/doc/etf.5
	test -f $(top_distdir)/doc/gcf.1

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

spins/spins.jar:
	test -f $@ || (cd "${top_srcdir}/spins/src" && $(ANT))

ProB-StateSpaceExtractor/ProB-StateSpaceExtractor-1.0.jar:
	test -f $@ || (cd "${top_srcdir}/ProB-StateSpaceExtractor" && "gradlew jar")
