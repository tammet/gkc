gkc
===

GKC is a reasoning system for large knowledge bases.

For compiled binaries for Linux and Windows, 
as well as instructions for running gkc and logfiles of
experimental runs, see the releases v0.1-beta and v0.1-alpha:

* https://github.com/tammet/gkc/releases/tag/v0.1-beta
* https://github.com/tammet/gkc/releases/tag/v0.1-alpha

Compiling
---------

There are three alternative ways to compile gkc,
from simpler to more complicated.

To compile the Linux or OS X binary yourself, use the 

    compile.sh
  
script in the top folder creating a static binary gkc 
in the top folder. This script copies one header file,
calls gcc once and does nothing else. The result 
is the binary `gkc` in the top folder.

We have tested compilation with gcc 5.4 and clang 3.8
under Ubuntu 16_04 and with clang under OS X Mojave: 
if the OS X compilation fails, remove the -static flag
from compile.sh and then try again. 

To compile the Windows binary yourself, use the

    compile.bat

script in the top folder creating an executable `gkc.exe`
in the fop folder. We have tested compilation under 
the 64-bit Windows  10 with the 2017 Visual Studio C
community edition command-line tool cl.

To compile with a simple makefile under Linux, do

    make -f smakefile

creating an executable `gkc` in the top folder.
See the `smakefile` for optionally compiling without
`flex` and `bison`.

To compile with autotools under Linux, do

    ./Bootstrap
    ./configure
    make

creating an executable `Main/gkc`
and optionally do

    sudo make install

to make gkc available on the system

Notice that GKC is licenced under AGPL v3.

Running
-------

Simplest way to run:

    ./gkc problem_file_name

Example:

    ./gkc Examples/steam.p

In order to select options and strategies, run gkc as 

    ./gkc -prove problem_file_name strategy_file_name

example:

    ./gkc -prove /opt/TPTP/Problems/NUM/NUM925+5.p runs.txt

where
* `-prove` is a command to gkc
* `problem_file_name` is a problem file in TPTP syntax, possibly containing include commands like 
`include('Axioms/CSR002+5.ax')`. The included files are searched either from the local folder or a fixed
path `/opt/TPTP/` . For this example the file `/opt/TPTP/Axioms/CSR002+5.ax` would be looked up. 
The TPTP problems and axioms are not included in the gkc distribution: 
for the TPTP syntax and problems, see http://www.tptp.org/
* `strategy_file_name` is a json-syntax file for giving options and strategies to gkc. See and use the
example files `unit.txt`, `neg.txt`, `runs.txt` in the `Examples` folder.


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
