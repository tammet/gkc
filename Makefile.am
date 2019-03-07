# $Id: $
# $Source: $
#
# Top level Makefile.am of WhiteDB: calling makes in subdirs
#

#AUTOMAKE_OPTIONS = foreign
AUTOMAKE_OPTIONS = subdir-objects

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = Db json Parser Printer Reasoner Test Main

# --- man ---

# man_MANS = Doc/gkc.7

# --- optional components ----


# -------- Tests  -------------------

TESTS = Test/common.test Test/log.test
Test/log.log: Test/common.log

# -------- Extras -------------------


EXTRA_DIST = Bootstrap README README.md INSTALL MANIFEST GPLLICENCE NEWS AUTHORS \
 config-w32.h config-gcc.h compile.sh compile.bat \
 Db/dbapi.h Db/rdfapi.h Db/indexapi.h \
 Parser/dbotter.y Parser/dbotter.l Parser/dbprolog.y Parser/dbprolog.l \
 Examples/steam.txt Examples.steam.p \
 Test/common.test Test/log.test \
 gkc.pc.in

# this conflicts with the distcheck target, so disabled for now.
#dist-hook:
#	cp $(top_distdir)/config-gcc.h $(top_distdir)/config.h

# -------- Pkg-config ---------------

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gkc.pc
