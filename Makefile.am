# $Id: $
# $Source: $
#
# Top level Makefile.am of WhiteDB: calling makes in subdirs
#

#AUTOMAKE_OPTIONS = foreign
AUTOMAKE_OPTIONS = subdir-objects

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = Db json Parser Reasoner Printer Test Main Examples
if PYTHON
SUBDIRS += Python
endif

# --- man ---

man_MANS = Doc/gkc.7

# --- optional components ----


# -------- Tests  -------------------

TESTS = Test/common.test Test/log.test
Test/log.log: Test/common.log

# -------- Extras -------------------


EXTRA_DIST = Bootstrap README README.md INSTALL MANIFEST GPLLICENCE NEWS AUTHORS \
 Doc/whitedb.7 Doc/python.txt Doc/Manual.txt \
 Doc/Install.txt Doc/Utilities.txt Doc/Tutorial.txt \
 wgdb.def config-w32.h config-gcc.h Doxyfile compile.sh compile.bat \
 unite.sh \
 Db/dbapi.h Db/rdfapi.h Db/indexapi.h \
 Python/compile.bat Python/compile.sh Python/tests.py \
 Parser/dbotter.y Parser/dbotter.l Parser/dbprolog.y Parser/dbprolog.l \
 Test/common.test Test/log.test \
 whitedb.pc.in

# this conflicts with the distcheck target, so disabled for now.
#dist-hook:
#	cp $(top_distdir)/config-gcc.h $(top_distdir)/config.h

# -------- Pkg-config ---------------

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = whitedb.pc
