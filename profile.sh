#!/bin/sh

# gprof-enabled compilation: 
# - run in /opt/whitedb, creates exe /opt/whitedb/Main/wgdb
# - run in /opt/whitedb/Rexamples ./rrun ana002.txt creating gmon.out
# - run in /opt/whitedb/Rexamples gprof ../Main/wgdb gmon.out > res.txt
# - look at the result: less res.txt

[ -z "$CC" ] && CC="cc"

if [ -z "$(which $CC 2>/dev/null)" ]; then
    echo "Error: No compiler found"
    exit 1
fi

[ -f config.h ] || cp config-gcc.h config.h
if [ config-gcc.h -nt config.h ]; then
  echo "Warning: config.h is older than config-gcc.h, consider updating it"
fi

# With reasoner and profiling (-pg switch):
${CC} -O2 -Wall -pg -o Main/wgdb Main/wgdb.c Db/dbmem.c \
  Db/dballoc.c Db/dbdata.c Db/dblock.c Db/dbindex.c Db/dbdump.c  \
  Db/dblog.c Db/dbhash.c Db/dbcompare.c Db/dbquery.c Db/dbutil.c Db/dbmpool.c \
  Db/dbjson.c Db/dbschema.c json/yajl_all.c \
  cjson/cjson.c cjson/cjson.h \
  Reasoner/build.c  Reasoner/clstore.c  Reasoner/clterm.c  Reasoner/derive.c  Reasoner/glb.c  Reasoner/mem.c  \
  Reasoner/printerrutils.c  Reasoner/rgenloop.c  Reasoner/rmain.c  Reasoner/subsume.c  Reasoner/unify.c \
  Reasoner/resolve.c Reasoner/simp.c Reasoner/strat.c Reasoner/history.c Reasoner/prob.c Reasoner/hash.c \
  Reasoner/clmeta.c Reasoner/guide.c  Reasoner/propagate.c\
  Printer/dbotterprint.c \
  Parser/dbotter.tab.c  Parser/dbotter.yy.c  Parser/dbparse.c  Parser/dbprolog.tab.c  Parser/dbprolog.yy.c \
  Parser/dbclausify.c \
  -lm  

  
# debug and testing programs: uncomment as needed
#$CC  -O2 -Wall -o Main/indextool  Main/indextool.c Db/dbmem.c \
#  Db/dballoc.c Db/dbdata.c Db/dblock.c Db/dbindex.c Db/dblog.c \
#  Db/dbhash.c Db/dbcompare.c Db/dbquery.c Db/dbutil.c Db/dbmpool.c \
#  Db/dbjson.c Db/dbschema.c json/yajl_all.c -lm
#$CC  -O2 -Wall -o Main/selftest Main/selftest.c Db/dbmem.c \
#  Db/dballoc.c Db/dbdata.c Db/dblock.c Db/dbindex.c Test/dbtest.c Db/dbdump.c \
#  Db/dblog.c Db/dbhash.c Db/dbcompare.c Db/dbquery.c Db/dbutil.c Db/dbmpool.c \
#  Db/dbjson.c Db/dbschema.c json/yajl_all.c -lm
