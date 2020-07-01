Examples for gkc 
================

We will first give a brief introduction to gkc input and output,
then look at several simple [introductory examples](#Introductory-examples)
and continue towards [more advanced use](#Advanced-examples). 

On Linux the pre-compiled executable is called gkc, on Windows gkc.exe
and on OS X (macOS) it is gkcosx. Please change the filename on OS X
to gkc or just use gkcosx in the following examples instead of gkc.

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
less memory like this:

    ./gkc example1.txt -mbsize 500

which would make it use only half a gigabyte of memory. 
The default for UNIX and Windows 64 is 5 gigabytes, i.e. -mbsize 5000

By default gkc uses four parallel processes on UNIX and a single
process on Windows. On UNIX you can tell it a number of parallel
processes to use like this (0 and 1 mean no parallel processes, any number
larger than 1 means this number of parallel processes):

    ./gkc example1.txt -parallel 0

On UNIX you can also force gkc to stop after N seconds by giving a time limit
parameter like this:

    ./gkc example1.txt -seconds 2


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
* [Examples with medium-hard blocks world problems](#Examples-with-medium-hard-blocks-world-problems)

These simple examples explain the input, output and basic
functioning of gkc. Run these yourself like

    ./gkc example1.txt

The simple logical language used in these examples is inspired
by the [Otter syntax](https://www.cs.unm.edu/~mccune/otter/). 
Gkc can also use other, richer input languages: more about these later.


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

Percentage character % is a line comment. 

The order of statements in the file is not really important, although it may cause minor
and essentially unpredictable differences in search behaviour. Essentially, gkc
creates and tries out its own search strategies, like SQL databases. It does not
follow a pre-determined predictable search strategy like PROLOG.

Output from gkc:

    result: proof found
    for example1.txt
    by run 2 fork 1 strategy {"max_seconds":1,"strategy":["unit"],"query_preference":1}

    proof:
     1: [in, axiom] -father(john,pete).
     2: [in, axiom] father(john,pete).
     3: [simp, 1, 2, fromaxiom] false

The output first indicates that the proof was found and then tells us
which search attempt with which search strategy found it. Gkc runs a large number
of search attempts with different search strategies both sequentially and in parallel:
run <N> indicates the number of attempt and fork <N> indicates which parallel process
found the proof. 

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
being a suitable continuation towards deeper understanding. However, the following examples are understandable without in-depth theoretical background.

Try to modify the file example1.txt by removing the statement `-father(john,pete).` and
run gkc again. After ca one second gkc will stop and output

    result: proof not found.
  
Although the task is trivial, gkc does not attempt to be clever and tries out a large number
of search strategies: this is why it takes a second to terminate. You can try running gkc
with the optional switch producing more information to see the whole process:

    ./gkc example1.txt -tptp 1

In case gkc does not understand the syntax of the input file it will give a json-formatted
error indicating a culprit line and piece of input like this:

    {"error": "syntax error, unexpected URI, expecting '.': file example1.txt place 'as' in line 3:
    foo bar ("}



###  Example 2 for answers

Like example 1, but we want to find a concrete person as an answer: we use the special
"$ans" predicate for this. Observe the "answer: $ans(pete). " line in the output
stemming from this answer predicate.

We are using a variable here: any word starting with a capital letter is considered
to be a variable. `X` is a variable in our example. You could also use, say `Something`
as a variable: it also starts with a capital letter. All the words starting with
a non-capital letter are constants, functions or predicates.

Vertical bar | is logical or. 

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
    by run 2 fork 1 strategy {"max_seconds":1,"strategy":["unit"],"query_preference":1}

    answer: $ans(pete).
    proof:
     1: [in, axiom] father(john,pete).
     2: [in, axiom] -father(john,X) | $ans(X).
     3: [mp, 1, 2, fromaxiom] $ans(pete).
   

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
    by run 4 fork 3 strategy {"max_seconds":1,"strategy":["negative_pref"],"query_preference":1}

    answer: $ans(mark).
    proof:
     1: [in, axiom] -father(X,Y) | -father(Z,X) | grandfather(Z,Y).
     2: [in, axiom] father(pete,mark).
     3: [mp, 1, 2, fromaxiom] -father(X,pete) | grandfather(X,mark).
     4: [in, axiom] father(john,pete).
     5: [mp, 3, 4, fromaxiom] grandfather(john,mark).
     6: [in, axiom] -grandfather(john,X) | $ans(X).
     7: [mp, 5, 6, fromaxiom] $ans(mark).


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
    by run 1 fork 0 strategy {"max_seconds":1,"strategy":["unit"],"query_preference":0}

    answer: $ans(mark) | $ans(mickey).
    proof:
     1: [in, axiom] -father(X,Y) | -father(Z,X) | grandfather(Z,Y).
     2: [in, axiom] -grandfather(john,X) | $ans(X).
     3: [mp, 1.2, 2, fromaxiom] -father(john,X) | -father(X,Y) | $ans(Y).
     4: [in, axiom] father(john,pete).
     5: [mp, 3, 4, fromaxiom] -father(pete,X) | $ans(X).
     6: [in, axiom] father(pete,mickey) | father(pete,mark).
     7: [mp, 5, 6.1, fromaxiom] father(pete,mickey) | $ans(mark).
     8: [in, axiom] father(john,pete).
     9: [mp, 7, 3.1, 8, fromaxiom] $ans(mark) | $ans(mickey).

Indeed, there is no way to give a definite answer, but gkc gives a correct answer indicating
that either mark or mickey is a grandson of john, or perhaps both are.

The [mp, 1.2, 2, fromaxiom] means that the 2th (numeration 0,1,2,...) literal in the clause at proof step 1 
was cut off with the first (0th) literal of the clause at proof step 2. In case the first literal is cut off,
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
    by run 3 fork 2 strategy {"max_seconds":1,"strategy":["negative_pref"],"query_preference":0}


    answer: $ans(pete).
    proof:
     1: [in, axiom] -father(X,Y) | ancestor(X,Y).
     2: [in, axiom] father(pete,mark).
     3: [mp, 1, 2, fromaxiom] ancestor(pete,mark).
     4: [in, axiom] -ancestor(X,mark) | $ans(X).
     5: [mp, 3, 4, fromaxiom] $ans(pete).


###  Example 6 for equalities and functions

Now we reformulate the whole thing with equalities and functions! 

father(john)=pete means, as expected, that pete is the father
of john and there can be no other fathers. If you also gave
father(john)=lucas this would make gkc to conclude that
pete and lucas are the same object, i.e. pete=lucas.

Importantly, two different constants are not considered inequal
by default (think of the constants as labels on objects: there could
be several different labels on one object):

    a!=b.

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
    by run 4 fork 3 strategy {"max_seconds":1,"strategy":["negative_pref"],"query_preference":1}

    answer: $ans(father(mark)).
    proof:
     1: [in, axiom] ancestor(father(X),X).
     2: [in, axiom] -ancestor(X,mark) | $ans(X).
     3: [mp, 1, 2, fromaxiom] $ans(father(mark)).

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
    by run 3 fork 2 strategy {"max_seconds":1,"strategy":["negative_pref"],"query_preference":0}

    answer: $ans(father(father(mark))).
    proof:
     1: [in, axiom] grandfather(father(father(X)),X).
     2: [in, axiom] -grandfather(X,mark) | $ans(X).
     3: [mp, 1, 2, fromaxiom] $ans(father(father(mark))).

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
    by run 1 fork 0 strategy {"max_seconds":1,"strategy":["unit"],"query_preference":0}

    proof:
     1: [in, axiom] -=(father(father(john)),mark).
     2: [in, axiom] =(father(john),pete).
     3: [in, axiom] =(father(pete),mark).
     4: [simp, 1, 2, 3, fromaxiom] false
   
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
    by run 2 fork 1 strategy {"max_seconds":1,"strategy":["unit"],"query_preference":1}

    answer: $ans(mark).
    proof:
     1: [in, axiom] -=(father(father(john)),?0) | $ans(?0).
     2: [in, axiom] =(father(john),pete).
     3: [in, axiom] =(father(pete),mark).
     4: [simp, 1, 2, 3, fromaxiom] -=(mark,X) | $ans(X).
     5: [r=, 4.0, fromaxiom] $ans(mark).

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

    ./gkc example10.txt multineg.txt
    
Output:

    result: proof found
    for example10.txt

    answers and proofs:

    answer: $ans(mike).
    proof:
     1: [in, axiom] =(father(mike),pete).
     2: [in, axiom] grandfather(father(father(X)),X).
     3: [=, 1, 2.0.2, fromaxiom] grandfather(father(pete),mike).
     4: [in, axiom] =(father(pete),mark).
     5: [simp, 3, 4, fromaxiom] grandfather(mark,mike).
     6: [in, axiom] -grandfather(mark,X) | $ans(X).
     7: [mp, 5, 6, fromaxiom] $ans(mike).

    answer: $ans(john).
    proof:
     1: [in, axiom] =(father(john),pete).
     2: [in, axiom] grandfather(father(father(X)),X).
     3: [=, 1, 2.0.2, fromaxiom] grandfather(father(pete),john).
     4: [in, axiom] =(father(pete),mark).
     5: [simp, 3, 4, fromaxiom] grandfather(mark,john).
     6: [in, axiom] -grandfather(mark,X) | $ans(X).
     7: [mp, 5, 6, fromaxiom] $ans(john).

The [=, 1, 2.0.2, fromaxiom] means that the clause at step 1 was used to replace a 2th subterm
(numbering 0,1,2,...) of the 0th atom of a clause at step 2 using equality.

Observe that the two answers have (obviously) different proofs.

Importantly, gkc will not really attempt to find two different `$ans(...)` answers,
but two different proofs, which could (or not, as in our case) actually lead to
the same `$ans(...)` answers.

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

The `simp` rule also replaces a part of a premiss, but does so in a `permanent` manner,
meaning that the original unchanged premiss is not used in the search after
the simplification replacement.

    result: proof found
    for algebra.txt
    by run 3 fork 2 strategy {"max_seconds":1,"strategy":["unit"],"query_preference":1,"weight_select_ratio":20}

    proof:
     1: [in, axiom] =(m(X,i(X)),e).
     2: [in, axiom] =(m(X,m(Y,Z)),m(m(X,Y),Z)).
     3: [=, 1, 2.0.3, fromaxiom] =(m(X3,e),m(m(X3,Y3),i(Y3))).
     4: [in, axiom] =(m(X,e),X).
     5: [simp, 3, 4, fromaxiom] =(X,m(m(X,Y),i(Y))).
     6: [=, 1, 5.0.3, fromaxiom] =(X,m(e,i(i(X)))).
     7: [=, 5.0.R, 5.0.3, fromaxiom] =(m(X,Y),m(X,i(i(Y)))).
     8: [in, axiom] -=(m(e,c),c).
     9: [=, 6.0.R, 7.0.4, 8, fromaxiom] false


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
nevertheless actually provable, try the fof-format problem LCL876+1
from the TPTP collection:

    ./gkc example12.txt
  
It will probably run until the time of about 8 minutes allocated 
by the automatic strategy selection runs out.

You can check out 

    ./gkc example12.txt -tptp 1
  
to see the progress.

Of course, it is likely that with a suitable search strategy
gkc will find a proof in a sensible amount of time and memory. Currently
we are simply unaware of which strategy it would be!

Using a time limit like

    ./gkc example12.txt -seconds 20
  
is useful to avoid very long runs.

###  Examples with medium-hard blocks world problems 

[Blocks world](https://en.wikipedia.org/wiki/Blocks_world)
is a classic family of toy problems: there is a robot arm
able to lift single blocks and to put them on top of other blocks.

The goal is to find a sequence of robot arm movements to produce
a required configuration of blocks: for example, a tower.

Without a specialized search strategy these planning problems
tend to be surprisingly hard for provers, including gkc.

Try out progressively harder versions of the blocks world problem:

    ./gkc blocks1.txt
    ./gkc blocks2.txt
    ./gkc blocks3.txt
    ./gkc blocks4.txt
    ./gkc blocks5.txt

All of these are modified versions of each other: some goals
at the end of the file are commented out while one is kept.

Look into any of these files to see an explanation of what
facts and rules there are and how is the task encoded.
  
The first and second problems are easy, third and fifth should take a few seconds,
the fourth ca one minute. The only difference between the third and fourth
is that the fourth asks for an actual answer, while the third only asks 
whether an answer exists. Clearly there is room for improving the answer-finding
efficiency of gkc!

In the [advanced examples chapter](#Advanced-examples)we will look at guiding gkc search by using
a strategy selection file. A suitable simple strategy for the third example 
is given in the file blocks_strat2.txt:

    {
      "print_level": 10,     
      "max_seconds": 5,
      "strategy":["query_focus"],
      "query_preference": 2
    } 

and for the fifth in the file blocks_strat3.txt:

    {
      "max_seconds":5,
      "strategy":["query_focus"],
      "query_preference":1
    } 

Using these as

    ./gkc blocks3.txt blocks_strat2.txt -parallel 0
    ./gkc blocks5.txt blocks_strat3.txt -parallel 0

gives us proofs in ca 50 milliseconds and 300 milliseconds, respectively.

Looking at the output of the fourth we see
that it uses a strategy {"max_seconds":25,"strategy":["unit"],"query_preference":1}
which we can put into our strategy file blocks_strat.txt 

    {
      "max_seconds":25,
      "strategy":["unit"],
      "query_preference":0
    }

to get the proof in ca eight seconds:

    ./gkc blocks4.txt blocks_strat.txt -parallel 0

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
* [Telling gkc what is actually the question clause in input](#Telling-gkc-what-is-actually-the-question-clause-in-input)
* [Directing the prover and changing the settings](#Directing-the-prover-and-changing-the-settings)
* [Example with json output](#Example-with-json-output)
* [Examples with arithmetic](#Examples-with-arithmetic)
* [Large theory batch files](#Large-theory-batch-files)

Several examples will use the fof (first order formula) syntax used by
the TPTP project:

* [TPTP project](http://www.tptp.org/)
* [fof syntax](http://tptp.org/TPTP/TR/TPTPTR.shtml#FormulaeSection)
* [fof example](http://www.tptp.org/cgi-bin/SeeTPTP?Category=Problems&Domain=SYN&File=SYN000+1.p)

### Whitespace variables numbers encodings 

This is not a really advanced topic, but important.

A symbol cannot, by default, contain any whitespace or special characters used
in the syntax like (, -, ~, =, ", comma etc. 

You can put whitespace or any symbol except a single quote
into symbols by surrounding the symbol with single quote symbols like this:

    'john smith'
  
which is treated as a constant, whereas 'John smith' will be treated
as a variable, since it still starts with a capital letter, even though
quoted.

Any symbol containing a character except an ascii letter, digit, underscore _,
or dollar $ will be printed out by surrounding it with single quotes.
As an exception, equality = and aritmetic expressions +, *, -, /, < will 
not be surrounded by quotes. 

Additionally you can make a symbol variable by prefixing it with a question
mark like this:

    ?smth

As said before, any symbol starting with a capital letter or a question mark 
is assumed to be a variable, and the rest are not. 

This holds for both the simple syntax in the previous examples and the 
fof formulas to be described next. 

Thus, in fof formulas a capital-letter-starting symbol is a variable even if
it is not explicitly quantified: since this will easily be confusing, it
is better to avoid such symbols unless they are explicitly quantified.

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
simple syntax as in example13.txt:

    include('steam_kb.txt'). % this is an include statement

    fof(a1, axiom, ( ! [X] : (wolf(X) => dangerous(X)))). % this is fof
    cnf(a2, axiom, (~dangerous(X) | ~good(X))). % this is cnf

    good(X). % this is a simple syntax (saying that everything is good)

Running ./gkc example13.txt will output:

    result: proof found
    for example13.txt
    by run 3 fork 2 strategy {"max_seconds":1,"strategy":["unit"],"query_preference":1}

    proof:
    1: [in,a1, axiom] -wolf(X) | dangerous(X).
    2: [in,a2, axiom] -good(XX0) | -dangerous(XX0).
    3: [in, axiom] good(XX0).
    4: [simp, 2, 3, fromaxiom] -dangerous(X).
    5: [in,$imp::pel47_1_2, extaxiom] wolf($sk7).
    6: [mp, 1.1, 4, 5, fromaxiom] false

where `[in,$imp::pel47_1_2, extaxiom]` indicates that the statement with the
name `pel47_1_2` was in one of the imported files. 

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
TPTP and written in fof syntax with connectives like implication =>, quantifiers forall ! [X] ..,
exists ? [X] ... etc. 
See the official [fof syntax](http://tptp.org/TPTP/TR/TPTPTR.shtml#FormulaeSection) and
[fof example](http://www.tptp.org/cgi-bin/SeeTPTP?Category=Problems&Domain=SYN&File=SYN000+1.p)
in [TPTP](http://www.tptp.org).

Gkc will first convert the complex formulas to a simple clause form (properly called
clause normal form) used in the previous examples. The statements we had in these
examples are called `clauses`. 

The first part of the steam.txt file contains comments describing the problem.
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

Each statement is terminated with a period symbol . and has a structure

    fof(statement_name, statement_role, statement).
  
where the statement_name will be used in the proof, the statement_role indicates whether
it is an axiom, an assumption or hypothesis, or a goal to be proved from these: the latter is either
conjecture (which has to be negated) or negated_conjecture (negated already).

Indicating the role enables provers to find a more suitable strategy. It does not (except the
conjecture case, which has to be negated) have a `logical` meaning.
     
The first example statement above is universally quantified (! symbol) and will be converted by gkc to a clause

    -wolf(X) | animal(X).
  
The second statement is existentially quantified (? symbol) and will be converted by gkc to a clause

    wolf($sk7).

where $sk7 is a new constant invented by gkc which should not occur in any other formula in the problem:
this procedure is called [Skolemization](https://en.wikipedia.org/wiki/Skolem_normal_form).
Gkc always uses the $sk prefix for such constants and functions, using a new number N for each
new one. The original formula is assumed not to contain $skN form symbols.

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
    for steam.p
    by run 1 strategy {"max_seconds":1,"strategy":["negative_pref"],"query_preference":1}

    proof:
     1: [in,pel47_7, axiom] -much_smaller(X,Y) | -eats(X,Z) | eats(Y,U) | eats(Y,X) | -plant(U) | -plant(Z) | -animal(X) | -animal(Y).
     2: [in,pel47_6_2, axiom] -grain(X) | plant(X).
     3: [in,pel47_6_1, axiom] grain($sk2).
     4: [mp, 2, 3, fromaxiom] plant($sk2).
     5: [mp, 1.4, 4, fromaxiom] -much_smaller(X,Y) | -eats(X,Z) | eats(Y,$sk2) | eats(Y,X) | -plant(Z) | -animal(Y) | -animal(X).
     6: [in,pel47_14, axiom] plant($sk1(X)) | -snail(X).
     7: [in,pel47_5_2, axiom] snail($sk3).
     8: [mp, 6.1, 7, fromaxiom] plant($sk1($sk3)).
     9: [mp, 5.4, 8, fromaxiom] -eats(X,$sk1($sk3)) | -much_smaller(X,Y) | eats(Y,$sk2) | eats(Y,X) | -animal(Y) | -animal(X).
     10: [in,pel47_14, axiom] eats(X,$sk1(X)) | -snail(X).
     11: [mp, 10.1, 7, fromaxiom] eats($sk3,$sk1($sk3)).
     12: [in,pel47_5_1, axiom] -snail(X) | animal(X).
     13: [mp, 12, 7, fromaxiom] animal($sk3).
     14: [mp, 9, 11, 13, fromaxiom] -much_smaller($sk3,X) | eats(X,$sk2) | eats(X,$sk3) | -animal(X).
     15: [in,pel47_8, axiom] much_smaller(X,Y) | -snail(X) | -bird(Y).
     16: [mp, 15.1, 7, fromaxiom] much_smaller($sk3,X) | -bird(X).
     17: [in,pel47_3_2, axiom] bird($sk5).
     18: [mp, 16.1, 17, fromaxiom] much_smaller($sk3,$sk5).
     19: [in,pel47_3_1, axiom] -bird(X) | animal(X).
     20: [mp, 19, 17, fromaxiom] animal($sk5).
     21: [mp, 14, 18, 20, fromaxiom] eats($sk5,$sk3) | eats($sk5,$sk2).
     22: [merge, 1.4.5, fromaxiom] -much_smaller(X,Y) | -eats(X,Z) | eats(Y,Z) | eats(Y,X) | -plant(Z) | -animal(Y) | -animal(X).
     23: [mp, 22.4, 4, fromaxiom] -much_smaller(X,Y) | -eats(X,$sk2) | eats(Y,$sk2) | eats(Y,X) | -animal(Y) | -animal(X).
     24: [mp, 21.1, 23.1, 20, fromaxiom] eats($sk5,$sk3) | -much_smaller($sk5,X) | eats(X,$sk2) | eats(X,$sk5) | -animal(X).
     25: [in,pel47_9, axiom] much_smaller(X,Y) | -bird(X) | -fox(Y).
     26: [mp, 25.1, 17, fromaxiom] much_smaller($sk5,X) | -fox(X).
     27: [in,pel47_2_2, axiom] fox($sk6).
     28: [mp, 26.1, 27, fromaxiom] much_smaller($sk5,$sk6).
     29: [in,pel47_2_1, axiom] -fox(X) | animal(X).
     30: [mp, 29, 27, fromaxiom] animal($sk6).
     31: [mp, 24.1, 28, 30, fromaxiom] eats($sk6,$sk5) | eats($sk5,$sk3) | eats($sk6,$sk2).
     32: [in,pel47_13, axiom] -eats(X,Y) | -snail(Y) | -bird(X).
     33: [mp, 32.1, 7, fromaxiom] -eats(X,$sk3) | -bird(X).
     34: [in,pel47_3_2, axiom] bird($sk5).
     35: [mp, 31.1, 33, 34, fromaxiom] eats($sk6,$sk5) | eats($sk6,$sk2).
     36: [mp, 35.1, 23.1, 30, fromaxiom] eats($sk6,$sk5) | -much_smaller($sk6,X) | eats(X,$sk2) | eats(X,$sk6) | -animal(X).
     37: [in,pel47_10, axiom] much_smaller(X,Y) | -fox(X) | -wolf(Y).
     38: [mp, 37.1, 27, fromaxiom] much_smaller($sk6,X) | -wolf(X).
     39: [in,pel47_1_2, axiom] wolf($sk7).
     40: [mp, 38.1, 39, fromaxiom] much_smaller($sk6,$sk7).
     41: [in,pel47_1_1, axiom] -wolf(X) | animal(X).
     42: [mp, 41, 39, fromaxiom] animal($sk7).
     43: [mp, 36.1, 40, 42, fromaxiom] eats($sk7,$sk6) | eats($sk6,$sk5) | eats($sk7,$sk2).
     44: [in,pel47, goal] -eats(X,Y) | -eats(Z,X) | -grain(Y) | -animal(X) | -animal(Z).
     45: [mp, 3, 44.2, fromgoal] -eats(X,Y) | -eats(Y,$sk2) | -animal(X) | -animal(Y).
     46: [mp, 21.1, 45.1, 20, fromgoal] eats($sk5,$sk3) | -eats(X,$sk5) | -animal(X).
     47: [mp, 43.1, 46.1, 30, fromgoal] eats($sk7,$sk6) | eats($sk7,$sk2) | eats($sk5,$sk3).
     48: [mp, 47.2, 33, 34, fromgoal] eats($sk7,$sk6) | eats($sk7,$sk2).
     49: [in,pel47_11, axiom] -eats(X,Y) | -grain(Y) | -wolf(X).
     50: [mp, 49.1, 3, fromaxiom] -eats(X,$sk2) | -wolf(X).
     51: [in,pel47_1_2, axiom] wolf($sk7).
     52: [mp, 48.1, 50, 51, fromgoal] eats($sk7,$sk6).
     53: [in,pel47_11, axiom] -eats(X,Y) | -fox(Y) | -wolf(X).
     54: [mp, 53.1, 27, fromaxiom] -eats(X,$sk6) | -wolf(X).
     55: [mp, 52, 54, 51, fromgoal] false

The proof contains only clauses created from the formulas: the original names of formulas used are indicated 
like

    [in,pel47_1_2, axiom] wolf($sk7).


Since the steamroller puzzle is a fairly easy problem for provers, several different search strategies
are able to solve it very quickly. Thus, if you run the gkc without the -parallel 0 parameter on UNIX,
four parallel processes are created with different search strategies, which start to run at approximately
the same time and it sometimes happens that one finishes first, while at other times another one may 
finish first. Thus you may get a different output in a somewhat unpredictable manner, for example

    result: proof found
    for steam.p
    by run 3 fork 2 strategy {"max_seconds":1,"strategy":["unit"],"query_preference":1}
    ...

or

    result: proof found
    for steam.p
    by run 4 fork 3 strategy {"max_seconds":1,"strategy":["negative_pref"],"query_preference":0}
    ...

where `run N` indicates the strategy used (numeration 1,2,...) and `fork M` indicates which parallel
process found it (numeration 0,1,2,..). The run number N and the fork number M have a strict
correspondence: the order by which runs are given to forks is pre-determined.

The proofs found by different strategies are likely also somewhat different.

As said before, gkc implements parallel processes using forks and only for UNIX, i.e. Linux and OS X.


###  Example for steam with a shared memory database

Examples in this section behave somewhat differently on Linux and
Windows, and do not function on OS X (maxOS): more about this below.

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

    ./gkc -provekb steam_query.txt

Observe that even for such a small problem, the proof will be found faster,
in a few milliseconds, as opposed to ca 70 milliseconds for the original
proof without the prepared memory database.

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
    ./gkc -provekb steam_query.txt -mbnr 1001
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

    ./gkc steam.txt -tptp 1
  
which will both use the TPTP format for proofs and will also write
more information about the problem, strategies selected, search process
and statistics. The latter are not really required by the TPTP format, but
gkc will add these anyway.

We will not write the whole output here. The parts are:

* Statistics about the clauses obtained by conversion.
* Selected strategies to be run in json format.
* Running overview of proof search, just to follow what is happening.
* Proof in the TPTP format.
* Statistics about the successful search.

The proof itself has the following structure:

    result: proof found
    by run 1 strategy {"max_seconds":1,"strategy":["negative_pref"],"query_preference":1}
    % SZS status Theorem for steam.p.

    % SZS output start CNFRefutation for steam.p
    ..

    fof('pel47_6_2_$sk', plain, (plant(X1) | ~grain(X1)),
      inference(negpush_and_skolemize,[],['pel47_6_2'])).
    fof('pel47_6_2', axiom, (! [X1] : (grain(X1) => plant(X1))),
      input). 
    fof('pel47_6_1_$sk', plain, grain($sk2),
      inference(negpush_and_skolemize,[],['pel47_6_1'])).
    fof('pel47_6_1', axiom, (? [X] : grain(X)),
      input). 
    ...
    cnf('2', plain, (~grain(X) | plant(X)),
      inference(cnf_transformation,[],['pel47_6_2_$sk'])).
    cnf('3', plain, (grain($sk2)),
      inference(cnf_transformation,[],['pel47_6_1_$sk'])).
    ...
    cnf('11', plain, (eats($sk3,$sk1($sk3))),
      inference(resolution,[],['10','7'])).
    cnf('12', plain, (~snail(X) | animal(X)),
      inference(cnf_transformation,[],['pel47_5_1_$sk'])).  
    ...
    cnf('55', plain, ($false),
      inference(resolution,[then_simplify],['52','54','51'])).
    % SZS output end CNFRefutation for steam.p

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

These `negated_conjecture` and 'assumption` roles were already
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

    ./gkc example1.txt strat.txt

There are two somewhat different ways to write the strategy file:
either indicate a single strategy (single run) like in the previous example,
or multiple runs like this:

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

    ./gkc example1.txt -tptp 1
  
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
*  "print_level": integer determining the level of output: useful values are between 0 and 50, default 10, for -tptp 1 gkc uses 15.
*  "print_json": 0 or 1, where 0 is default and 1 forces json output.
*  "print_tptp": 0 or 1, where 0 is default and 1 forces tptp-style proof output
*  "max_size", "max_length", "max_depth", "max_weight" indicate limits on kept clauses, defaults are 0.
*  "equality" : 1 or 0, with 1 being default and 0 prohibiting equality handling.
*  "rewrite" : 1 or 0, with 1 being default and 0 prohibiting using equations for rewriting.
*  "max_seconds": N being an integer limit of seconds for one run, default 0 (no limit).
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

### Example with json output 

Json output in gkc is experimental and will not always produce a correct result.
Gkc will produce json output if the strategy file contains a setting

    "print_json": 1

The clauses in answers and proofs will be still presented as
the simple language strings, not as json structures.

NB! The tptp language setting does not match well with the "print_json": 1
setting: using these together will produce a messy output, neither correct json
nor tptp output.

Given a multineg_json.txt file:
  
    {
    "max_answers":2,
    "print_json": 1,
    "strategy": ["negative_pref"]
    }

the previous example run

    ./gkc example3.txt multineg_json.txt

will output a json structure:

    {"result": "proof found",

    {"answers": [
    [
    {"answer": "$ans(mark)."},
    {"proof":
    [
    [1, ["in", "axiom"], "-father(X,Y) | -father(Z,X) | grandfather(Z,Y)."],
    [2, ["in", "axiom"], "father(pete,mark)."],
    [3, ["mp", 1, 2, "fromaxiom"], "-father(X,pete) | grandfather(X,mark)."],
    [4, ["in", "axiom"], "father(john,pete)."],
    [5, ["mp", 3, 4, "fromaxiom"], "grandfather(john,mark)."],
    [6, ["in", "axiom"], "-grandfather(john,X) | $ans(X)."],
    [7, ["mp", 5, 6, "fromaxiom"], "$ans(mark)."]
    ]}
    ]
    ]}
    }


###  Examples with arithmetic 

Gkc supports integer and floating point arithmetic, but only in
a fairly limited and experimental way: do not be surprised if
something does not work as expected.

Arithmetic expressions may contain +, -, *, / and should be given 
in the infix form, like in
  
    p(1+2)
    p((2*X)+1)

while the single arithmetic comparison operation `<` except equality = 
should be given in the prefix form like

    <(X,2).
  
or negated like

    -<(X,2).

NB! In case a variable or a non-numeric constant is a first element of the 
arithmetic expression, you have to surround the variable or constant with single quotes,
like in

    p('X'*2)
  
otherwise the whole expression will be parsed as a single variable X*2.

First, a trivial example arithmetic0.txt:

    1=2.

As expected, gkc produces output:

    result: proof found
    for arithmetic0.txt
    by run 1 fork 0 strategy {"max_seconds":1,"strategy":["unit"],"query_preference":1}

    proof:
    1: [in, axiom] =(1,2).
    2: [simp, 1, fromaxiom] false

Observe that although it is obvious that 1!=2, ordinary non-numeric constants may
be equal even if different: 

    a=b.
    1=b.

does not lead to a contradiction!

Equality also works as expected when comparing floating point numbers:

    2.01!=2.01.

gives a contradiction, while a comparison of a floating point 2.0 and integer 2 does not:

    2.0!=2.

On the other hand, adding integer 1 to a floating point 1.0 gives a floating point 2.0
and the following input gives a contradiction:

    2.0!=1.0+1.

Both of the following comparison operations also produce a contradiction:

    <(2,2.0).
    <(2.0,2).

Next we will take up rules and calculations.

Given an input file arithmetic1.txt:

    p(1).
    -p(X) | p(1+X).
    -p(10).
  
gkc produces output:

    result: proof found
    for arithmetic1.txt
    by run 1 strategy {"max_seconds":1,"strategy":["unit"],"query_preference":0}

    proof:
     1: [in, axiom] p(+(1,X)) | -p(X).
     2: [in, axiom] p(1).
     3: [mp, 1.1, 2, fromaxiom] p(2).
     4: [mp, 3, 1.1, fromaxiom] p(3).
     5: [mp, 4, 1.1, fromaxiom] p(4).
     6: [mp, 5, 1.1, fromaxiom] p(5).
     7: [mp, 6, 1.1, fromaxiom] p(6).
     8: [mp, 7, 1.1, fromaxiom] p(7).
     9: [mp, 8, 1.1, fromaxiom] p(8).
     10: [mp, 9, 1.1, fromaxiom] p(9).
     11: [in, axiom] -p(10).
     12: [mp, 10, 1.1, 11, fromaxiom] false

Observe that the proof is given in the prefix form, which is NOT 
how gkc wants to see arithmetic input!

Given input file arithmetic2.txt (observe quotes around X!):

    p(1).
    -p(X) | p(('X'+1)*2).
    -p(X) | X!=10 | $ans(X).

gkc produces output:

    result: proof found
    for arithmetic2.txt
    by run 1 strategy {"max_seconds":1,"strategy":["unit"],"query_preference":0}

    answer: $ans(10).
    proof:
     1: [in, axiom] p(*(+(X,1),2)) | -p(X).
     2: [in, axiom] p(1).
     3: [mp, 1.1, 2, fromaxiom] p(4).
     4: [mp, 3, 1.1, fromaxiom] p(10).
     5: [in, axiom] -=(X,10) | -p(X) | $ans(X).
     6: [mp, 4, 5.1, fromaxiom] $ans(10).

Given input file arithmetic3.txt:

    p(2).
    -p(X) | p(2*X).
    -p(X) | <(X,128).

gkc produces output:

    result: proof found
    for arithmetic3.txt
    by run 1 strategy {"max_seconds":1,"strategy":["unit"],"query_preference":0}

    proof:
     1: [in, axiom] p(*(2,X)) | -p(X).
     2: [in, axiom] p(2).
     3: [mp, 1.1, 2, fromaxiom] p(4).
     4: [mp, 3, 1.1, fromaxiom] p(8).
     5: [mp, 4, 1.1, fromaxiom] p(16).
     6: [mp, 5, 1.1, fromaxiom] p(32).
     7: [mp, 6, 1.1, fromaxiom] p(64).
     8: [mp, 7, 1.1, fromaxiom] p(128).
     9: [in, axiom] <(X,128) | -p(X).
     10: [mp, 8, 9.1, fromaxiom] false

Given input file arithmetic4.txt using a floating point number:

    p(2.1).
    -p(X) | p(2*X).
    -p(X) | <(X,128).

gkc produces output:  

    result: proof found
    for arithmetic4.txt
    by run 1 strategy {"max_seconds":1,"strategy":["unit"],"query_preference":0}

    proof:
     1: [in, axiom] p(*(2,X)) | -p(X).
     2: [in, axiom] p(2.100000).
     3: [mp, 1.1, 2, fromaxiom] p(4.200000).
     4: [mp, 3, 1.1, fromaxiom] p(8.400000).
     5: [mp, 4, 1.1, fromaxiom] p(16.800000).
     6: [mp, 5, 1.1, fromaxiom] p(33.600000).
     7: [mp, 6, 1.1, fromaxiom] p(67.200000).
     8: [mp, 7, 1.1, fromaxiom] p(134.400000).
     9: [in, axiom] <(X,128) | -p(X).
     10: [mp, 8, 9.1, fromaxiom] false


Due to the experimental nature, arithmetic is automatically
switched off if the tptp proof format is selected, either by
the `-tptp 1` parameter or the `"print_tptp": 1` given in the
strategy file. 

The arithmetic syntax used by gkc does not currently conform
to the TPTP format. 

### Large theory batch files

Gkc is capable of time-efficiently handling a special format of 
packaging a large number of proof tasks into a single batch run file:
* [CASC J10 design: Problems](http://www.tptp.org/CASC/J10/Design.html#Problems)
* [CASC J10 design: System properties](http://www.tptp.org/CASC/J10/Design.html#SystemProperties)
contain a description of the format and specific requirements. 

In order to try out a simple example, do:

    mkdir out
    ./gkc largebatch.txt out

Here gkc determines automatically that the largebatch.txt
has a specific format and should be handled as a batch of
problems, with output files put into the out folder.

This capability is only available under UNIX.



