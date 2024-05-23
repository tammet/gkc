This folder contains relevant data and binaries for sharing experiments:

* gkc: the binary implementing sharing: see below for instructions.
* dummykb.txt: used for initializing the shared memory database
* rdiff.py: a python script to compare experimental logs below
* base_sequential_240s.txt: no sharing, no parallel processes, 240 seconds per problem, 500 problems
* base_parallel4_60s.txt: no sharing, 4 parallel processes, 60 seconds per problem, 500 problems
* base_parallel8_30s.txt: no sharing, 8 parallel processes, 30 seconds per problem, 500 problems
* base_parallel8_60s.txt: no sharing, 8 parallel processes, 60 seconds per problem, 500 problems
* sharedunits_parallel4_60s.txt: sharing unit, 4 parallel processes, 60 seconds per problem, 500 problems
* sharedunits_parallel4_10000limit_60s.txt: sharing unit, 4 parallel processes, max 10000 units shared, 60 seconds per problem, 500 problems
* sharedunits_parallel4_1000limit_60s.txt: sharing unit, 4 parallel processes,max 1000 units shared, 60 seconds per problem, 500 problems
* sharedunits_parallel4_100limit_60s.txt: sharing unit, 4 parallel processes,max 100 units shared, 60 seconds per problem, 500 problems
* sharedunits_parallel4_processed_60s.txt: sharing unit, 4 parallel processes, only processed units shared, 60 seconds per problem, 500 problems
* sharedunits_parallel8_1000limit_60s.txt: sharing unit, 8 parallel processes,max 1000 units shared, 60 seconds per problem, 500 problems
* sharedunits_parallel8_30s.txt: sharing unit, 8 parallel processes, 30 seconds per problem, 500 problems
* sharedunits_sequential_240s.txt: sharing unit, no parallelism, 240 seconds per problem, 500 problems
* sharedunitsdoubles_sequential_240s.txt: sharing unit and two-literal clauses, no parallel processes, 240 seconds per problem, 500 problems
* sharedunitsdboublesfull_1_hard_sequential.txt: sharing unit, two-literal and full clauses, ran on the 190 problems gkc failed to prove on CASC
* sharedunitsdboublesfull_2_hard_sequential.txt: same as previous, with a different set of full clause selection parameters
* sharedunitsdboublesfull_3_hard_sequential.txt: same as previous, with a different set of full clause selection parameters
* sharedunitsdboublesfull_4_hard_sequential.txt: same as previous, with a different set of full clause selection parameters

Log files of experiments
------------------------

The files base_sequential_240s.txt ... sharedunitsdboublesfull_4_hard_sequential.txt are the results of running the experimental version of gkc with 
different sharing and parallelism options on either all the 500 FOF problems of CASC from 2023, or just the set of 190 problems gkc could not prove during CASC.
The timing limits (either 60 or 240 seconds) were forced with the linux timeout utility.

A number of additional log files have been produced during the experiments, but since these were not relevant for the lessons learned, we have not
included these.

The log file lines corresponding to failed attempts look like this:

    GEO525+1,Failed,0,,60
  
The log file lines corresponding to successful attempts look like this:  

    GEO525+1,Proved,94,4.16,0.083529,{max_dseconds:1;strategy:[negative_pref];query_preference:0;depth_penalty:100;sine:1}
  
with the fields: problem name, result, which strategy found proof (1,2,3,....), overall time, running time of the proof-finding strategy run, 
the concrete strategy which found the proof.

Comparing log files
-------------------

The small python script rdiff.py can be used to compare log files (of possibly different sets of problems), like this:

    ./rdiff.py base_parallel4.txt sharedunits_parallel4.txt
  
The script prints out all these problems where one log file contains a proof and the other contains a failed proof attempt: 
the left log file line is printed first, followed by a corresponding right log file line. Finally the script prints out the number of proofs found in
the left log file, for which the right log contains a failed attempt, and analogously the number of proofs found in the right log file,
for which the left log contains a failed attempt. Example of the end of output of the call above:

    CSR249+1,Proved,205,35.04,2.200509,{max_dseconds:50;strategy:[hardness_pref;posunitpara];query_preference:1}
    CSR249+1,Failed,0,,60
    
    GEO525+1,Failed,0,,60
    GEO525+1,Proved,94,4.16,0.083529,{max_dseconds:1;strategy:[negative_pref];query_preference:0;depth_penalty:100;sine:1}
    
    leftmore:   16
    rightmore:  36


Running experimental gkc
------------------------

The experimental sharing-capable version of gkc in this folder can be run on linux only. The source for this binary can be found from the development branch dev7.
In order to run the sharing-capable version of gkc, you have to preload the shared memory database  
every time before running gkc, and then use a suitable set of keys for selecting the sharing type and parallelism.

Importantly, parallel running is OK only for base case without sharing or sharing unit clauses. For the -shareddouble and -sharedfullcl keys, locking has not
been implemented and thus these keys can be used only with the -parallel 0 key.

Example with no sharing and 4 parallel processes:

    ./gkc problemfile -parallel 0 -print 15

Example with shared units and 4 parallel processes:

    ./gkc -readkb dummykb.txt -mbsize 10000
    ./gkc problemfile -usekb -parallel 4 -sharedunits -print 15
  
Example with shared units, doubles and no parallelism:

    ./gkc -readkb dummykb.txt -mbsize 10000
    ./gkc problemfile -usekb -parallel 0 -sharedunits -shareddouble -print 15  
  
Example with shared units, doubles, full clauses and no parallelism:

    ./gkc -readkb dummykb.txt -mbsize 10000
    ./gkc problemfile -usekb -parallel 0 -sharedunits -shareddouble -sharedfullcl -print 15   
  


