#!/bin/sh

# Compile to wasm using emscripten:
# produces files 
#   gkcjs.wasm (main result)
#   gkcjs.js (automatic glue code for js-C integration built by emscripten)
#   gkcjs.html (emscripten generated html page to see if result works: better do not use directly)

# Try running the output under node like this:
# node gkcjs.js Examples/steam.txt -parallel 0
# and use gkcjs_try.html as a practical html page to run the prover:
# has to be served by a web server, for example, run from the same folder
#   python3 -m http.server
# then open with Firefox or Chrome
#   http://localhost:8000/gkc.html

# Note: 
# * uses pre-built parser in the Builtparser folder, not the bison/flex combo
# * configuration is set in config.h

# First set the correct path and check the emscripten compiler to be used instead of

CC="/opt/gkcjs/emsdk/upstream/emscripten/emcc"

${CC} -Wall -Wmissing-field-initializers -Wno-unused-variable -O2 \
  Main/gkc.c \
  Db/dbmem.c \
  Db/dballoc.c Db/dbdata.c Db/dblock.c Db/dbindex.c Db/dbdump.c  \
  Db/dblog.c Db/dbhash.c Db/dbcompare.c Db/dbquery.c Db/dbutil.c Db/dbmpool.c \
  Db/dbjson.c Db/dbschema.c \
  json/yajl_all.c \
  cjson/cjson.c \
  Reasoner/build.c  Reasoner/clstore.c  Reasoner/clterm.c  Reasoner/derive.c  \
  Reasoner/glb.c  Reasoner/mem.c  \
  Reasoner/printerrutils.c  Reasoner/rgenloop.c  Reasoner/rmain.c  Reasoner/subsume.c  Reasoner/unify.c \
  Reasoner/resolve.c Reasoner/simp.c Reasoner/strat.c Reasoner/rewrite.c Reasoner/history.c \
  Reasoner/prob.c Reasoner/hash.c \
  Reasoner/clmeta.c Reasoner/guide.c  Reasoner/propagate.c Reasoner/init.c \
  Reasoner/analyze.c Reasoner/chains.c Reasoner/prop.c\
  Printer/dbotterprint.c \
  Builtparser/dbotter.tab.c  Builtparser/dbotter.yy.c  Builtparser/dbparse.c \
  Builtparser/dbclausify.c \
  Parser/jsparse.c \
  -o gkcjs.html \
  -s TOTAL_MEMORY=1000MB  -s EXPORTED_FUNCTIONS=["_main"]  \
  -s EXTRA_EXPORTED_RUNTIME_METHODS=["cwrap","ccall","callMain"]  -lm 

# Notes:
#
# * TOTAL_MEMORY parameter:
#   - cannot be over 2000MB
#   - OK for Chrome with 2000MB
#   - Not ok for Firefox with 2000MB, but OK with 1000MB
#
# * -s EXPORTED_FUNCTIONS and -s EXTRA_EXPORTED_RUNTIME_METHODS are for
#   calling C programs from js 
#
# * exported callMain should actually be enough (seems to be not well documented, though)
#
# You could try adding things like
#  --closure 1



