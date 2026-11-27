#!/bin/sh
# The self test of the perfect discrimination tree (strategy key ti_selftest)
# on any input files, for example TPTP problems:
#
#   Test/termindex/selftest.sh [-b path/gkc] [-c] file...
#
# -c runs the test on the compressed form of the tree (tree_compress 1).
#
# For each file it prints the result line of the self test. The exit code
# is 0 only when every file gives "wrong 0" and "differences 0".
bin=./gkc
if [ "$1" = "-b" ]; then bin=$2; shift 2; fi
comp=0
if [ "$1" = "-c" ]; then comp=1; shift; fi
rc=0
for f in "$@"; do
  line=$($bin -parallel 0 -strategytext "{\"strategy\":[\"unit\"],\"ti_selftest\":1,\"tree_compress\":$comp}" "$f" 2>&1 | grep "^TI_TEST \(tree\|result\)" | tr '\n' ' ')
  echo "$f: $line"
  case "$line" in
    *"wrong 0 "*"differences 0"*) ;;
    *) rc=1 ;;
  esac
done
exit $rc
