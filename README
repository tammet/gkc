gkc 
===

GKC is a high-performance first order logic reasoning system optimized
for large knowledge bases. It can prove formulas, answer questions and
convert between different logical syntaxes. See the 
[paper](https://link.springer.com/chapter/10.1007%2F978-3-030-29436-6_32)
for an overview of gkc.

This README serves as a compact manual: a longer tutorial with examples is
available at [Examples/README.md](Examples/README.md). A Wasm version running 
in the browser with multiple commented examples is available as
[logictools.org](http://logictools.org).

For casc 2021 please use the compiled Linux binary and sources for version 0.7: 
<https://github.com/tammet/gkc/releases/tag/v0.7.0>

For non-casc purposes it is recommended to use the version 0.6 with compiled binaries 
for Linux, Windows and macOS:
<https://github.com/tammet/gkc/releases/tag/v0.6.0>

GKC is licenced under [AGPL v3](https://www.gnu.org/licenses/agpl-3.0.en.html). 
The author of GKC is Tanel Tammet (tanel.tammet@gmail.com).

GKC is used as a building block for developing specialized methods 
and strategies for commonsense reasoning, including nonmonotonic reasoning,
probabilistic reasoning and machine learning methods. We consider natural language
question answering systems as the main potential application these specialized methods.
See the paper "Confidences for Commonsense Reasoning" in the CADE 28 (2021) proceedings. 
GKC itself does not contain these mechanisms.

Contents:

* [Compiling and installing](#Compiling-and-installing)
* [Running and options](#Running-and-options)
* [Input syntax](#Input-syntax)
* [Strategy file](#Strategy-file)
* [Architecture](#Architecture)



Compiling and installing
------------------------

The GKC system consists of a single standalone executable
with no dependencies, called  `gkc` for linux, 
`gkc.exe` for 64-bit windows, `gkc32.exe` for 32-bit windows,
`gkcosx` for macOS. The pre-compiled executables are 
included in the [release 0.6.0](https://github.com/tammet/gkc/releases/tag/v0.6.0).
The [release 0.7.0](https://github.com/tammet/gkc/releases/tag/v0.7.0) is intended
for the [CASC 28 competition](http://www.tptp.org/CASC/28/).

GKC is written in C. There are three alternative ways
to compile gkc, from simpler to more complicated:

First, to compile the Linux binary yourself, use the 

    compile.sh
  
script in the top folder creating a static binary gkc 
in the top folder. This script calls gcc once,
the result is the binary `gkc` in the top folder.

We have tested compilation with gcc 5.4, 7.5 and 9.3, 
clang 3.8 and 6.0 under Ubuntu 16, 18 and 20. 

To compile the Windows binary yourself, use the

    compile.bat

script in the top folder creating an executable `gkc.exe`
in the fop folder. We have tested compilation of both 
the 32-bit and 64-bit versions under 
the 64-bit Windows 10 with the 2017 Visual Studio C
community edition command-line tool cl.

To compile the OS X (macOS) binary yourself, use the

    compile_osx.sh

script, which creates a dynamically linked version using
clang. We have tested Apple clang version 11.0.3 
on macOS Catalina 10.15.4.

To compile the Wasm binaries yourself, use the

    compile_wasm.sh

script, which uses [emscripten](https://emscripten.org/) 
(tested with emcc version 1.39.18) and creates files 
`gkcjs.wasm` and `gkcjs.js` as used at [logictools.org](http://logictools.org).

Second, to compile with a simple makefile under Linux, do

    make

which uses `makefile`, creating an executable `gkc` 
in the top folder. This option uses `flex` and `bison`
utilities. In case these two are not available, you
can change the `makefile` for optionally compiling without
them: check the comments in the `makefile`.

The third option is to compile with the autotools under Linux:
install `bison`, `flex`, `automake` and `libtool`, rename `makefile`
to something different and then do

    ./Bootstrap
    ./configure
    make

which will first create `Makefile`
and then use this to create a dynamically linked executable `Main/gkc`
and finally do

    sudo make install

to make gkc available on the system. Unless it is alreay there, you may
need to add `/usr/local/lib` to the environment variable `LD_LIBRARY_PATH`
to run gkc installed by autotools. 


Running and options
-------------------

Simplest way to run

    ./gkc problem_file_name

Example:

    ./gkc Examples/steam.txt
    
GKC will automatically pick and try out several search strategies 
one after another, using four parallel processes by default on UNIXes.

Please see the [Examples/README.md](Examples/README.md) for a tutorial of using gkc.

In case gkc gives an immediate error about memory or a failing fork, decrease the amount of
initially allocated memory, like

    ./gkc Examples/steam.txt -mbsize 500

In order to select options and strategies yourself, run gkc as 

    ./gkc problem_file_name -strategy strategy_file_name

Example:

    ./gkc Examples/blocks3.txt -strategy Examples/querystrat.txt

where
* `problem_file_name` is a problem file in the simple, TPTP or JSON syntax, 
  possibly containing include commands like `include('Axioms/CSR002+5.ax')`. 
  The included files are searched either under the problem folder,
  from the folder indicated by the environment variable TPTP or a fixed
  path `/opt/TPTP/`.  The TPTP problems and axioms are not included in the gkc distribution: 
  for the TPTP syntax and problems, see <http://www.tptp.org/>
* `strategy_file_name` is a json-syntax file for giving options and strategies to gkc
  as described below.

On Linux and Windows GKC can parse and prepare a large axiom file into a shared memory database
to perform quick independent queries upon this pre-built base. Several shared memory databases
may be present at the same time: use different memory database numbers for indicating which 
to load or use. The memory database can be dumped to a disk for quick loading later. 

The following is a list of available options as output by `./gkc -help`:
   
    basic proof search with an automatic strategy:
      gkc <logic file_1> ...<logic file_N>
        read all logic files and then search for proof, where 
        *.js* file suffix indicates json format
        and you can optionally input logic directly from command line:
        ... -text 'logic text' ... -jstext 'logic text in json' 

    options for proof search with a user-determined strategy:
      -strategy <strategy file>
        use the json <strategy file> to determine proof search strategy and options
      -strategytext 'strategy text in json' 
        alternatively input strategy text directly from command line

    options and parameters for using the shared memory database of axioms:
      -usekb
        use the axioms in the shared memory database in addition to other input
      -mbnr <shared memory database nr>
        if omitted, number 1000 is used
      gkc -readkb <logic file>
        parse and load a logic file into the shared memory database
      gkc -writekb <dump file>
        write the present shared memory database to a file for faster loading later
      gkc -loadkb <dump file>
        load a shared memory database from a file
      gkc -readwritekb <filename> <dump file>
        parse and load a file into shared memory database and write this to a file
      gkc -deletekb
        delete the present shared memory database (not necessary for reading a new one)

    options with numeric parameters:
      -seconds <n>
        use <n> as an upper limit of running time in seconds; default unrestricted
      -parallel <nr of parallel processes to run>
        UNIX only; if omitted, 4 worker processes and 1 parent used
      -mbsize <megabytes to allocate initially>
        if omitted, 5000 megabytes assumed for UNIX, 1000 for 32-bit Windows
      -print <nr>
        indicate the amount of output: 10 is default, bigger numbers give more,
        useful values are 1,10,11,12,13,14,20,30,40,50,51

    options without parameters:
      -tptp
        if present, use tptp output format, otherwise a simple output format
      -json
        if present, use json output format, otherwise a simple output format
      -convert
        if present, convert input format only, otherwise search for proof;
        use along with -tptp or -json options 
      -clausify
        if present, convert to clauses only, otherwise search for proof
      -derived
        print out all the derived clauses
      -version
        show gkc version
      -help
        show this help text;
        see https://github.com/tammet/gkc for details and examples
      

The output level can be set by the `-print N` parameter with bigger N giving cumulatively
more details. The default level is 10. Sensible levels are:

* 1: only show if proof has been found or not
* 10: show the proof
* 11: show strategy used in the successful run
* 12: show all runs with their strategies
* 15: show statistics and the set of all runs planned along with their strategies
* 20: show *given* clauses (a tiny subset of input or derived clauses)
* 40: show all derived clauses
* 50: show details or rule selection and application
* 60: show contents of main internal datastructures

Use the  `-parallel 0` parameter on UNIXes for the print level 20 and above: otherwise 
the printouts from several parallel process will be mixed up.

The parameter `-derived` shows all the derived clauses: use it along the `-parallel 0`
parameter and preferably indicating a single run in the strategy file.

The `-convert` and `-clausify` parameters make gkc to not prove the input, but convert it 
to a format indicated by the `-tptp` or `-json` key, if present. The  `-clausify` parameter
performs clausification, while `-convert` does not.


The *N* in `-mbsize N` can be safely decreased for all but very large problems: `-mbsize 1000`
is a safe bet.

Input syntax
------------

GKC reads files in one of three formats with equivalent expressive power: 

* *Simple syntax*, see [Examples/README.md](Examples/README.md) and the description below.
* TPTP FOF and CNF syntaxes, see [Examples/README.md](Examples/README.md) and 
  the [TPTP docs](http://www.tptp.org/TPTP/TR/TPTPTR.shtml).
* *JSON* syntax described in the [JSON-LD-LOGIC proposal](https://github.com/tammet/json-ld-logic)
  and showcased in the [json playground](http://logictools.org/json).

The *simple syntax* is an extension of the
TPTP FOF and CNF syntaxes as described in the
[TPTP technical manual](http://tptp.org/TPTP/TR/TPTPTR.shtml#ProblemPresentation)
combining them into one and extending with a list
type and a few convenience predicates and constructions.

The *simple syntax* input is a list of logical formulas, each terminated with
a period. The whole list is interpreted as a big conjunction (*and*) of formulas.

Each *formula* is either a *literal* (a positive or a negative *atom*) or
is constructed from simpler formulas using logical operators like 

* `&` for conjunction (and), 
* `|` for disjunction (or),
* `-` or `~` for negation (not),
* `=>` for implication (if-then), 
* "for all values of variables holds ..." construction `! [variables] : formula` (example `! [X,Y] : p(X,Y)`) 
* "exist values of variables so that ..." construction `? [variables] : formula`.

Additional logical operators will be described later.  

An *atom* is a predicate symbol applied to a number of *term* arguments like
`father(john,pete)` and denoting a relation between the arguments.
A negated atom has a minus sign prepended
like `-father(john,pete)`.

*Equality* is a special predicate written like `gender(john)=male`
and meaning, as expected, that the term on the left side is equal to the term on the
right side. *Inequality* is another special predicate written like 
`gender(john)!=female`

The *terms* denote objects. A simplest term is just a symbol/constant 
like `john`: think of it as a post-it note glued to some real thing or idea: 
a real object may have many such notes glued to it. A *variable* like `X`
is also a term: think of it as placeholder which can be replaced by any other term.

Variables must start with an upper-case character, other symbols with a lower-case character.
There are also other options for symbols, more about these below.

A variable which is not in the scope of a *for all*  `!` or 
*exists* quantifier  `?` is treated as if it is bound by the 
*for all* quantifier. In fact, explicit quantifiers can be skipped
altogether: they are just a convenience construction.

Numbers like 12 or 3.45 can be also used as constant terms.  
  
More complex terms can be constructed from simpler terms using *functions* like
`age(mother(john))` or `$sum(X,$multiply(2,X))`. A value of
such a function is not always defined: it can be used just as a constructor for
complex terms. A function may also be defined on some other terms, like `$sum`
is defined on numbers, but not on lists. You may define functions yourself using equality
like `mother(john)=eve` or `age(mother(john))=age(mother(pete))`.

Both line comments `%` and multi-line comments `/* ... */`
are available.

Ordinary symbols must start with lower-case character and 
must not contain any whitespace or non-alphanumeric characters
like `(, -, ~, =, "` etc: however, underscore `_`
and dollar `$` are allowed. 

You can put whitespace or any symbol except a single quote
into symbols by surrounding the symbol with single quote symbols like this:
`'John Smith'`. Internal quotes must be prefixed by a backslash
`\`.

Additionally you can make a symbol variable by prefixing it with a question
mark like this:  `?smth`.

Double quotes surrounding a symbol like `"John Smith"`
have a special meaning: the symbol is treated as *distinct* (essentially,
a *string*) meaning that it is known to be unequal to any syntactically 
different distinct symbols, numbers and lists. This has real effect for 
equality and string predicates, but not otherwise.

In addition to the typically used constructions `&`,`|`,
`-`/`~`, `=>`,
`!`, `?` the syntax provides a number of additional infix logical
constructions with two arguments. The full list is:

* `|` for disjunction,
* `&` for conjunction, 
* `<=>` for equivalence, 
* `=>` for implication, 
* `<=` for reverse implication, 
* `<~>` for non-equivalence (XOR), 
* `~|` for negated disjunction (NOR), 
* `~&` for negated conjunction (NAND), 
* `@` for application, used mainly in the higher-order context in TPTP.

There are several predicates and function symbols with a pre-defined meaning: all these
are prefixed with a dollar `$`.

`$ans` is used for finding the actual terms we look for as answers: see the
examples and explanations in the tutorial [Examples/README.md](Examples/README.md).

The simple and fof/cnf format problems may contain include statements like

    include('Axioms/GEO004+2.ax').

in which case the included path is searched for from either directly
under the folder of the input problem path, and if not found there,
then under the folder indicated by the environment variable TPTP,
and if not found there, then under the folder /opt/TPTP

GKC may introduce new predicates, function and constant
symbols while clausifying fof formulas: these will start
with a $ character: skolem functions/constants start with $sk and 
introduced definition predicates with $df.


### Arithmetic
  

The numbers and arithmetic functions and predicates are defined following the 
[TPTP arithmetic system](http://www.tptp.org/TPTP/TR/TPTPTR.shtml#Arithmetic)
plus a few convenience operators for writing infix terms:

* Type detection predicates $is_int, $is_real.
* Comparison predicates $less, $lesseq, $greater, $greatereq.
* Type conversion functions $to_int, $to_real.
* Arithmetic functions on integers and reals:
  $sum, $difference, $product, 
  $quotient, $quotient_e,
  $remainder_e, $remainder_t, $remainder_f, 
  $floor, $ceiling,
  $uminus, $truncate, $round.
 
 Note: these comparison predicates and arithmetic functions take exactly two arguments.

 Example: `$less($sum(1,$to_int(2.1)),$product(3,3))`.

* Additional convenience predicate is used: $is_number is true
if and only if $is_int or $is_real is true.

* Additional infix convenience functions +, -, *, / are
used with the same meaning as $sum, $difference, $product and 
$quotient, respectively.

Example: `$less(1+(1+2),(3*3))`

Note: these convenience functions take also exactly two arguments.

NB! Do not use a variable or a non-numeric constant as a first element of the 
infix arithmetic expression like `p(X*2)`, otherwise 
the whole expression will be parsed as a single variable `X*2`. No such restrictions
apply for the prefix form.

 
### Lists
 

You can use the list syntax like `[]` for the empty list and `[a,b,c]`
for a three-element list. The bracket notation is syntactic sugar for
the constant `$nil` and functional list-constructing term 
`$list(a,$list(b,$list(c,$nil)))`, respectively.

Observe that `$list(X0,X1)` constructs a list by prepending `X0` to the list `X1`,
which is generally different from a two-element list `[X0,X1]`.

Terms constructed using `$list` or `$nil` are interpreted as having a 
*list type*:

* A *list type object* is inequal to any number or a distinct symbol.
* Syntactically different *list type objects* A and B are unequal if at any position the corresponding
elements of A and B are unequal typed values: numbers, lists or distinct symbols.  

The following predicate and two functions are defined on lists:

* `$is_list(L)`  evaluates to *true* if A is a list and *false* is A 
  is a number or a distinct symbol.
* `$first(L)`  returns the first element of the list.
* `$rest(L)`  returns the rest of the list, i.e. the result of removing the first element.

These functions can be applied to non-list arguments, where they are left as is and not 
evaluated.

 
### Distinct symbols as strings
 

Symbols in *double quotes* like in `"person"` stand for
for *distinct symbols*
which can be viewed as a *string type*. A distinct symbol is not equal to any 
other syntactically different distinct symbol and not equal to any numbers or lists.

Gkc defines a function and three predicates on distinct symbols:
  
* `$strlen(S)` returns the integer length of a distinct symbol S as a string.
* `$substr(A,B)` evaluates to *true* if a distinct symbol A is a substring of a distinct symbol B, 
     and *false* otherwise, provided that A and B are distinct symbols.  
* `$substrat(A,B,C)` evaluates to *true* if a distinct symbol A is a substring of a 
     distinct symbol B exactly at the integer position C (starting from 0), and *false* otherwise,
     provided that A and B are distinct symbols and C is an integer.  
* `$is_distinct(A)` evaluates to *true* if A is a distinct symbol and 
  *false* if A is a number or a list.  


Strategy file
-------------

The strategy file, if given like

    ./gkc blocks3.txt -strategy querystrat.txt 

determines both the strategy selection and optionally time limits and
the output level and syntax. 

In case the strategy file is not given, GKC will automatically
build a temporary strategy string in memory, running several
different search strategies sequentially, with increasing
time limits after each batch.

The strategy file is a json file like the following example,
indicating that one single strategy should be run. For more
examples, see strat.txt, multineg.txt, runs.txt in the Examples 
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

The list "strategy": [...] contains the main search strategy indicators, default off:

* "query_focus" : use a goal-oriented set-of-support strategy with binary resolution 
* "negative_pref" : use binary resolution with negative literals preferred
* "positive_pref" : use binary resolution with positive literals preferred
* "hardness_pref" : use binary resolution with "hardest" (similar to weight) literals preferred
* "knuthbendix_pref" : use binary resolution with knuth-bendix ordering of literals 
* "hyper" : use hyperresolution, with negative literals preferred
* "posunitpara": perform paramodulation from units only
* "prohibit_nested_para": disallow paramodulation if either parent is derived by paramodulation
* "max_ground_weight": use the weight of the heaviest literal as the base weight of a clause
* "unit", "double" or "triple" : use binary unit resolution  or its generalization:
  (one of the arguments must be unit, a two-literal or three-literal clause,
   respectively. These may be added to the list in addition to the previous
   strategy indicators, for example, like ["query_focus","unit"].
      
Other useful parameters:

* "print": 0 or 1, where 0 prohibits almost all printing, default 1.
* "print_level": integer determining the level of output: useful values are between 0 and 60, default 10.
* "print_json": 0 or 1, where 0 is default and 1 forces json output.
* "print_tptp": 1 or 0, where 1 is default and 0 forces non-tptp-style proof output
* "max_size", "max_length", "max_depth", "max_weight" indicate limits on kept clauses, defaults are 0.
* "equality" : 1 or 0, with 1 being default and 0 prohibiting equality handling.
* "rewrite" : 1 or 0, with 1 being default and 0 prohibiting using equations for rewriting.
* "max_dseconds": N being an integer limit of deciseconds (0.1 seconds) for one run, default 0 (no limit).
* "max_seconds": N being an integer limit of seconds for one run, default 0 (no limit), 
  as an alternative to "max_dseconds".
* "weight_select_ratio": N indicating the ratio of picking by order derived / clause weight, default is 5.
* "max_answers": N indicating the maximal number of proofs searched for until search stops, default is 1.
* "reverse_clauselist": N either default 0 or 1, where 1 sets a non-standard order for input clauses.
* "sine": input filter with N either 1 or 2 where 1 is a less restrictive (accepts more clauses) and 2 more restrictive. NB! Sine is automatically switched off if the -provekb switch is used.
* "depth_penalty": additional penalty for clause depth, default 1
* "length_penalty": additional penalty for clause length, default 1
* "var_weight": weight of a variable, default 5
* "var_weight": weight of a repeated variable, default 7
* "query_preference": N being 0, 1, 2 or 3 indicates which parts of the problem are treated as
   goals, assumptions or axioms:
    * 0 stands for no goal/assumption preference.
    * 1 stands for input preference (the assumption and conjecture formulas of fof)
    * 2 stands for making non-included formulas assumptions
    * 3 stands for considering only the negative clauses from conjecture to be goals

For "max_seconds"<2 gkc will automatically use immediate check for contradiction when a clause is derived.
For problems with less than 1000 input clauses, gkc will use sine for input clause ordering.

Architecture
------------

These standard inference rules have been implemented:

* Binary resolution with optionally the set of support strategy, ordered resolution or unit restriction.
* Hyperresolution.
* Factorization.
* Paramodulation and demodulation with the Knuth-Bendix ordering. 

GKC does not currently implement any propositional inferences or instance generation.

By default GKC uses multiple strategies run sequentially and/or distributed between
several parallel processes: by default, four parallel processes on UNIX and a single
process on Windows. By default the strategies have a time limit starting 
at 0.1 seconds or more for each, then 1, 5, 25, 125 etc seconds once the whole batch
has been performed. The strategy selection takes into consideration the basic properties
of the problem, in particular its approximate size. There is no interplay between 
different strategies.

We perform the selection of a given clause by using several queues in order to spread 
the selection relatively uniformly over these categories of derived clauses and their 
descendants: axioms, external axioms, assumptions and goals. 

GKC is implemented in C. The data representation machinery is built upon a shared 
memory graph database [Whitedb](http://whitedb.org), enabling it to solve multiple
different queries in parallel processeses without a need to repeatedly parse or load the large
parsed knowledge base from the disk.

An interesting aspect of GKC is the pervasive use of hash indexes, 
feature vectors and fingerprints, while no tree indexes are used. 


  












