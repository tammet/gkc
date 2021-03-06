@rem Build a gkc prover gkc.exe under Windows using the command-line C compiler cl

@rem Note: 
@rem * uses pre-built parser in the Builtparser folder, not the bison/flex combo
@rem * configuration is set in config-w32.h
@rem * compiler switches /w... suppress specific unnecessary warnings

cl /O2 /W2 /w34005 /wd4477 /wd4311 /wd4133 /Fegkc ^
  Main/gkc.c ^
  Db/dbmem.c Db/dballoc.c Db/dbdata.c ^
  Db/dblock.c Db/dbindex.c Db/dbdump.c Db/dblog.c Db/dbhash.c Db/dbcompare.c ^
  Db/dbquery.c Db/dbutil.c Db/dbmpool.c Db/dbjson.c Db/dbschema.c ^
  json/yajl_all.c ^
  cjson/cjson.c ^
  Reasoner/build.c  Reasoner/clstore.c  Reasoner/clterm.c  Reasoner/derive.c ^
  Reasoner/glb.c  Reasoner/mem.c Reasoner/printerrutils.c  Reasoner/rgenloop.c ^
  Reasoner/rmain.c  Reasoner/subsume.c  Reasoner/unify.c Reasoner/resolve.c ^
  Reasoner/simp.c Reasoner/strat.c Reasoner/rewrite.c Reasoner/history.c Reasoner/prob.c ^
  Reasoner/hash.c Reasoner/clmeta.c Reasoner/guide.c  Reasoner/propagate.c ^
  Reasoner/analyze.c Reasoner/chains.c Reasoner/prop.c ^
  Reasoner/init.c ^
  Printer/dbotterprint.c ^
  Builtparser/dbotter.tab.c  Builtparser/dbotter.yy.c  Builtparser/dbparse.c ^
  Builtparser/dbprolog.tab.c  Builtparser/dbprolog.yy.c Builtparser/dbclausify.c Builtparser/jsparse.c 

