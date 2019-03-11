gkc
===

GKC is a reasoning system for large knowledge bases.

For a compiled binary for Linux as well as instructions
for running gkc and logfiles of experimental runs,
see the release v0.1-alpha:

https://github.com/tammet/gkc/releases/tag/v0.1-alpha

Compiling
---------

To compile the Linux binary yourself, use the 

    compile.sh
  
script in the top folder creating a static binary gkc 
in the top folder. This script copies one header file,
calls gcc once and does nothing else. The result 
is the binary `gkc` in the top folder.

To compile the Windows binary yourself, use the

    compile.bat

script in the top folder creating an executable `gkc.exe`
in the fop folder. We have tested compilation under 
the 64-bit Windows  10 with the 2017 Visual Studio C
community edition command-line tool cl.

To compile with autotools under Linux, do

    ./Bootstrap
    ./configure
    make

creating an executable `Main/wgc`
and optionally do

    sudo make install

to make gkc available on the system

Notice that GKC is licenced under AGPL v3.

Running
-------

Run gkc as 

    gkc lrunreasoner problem_file_name strategy_file_name

examples:

    gkc lrunreasoner Examples/steam.txt Examples/unit.txt
    gkc lrunreasoner /opt/TPTP/Problems/NUM/NUM925+5.p Examples/runs.txt

where
* *lrunreasoner* is a command to gkc
* *problem_file_name* is a problem file in TPTP syntax, possibly containing
include commands like `include('Axioms/CSR002+5.ax')`. 
The included files are searched either from the local folder or a fixed
path `/opt/TPTP/` . For this example the file `/opt/TPTP/Axioms/CSR002+5.ax`
would be looked up.
* *strategy_file_name* is a json-syntax file for giving options and strategies
to gkc. See the examples in the following Experimental results chapter.

Experimental results
--------------------

You can find experiment logs in the following release:
https://github.com/tammet/gkc/releases/tag/v0.1-alpha

* The file `gkc_v01_on_cascj9fof.tar.gz` contains the full logs for the CASC-J9 FOF 
division problems, see the readme.txt in the unpacked file. 
* The `gkc_v01_on_cascj9fof.tar.gz` contains separate one-run-only logs
for the seven largest problems from the previous set.
* The runs.txt strategy file was used for proof search as logged 
in `gkc_v01_on_cascj9fof.tar.gz`
* The query.txt strategy file was used for proof search logged 
in `gkc_v01_on_cascj9fof.tar.gz`
