gkc
===

GKC is a reasoning system for large knowledge bases, available at
<https://github.com/tammet/gkc>

For compiled binaries for Linux and Windows, as well as 
instructions for running gkc, see the latest release,
either v0.4 or newer:

<https://github.com/tammet/gkc/releases/tag/v0.4>

GKC is licenced under AGPL v3. The author of GKC
is Tanel Tammet (tanel.tammet@gmail.com).

Compiling and installing
------------------------

The GKC system consists of a single standalone executable, 
called  `gkc` for linux/mac, `gkc.exe` for windows. 

GKC is written in C. There are three alternative ways
to compile gkc, from simpler to more complicated:

First, to compile the Linux or OS X binary yourself, use the 

    compile.sh
  
script in the top folder creating a static binary gkc 
in the top folder. This script calls gcc once,
the result is the binary `gkc` in the top folder.

We have tested compilation with gcc 5.4 and clang 3.8
under Ubuntu 16_04 and with clang under OS X Mojave: 
if the OS X compilation fails, remove the -static flag
from compile.sh and then try again. 

To compile the Windows 32-bit binary yourself, use the

    compile.bat

script in the top folder creating an executable `gkc.exe`
in the fop folder. We have tested compilation under 
the 64-bit Windows 10 with the 2017 Visual Studio C
community edition command-line tool cl.

Second, to compile with a simple makefile under Linux, do

    make

which uses `makefile`, creating an executable `gkc` 
in the top folder. This option uses `flex` and `bison`
utilities. In case these two are not available, you
can change the `makefile` for optionally compiling without
them: check the comments in the `makefile`.

Third, to compile with the autototools under Linux, first 
rename `makefile`to something different and then do

    ./Bootstrap
    ./configure
    make

which will first create `Makefile`
and then use this to create an executable `Main/gkc`
and optionally do

    sudo make install

to make gkc available on the system


Running
-------

Simplest way to run

    ./gkc problem_file_name

Example:

    ./gkc Examples/steam.p
    
GKC will automatically pick and try out several search strategies 
one after another.

In order to select options and strategies yourself, run gkc as 

    ./gkc problem_file_name strategy_file_name

Example:

    ./gkc /opt/TPTP/Problems/NUM/NUM925+5.p Examples/runs.txt

where
* `problem_file_name` is a problem file in TPTP syntax, possibly containing include commands like 
`include('Axioms/CSR002+5.ax')`. The included files are searched either under the problem folder,
from the folder indicated by the environment variable TPTP or a fixed
path `/opt/TPTP/`.  The TPTP problems and axioms are not included in the gkc distribution: 
for the TPTP syntax and problems, see <http://www.tptp.org/>
* `strategy_file_name` is a json-syntax file for giving options and strategies to gkc. See and use the
example files `unit.txt`, `neg.txt`, `runs.txt` in the `Examples` folder.

GKC can parse and prepare a large axiom file into a shared memory database to perform quick
independent queries upon this pre-built base. Several shared memory databases may be present
at the same time: use different memory database numbers for indicating which to load or use.
The memory database can be dumped to a disk for quick loading later. 

The following is a list of available commands as output by `./gkc -help`:
    
    usage:
    basic proof search with a default strategy:
      gkc <problem file>
    proof search with a strategy selection file:
      gkc -prove <problem file> <strategy file>
    parse and load a file into shared memory database:
      gkc -readkb <axioms file>
    proof search using the shared memory database as prepared additional axioms:
      gkc -provekb <problem file> <strategy file>
    write the present shared memory database to a file for fast loading:
      gkc -writekb <dump file>
    load a shared memory database from a file:
      gkc -loadkb <dump file>
    parse and load a file into shared memory database and write this to a file:
      gkc -readwritekb <axioms file> <dump file>
    delete the present shared memory database (not necessary for reading a new one):
      gkc -deletekb
    show gkc version:
      gkc -version

    where:
      <problem file> should be in TPTP FOF or CNF syntax or Otter CNF syntax
      <strategy file> is an optional json file: see neg.txt, runs.txt in Examples
      <axioms file> is like a <problem file> 
      <dump file> stores the parsed and prepared database for fast loading 

    additional optional parameters:
      -mbsize <megabytes to allocate>
        if omitted, 1000 megabytes assumed
      -mbnr <shared memory database nr>
        if omitted, 1000 used

Input syntax
------------

GKC reads files in both the 

* TPTP first order form (fof) syntax, see Examples/steam.p and <http://www.tptp.org/TPTP/TR/TPTPTR.shtml>
* Simple clause form syntax, see Examples/steam.txt and the description below.

In both cases the percentage % symbol is a line comment.

