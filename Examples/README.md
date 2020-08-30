Examples: a tutorial 
====================

We will first give a brief introduction to gkc input and output, then 

* look at several simple [introductory examples](#Introductory-examples),
* continue towards [more advanced use](#Advanced-examples) and 
* finally [JSON syntax examples](#JSON-examples).

On Linux the pre-compiled executable is called gkc, on Windows gkc.exe
and on OS X (macOS) it is gkcosx. Please change the filename on OS X
to gkc or just use gkcosx in the following examples instead of gkc.

The Wasm version for use in the browser consists of the files gkcjs.wasm
and gkcjs.js: see the [logictools.org](http://logictools.org) for the
browser version in action along with the examples selected and explained
as a tutorial.

Call gkc from the command line from UNIX (i.e. Linux or OS X) like this:

    ./gkc problemfilename
    
and from Windows like this:

    gkc problemfilename 

Try it out on a trivial problem in the Examples folder:

    ./gkc example1.txt

Unless gkc is already in the Examples folder, you may want to copy it there:

    cp gkc Examples/

You may want to send output to a file in a standard way like this:

    ./gkc example1.txt > out.txt

In case you have very little memory on your computer, gkc may
complain about memory and terminate. In this case tell it to use
less initally allocated memory like this:

    ./gkc example1.txt -mbsize 500

which would make it use only half a gigabyte of memory. 
The default for UNIX and Windows 64 is 5 gigabytes, i.e. -mbsize 5000.
Larger amounts of initially allocated memory make startup slightly slower.

You can give an arbitrary number of files as input, interpreted as if they were
connected with *and*, like

    ./gkc steam_kb.txt steam_query.txt

and optionally give input directly from the command line using the `-text` key, like

    ./gkc steam_kb.txt -text '-wolf(X).'

Gkc understands three different syntaxes:
* *simple* syntax described below
* [TPTP FOF and CNF syntax](http://tptp.org/TPTP/TR/TPTPTR.shtml#FormulaeSection)
* *JSON syntax* described in the [JSON-LD-LOGIC](https://github.com/tammet/json-ld-logic) proposal.

Json syntax input files must use the suffix starting with *.js* like `foo.js` or
`bar.json`. Json direct input from command line must use the `-jstext` key like

    ./gkc steam_kb.txt -jstext '[["-wolf","?:X"]]'

By default gkc uses four parallel processes on UNIX and a single
process on Windows. On UNIX you can tell it a number of parallel
processes to use like this (0 and 1 mean no parallel processes, any number
larger than 1 means this number of parallel processes):

    ./gkc example1.txt -parallel 0

On UNIX you can also force gkc to stop after N seconds by giving a time limit
parameter like this:

    ./gkc example1.txt -seconds 2

Calling gkc without parameters will output a summary of all the command line
options.

Introductory examples
---------------------

Topics covered:

* [Example 1 for basics](#Example-1-for-basics)
* [Example 2 for answers](#Example-2-for-answers)
* [Example 3 for rules](#Example-3-for-rules)
* [Example 4 for indefinite answers](#Example-4-for-indefinite-answers)
* [Example 5 for more rules](#Example-5-for-more-rules)
* [Example 6 for equalities and functions](#Example-6-for-equalities-and-functions)
* [Example 7 for grandfather with functions](#Example-7-for-grandfather-with-functions)
* [Example 8 for equalities used in proof](#Example-8-for-equalities-used-in-proof)
* [Example 9 for reflexivity used in proof](#Example-9-for-reflexivity-used-in-proof)
* [Example 10 for multiple answers](#Example-10-for-multiple-answers)
* [Example algebra](#Example-algebra)
* [Example with an unprovable problem](#Example-with-an-unprovable-problem)
* [Example with a hard problem](#Example-with-a-hard-problem)
* [Examples with nontrivial blocks world problems](#Examples-with-nontrivial-blocks-world-problems)

These simple examples explain the input, output and basic
functioning of gkc. Run these yourself like

    ./gkc example1.txt

The simple logical language used in these examples is an extension
of the TPTP FOF and CNF syntaxes as described in the
[TPTP technical manual](http://tptp.org/TPTP/TR/TPTPTR.shtml#ProblemPresentation),
combining them into one via allowing free variables,
making the fof(...) and cnf(...) constructions optional and 
extending with a list type and a few convenience predicates and constructions. 
Gkc can also use the pure TPTP syntax and the json syntax as described in the 
[JSON-LD-LOGIC](https://github.com/tammet/json-ld-logic) proposal: more about these later.


### Example 1 for basics

This is a really trivial example.

We will give two facts: john is a father of pete and
pete is a father of mark. We will ask whether from these
two facts we can derive that john is a father of pete: 
obviously we can.

The facts and the question are written in predicate logic, with the
question posed as a negation, from which gkc derives contradiction.

Why the negation and contradiction? Suppose want to prove a & b => a.
If we look for contradiction, then we should try to find contradiction from
-(a & b => a) which is equivalent to  a & b & -a. Essentially, giving
a goal to be proved from axioms (i.e. known facts / rules) as a negated 
statement is a convenient way to organize proof search.

Input file example1.txt:

    father(john,pete).
    father(pete,mark).

    -father(john,pete).

    % what this means:
    % let us want to prove a & b => a
    % if we look for contradiction, then we should
    % try to find contradiction from
    % -(a & b => a)
    % which is equivalent to
    % a & b & -a

Notice that each statement in the input file is terminated with a period sign. 
Minus sign - means negation. I.e. -father(john,pete) means 
`john is NOT the father of pete`.

Percentage character % is a line comment. Multi-line comments `/* .... */`
can be used as well. 

The order of statements in the file is not really important, although it may cause minor
and essentially unpredictable differences in search behaviour. Essentially, gkc
creates and tries out its own search strategies, like SQL databases. It does not
follow a pre-determined predictable search strategy like PROLOG.

Output from gkc:

    result: proof found
    for example1.txt 

    proof:
    1: [in] -father(john,pete).
    2: [in] father(john,pete).
    3: [simp, 1, 2] false

The output first indicates that the proof was found. 
The `proof` block gives us numbered steps of the proof found:
each step is either a used input fact / rule or a derived fact / rule.

The [in,axiom] means that this fact/rule was given in input.

The [mp, 1, 2, fromaxiom] (not present in this proof) means that this fact / rule 
was derived by modus ponens (i.e. the resolution rule)
from previous steps 1 and 2. More concretely, the first literals of both were cut off and
the rest were glued together. The "fromaxiom" piece of information is informational: 
it indicates the fact / rule was derived from ordinary axioms, not a specially marked goal.

The [simp, 1, 2,fromaxiom] means the same as the [mp,...] above, just a specially simple case.

The [Resolution (logic) wiki page](https://en.wikipedia.org/wiki/Resolution_(logic)) 
is a good short intro to the general principles of the derivation rules with the 
[course materials of Geoff Sutcliffe](http://lambda.ee/w/images/0/06/Geoffreasoningnotes.pdf) 
being a suitable continuation towards deeper understanding. After this you may want to read
an implementation 
[tutorial] (https://resources.mpi-inf.mpg.de/departments/rg1/conferences/vtsa09/slides/schulz.pdf)
by Stephan Schulz and a 
[detailed theoretical coursebook] (http://resources.mpi-inf.mpg.de/departments/rg1/teaching/autrea-ss12/script/script.pdf) by Marek Kosta and Christoph Weidenbach.

However, the following examples are understandable without in-depth theoretical background.

Gkc runs a large number of search attempts with different search strategies both 
sequentially and in parallel: you can see which concrete strategy found the proof
by increasing the print level like this:

    ./gkc example1.txt -print 11

    result: proof found
    for example1.txt 
    by run 1 fork 0 strategy {"max_dseconds":1,"strategy":["negative_pref"],"query_preference":0}   

    proof:
    1: [in, axiom] -father(john,pete).
    2: [in, axiom] father(john,pete).
    3: [simp, 1, 2, fromaxiom] false

Here *run N* indicates the number of attempt and *fork M* (only for UNIXes) shows which 
parallel process found the proof. The `"max_dseconds":1` says that gkc gave this strategy one 
decisecond (0.1 seconds) time limit. The `axiom` and `fromaxiom` also appear at this
print level, indicating the type of the input and the origin of derived clauses, stemming
from the optional *role* argument used by the TPTP syntax.

Try to modify the file example1.txt by removing the statement `-father(john,pete).` and
run gkc again. After ca one second gkc will stop and output

    result: proof not found.
  
Although the task is trivial, gkc does not attempt to be clever and still tries out a large number
of search strategies: this is why it takes a second to terminate. You can try running gkc
with the optional switch producing more information to see a lot more about the whole process:

    ./gkc example1.txt -print 15

In case gkc does not understand the syntax of the input file it will give a json-formatted
error indicating a culprit line and piece of input like this:

    {"error": "syntax error, unexpected URI, expecting '.': file example1.txt place 'as' in line 3:
    foo bar ("}

The proofs in the following examples may vary between different runs, depending
on which parallel process found it first. You can block gkc from using parallel 
processes on UNIXes by the `-parallel 0` argument like

    ./gkc example1.txt -parallel 0


###  Example 2 for answers

Like example 1, but we want to find a concrete person as an answer: we use the special
"$ans" predicate for this. Observe the `answer: $ans(pete).` line in the output
stemming from this answer predicate.

We are using a variable here: any word starting with a capital letter is considered
to be a variable. `X` is a variable in our example. You could also use, say `Something`
as a variable: it also starts with a capital letter. All the words starting with
a non-capital letter are constants, functions or predicates.

Vertical bar | is logical *or*. 

Importantly, any rule like a & b => c should be represented
as -a | -b | c where negated atoms are essentially on the left side of the implication
and the positive atoms on the right side. For example, -a | -b | c | d is equivalent
to (a & b) => (c | d). You can use =>, <=> and quantifiers directly with the fof syntax
of TPTP: this will be described later.

Input file example2.txt:

    father(john,pete).
    father(pete,mark).

    -father(john,X) | $ans(X).
  
Output:

    result: proof found
    for example2.txt 

    answer: $ans(pete).
    proof:
    1: [in] father(john,pete).
    2: [in] -father(john,X) | $ans(X).
    3: [mp, 1, 2] $ans(pete).
       
Notice that gkc outputs a line `answer: $ans(pete).` indicating the substitution
made for `X`: pete is the answer we were looking for.

Modify the example and try out the line

    -father(Y,X) | $ans(Y,X).

You will get `answer: $ans(john,pete).` in the output.
It is possible to force gkc to give more answers than just one: more
about that later.


### Example 3 for rules

Now we add a grandfather rule and ask for a grandchild of John.

Input file example3.txt:

    father(john,pete).
    father(pete,mark).

    % equivalent to (father(X,Y) & father(Y,Z)) => grandfather(X,Z).
    -father(X,Y) | -father(Y,Z) | grandfather(X,Z).

    -grandfather(john,X) | $ans(X).

Output:

    result: proof found
    for example3.txt 

    answer: $ans(mark).
    proof:
    1: [in] -father(X,Y) | -father(Z,X) | grandfather(Z,Y).
    2: [in] father(pete,mark).
    3: [mp, 1, 2] -father(X,pete) | grandfather(X,mark).
    4: [in] father(john,pete).
    5: [mp, 3, 4] grandfather(john,mark).
    6: [in] -grandfather(john,X) | $ans(X).
    7: [mp, 5, 6] $ans(mark).

###  Example 4 for indefinite answers

Let us make it unclear which sons pete actually has:

Input file example4.txt:

    father(john,pete).

    % either the first or a second or both facts are true:
    father(pete,mark) | father(pete,mickey).

    % equivalent to (father(X,Y) & father(Y,Z)) => grandfather(X,Z).
    -father(X,Y) | -father(Y,Z) | grandfather(X,Z).

    -grandfather(john,X) | $ans(X).

Output:

    result: proof found
    for example4.txt 

    answer: $ans(mark) | $ans(mickey).
    proof:
    1: [in] -father(X,Y) | -father(Z,X) | grandfather(Z,Y).
    2: [in] father(pete,mickey) | father(pete,mark).
    3: [mp, 1, 2.1] father(pete,mickey) | -father(X,pete) | grandfather(X,mark).
    4: [in] father(john,pete).
    5: [mp, 3.1, 4] grandfather(john,mark) | father(pete,mickey).
    6: [in] -grandfather(john,X) | $ans(X).
    7: [mp, 5, 6] father(pete,mickey) | $ans(mark).
    8: [mp, 7, 1] $ans(mark) | -father(X,pete) | grandfather(X,mickey).
    9: [mp, 8.1, 4] grandfather(john,mickey) | $ans(mark).
    10: [mp, 9, 6] $ans(mark) | $ans(mickey).

Indeed, there is no way to give a definite answer, but gkc gives a correct answer indicating
that either mark or mickey is a grandson of john, or perhaps both are.

The [mp, 8.1, 4] means that the 1th (numeration 0,1,2,...) literal in the clause at proof step 8 
was cut off with the first (0th) literal of the clause at proof step 4. In case the first literal is cut off,
the N.0 is simplified to N, as in the previous examples.

I.e. literals in a clause are numbered 0, 1, 2, etc and the number 0 is not added to the step number.

###  Example 5 for more rules

To make matters a bit more complicated, we add an ancestor rule 
and look for ancestors of mark.

Input file example5.txt:

    father(john,pete).
    father(pete,mark).

    % equivalent to (father(X,Y) & father(Y,Z)) => grandfather(X,Z).
    -father(X,Y) | -father(Y,Z) | grandfather(X,Z).

    -ancestor(X,Y) | -ancestor(Y,Z) | ancestor(X,Z).
    -father(X,Y) | ancestor(X,Y).

    -ancestor(X,mark) | $ans(X).

Output:

    result: proof found
    for example5.txt 

    answer: $ans(pete).
    proof:
    1: [in] -father(X,Y) | ancestor(X,Y).
    2: [in] father(pete,mark).
    3: [mp, 1, 2] ancestor(pete,mark).
    4: [in] -ancestor(X,mark) | $ans(X).
    5: [mp, 3, 4] $ans(pete).


###  Example 6 for equalities and functions

Now we reformulate the whole thing with equalities and functions! 

`father(john)=pete` means, as expected, that pete is the father
of john and there can be no other fathers. If you also gave
`father(john)=lucas` this would make gkc to conclude that
pete and lucas are the same object, i.e. pete=lucas.

Importantly, two different constants are not considered inequal
by default (think of the constants as labels on objects: there could
be several different labels on one object):

    a=b.

does not give a contradiction.

Notice that the following proof does not use equalities, just functions.

Input file example6.txt:

    %father(john,pete).
    father(john)=pete.

    %father(pete,mark).
    father(pete)=mark.

    % equivalent to (father(X,Y) & father(Y,Z)) => grandfather(X,Z).
    %-father(X,Y) | -father(Y,Z) | grandfather(X,Z).
    grandfather(father(father(X)),X).

    -ancestor(X,Y) | -ancestor(Y,Z) | ancestor(X,Z).
    ancestor(father(X),X).

    -ancestor(X,mark) | $ans(X).

Output:

    result: proof found
    for example6.txt 

    answer: $ans(father(mark)).
    proof:
    1: [in] ancestor(father(X),X).
    2: [in] -ancestor(X,mark) | $ans(X).
    3: [mp, 1, 2] $ans(father(mark)).
   
See that gkc was happily answering `father(mark)` although we have
not said who the father of mark actually is! The functions like `father` 
do not have to be defined on all the possible objects, they can be partially
known and partially unknown.

###  Example 7 for grandfather with functions

Returning to asking about grandfathers, this time using equalities
and functions. Again, notice that the proof does not use equalities, just functions.

Input file example7.txt:

    %father(john,pete).
    father(john)=pete.

    %father(pete,mark).
    father(pete)=mark.

    % equivalent to (father(X,Y) & father(Y,Z)) => grandfather(X,Z).
    %-father(X,Y) | -father(Y,Z) | grandfather(X,Z).
    grandfather(father(father(X)),X).

    -ancestor(X,Y) | -ancestor(Y,Z) | ancestor(X,Z).

    -grandfather(X,mark) | $ans(X).

Output:

    result: proof found
    for example7.txt 

    answer: $ans(father(father(mark))).
    proof:
    1: [in] grandfather(father(father(X)),X).
    2: [in] -grandfather(X,mark) | $ans(X).
    3: [mp, 1, 2] $ans(father(father(mark))).   

Again, father of mark is unknown (undefined) in our example, as well as the
father of the father of mark. 


###  Example 8 for equalities used in proof

Next we ask very concretely whether the father of the father of john is mark.
Here the proof actually does use equalities, but only for simplification.

Input file example8.txt:

    %father(john,pete).
    father(john) = pete.

    %father(pete,mark).
    father(pete) = mark.

    % equivalent to (father(X,Y) & father(Y,Z)) => grandfather(X,Z).
    %-father(X,Y) | -father(Y,Z) | grandfather(X,Z).
    grandfather(father(father(X)),X).

    -ancestor(X,Y) | -ancestor(Y,Z) | ancestor(X,Z).

    father(father(john)) != mark.

Output:


    result: proof found
    for example8.txt 

    proof:
    1: [in] -=(father(father(john)),mark).
    2: [in] =(father(john),pete).
    3: [in] =(father(pete),mark).
    4: [simp, 1, 2, 3] false
   
Observe that gkc outputs equalities in a prefix form: instead of
infix a=b it writes =(a,b). These two forms have the same meaning
for gkc: you could use both.

Negation of equality can be also written either in the infix form as

    a!=b
  
or in the prefix form as

    -=(a,b)

The last step of the proof uses the `simp` rule which here replaces 
father(john) subterm in the -=(father(father(john)),mark) statement with
pete using the equality at step 2, 
giving -=(father(pete),mark), which then contradicts the step 3.


###  Example 9 for reflexivity used in proof

We will ask the same question as in the previous example, but this time
using the $ans predicate to find a grandfather of john. 

Input file example9.txt:

    %father(john,pete).
    father(john) = pete.

    %father(pete,mark).
    father(pete) = mark.

    % equivalent to (father(X,Y) & father(Y,Z)) => grandfather(X,Z).
    %-father(X,Y) | -father(Y,Z) | grandfather(X,Z).
    grandfather(father(father(X)),X).

    -ancestor(X,Y) | -ancestor(Y,Z) | ancestor(X,Z).

    father(father(john)) != X | $ans(X).

Output:


    result: proof found
    for example9.txt 

    answer: $ans(mark).
    proof:
    1: [in] -=(father(father(john)),X0) | $ans(X0).
    2: [in] =(father(john),pete).
    3: [in] =(father(pete),mark).
    4: [simp, 1, 2, 3] -=(mark,X) | $ans(X).
    5: [r=, 4.0] $ans(mark).

Here the proof uses the reflexivity rule `r=` which is basically the
standard property of equality: `X=X'.


###  Example 10 for multiple answers

Now let us look at how to get several answers, not just one.
We will also introduce mothers and a mother-side grandfather
rule.

Input:

    father(john) = pete.
    father(mike) = pete.
    mother(john) = eve.
    mother(mike) = eve.

    father(pete) = mark.
    mother(eve) = mary.

    grandfather(father(father(X)),X).
    grandfather(father(mother(X)),X).

    -grandfather(mark,X) | $ans(X).

We will also use a separate json-syntax strategy file multineg.txt
in the same folder to tell gkc specific details of what
to look for and how.

The `"max_answers":2` part tells gkc that it should
try to find at least 2 answers and stop after that.

The "strategy": ["negative_pref"] indicates that one specific
strategy (here a conventional negative-preference ordered binary resolution)
is to be used for proof search, without any parallel or sequential attempts
with different search strategies.

multineg.txt file:
  
    {
    "max_answers":2,
    "strategy": ["negative_pref"]
    }

And you should run gkc like this to make it use the multineg.txt strategy file:

    ./gkc example10.txt -strategy multineg.txt
    
Output:

    result: proof found
    for example10.txt 

    answer: $ans(mike).
    proof:
    1: [in] =(father(mike),pete).
    2: [in] grandfather(father(father(X)),X).
    3: [=, 1, 2.0.2] grandfather(father(pete),mike).
    4: [in] =(father(pete),mark).
    5: [simp, 3, 4] grandfather(mark,mike).
    6: [in] -grandfather(mark,X) | $ans(X).
    7: [mp, 5, 6] $ans(mike).

    answer: $ans(john).
    proof:
    1: [in] =(father(john),pete).
    2: [in] grandfather(father(father(X)),X).
    3: [=, 1, 2.0.2] grandfather(father(pete),john).
    4: [in] =(father(pete),mark).
    5: [simp, 3, 4] grandfather(mark,john).
    6: [in] -grandfather(mark,X) | $ans(X).
    7: [mp, 5, 6] $ans(john).

The [=, 1, 2.0.2] means that the clause at step 1 was used to replace a 2th subterm
(numbering 0,1,2,...) of the 0th atom of a clause at step 2 using equality.

Observe that the two answers have (obviously) different proofs.

Try to modify the multineg.txt file to ask for three answers: "max_answers":3,
and then run gkc again. It will give the same output as before, but will add the 
last line at the end:

    result: proof not found.
  
essentially indicating that the required number of different proofs (3) were not found.  


###  Example algebra

Next we will take a question from abstract algebra,
axiomatized wholly by equalities: the file algebra.txt contains

    % A question from abstract algebra. Here m is an arbitrary binary
    % operation (similar to arithmetic multiplication) for 
    % which the following three axioms hold.

    m(X,i(X)) = e .   % i is an inverse function like one divided by X
    m(X,e) = X .      % e is a unit element like 1 when on the right side
    m(X,m(Y,Z)) = m(m(X,Y),Z) . % m is an associative operation

    % Question: is e also a unit element when it is on the left
    % side of multiplication m?
    %
    % We get the following clause by negating `forall X. m(e,X) = X`
    % as `exists X. m(e,X) != X`
    % and using an arbitrary unknown constant `c` for existentially 
    % quantified `X`.
    % In short, if the next clause gives contradiction, then it will
    % also give a contradiction for anything you can put in place of `c`.

    m(e,c) != c .

The following proof uses given equalities to derive several new equalities.
The `=` rule basically replaces parts of one premiss matching (unifying)
one side of the second premiss equality with the other side of the equality.

The `simp` rule also replaces a part of a premiss, but does so in a *permanent*s manner,
meaning that the original unchanged premiss is not used in the search after
the simplification replacement.

    result: proof found
    for algebra.txt 

    proof:
    1: [in] =(m(X,i(X)),e).
    2: [in] =(m(X,m(Y,Z)),m(m(X,Y),Z)).
    3: [=, 1, 2.0.3] =(m(X3,e),m(m(X3,Y3),i(Y3))).
    4: [in] =(m(X,e),X).
    5: [simp, 3, 4] =(X,m(m(X,Y),i(Y))).
    6: [=, 1, 5.0.3] =(X,m(e,i(i(X)))).
    7: [=, 5.0.R, 5.0.3] =(m(X,Y),m(X,i(i(Y)))).
    8: [in] -=(m(e,c),c).
    9: [=, 6.0.R, 7.0.4, 8] false

The [=, 1, 2.0.3, fromaxiom] means that the clause at step 1 was used to replace a 3th subterm
(numbering 0,1,2,...) of the 0th atom of a clause at step 2 using equality.


###  Example with an unprovable problem

A simple clause set 

    p(a).
    -p(X) | p(f(X)).

in example11.txt produces an unlimited number of clauses like

    p(f(a))
    p(f(f(a)))
    p(f(f(f(a)))
    ...

and obviously does not lead to a contradiction.

However, gkc does not attempt to detect unprovability and will
run either forever or for a very long time, until it runs out
of allocated time or memory. Thus, running

    ./gkc example11.txt
  
will produce no output for quite some time.

Running gkc with the -tptp 1 parameter added will cause it
to produce a lot of output, indicating the progress of proof
search runs. Try it:

    ./gkc example11.txt -tptp 1
  
and you will see that for some strategies
gkc terminates very quickly, while for others it runs until the
given time is out.

### Example with a hard problem

As an example of a small but a really hard problem for gkc which is
nevertheless actually provable, try the fof-format problem 
[example12.txt](example12.txt) (LCL876+1 from the TPTP collection):

    ./gkc example12.txt
  
It will probably run until the time of about 8 minutes allocated 
by the automatic strategy selection runs out.

You can check out 

    ./gkc example12.txt -print 12
  
to see the progress.

Of course, it is likely that with a suitable search strategy
gkc will find a proof in a sensible amount of time and memory. Currently
we are simply unaware of which strategy it would be!

Using a time limit like

    ./gkc example12.txt -seconds 20
  
is useful to avoid very long runs.

###  Examples with nontrivial blocks world problems 

[Blocks world](https://en.wikipedia.org/wiki/Blocks_world)
is a classic family of toy problems: there is a robot arm
able to lift single blocks and to put them on top of other blocks.

The goal is to find a sequence of robot arm movements to produce
a required configuration of blocks: for example, a tower.

Without a specialized search strategy these planning problems
tend to be surprisingly hard for provers, including gkc.

First, try out a simple version of a blocks world problem:

    ./gkc blocks.txt

Look into the [blocks.txt](blocks.txt) file to see an explanation
of what facts and rules there are and how is the task encoded.

The two harder versions of the blocks world problem (obtained
by commenting out the first question and uncommenting second
or third in blocks.txt are `blocks2.txt` and `blocks3.txt`,
respectively.

Running

    ./gkc blocks3.txt -print 11

will probably take over ten seconds with a large number of search
strategies tried out and the output of the successful search 
starting (if -parallel 0 were added) as

    result: proof found
    for blocks3.txt 
    by run 39 strategy {"max_dseconds":10,"strategy":["query_focus"],"query_preference":1}

    answer: $ans(do(putdown(b,a),do(pickup(b),do(putdown(a,d),do(pickup(a),
            do(putdown(d,c),do(pickup(d),do(putdown(c,table),do(pickup(c),s0))))))))).
    proof:
    ...

In the following [advanced examples chapter](#Advanced-examples) we will look at
guiding gkc search by using a strategy selection file. A suitable simple strategy
for the second and third examples is given in the file `querystrat.txt`:

    {
    "max_seconds":0,
    "strategy":["query_focus"],
    "query_preference":1
    }

and then running 

   ./gkc blocks3.txt -strategy querystrat.txt
   

There is a lot of interesting research on the topic of
[improving blocks world planning](https://ai.dmi.unibas.ch/_files/teaching/hs12/search-opt-seminar/slides/08_blocks_world_revisited.pdf).


Advanced examples
-----------------


Next we will look at richer input languages and more advanced capabilities of gkc. 

Topics considered:
* [Whitespace variables numbers encodings](#Whitespace-variables-numbers-encodings)
* [Included files and mixing simple syntax with fof and cnf syntax](#Included-files-and-mixing-simple-syntax-with-fof-and-cnf-syntax)
* [Example for TPTP fof syntax with steam](#Example-for-TPTP-fof-syntax-with-steam)
* [Example for steam with a shared memory database](#Example-for-steam-with-a-shared-memory-database)
* [Example for steam with the TPTP format output](#Example-for-steam-with-the-TPTP-format-output)
* [Example with json output](#Example-with-json-output)
* [Telling gkc what is actually the question clause in input](#Telling-gkc-what-is-actually-the-question-clause-in-input)
* [Directing the prover and changing the settings](#Directing-the-prover-and-changing-the-settings)
* [Output detail level](#Output-detail-level)
* [Conversion and clausification](#Conversion-and-clausification)
* [Arithmetic](#Arithmetic)
* [Lists](#Lists)
* [Distinct symbols as strings](#Distinct-symbols-as-strings)
* [TPTP examples](#TPTP-examples)


Several examples will use the fof (first order formula) syntax used by
the TPTP project:

* [TPTP project](http://www.tptp.org/)
* [fof syntax](http://tptp.org/TPTP/TR/TPTPTR.shtml#FormulaeSection)
* [fof example](http://www.tptp.org/cgi-bin/SeeTPTP?Category=Problems&Domain=SYN&File=SYN000+1.p)

### Whitespace variables numbers encodings 

This is not a really advanced topic, but important.

Gkc follows the TPTP conventions in the way it treats special characters, 
with a few additions. A normal symbol must not contain any whitespace or 
non-alphanumeric characters like (, -, ~, =, ", comma etc: however, 
underscore `_` and dollar `$` are allowed, with predicates and functions with
a special predefined meaning being prefixed with a dollar `$`.

You can put whitespace or any character into symbols by surrounding the 
symbol with single quote symbols like this:

    'John \'Smith\'!' 
    
which is treated as a constant, despite that it starts with a uppercase character
inside the quotes. Notice that internal quote symbols must be prefixed by backslash.
   
Any symbol containing a character except an ascii letter, digit, underscore _,
or dollar $ will be printed out by surrounding it with single quotes.
As an exception, equality = and aritmetic expressions +, *, -, / will 
not be surrounded by quotes. 

Double quotes like `"John Smith"` indicate that a symbol is *distinct* (essentially,
a string), unequal to any other distinct symbol, number or a list. 
Double quotes inside double quoted symbols must be prefixed by a backslash. 
More about distinct symbols in the later examples.

Additionally you can make a symbol variable by prefixing it with a question
mark like this:

    ?smth

As said before, any symbol starting with a capital letter or a question mark 
is assumed to be a variable, and the rest are not. 

This holds for both the simple syntax in the previous examples and the 
fof formulas to be described next. 

Thus, in fof formulas an uppercase-character-starting symbol is a variable 
even if it is not explicitly quantified.

You can also use integers like 71 or period-separated decimals like 1.35
as constants.

Gkc is agnostic towards using different character encodings: it uses
c-strings, i.e. 0-terminated byte sequences and does not care about encodings.


### Included files and mixing simple syntax with fof and cnf syntax

Problems may contain several include statements like

    include('Axioms/GEO004+2.ax').
    include('steam_kb.txt').

in which case the included path is searched sequentially from the following
locations until found:
* in the folder of the input problem path,
* in the folder indicated by the environment variable TPTP,
* in the folder /opt/TPTP.

When an included file is found, its contents are treated as if they were a part of the
main problem file. The include file may again contain includes. When an include file
is not found, gkc will output an error and stop.

The fof and cnf syntax described in the following section can be mixed with the
simple syntax as in `example13.txt`:

    include('steam_kb.txt'). % this is an include statement

    fof(a1, axiom, ( ! [X] : (wolf(X) => dangerous(X)))). % this is fof
    cnf(a2, axiom, (~dangerous(Y) | ~good(Y))). % this is cnf

    good(Z). % this is a simple syntax (saying that everything is good)

Running `./gkc example13.txt` will output:

    result: proof found
    for example13.txt 

    proof:
    1: [in,a1] -wolf(X) | dangerous(X).
    2: [in,$inc_pel47_1_2] wolf($sk7).
    3: [mp, 1, 2] dangerous($sk7).
    4: [in,a2] -good(X0) | -dangerous(X0).
    5: [in] good(X0).
    6: [simp, 4, 5] -dangerous(X).
    7: [mp, 3, 6] false


where `[in,$inc_pel47_1_2]` indicates that the statement with the
name `pel47_1_2` was in one of the included files. 

For a search strategy with a setting "query_preference": 2, 
gkc treats imported axioms as having a bit lower priorities
than the statements in the main file. For all the other strategies being
imported or not does not make a difference, although in the general case the order of clauses
may sometimes cause minor unpredictable differences in proof search.

The example also shows that when printing clauses, gkc does not generally use the same variable
names as in the input file. Expect it to prefer variable names X,Y,Z,U,V,W (optionally
followed by a number) in the printout. For input clauses the variables are sometimes
printed as XXN for some number N.

###  Example for TPTP fof syntax with steam 

The steam.txt file contains a classic "Schubert's Steamroller" puzzle taken from
TPTP and written in fof syntax with connectives like implication `=>`, quantifiers `forall ! [X] ...`,
`exists ? [X] ...` etc. 

See the official [fof syntax](http://tptp.org/TPTP/TR/TPTPTR.shtml#FormulaeSection) and
[fof example](http://www.tptp.org/cgi-bin/SeeTPTP?Category=Problems&Domain=SYN&File=SYN000+1.p)
in [TPTP](http://www.tptp.org).

Gkc will first convert the complex formulas to a simple clause form (properly called
*clause normal form*) used in the previous examples. The statements we had in these
examples are called *clauses*. 

The first part of the [steam.txt](steam.txt) file contains comments describing the problem.
The rest contains full first order formulas like 

    fof(pel47_1_1,axiom,
        ( ! [X] :
            ( wolf(X)
           => animal(X) ) )).

    fof(pel47_1_2,axiom,
        ( ? [X1] : wolf(X1) )).

    fof(pel47,conjecture,
        ( ? [X,Y] :
            ( animal(X)
            & animal(Y)
            & ? [Z] :
                ( grain(Z)
                & eats(Y,Z)
                & eats(X,Y) ) ) )).

Each statement in the TPTP FOF language is terminated with a period symbol . and has a structure

    fof(statement_name, statement_role, fof_statement).

while each statement in the TPTP CNF language has a form

    cnf(statement_name, statement_role, cnf_statement).    
  
where the *statement_name* will be used in the proof, the *statement_role* indicates whether
it is an axiom, an assumption or hypothesis, or a goal to be proved from these: the latter is either
conjecture (which has to be negated) or negated_conjecture (negated already).

Gkc allows the *fof_statement* to contain free variables, differently from the TPTP FOF
syntax which does not allow free variables. The *cnf_statement* is a *clause* 
(disjunction of atoms or negated atoms) where all the variables are free variables.

Indicating the role enables provers to find a more suitable strategy. It does not (except the
conjecture case, which has to be negated) have a *logical* meaning.
     
The first example statement above is universally quantified (`!` symbol) and will be converted by gkc to a clause

    -wolf(X) | animal(X).
  
The second statement is existentially quantified (`?` symbol) and will be converted by gkc to a clause

    wolf($sk7).

where `$sk7` is a new constant invented by gkc which should not occur in any other formula in the problem:
this procedure is called [Skolemization](https://en.wikipedia.org/wiki/Skolem_normal_form).
Gkc always uses the `$sk` prefix for such constants and functions, using a new number N for each
new one. The original formula is assumed not to contain `$skN` form symbols.

Importantly, gkc assumes the *free variables* in formulas or simple clauses 
with the `conjecture` role are not implicitly bound by a universal quantifier (*for all*)
in the formula: instead, they are implicitly bound by the existential quantifier (*exists*): 
this corresponds better to the intuitive understanding of free variables in the conjecture.
Thus  `cnf(question,conjecture,p(X))` is equivalent to `fof(question,conjecture,? [X]: p(X))`
which is equivalent to `fof(question,negated_conjecture,! [X]: -p(X))`.

The third statement expresses the question to be proved: is 
there an animal that likes to eat a grain eating animal?
It has a conjecture role and has to be first negated and then converted to 
clauses:

    -eats(X,Y) | -eats(Z,X) | -grain(Y) | -animal(X) | -animal(Z).

In general, one formula in the input may create several clauses and the optimized
algorithm for creating such clauses is nontrivial, sometimes involving mini-scoping and the creation of new
definitions (new predicates) to make the size and number of generated clauses smaller.

Ask gkc to prove steam.txt without using parallel processes:
  
    ./gkc steam.txt -parallel 0 

producing output:

    result: proof found
    for steam.txt 

    proof:
    1: [in,pel47_7] -much_smaller(X,Y) | -eats(X,Z) | eats(Y,U) | eats(Y,X) | -plant(U) | -plant(Z) | -animal(X) | -animal(Y).
    2: [in,pel47_6_2] -grain(X) | plant(X).
    3: [in,pel47_6_1] grain($sk2).
    4: [mp, 2, 3] plant($sk2).
    5: [mp, 1.4, 4] -much_smaller(X,Y) | -eats(X,Z) | eats(Y,$sk2) | eats(Y,X) | -plant(Z) | -animal(Y) | -animal(X).
    6: [in,pel47_14] plant($sk1(X)) | -snail(X).
    7: [in,pel47_5_2] snail($sk3).
    8: [mp, 6.1, 7] plant($sk1($sk3)).
    9: [mp, 5.4, 8] -eats(X,$sk1($sk3)) | -much_smaller(X,Y) | eats(Y,$sk2) | eats(Y,X) | -animal(Y) | -animal(X).
    10: [in,pel47_14] eats(X,$sk1(X)) | -snail(X).
    11: [mp, 10.1, 7] eats($sk3,$sk1($sk3)).
    12: [in,pel47_5_1] -snail(X) | animal(X).
    13: [mp, 12, 7] animal($sk3).
    14: [mp, 9, 11, 13] -much_smaller($sk3,X) | eats(X,$sk2) | eats(X,$sk3) | -animal(X).
    15: [in,pel47_8] much_smaller(X,Y) | -snail(X) | -bird(Y).
    16: [mp, 15.1, 7] much_smaller($sk3,X) | -bird(X).
    17: [in,pel47_3_2] bird($sk5).
    18: [mp, 16.1, 17] much_smaller($sk3,$sk5).
    19: [in,pel47_3_1] -bird(X) | animal(X).
    20: [mp, 19, 17] animal($sk5).
    21: [mp, 14, 18, 20] eats($sk5,$sk3) | eats($sk5,$sk2).
    22: [merge, 1.4.5] -much_smaller(X,Y) | -eats(X,Z) | eats(Y,Z) | eats(Y,X) | -plant(Z) | -animal(Y) | -animal(X).
    23: [mp, 22.4, 4] -much_smaller(X,Y) | -eats(X,$sk2) | eats(Y,$sk2) | eats(Y,X) | -animal(Y) | -animal(X).
    24: [mp, 21.1, 23.1, 20] eats($sk5,$sk3) | -much_smaller($sk5,X) | eats(X,$sk2) | eats(X,$sk5) | -animal(X).
    25: [in,pel47_9] much_smaller(X,Y) | -bird(X) | -fox(Y).
    26: [mp, 25.1, 17] much_smaller($sk5,X) | -fox(X).
    27: [in,pel47_2_2] fox($sk6).
    28: [mp, 26.1, 27] much_smaller($sk5,$sk6).
    29: [in,pel47_2_1] -fox(X) | animal(X).
    30: [mp, 29, 27] animal($sk6).
    31: [mp, 24.1, 28, 30] eats($sk6,$sk5) | eats($sk5,$sk3) | eats($sk6,$sk2).
    32: [in,pel47_13] -eats(X,Y) | -snail(Y) | -bird(X).
    33: [mp, 32.1, 7] -eats(X,$sk3) | -bird(X).
    34: [in,pel47_3_2] bird($sk5).
    35: [mp, 31.1, 33, 34] eats($sk6,$sk5) | eats($sk6,$sk2).
    36: [mp, 35.1, 23.1, 30] eats($sk6,$sk5) | -much_smaller($sk6,X) | eats(X,$sk2) | eats(X,$sk6) | -animal(X).
    37: [in,pel47_10] much_smaller(X,Y) | -fox(X) | -wolf(Y).
    38: [mp, 37.1, 27] much_smaller($sk6,X) | -wolf(X).
    39: [in,pel47_1_2] wolf($sk7).
    40: [mp, 38.1, 39] much_smaller($sk6,$sk7).
    41: [in,pel47_1_1] -wolf(X) | animal(X).
    42: [mp, 41, 39] animal($sk7).
    43: [mp, 36.1, 40, 42] eats($sk7,$sk6) | eats($sk6,$sk5) | eats($sk7,$sk2).
    44: [in,pel47] -eats(X,Y) | -eats(Z,X) | -grain(Y) | -animal(X) | -animal(Z).
    45: [mp, 3, 44.2] -eats(X,Y) | -eats(Y,$sk2) | -animal(X) | -animal(Y).
    46: [mp, 21.1, 45.1, 20] eats($sk5,$sk3) | -eats(X,$sk5) | -animal(X).
    47: [mp, 43.1, 46.1, 30] eats($sk7,$sk6) | eats($sk7,$sk2) | eats($sk5,$sk3).
    48: [mp, 47.2, 33, 34] eats($sk7,$sk6) | eats($sk7,$sk2).
    49: [in,pel47_11] -eats(X,Y) | -grain(Y) | -wolf(X).
    50: [mp, 49.1, 3] -eats(X,$sk2) | -wolf(X).
    51: [in,pel47_1_2] wolf($sk7).
    52: [mp, 48.1, 50, 51] eats($sk7,$sk6).
    53: [in,pel47_11] -eats(X,Y) | -fox(Y) | -wolf(X).
    54: [mp, 53.1, 27] -eats(X,$sk6) | -wolf(X).
    55: [mp, 52, 54, 51] false


The proof contains only clauses created from the formulas: the original names of formulas used are indicated 
like

    [in,pel47_1_2] wolf($sk7).


Since the steamroller puzzle is a fairly easy problem for provers, several different search strategies
are able to solve it very quickly. Thus, if you run the gkc without the `-parallel 0` parameter on UNIX,
four parallel processes are created with different search strategies, which start to run at approximately
the same time and it sometimes happens that one finishes first, while at other times another one may 
finish first. Use the `-print 11` or `-print 12` parameters to see just the strategy finding
a proof or all the runs with different strategies tried out, respectively.

Using `-print 11` you may get a different output in a somewhat unpredictable manner, for example

    result: proof found
    for steam.txt
    by run 3 fork 2 strategy {"max_seconds":1,"strategy":["unit"],"query_preference":1}
    ...

or

    result: proof found
    for steam.txt
    by run 4 fork 3 strategy {"max_seconds":1,"strategy":["negative_pref"],"query_preference":0}
    ...

where `run N` indicates the strategy used (numeration 1,2,...) and `fork M` indicates which parallel
process found it (numeration 0,1,2,..). The run number N and the fork number M have a strict
correspondence: the order by which runs are given to forks is pre-determined.

The proofs found by different strategies are likely also somewhat different.

As said before, gkc implements parallel processes using forks and only for UNIX, i.e. Linux and macOS.


###  Example for steam with a shared memory database

Examples in this section behave somewhat differently on Linux and
Windows, and do not function on macOS: more about this below.

Very large axiom sets take a long time to parse and index. In case you have many different questions
to ask, which all use the same large axiom set, it makes sense to parse, convert and index 
the axiom set before the questions are asked, so that each individual question does not
have to go through that process.

In order to try this, there are two files created from steam.txt:
* steam_kb.txt contains all axioms in steam.txt, but not the conjecture,
* steam_query.txt contains only the conjecture.

Read in an axiom set into persistant shared memory like this:

    ./gkc -readkb steam_kb.txt
  
which parses, converts and indexes the file steam_kb.txt, outputs

    Data parsed into the shared memory db, starting to build indexes.
    Db ready in shared memory.

does not attempt to prove anything and stops.

Since steam_kb.txt is a small file, loading it into memory does not really make proof
search noticeably faster. Large axiom sets like 
[CSR002+5.ax in TPTP](http://www.tptp.org/cgi-bin/SeeTPTP?Category=Axioms&File=CSR002+5.ax)
contain several million statements: parsing and indexing this file takes ca 30
seconds for gkc.

You can then start asking questions from this persistant memory image like this:

    ./gkc steam_query.txt -usekb

On Windows, the same machinery has additional requirements: 

First, the default amount of memory requested by gkc may be too large: run
  
    gkc -readkb steam_kb.txt -mbsize 1000 

to ask for just 1GB of memory. Second, the process does NOT stop, but says

    Shared memory kb is available while this program is running.
    Press any key to free shared memory kb and exit . . .

and waits. Shared memory database is available while the process
is still waiting, but once it is stopped, the database is no longer available. Thus
on Windows the machinery has to be run in two separate command line windows.

OS X severely restricts the size of the shared memory and it is really
complicated to change the restriction. Hence you may simply assume the
shared memory functionality of gkc is not available on OS X.

On UNIX, once the memory database is no longer needed, delete it by doing

    ./gkc -deletekb
  
It is possible to have a number of different shared memory databases available at
the same time. Thus the databases have a number, which by default is 1000.

To indicate which shared memory database should be loaded, used or deleted,
use the optional `-mbnr <shared memory database nr>` parameter like this:

    ./gkc -readkb steam_kb.txt -mbnr 1001
    ./gkc steam_query.txt -usekb -mbnr 1001
    ./gkc -deletekb -mbnr 1001
  
Only one memory database at a time can be used for answering one particular query.

It is also possible to write the present shared memory database to a file for 
faster loading later:

    ./gkc -writekb <dump file>

and then read it in later by doing:

    ./gkc -loadkb <dump file>

thus avoiding the prolonged parsing/converting/indexing process of reading
a large file of statements.

As a base technology for both the shared memory database and 
its internal representation of data gkc uses [Whitedb](http://whitedb.org).


###  Example for steam with the TPTP format output

The simple output format used in all the previous examples does not directly contain
the original input formulas of the fof language of TPTP and the steps in conversion, 
only the names of the formulas. Additionally, TPTP suggests using a particular
format of output, including proved-successfully-marker and the begin and end
of proof.

To use the TPTP suggested output format, say:

    ./gkc steam.txt -tptp 
  
which will use the TPTP format for proofs.

We will not write the whole output here. The proof has the following structure:

    result: proof found
    for steam.txt 
    % SZS status Theorem for steam.txt .

    % SZS output start CNFRefutation for steam.txt 
    fof('pel47_7_$sk', plain, (((eats(X1,X2) | ((~eats(X2,X3) | ~plant(X3)) | (~much_smaller(X2,X1) | ~animal(X2)))) | (eats(X1,X4) | ~plant(X4))) | ~animal(X1)),
      inference(negpush_and_skolemize,[],['pel47_7'])).
    fof('pel47_7', axiom, (! [X] : (animal(X) => ((! [Y] : (plant(Y) => eats(X,Y))) | (! [Y1] : (((animal(Y1) & much_smaller(Y1,X)) & (? [Z] : (plant(Z) & eats(Y1,Z)))) => eats(X,Y1)))))),
      input). 
    fof('pel47_8_$sk', plain, (much_smaller(X2,X1) | ((~caterpillar(X2) & ~snail(X2)) | ~bird(X1))),
      inference(negpush_and_skolemize,[],['pel47_8'])).
    fof('pel47_8', axiom, (! [X,Y] : ((bird(Y) & (snail(X) | caterpillar(X))) => much_smaller(X,Y))),
      input). 
    ...
    cnf('1', plain, (~much_smaller(X,Y) | ~eats(X,Z) | eats(Y,U) | eats(Y,X) | ~plant(U) | ~plant(Z) | ~animal(X) | ~animal(Y)),
      inference(cnf_transformation,[],['pel47_7_$sk'])).
    cnf('2', plain, (much_smaller(X,Y) | ~snail(X) | ~bird(Y)),
      inference(cnf_transformation,[],['pel47_8_$sk'])).
    cnf('3', plain, (snail($sk3)),
      inference(cnf_transformation,[],['pel47_5_2_$sk'])).
    ...
    cnf('11', plain, (animal($sk3)),
      inference(resolution,[],['10','3'])).
    cnf('12', plain, (~bird(X) | animal(X)),
      inference(cnf_transformation,[],['pel47_3_1_$sk'])).
    cnf('13', plain, (animal($sk5)),
      inference(resolution,[],['12','5'])).
    ...
    cnf('58', plain, ($false),
      inference(resolution,[then_simplify],['41','57','40'])).
    % SZS output end CNFRefutation for steam.txt

where each statement has either a form with a fof-formula

    fof(formula_name, formula_role, formula, 
        conversion_rule_and_sources).
    
or a form with a simple clause
   
    cnf(formula_name, plain, clause, 
        derivation_rule_and_sources).

The cnf-statements are also allowed in the TPTP format as valid input.

The first block with fof formulas contains the input formulas after some trivial
simplifications along with one intermediate step of pushing negation inwards and skolemizing
like

    inference(negpush_and_skolemize,[],['pel47_6_1'])).

The second block contains cnf clauses, which are either input like
    inference(cnf_transformation,[],['pel47_6_1_$sk'])) 
or derived by proof search like
    inference(resolution,[then_simplify],['52','54','51'])).

The cnf clauses are the same clauses as output by the simple default format,
just presented in the TPTP language along with the TPTP-style derivation step
and source information.


### Example with json output 

Gkc will produce *JSON* output if passed the key `-json` on the command line.
For example, 

    ./gkc example3.txt -json 

will output

    {"result": "proof found",

    "answers": [
    {
    "answer": [["$ans","mark"]],
    "proof":
    [
    [1, ["in"], [["-father","?:X","?:Y"], ["-father","?:Z","?:X"], ["grandfather","?:Z","?:Y"]]],
    [2, ["in"], [["father","pete","mark"]]],
    [3, ["mp", 1, 2], [["-father","?:X","pete"], ["grandfather","?:X","mark"]]],
    [4, ["in"], [["father","john","pete"]]],
    [5, ["mp", 3, 4], [["grandfather","john","mark"]]],
    [6, ["in"], [["-grandfather","john","?:X"], ["$ans","?:X"]]],
    [7, ["mp", 5, 6], [["$ans","mark"]]]
    ]}
    ]}

The results of successful searches are json objects with two keys:

* `result` indicates whether the proof was found,
* `answers` contains a list of all answers and proofs for these answers. 
  A proof may contain the `answer` key and always contains the `proof` key.
  The value of the latter is a list with the numbered steps of the proof found:
  each step has a form `[formula id, [derivation rule, source 1, ..., source N], formula]` 
  where the *formula* either stems from an input fact / rule or is derived
  from the indicated sources. A source may be represented as a formula id or a list with 
  the formula id as a first element and the rest indicating an exact position in the source formula.

Have a look at the *JSON* examples explained in the later section or try out
the [JSON-LD-LOGIC playground](http://localhost:8000/json.html).


###  Telling gkc what is actually the question clause in input 

Gkc does not know which clause is a question: this is ok,
but it is bad for efficiency in case of large clause sets.

There is a simple way to tell gkc that some clause or a formula is indeed a question
and gkc should heavily focus on that clause or a formula.

Instead of writing your negated question like

    -father(john,pete).
  
write it with this wrapping  as a fof formula (observe double parentheses at the end)

    fof(q1,negated_conjecture,
        -father(john,pete)).

or a cnf formula (i.e. simple clause) in TPTP language

    cnf(q1,negated_conjecture,
        -father(john,pete)).

and then gkc knows that this clause should get priority in search.

Similarly, you can tell gkc that a clause or a formula is not just
an arbitrary axiom which may or may not be used, but an important
assumption likely to be used in the proof:

    cnf(a1,assumption,
        person(john)).

These `negated_conjecture` and `assumption` roles were already
mentioned previously when describing fof syntax: it is worth stressing
that they are useful and can be also intermingled with the simple
syntax.


###  Directing the prover and changing the settings 

By default gkc automatically selects a number of strategies to run one-by one
and outputs either message that the proof was found along with the proof or a message
that no proof was found.

You can tell gkc the exact strategy and output level by writing a small
json strategy file strat.txt like this:

    {
      "print_level": 10,     
      "max_seconds": 60,
      "strategy":["query_focus"],
      "query_preference": 1
    }   

and then call the prover like this to make it use the strategy file:

    ./gkc example1.txt -strategy strat.txt

or alternatively giving the strategy json text directly on the command line:

    ./gkc example1.txt -strategytext ' .... '

There are two somewhat different ways to write the strategy file:
either indicate a single strategy (single run) like in the previous example,
or multiple runs like this (try strategy example `runs.txt`):

    {
    "print_level": 15,
    "runs":[
    {"max_seconds": 1,"strategy":["unit"],"query_preference":0},
    {"max_seconds": 1,"strategy":["unit"],"query_preference":1},
    {"max_seconds": 1,"strategy":["negative_pref"],"query_preference":0},
    {"max_seconds": 1,"strategy":["negative_pref"],"query_preference":1},
    {"max_seconds": 1,"strategy":["negative_pref"],"query_preference":0,"weight_select_ratio":100, "depth_penalty":100, "length_penalty":100},
    {"max_seconds": 1,"strategy":["hardness_pref","posunitpara"],"query_preference":0,"weight_select_ratio":20,"depth_penalty": 50, "length_penalty":100},

    {"max_seconds": 5,"strategy":["unit"],"query_preference":0},
    {"max_seconds": 5,"strategy":["unit"],"query_preference":1},
    {"max_seconds": 5,"strategy":["negative_pref"],"query_preference":0}
    ]
    }
  
In the latter case gkc will try out all these strategies in that order, by default using the parallel processes 
so that each process gets a subsequence of these strategies.

A simple way to obtain such run sequences is to run

    ./gkc example1.txt -print 13
  
which will then automatically construct and print out a suitable full strategy json
with many runs, which you can simply copy and paste into your own file for later modification.

The default value for limit-type fields starting with max_ is 0, indicating that no limit is set.

In case "equality": N is not set to 0, GKC uses reflexivity, paramodulation and demodulation 
with knuth-bendix ordering for handling equality.

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
* "unit", "double" or "triple" : use binary unit resolution or its generalization: (one of the arguments must be unit, a two-literal or three-literal clause,  
respectively. These may be added to the list in addition to the previous strategy indicators, for example, like ["query_focus","unit"].

Other useful parameters, to be used outside the "strategy": [...] list:
 
*  "print": 0 or 1, where 0 prohibits almost all printing, default 1.
*  "print_level": integer determining the level of output: useful values are between 0 and 50, default 10
*  "print_json": 0 or 1, where 0 is default and 1 forces json output.
*  "print_tptp": 0 or 1, where 0 is default and 1 forces tptp-style proof output
*  "max_size", "max_length", "max_depth", "max_weight" indicate limits on kept clauses, defaults are 0.
*  "equality" : 1 or 0, with 1 being default and 0 prohibiting equality handling.
*  "rewrite" : 1 or 0, with 1 being default and 0 prohibiting using equations for rewriting.
*  "max_dseconds": N being an integer limit of deciseconds (0.1 seconds) for one run, default 0 (no limit).
*  "max_seconds": N being an integer limit of seconds for one run, default 0 (no limit); 
                  this is an alternative to "max_dseconds".
*  "weight_select_ratio": N indicating the ratio of picking by order derived / clause weight, default is 5.
*  "max_answers": N indicating the maximal number of proofs searched for until search stops, default is 1.
*  "reverse_clauselist": N either default 0 or 1, where 1 follows the actual order for input clauses, starting from the end.
*  "sine": input filter with N either 1 or 2 where 1 is a less restrictive (accepts more clauses) and 2 more restrictive. NB! Sine is automatically switched off if the -provekb switch is used.
*  "depth_penalty": additional penalty for clause depth, default 1
*  "length_penalty": additional penalty for clause length, default 1
*  "var_weight": weight of a variable, default 5
*  "var_weight": weight of a repeated variable, default 7
*  "query_preference": N being 0, 1, 2 or 3 indicates which parts of the problem are treated as goals, assumptions or axioms:
      0 stands for no goal/assumption preference.
      1 stands for input preference (the assumption and conjecture formulas of fof)
      2 stands for making non-included formulas assumptions
      3 stands for considering only the negative clauses from conjecture to be goals

For "max_seconds"<2 gkc will automatically use immediate check for contradiction when a clause is derived. 

### Output detail level

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

The parameter `-derived` shows all the derived clauses: use it along the `-parallel 0`
parameter and preferably indicating a single run in the strategy file.

### Conversion and clausification

The `-convert` parameter causes gkc to not prove the input, but convert it 
to a format indicated by the `-tptp` or `-json` key, if present.

For example, `./gkc example2.txt -convert -json`
outputs JSON following the  [JSON-LD-LOGIC](https://github.com/tammet/json-ld-logic) proposal:

    [
    ["father","john","pete"],
    ["father","pete","mark"],
    [["-father","john","?:X"], "|", ["$ans","?:X"]]
    ]

while `./gkc example2.txt -convert -tptp` outputs

    cnf('frm_1',axiom,father(john,pete)).
    cnf('frm_2',axiom,father(pete,mark)).
    cnf('frm_3',axiom,~father(john,X) | $ans(X)).
 
The `-clausify` key causes gkc to not prove the input, but clausify it, i.e. convert it to the 
[conjunctive normal form](https://en.wikipedia.org/wiki/Conjunctive_normal_form)
and finally format the result according to the `-tptp` or `-json` key, if present.
In general, one formula in the input may create several clauses and the optimized
algorithm for creating such clauses is nontrivial, sometimes involving mini-scoping 
and the creation of new definitions (new predicates) to make the size and number of
generated clauses smaller. The *skolem functions* generated by gkc have a form `$skN` for
some number N.

For example, `./gkc steam.txt -clausify` creates output starting with

    animal(X0) | -wolf(X0).
    wolf($sk7).
    animal(X0) | -fox(X0).
    fox($sk6).
    animal(X0) | -bird(X0).
    ...


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

Although gkc defines several functions and predicates on numbers, it
does not, by default, know the properties of these functions except simple evaluation
of ground terms. Citing TPTP:

The extent to which ATP systems are able to work with the arithmetic predicates and
functions can vary, from a simple ability to evaluate ground terms, e.g., 
`$sum(2,3)` can be evaluated to 5, through an ability to instantiate variables 
in equations involving such functions, e.g., `$product(2,$uminus(X)) = $uminus($sum(X,2))` 
can instantiate `X` to 2, to extensive algebraic manipulation capability. 
The syntax does not axiomatize arithmetic theory, but may be used to write axioms of the theory. 

The same general principle holds for lists and distinct symbols interpreted as strings.

For example, gkc cannot prove an example formula `["exists",["X"],["$is_number","X"]]` 
unless neccessary additional axioms are given.
 
Try the simple examples `arithmetic1.txt`, `arithmetic2.txt`, `arithmetic3.txt`, 
`arithmetic4.txt`. The last of these produces output

    result: proof found
    for arithmetic4.txt 

    proof:
    1: [in] p($product(2,X)) | -p(X).
    2: [in] p(2.100000).
    3: [mp, 1.1, 2] p(4.200000).
    4: [mp, 3, 1.1] p(8.400000).
    5: [mp, 4, 1.1] p(16.800000).
    6: [mp, 5, 1.1] p(33.600000).
    7: [mp, 6, 1.1] p(67.200000).
    8: [mp, 7, 1.1] p(134.400000).
    9: [in] $less(X,128) | -p(X).
    10: [mp, 8, 9.1] false


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

The example `lists.txt` gives output

    result: proof found
    for lists.txt 

    answer: $ans(company2).
    proof:
    1: [in] =(listcount($nil),0).
    2: [in] =(listcount($list(X,Y)),$sum(listcount(Y),1)).
    3: [=, 1, 2.0.6] =(listcount($list(X,$nil)),1).
    4: [=, 3, 2.0.6] =(listcount($list(X,$list(Y,$nil))),2).
    5: [=, 4, 2.0.6] =(listcount($list(X,$list(Y,$list(Z,$nil)))),3).
    6: [in] -$greater(listsum(X),100) | -$greater(listcount(Y),2) | -revenues(Z,X) | -clients(Z,Y) | type(Z,goodcompany).
    7: [in] -type(X,goodcompany) | $ans(X).
    8: [mp, 6.4, 7] -$greater(listsum(X),100) | -$greater(listcount(Y),2) | -revenues(Z,X) | -clients(Z,Y) | $ans(Z).
    9: [=, 5.1.L, 8.0.1] -clients(X,$list(Y,$list(Z,$list(U,$nil)))) | -$greater(listsum(V),100) | -revenues(X,V) | $ans(X).
    10: [in] revenues(company2,$list(10,$list(20,$list(50,$list(60,$nil))))).
    11: [in] clients(company2,$list(a,$list(b,$list(c,$nil)))).
    12: [mp, 9.2, 10, 11] -$greater(listsum($list(10,$list(20,$list(50,$list(60,$nil))))),100) | $ans(company2).
    13: [in] =(listsum($nil),0).
    14: [in] =(listsum($list(X,Y)),$sum(listsum(Y),X)).
    15: [=, 13, 14.0.6] =(listsum($list(X,$nil)),$sum(0,X)).
    16: [=, 15, 14.0.6] =(listsum($list(X,$list(Y,$nil))),$sum($sum(0,Y),X)).
    17: [=, 16, 14.0.6] =(listsum($list(X,$list(Y,$list(Z,$nil)))),$sum($sum($sum(0,Z),Y),X)).
    18: [=, 17, 14.0.6] =(listsum($list(X,$list(Y,$list(Z,$list(U,$nil))))),$sum($sum($sum($sum(0,U),Z),Y),X)).
    19: [simp, 12, 18] $ans(company2).


 
### Distinct symbols as strings
 

Symbols in *double quotes* like in `"person"` stand for
for *distinct symbols* which can be viewed as a *string type*. 
A distinct symbol is not equal to any other syntactically different 
distinct symbol and not equal to any numbers or lists.


Gkc defines a function and three predicates on distinct symbols:
  
* `$strlen(S)` returns the integer length of a distinct symbol S as a string.
* `$substr(A,B)` evaluates to *true* if a distinct symbol A is a substring of a distinct symbol B, 
     and *false* otherwise, provided that A and B are distinct symbols.  
* `$substrat(A,B,C)` evaluates to *true* if a distinct symbol A is a substring of a 
     distinct symbol B exactly at the integer position C (starting from 0), and *false* otherwise,
     provided that A and B are distinct symbols and C is an integer.  
* `$is_distinct(A)` evaluates to *true* if A is a distinct symbol and 
  *false* if A is a number or a list.  

The example `distinct.txt` gives output  

    result: proof found
    for distinct.txt 

    proof:
    1: [in] -type(X,Y) | -type(Z,U) | -=(Z,X) | =(U,Y).
    2: [r=, 1.2] -type(X,Y) | -type(X,Z) | =(Y,Z).
    3: [in] =(smith1,smith2).
    4: [in] type(smith1,'John Smith').
    5: [=, 3, 4.0.1] type(smith2,'John Smith').
    6: [mp, 2, 5] -type(smith2,X) | =('John Smith',X).
    7: [in] type(smith2,"dog").
    8: [mp, 6, 7] =('John Smith',"dog").
    9: [in] type(smith1,"person").
    10: [=, 3, 9.0.1] type(smith2,"person").
    11: [mp, 6, 10] =('John Smith',"person").
    12: [=, 8, 11.0.1] false


Finding the last proof is, in a sense, nontrivial. Since gkc does not assume any axioms 
for distinct symbols, by default there are also no inequality axioms between distinct symbols
like `"person"!="dog"`. Finding a proof without these axioms requires that the
search strategy happens to generate evaluable literals like `"person"="dog"` in 
derived clauses. This may actually happen for some search strategies, but not for others.

The example `blocks4.txt` uses distinct symbols instead of the axiomatization of 
inequality and is otherwise the same as `blocks.txt`

### TPTP examples

The `Examples` folder contains a selection of small and relatively easy examples
from the different domains of the TPTP collection:

* nlp.txt
* organization.txt
* boolean.txt
* medicine.txt
* dreadbury.txt
* group.txt
* set.txt

These examples start with the explanatory comments from TPTP. 



Json examples
-------------

Gkc implements the [JSON-LD-LOGIC](https://github.com/tammet/json-ld-logic) proposal.
The main goals of JSON-LD-LOGIC are:

* Providing a simple format for the programmatic management of logical problems and proofs.
* Compatibility with the [TPTP](http://tptp.org "TPTP") language as described in the  
  [TPTP technical manual](http://tptp.org/TPTP/TR/TPTPTR.shtml#ProblemPresentation).
* Compatibility with [JSON-LD](https://json-ld.org/), 
  see the [W3C recommendation](https://www.w3.org/TR/json-ld11/). 

Topics considered:

* [Main features](#Main-features)
* [Core examples](#Core-examples)
* [Full JSON-LD-LOGIC examples](#Full-JSON-LD-LOGIC-examples)


### Main features

The main features of the syntax are:

* Terms, atoms and logical formulas are represented as JSON lists with predicate/function symbols 
  in the first position (prefix form) like `["father","john","pete"]` or
  `["forall",["X"],[["is_father","X"],"=>",["exists",["Y"],["father","X","Y"]]`.
* JSON-LD semantics in RDF is represented by the`"$arc"` predicate for triplets
  like `["$arc","pete","father","john"]` and an
  `"$narc"` predicate for named triplets aka quads like `["$narc","pete","father","john","eveknows"]`.
* JSON objects aka maps like  
  `{"son":"pete", "age":30, "@logic": [{"@id":"?:X","son":"?:Y"},"=>",{"@id":"?:Y","parent":"?:X"}}` 
  are interpreted as logical statements and use the special `"@logic"` key for inserting full *FOL*.
* JSON strings can represent ordinary constant/function/predicate symbols like `"foo"`,
  bound variables likes `"X"`, free variables like `"?:X"`, blank nodes like `"_:b0"` 
  and distinct symbols like `"#:bar"`, the latter three using special JSON-LD-style *prefixes*. 
* Arithmetic, a list type, string operations on distinct symbols and the semantics for `null` are defined.  
* JSON lists in JSON-LD like `{"@list":["a",4]}` are translated to nested typed terms
  using the `"$list"` and `"$nil"` functions: `["$list","a",["$list",4,"$nil"]]`.

You may want to have a look at the conversion of JSON examples to the simple or TPTP FOF
syntax by saying, for example, `./gkc jsexample1.js -convert -tptp` or 
`./gkc jsexample1.js -clausify -tptp`. Similarly one can convert the simple or TPTP 
syntax examples to JSON by using the key `-jsont` instead of `-tptp`.


### Core examples  

Examples `jsexample1.js` ... `jsexample6.js` illustrate the basic core of the 
JSON-LD-LOGIC with the clear correspondence to the simple logic syntax used in
most of the previous non-JSON examples.

It is best, although not obligatory, to run the examples with the `-json` parameter
telling gkc to output proofs in the JSON format like this:

    ./gkc jsexample1.js -json

with the `jsexample1.js` being:

    [
    ["father","john","pete"],
    ["father","pete","mark"],
    ["forall", ["X","Y","Z"], [[["father","X","Y"], "&", ["father","Y","Z"]], "=>", ["grandfather","X","Z"]]],

    // negated question: does there exist somebody who has john as a grandfather?
    ["forall", ["X"], ["not",["grandfather","john","X"]]]
    ]

giving output:    

    {"result": "proof found",

    "answers": [
    {
    "proof":
    [
    [1, ["in", "frm_3"], [["-father","?:X","?:Y"], ["-father","?:Z","?:X"], ["grandfather","?:Z","?:Y"]]],
    [2, ["in", "frm_2"], [["father","pete","mark"]]],
    [3, ["mp", 1, 2], [["-father","?:X","pete"], ["grandfather","?:X","mark"]]],
    [4, ["in", "frm_1"], [["father","john","pete"]]],
    [5, ["in", "frm_4"], [["-grandfather","john","?:0"]]],
    [6, ["mp", 3, 4, 5], false]
    ]}
    ]}

Important to notice in the core examples:

* Strings prefixed by `?:` like `?:X` are free variables implicitly assumed 
  to be quantified by "forall". Upper/lower case of the first character does not
  play any role here.

* Although comments are not a part of the JSON syntax, gkc accepts the use 
  of both line comments `//` and multi-line comments `/* ... */` in JSON.

* The `["if" ... "then" ....]` construction is a convenient alternative to implication:
  a list of formulas before "then" is treated as a conjunction premiss of the implication
  and the list after "then" as a disjunction consequent of the implication. 

  For example, `["if", "a", "b", "c", "then", "d", "e"]` is translated as 
  `[["a","&","b","&","c"], "=>", ["d","|","e"]]`.


### Full JSON-LD-LOGIC examples

Examples `jsexample7.js` ... `jsexample13.js` illustrate the specifics of JSON-LD
interpretation/conversion and built-in datatypes.

`jsexample7.js` illustrates the use of the special `$arc` predicate to translate
JSON-LD objects/maps and the convenience key `"@question"` converting the
value to an implication using the `"$ans"` predicate:

    [
    {"@id":"pete", "father":"john"},
    {"@id":"mark", "father":"pete"},
    {"@name":"gfrule",
    "@logic": ["if", ["$arc","?:X","father","?:Y"],["$arc","?:Y","father","?:Z"], "then", ["$arc","?:X","grandfather","?:Z"]]
    },
    {"@question": ["$arc","?:X","grandfather","john"]}
    ]

`jsexample8.js` illustrates the use of JSON-LD objects/maps *inside* the `@logic` 
value and has exactly the same meaning as the previous example:

    [
    {"@id":"pete", "father":"john"},
    {"@id":"mark", "father":"pete"},
    {"@name":"gfrule",
    "@logic": ["if", {"@id":"?:X","father":"?:Y"}, {"@id":"?:Y","father":"?:Z"}, "then", {"@id":"?:X","grandfather":"?:Z"}]
    },
    {"@question": {"@id":"?:X","grandfather":"john"}}
    ]

`jsexample9.js` demonstrates the conversion of symbols by the JSON-LD `@context`,
the use of multiple values in `["mark","michael"]` and the JSON-LD `"@base"` and `"@type"` keys. 
See how the proof contains absolute names instead of the shorthand names of symbols
used in input.

`jsexample10.js` shows that nesting can be arbitrarily deep and nested objects/maps may 
 contain the `@logic` key at any level. The example contains a nested "child" value indicating
that the person we describe (with no id given) has two children with ages 10 and 2, 
but nothing more is known about them. We also know the person has a father `john`. 
The rules state the son/daughter and mother/father correspondence, define children 
with ages between 19 and 6 as schoolchildren and define both the maternal and paternal
grandfather. The question is to find a schoolchild with "john" as a grandfather. 

The proof contains new unique blank nodes `"_:crtd_..."` built for each object/map
without an explicit `"@id"` key. 

`jsexample11.js` introduces the mapping of JSON-LD *named graphs* to a four-argument
predicate `$narc` (named arc) and showcases a rule merging several named graphs into one.

`jsexample12.js` demonstrates the use of the *list* datatype stemming from the
JSON-LD `"@list"`key like in `"clients": {"@list":["a","b","c"]}`.

Have a look at the differences between `jsexample12.js` 

    [
    {
    "@id":"company1",
    "clients": {"@list":["a","b","c"]},
    "revenues": {"@list":[10,20,50,60]}
    },

    [["listcount","$nil"],"=",0],
    [["listcount",["$list","?:X","?:Y"]],"=",["+",1,["listcount","?:Y"]]],

    [["listsum","$nil"],"=",0],
    [["listsum",["$list","?:X","?:Y"]],"=",["+","?:X",["listsum","?:Y"]]],

    ["if",
    {"@id":"?:X","clients":"?:C","revenues":"?:R"},
    ["$greater",["listcount","?:C"],2], 
    ["$greater",["listsum","?:R"],100],
    "then", 
    {"@id":"?:X","@type":"goodcompany"} 
    ],

    {"@question": {"@id":"?:X","@type":"goodcompany"}}
  
and an analogous simple-syntax example `lists.txt`: 

    clients(company1,[a]).
    revenues(company1,[100,20,50,60]).
    clients(company2,[a,b,c]).
    revenues(company2,[10,20,50,60]).
    =(listcount([]),0).
    =(listcount($list(X0,X1)),listcount(X1)+1).
    =(listsum([]),0).
    =(listsum($list(X0,X1)),listsum(X1)+X0).
    (clients(X0,X1) & revenues(X0,X2) & $greater(listcount(X1),2) & $greater(listsum(X2),100)) 
      =>
      type(X0,goodcompany).
    type(X0,goodcompany) =>  $ans(X0).

The latter does *not* use the embedding of atoms using the special `$arc` predicate
or a special meaning of the `"@type"`key, but is otherwise almost the same, 
except that it adds a second company to the problem setting.

`jsexample13.js` uses distinct symbols prefixed with `#:` like `"#:person"`
corresponding to the simple and TPTP symbols surrounded by double quotes 
like `"person"`:


    [
    {
    "@id":"smith1",
    "@type":["#:person","baby"],
    "name":"John Smith"  
    },
    {
    "@id":"smith2",
    "@type":["#:dog","newborn"],  
    "name":"John Smith"  
    },
    ["if", 
    {"@id":"?:X","@type":"?:T1"},
    {"@id":"?:Y","@type":"?:T2"},
    ["?:T1","!=","?:T2"],
    "then", 
    ["?:X","!=","?:Y"]
    ],
    {"@question": ["smith1","!=","smith2"]}
    ]

Again, `jsexample13.js` is almost the same as the simple syntax example
`distinct.txt`, except for the lack of embedding into `$ans` and the special
meaning of `@type`:

    type(smith1,"person").
    type(smith1,baby).
    type(smith1,'John Smith').
    type(smith2,"dog").
    type(smith2,newborn).
    type(smith2,name,'John Smith').
    (type(X0,X2) & type(X1,X3) & X2!=X3) => X0!=X1.
    smith1=smith2.