The fof format problems may contain include statements like

    include('Axioms/GEO004+2.ax').

in which case the included path is searched for from either directly
under the folder of the input problem path, and if not found there,
then under the folder indicated by the environment variable TPTP,
and if not found there, then under the folder /opt/TPTP

GKC may introduce new predicates, function and constant
symbols while clausifying fof formulas: these will start
with a $ character: skolem functions/constants start with $sk and 
introduced definition predicates with $df.

The simple clause form syntax assumes clauses like

    -p(X,f(X)) | r(X,a).

where - is a negation symbol, | is a logical or, variables
start with an upper case character or a question mark ?, 
functions/constants/predicates start with a lower case character 
and the clause ends with a period.

The special predicate = is assumed to stand for equality and the 
predicate $ans for an answer-predicate.

Strategy file
-------------

The strategy file, if input, determines both the strategy
selection and output level and type.

In case the strategy file is not given, GKC will automatically
build a temporary strategy string in memory, running several
different search strategies sequentially, with increasing
time limits after each batch.

The strategy file is a json file like the following example,
indicating that one single strategy should be run. For more
examples, see query.txt, neg.txt, unit.txt, runs.txt in the Examples 
folder.

        {
        "print_level": 15, 
        "max_size":0,
        "max_depth": 0,
        "max_length":0,
        "max_weight": 0,
        "max_size": 0,
        "max_seconds": 10,
        "weight_select_ratio": 5,
        "reverse_clauselist": 0,
        "strategy":["query_focus"],
        "query_preference": 1
        }  

The following example indicates that several strategies should
be run one after another:

        {
        "print":1,
        "print_level": 15,
        "max_size": 0,
        "max_depth": 0,
        "max_length": 0,
        "max_seconds": 0,
        "equality":0,
        "runs":[
            {"max_seconds": 1, "strategy":["negative_pref"], "query_preference": 1},
            {"max_seconds": 1, "strategy":["negative_pref"], "query_preference": 0},   
            {"max_seconds": 1, "strategy":["query_focus"], "query_preference": 2},   

            {"max_seconds": 5, "strategy":["negative_pref"], "query_preference": 1},
            {"max_seconds": 5, "strategy":["negative_pref"], "query_preference": 0},
            {"max_seconds": 5, "strategy":["hardness_pref"], "query_preference": 0},
            {"max_seconds": 5, "strategy":["unit"], "query_preference": 0}
        ]
        }

The default value for limit-type fields starting with max_ is 0, indicating
that no limit is set.

In case "equality": N is not set to 0, GKC uses reflexivity, paramodulation and
demodulation with knuth-bendix ordering for handling equality.

The list "strategy": [...] contains the main search strategy indicators, either

* "query_focus" : use a goal-oriented set-of-support strategy with binary resolution 
* "negative_pref" : use binary resolution with negative literals preferred
* "positive_pref" : use binary resolution with positive literals preferred
* "hardness_pref" : use binary resolution with "hardest" literals preferred
* "hardness_pref" : use binary resolution with "hardest" literals preferred
* "knuthbendix_pref" : use binary resolution with knuth-bendix ordering of literals 
* "hyper" : use hyperresolution, with negative literals preferred
* "unit", "double" or "triple" : use binary unit resolution  or its generalization:
  (one of the arguments must be unit, a two-literal or three-literal clause,
   respectively. These may be added to the list in addition to the previous
   strategy indicators, for example, like ["query_focus","unit"].
      
Other useful parameters:

* "print": 0 or 1, where 0 prohibits almost all printing, default 1.
* "print_level": integer determining the level of output: useful values are between 0 and 50, default 10.
* "print_json": 0 or 1, where 0 is default and 1 forces json output.
* "max_size", "max_length", "max_depth", "max_weight" indicate limits on kept clauses, defaults are 0.
* "equality" : 1 or 0, with 1 being default and 0 prohibiting equality handling.
* "max_seconds": N being an integer limit of seconds for one run, default 0 (no limit).
* "weight_select_ratio": N indicating the ratio of picking by order derived / clause weight, default is 5.
* "max_answers": N indicating the maximal number of proofs searched for until search stops, default is 1.
* "reverse_clauselist": N either default 0 or 1, where 1 sets a non-standard order for input clauses.
* "query_preference": N being 0, 1, 2 or 3 indicates which parts of the problem are treated as
   goals, assumptions or axioms:
    * 0 stands for no goal/assumption preference.
    * 1 stands for input preference (the assumption and conjecture formulas of fof)
    * 2 stands for making non-included formulas assumptions
    * 3 stands for considering only the negative clauses from conjecture to be goals




  












