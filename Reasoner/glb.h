/*
*
* Copyright (c) Tanel Tammet 2004-2019
*
* Contact: tanel.tammet@gmail.com                 
*
* This file is part of GKC
*
* GKC is free software: you can redistribute it and/or modify
* it under the terms of the GNU Affero General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
* 
* GKC is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU Affero General Public License
* along with GKC.  If not, see <http://www.gnu.org/licenses/>.
*
*/


 /** @file glb.h
 *  Reasoner globals.
 *
 */


#ifndef DEFINED_GLB_H
#define DEFINED_GLB_H

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <time.h>
#include <stdio.h>
#include <stdint.h>

#include "types.h"
#include "arithinst.h"


#define NROF_DYNALLOCINITIAL_ELS    10000000

#ifdef __EMSCRIPTEN__

#define NROF_QUEUE_TERMBUF_ELS      40000000 // 40 M of int els
#define NROF_HYPER_TERMBUF_ELS       5000000 // 5 M of int els                             
#define NROF_ACTIVE_TERMBUF_ELS     10000000 // 10 M of int els
#define NROF_GIVEN_TERMBUF_ELS       1000000 // 1 M of gint els
#define NROF_DERIVED_TERMBUF_ELS     1000000 // 1 M of gint els
#define NROF_SIMPLIFIED_TERMBUF_ELS  1000000 // 1 M of gint els

#else

#define NROF_GIVEN_TERMBUF_ELS      10000000 // 10 M of gint els
#define NROF_DERIVED_TERMBUF_ELS    10000000 // 10 M of gint els
#define NROF_SIMPLIFIED_TERMBUF_ELS 10000000 // 10 M of gint els

#ifdef _WIN32
#ifdef _WIN64
#define NROF_QUEUE_TERMBUF_ELS   200000000
#define NROF_HYPER_TERMBUF_ELS   100000000
#define NROF_ACTIVE_TERMBUF_ELS  100000000
#else
#define NROF_QUEUE_TERMBUF_ELS    20000000
#define NROF_HYPER_TERMBUF_ELS    10000000                              
#define NROF_ACTIVE_TERMBUF_ELS   20000000 
#endif
#else
#define NROF_QUEUE_TERMBUF_ELS   200000000  // 200 M of gint els
#define NROF_HYPER_TERMBUF_ELS   100000000  // 100 M of gint els
#define NROF_ACTIVE_TERMBUF_ELS  100000000  // 100 M of gint els
#endif
#endif

#define NROF_CUT_CLVEC_ELS   100
#define NROF_REWRITE_CLVEC_ELS  10000
#define NROF_HYPER_QUEUE_ELS 100000

#define NROF_VARBANKS   6 
#define NROF_VARSINBANK 1000
// bank 4: the rename area of the build (unreal variables)
#define RENAME_VARBANK 4
#define FIRST_UNREAL_VAR_NR (RENAME_VARBANK*NROF_VARSINBANK)
// bank 5: the variables of the discrimination tree (termindex.c); no term
// that the tree is asked about has a variable of this bank
#define TREE_VARBANK 5

#define NROF_WEIGHTQUEUE_ELS 200
#define NROF_CLTERM_HASHVEC_ELS       1000000 // was 1000000
#define NROF_ACTIVE_UNIT_HASHVEC_ELS  1000000 // was 1000000
#define NROF_CLTERM_HASHNODESVEC_ELS 10000000 // 10 M used if MALLOC_HASHNODES not set

#define NROF_ATOM_OCCURRENCES_HASHVEC_ELS 1000000



#define TERMHASH_INITIAL_BUCKETSIZE 4
#define ATOMHASH_INITIAL_BUCKETSIZE 4

#define MAX_CLAUSE_LEN 1000
#define INITIAL_SORTVEC_LEN 10
#define INITIAL_CLTMPVEC_LEN 10
#define INITIAL_VARTMPVEC_LEN 10
#define INITIAL_URITMPVEC_LEN 1000

#define INITIAL_ANSWERS_LEN 1000

#define MAX_CLPRIOR 10000 // priorities differentiated in a priority queue for picking clauses
/*
#define NROF_PRIORQUEUES 4
#define GOAL_
*/

#define MALLOC_HASHNODES // if defined, use malloc for hashnodes, else alloc from g->hash_nodes
//#undef MALLOC_HASHNODES

// prop

#define NROF_PROP_VARVALS_ELS 1000000
#define NROF_PROP_CLAUSES_ELS 1000000

// /opt/lingeling/lingeling --verbose=-1 -T 1 /opt/lingeling/tst2.txt > out.txt
// s UNSATISFIABLE
// s SATISFIABLE

#define DEFAULT_PROP_SOLVER_NAME "/opt/gkc/Prop/kissat-rel-3.0.0/build/kissat"
#define DEFAULT_PROP_FILE_NAME "/tmp/gkc_prop_inXXXXXX"
#define DEFAULT_PROP_SOLVER_OUTFILE_NAME "/tmp/gkc_prop_outXXXXXX"

#define MAX_FILENAME_LEN 80

#define MAX_GUIDETEXT_LEN 500

#define SINE_K_VALUES_SIZE 10000000 // bytestring 10 mbyte

// #define SHARED_DERIVED  // if set, keep some clauses over different passes

/* ======== Structures ========== */

/** glb contains global values for requests.
* 
* requests should use a single global variable g, which
* is a pointer to glb structure.
* 
*/

typedef struct glb {
  
  void* db;             /**< mem database used as a base for offset: either kb_db or child_db here */
  void* kb_db;          /**< shared-mem knowledge base */
  void* child_db;       /**< local child db: if kb_db present, then child_db->kb_db is set to this */
  gint db_offset;       /**< offset of the db ptr */
  gint inkb;            /**< 1 if g struct is inside shared kb, 0 otherwise */
  void* kb_g;            /**< if external shared mem kb_db is present, then the g ptr inside kb_db, else NULL */ 
  void* local_db;        /** this will be set to local db in any case in rmain.c */  
  char* filename;       /**< input filename */
  char* outfilename;       /**< output filename */

  /* === shared data block === */
  
  gint initial_cl_list; /**< offset of the initial clause list in a shared db */
  gint shared_clid_next; /** next id of the clause */

  cveco clbuilt;        /**< vector containing built clauses, newest last. 0: vec len, 1: index of next unused vec elem */
  cveco clactive;      /**< vector containing active clauses, newest last.  */
  cveco clactivesubsume; /**< vector containing active clauses with metainfo for subsumption */
  
  cveco clpickstack;   /**< vector containing built clause stack to be selected as given before using queue (hyperres eg) */
  cveco clqueue;        /**< vector containing kept clauses, newest last. 0: vec len, 1: index of next unused vec elem */
  gint clqueue_given;  /**< index of next clause to be taken from clqueue */     
  gint clpickpriorqueue;  /**< priority queue for picking clauses as as given, maybe interleaved with clqueue */

  gint clpick_queues;   /**< vec of several queue structures */
  gint clpick_given;    /**< index of next queue to be taken from vec of queues */

#ifdef SHARED_DERIVED    
  cveco shared_clbuilt;   /**< vector containing shared built clauses, newest last. 0: vec len, 1: index of next unused vec elem */
  gint shared_hash_neg_groundunits; /**< shared hash structure for cutting off with negative ground unit clauses */
  gint shared_hash_pos_groundunits; /**< shared hash structure for cutting off with positive ground unit clauses */
#endif

  gint hash_neg_groundunits; /**< hash structure for cutting off with negative ground unit clauses */
  gint hash_pos_groundunits; /**< hash structure for cutting off with positive ground unit clauses */

  gint hash_neg_grounddoubles; /**< hash structure for cutting off with negative ground double clauses */
  gint hash_pos_grounddoubles; /**< hash structure for cutting off with positive ground double clauses */
  gint hash_doublepairs; /**< two-literal clauses keyed by a pair of literals, for the double cut (64-bit hash builds) */
  gint doublepair_seq;   /**< insertion number of the next two-literal clause in hash_doublepairs */

  gint hash_neg_active_groundunits; /**< hash structure for subsuming with negative ground unit clauses in active */
  gint hash_pos_active_groundunits; /**< hash structure for subsuming with positive ground unit clauses in active */

  gint hash_atom_occurrences; /**< hash structure for atom occurrences, used for unit propagation */

  //veco clweightqueue;
  veco hash_neg_atoms;  /**< hash structure for all negative literals in active clauses inside all clauses  */
  veco hash_pos_atoms;  /**< hash structure for all positive literals in active clauses inside all clauses  */
  veco hash_neg_units;  /**< hash structure for all negative literals in unit active clauses  */
  veco hash_pos_units;  /**< hash structure for all positive literals in unit active clauses  */
  //veco hash_units;      /**< hash structure for: initialized, but not really used */
  veco hash_para_terms; /**< hash structure for paramodulating from: terms here will be unified with newly given eq clauses */
  veco hash_eq_terms;   /**< hash structure for paramodulation into: equality args here will be unified with given clause subterms */
  veco hash_rewrite_terms; /**< hash structure for left sides of rewrite rules (oriented equalities) */
 
#ifndef MALLOC_HASHNODES  
  cvec hash_nodes;      /**< area for allocating hash nodes from **/
#endif

  /* prop reasoning stuff */

  gint prop_counter; // set to the next prop integer to take: max prop var + 1
  int use_prop_constant; // normally set to 0: else 1 to use prop const for all vars
  gint prop_constant; // the constant to replace all vars for instantiation iff use_prop_constant!=0

  veco prop_hash_atoms; // hash struct for keeping ints corresponding to grounded atoms 
  veco prop_hash_clauses; // hash struct for keeping all derived clauses for equal-clause subsumption
  cveco prop_varvals; // array for assigned prop values (0 unknown, 1 true, -1 false)
  cveco prop_groundings; // array or atom / term pairs for all prop vars
  cveco prop_clauses; // array for all prop clauses worth keeping
  cveco prop_varval_clauses; // array for clauses for all assigned prop values, at same indexes

  char* prop_file_name; // file name to dump prop vars to
  char* prop_solver_name; // solver name to call
  char* prop_solver_outfile_name; // file name for solver to store the redil

  /* == local data block === */
       
  gint local_clid_next; /** next id of the clause */     
  gint proof_found;
  gptr proof_answer;
  gint proof_history;
  gint alloc_err; // set to 1 in case of alloc errors: should cancel search  
  
  vec varbanks; // 0: input (passive), 1: given clause renamed, 
                // 2: active clauses, 3: derived clauses, 
                // 4: tmp rename area (vals always UNASSIGNED, never set to other vals!)
  cvec varstack;                  

  cvec xcountedvarlist;
  cvec ycountedvarlist;
    
  //int tmp_build_weight;
  //vec tmp1_cl_vec;
  //vec tmp2_cl_vec;
  
  vec tmp_litinf_vec;  // used by subsumption and by prop instantiation
  vec tmp_hardnessinf_vec; // used by hardness calc
  vec tmp_resolvability_vec; // used for resolvability of literals
  vec tmp_sort_vec; // used for sorting the initial clause list

  cvec given_termbuf;
  cvec simplified_termbuf;
  cvec derived_termbuf;    
  cvec queue_termbuf;
  cvec hyper_termbuf;
  cvec active_termbuf;

  cvec cut_clvec; // used for storing cutters of a derived clause
  cvec rewrite_clvec; // used for storing rewriters of a clause
  cvec hyper_queue; // storing partially derived clauses during hyperres for picking as given
  cvec answers; // storing answer/proof pairs
  char* sine_k_values; // k-values bytestring indexed by clause id: each byte contains k 
  gint sine_k_bytes; // length of sine_k_values bytestring
  char* backsubsume_values;
  gint backsubsume_bytes; //
  char* sine_uri_k_values; // k-values bytestring indexed by uri id: each byte contains k 
  gint sine_uri_k_bytes; // length of sine_uri_k_values bytestring


  cvec tmp_clinfo; // used for storing additional clause info while ordering for knuth-bendix
  cvec tmp_varinfo; // used for storing additional var info while ordering for knuth-bendix 
  cvec tmp_uriinfo; // used for storing uri info in a clause while doing sine

  /* parser configuration */
  
  int parse_caps_as_var;
  int parse_question_as_var;
  int parse_newpred_strat; // introduce new predicates during distribution phase of clausification?
  int store_fof_skolem_steps; 
  int store_fof_source; 

  /* parser temporary vars set by parser */

  int parse_is_included_file;
  char* parse_skolem_prefix;  
  //int parse_skolem_nr; // moved to dballoc.h
  char* parse_newpred_prefix; 
  //int int parse_newpred_nr; // moved to dballoc.h
  char* parse_errmsg;
  int parse_errflag;
  
  /* unification/matching configuration */
  
  int unify_samelen;      // 1 if unifiable terms need not have same length, 0 otherwise
  int unify_maxuseterms;  // max nr of rec elems unified one after another: t1,t2,t3 gives 3
                          // 0 if no limit 
  int unify_firstuseterm; // rec pos where we start to unify 
  int unify_funpos;       // rec pos of a fun/pred uri
  int unify_funarg1pos;   // rec pos of a fun/pred first arg
  int unify_funarg2pos;   // rec pos of a fun/pred second arg
  int unify_footerlen;    // obligatory amount of unused gints to add to end of each created term 
  
  /* strategy selection */
  
  //int max_forks;
  int sine_strat; // 0 if none, 1 if weak, 2 if strong
  int sine_strat_used; // 0 if not actually used, 1 if used: set automatically by analyze if sine_strat!=0
  int sine_order_strat; // 0 if none, 1 if on
  int required_answer_nr;
  int pick_given_queue_ratio;
  int pick_given_queue_ratio_counter;
  int next_pick_given_queue_block_nr;

  int cl_depth_penalty;
  int cl_length_penalty;
  int var_weight;
  int repeat_var_weight;
  int atom_poseq_penalty;
  int use_max_ground_weight;
  int use_max_weight;  

  int cl_maxkeep_weightlimit;
  int cl_maxkeep_sizelimit;
  int cl_maxkeep_depthlimit;
  int cl_maxkeep_lengthlimit;

  int cl_pick_queue_strategy; // how to spread clauses between different queues: 0 
                              // 0: one only,
                              // 1: exactly as marked, 
                              // 2: make non-included axioms assumptions and positive conjecture part assumptions
                              // 3: only fully negative clauses of goal made goal and no assumptions (ie made axioms)

  int use_hornpref;
  int use_posweightdoublepref;
  int use_negweightdoublepref;

  /* dynamically changed limits */

  int max_run_seconds;
  int max_seconds;
  int max_run_dseconds;
  int max_dseconds;
  int max_given; // stop a run after this many given clauses, 0: no limit
  int queue_weight_bound; // experimental: with queue_weight_limit, drop a clause before its build by a lower bound of its weight
  int queue_weight_limit; // experimental weight limit per queue block: 0 off, 1 estimate from all picks, 2 from the recent picks, 3 as 2 but no unit is dropped, 4 as 3 and no two-literal clause, limit drops count in avg_kept_weight
  int cl_keep_weightlimit;
  int cl_keep_sizelimit;
  int cl_keep_depthlimit;
  int cl_keep_lengthlimit;
  
  int reverse_clauselist_strat; // initially reverse clause list, do not sort (only for non-query)
  int queryfocus_strat;
  int queryfocusneg_strat;
  int hyperres_strat;
  int relaxed_hyperres_strat;
  int goalweight_normal_strat; // if 1, normal initial weights are applied, else special low weights
  int unit_active_strat;    // if 1, make all units active for sos strategies
  int weightorder_strat;          // clause ordering for resolvability of literals
  int negpref_strat;          // clause ordering for resolvability of literals
  int pospref_strat;          // clause ordering for resolvability of literals
  int knuthbendixpref_strat;  // clause ordering for resolvability of literals
  int hardnesspref_strat;     // clause ordering for resolvability of literals
  int res_shortarglen_limit; // max non-ans len of the shortest res argument (generalization of unit)
  int res_arglen_limit; // if non-zero, do not resolve upon longer clauses and do not ever para
  int res_strict_arglen_limit; // if non-zero,  do para on units if res_arglen_limit<2
  int posunitpara_strat;
  int rewrite_only_strat; // if non-zero, do not store para terms: only store demodulators
  int instgen_strat;
  int propgen_strat;
  int back_subsume;
  int forward_subsume_derived;
  int propagate;
  int use_equality_strat; // general strategy
  int use_equality; // current principle
  int use_comp_funs_strat; // general strategy
  int use_comp_funs; // current principle
  int use_comp_arithmetic; // current principle for arithmetic
  int arith_instantiation; // 0 off, 1 conservative, 2 stronger
  int arith_inst_max_vars;
  int arith_inst_candidate_limit;
  int arith_inst_probe_limit;
  int arith_inst_keep_limit;
  int arith_inst_depth_limit;
  int arith_inst_global_limit;
  unsigned int arith_inst_explicit_mask;
  int have_nonground_arithmetic;
  int use_rewrite_terms_strat; // general strategy
  int have_rewrite_terms; // observation  
  int use_strong_unit_cutoff; 
  int strong_cut_strat; // strategy key strong_cut: -1 absent (the rule of max_seconds and max_dseconds), 0 off, 1 on
  int unit_variant_hash; // strategy key unit_variant_hash: 1 the unit tables hash with per-literal variable numbers and test a variant
  int use_strong_duplicates; // iff 1, then unique var based duplicate removal
  int prohibit_nested_para;  
  int prohibit_unordered_para;
  int prohibit_deep_para;


  int max_proofs;
  int store_history;
  int endgame_mode; 

  /*  printout configuration */
  

  int print_flag;
  int print_json;
  int print_tptp;
  int print_clauses_json;
  int print_clauses_tptp;
  int print_proof_tptp;
  int print_level_flag;
  int print_derived;
  int print_clause_history;
  int print_history_extra;  
  int print_fof_conversion_proof;
  
  int parser_print_level;
  int print_initial_parser_result;
  int print_generic_parser_result;
  
  int print_initial_active_list;
  int print_initial_passive_list;
  
  int print_given_interval_trace;
  int print_initial_given_cl;
  int print_final_given_cl;
  int print_active_cl;
  int print_litterm_selection;
  int print_partial_derived_cl;
  int print_derived_cl;
  int print_derived_subsumed_cl;
  int print_derived_precut_cl;
  
  int print_clause_detaillevel;
  int print_runs;
  int print_stats;
  int print_datastructs;
  int print_sine;
  
  /* tmp variables */
  
  gint* tmp_unify_vc;       // var count in unification
  gint  tmp_unify_occcheck; // occcheck necessity in unification (changes)
  gint  tmp_unify_do_occcheck;
  gint  tmp_rewrites; // count of rewrites during one clause simplification
  char* tmp_printbuf; // temporary buffer for printing, malloced as needed
  int arith_inst_probe_active;
  int tmp_arithinst_calc_count;
  int tmp_arithinst_deleted_lits;
  int tmp_arithinst_tautology;

  int arith_inst_global_candidate_count;
  wr_arith_value arith_inst_global_candidates[WR_ARITH_MAX_GLOBAL_VALUES];
  wr_arith_cache_entry* arith_inst_cache;
  int arith_inst_cache_size;
  int arith_inst_cache_saturated;

  /* build control: changed in code */
  
  int  build_rewrite; // current switch: to rewrite or not during building
  gint build_subst;    // subst var values into vars
  gint build_calc;     // do fun and pred calculations
  gint build_dcopy;    // copy nonimmediate data (vs return ptr)
  gptr build_buffer;   // build everything into tmp buffer (vs main area)
                       // points to NULL or given_termbuf, derived_termbuf etc
  
  gint build_rename;   // do var renaming
  gint build_rename_maxseenvnr; // tmp var for var renaming
  gint build_rename_vc;    // tmp var for var renaming
  gptr build_rename_bank;  // points to bank of created vars
  gint build_rename_banknr; // nr of bank of created vars  

  /* statistics */
  
  int stat_wr_mallocs;
  int stat_wr_callocs;
  int stat_wr_reallocs;
  int stat_wr_frees;
  gint stat_wr_malloc_bytes;
  gint stat_wr_calloc_bytes;
  gint stat_wr_realloc_bytes;
  gint stat_wr_realloc_freebytes;
  
  int stat_built_cl;
  int stat_derived_cl;
  int stat_derived_partial_hyper_cl;
  int stat_binres_derived_cl;
  int stat_instgen_derived_cl;
  int stat_arithinst_clauses_scanned;
  int stat_arithinst_frontiers;
  int stat_arithinst_candidates;
  int stat_arithinst_probes;
  int stat_arithinst_calculated;
  int stat_arithinst_rejected_no_progress;
  int stat_arithinst_rejected_tautology;
  int stat_arithinst_rejected_cache;
  int stat_arithinst_rejected_budget;
  int stat_arithinst_truncated;
  int stat_arithinst_kept;
  int stat_arithinst_proofs;
  int stat_prop_inst_derived_cl;
  int stat_propagated_derived_cl;
  int stat_factor_derived_cl;
  int stat_para_derived_cl;
  int stat_kept_cl;
  int stat_made_rewriters;
  
  int stat_hyperres_partial_cl;
  int stat_tautologies_discarded;
  int stat_weight_discarded_building;
  int stat_weight_discarded_cl;
  int stat_internlimit_discarded_cl;
  int stat_derived_overflow_dropped; // derived clauses dropped: did not fit into derived_termbuf
  int stat_queue_limit_dropped; // derived clauses dropped by the weight limit of their queue block
  int stat_queue_bound_dropped; // derived clauses dropped before their build by queue_weight_bound
  int derived_build_active;  // 1 while a wr_process_*_result builds its half-built clause
  int derived_build_overflow; // 1 when derived_termbuf overflowed during that build
  int derived_overflow_timeout; // 1 when the time limit passed at a drop: derivation stops
  gint* lithash_atoms; // atoms of the current derived clause whose literal hash is known
  gint* lithash_vals;  // their hashes, computed once in wr_derived_cl_cut_and_subsume
  gint* lithash_final; // the hashes by literal position of the final clause
  int lithash_n;       // number of pairs in lithash_atoms and lithash_vals
  int lithash_size;    // capacity of the three arrays
  int early_weight_known; // 1 when early_weight holds the weight values of the current clause
  int early_weight[4];    // weight, size, depth and length from the early weight check
  int stat_internlimit_discarded_para;
  int stat_given_candidates;
  int stat_given_candidates_hyper;
  int stat_given_candidates_at_endgame; 
  int stat_given_used;  
  int stat_given_used_at_endgame; 
  int stat_simplified;
  int stat_simplified_given;
  int stat_simplified_derived;
  int stat_backward_subsumed;
  int stat_propagated_subsumed;
  int stat_forward_subsumed;
  int stat_derived_cut;
  int stat_derived_rewritten;
  int stat_clsubs_attempted;
  gint stat_clsubs_top_meta_attempted;
  gint stat_clsubs_top_meta_failed;
  gint stat_clsubs_top_meta_nonpref_attempted;
  gint stat_clsubs_top_meta_nonpref_succeeded;
  gint stat_clsubs_top_meta_pref_attempted;
  gint stat_clsubs_top_meta_pref_succeeded;
  gint stat_clsubs_top_meta_pref1_succeeded;
  gint stat_clsubs_top_meta_pref2_succeeded;
  gint stat_clsubs_top_meta_pref3_succeeded;  
  int stat_clsubs_meta_attempted;
  int stat_clsubs_meta_failed;
  int stat_clsubs_predsymbs_attempted;
  int stat_clsubs_unit_attempted;
  int stat_clsubs_full_attempted;
  int stat_clsubs_fact_groundunit_found;
  int stat_clsubs_rule_groundunit_found;
  int stat_forwardsubs_attempted;
  int stat_lit_hash_added;
  int stat_lit_hash_computed;
  int stat_lit_hash_match_found;
  int stat_lit_hash_match_miss;
  int stat_prop_hash_match_miss;
  int stat_lit_hash_cut_ok;  
  int stat_lit_strong_cut_ok;
  int stat_lit_hash_subsume_ok;
  int stat_atom_hash_added;
  int stat_atom_hash_computed;
  int stat_atom_hash_match_found;
  int stat_atom_hash_match_miss;
  
  int log_level;

  int tmp1;
  int tmp2;
  int tmp3;
  int tmp4;
  int tmp5;

  int tmp6;
  int tmp7;
  int tmp8;
  int tmp9;

  /* input clause set analysis */

  int in_has_fof;
  int in_has_nonground_arithmetic;

  int in_clause_count;
  int in_rule_clause_count;
  int in_fact_clause_count;
  int in_answer_clause_count;
  int in_ground_clause_count;
  int in_unit_clause_count;
  int in_horn_clause_count;
  int in_pos_clause_count;
  int in_neg_clause_count;
  int in_poseq_clause_count;
  int in_negeq_clause_count;
  int in_unitposeq_clause_count;
  int in_chain_clause_count;
  int in_min_length;
  int in_max_length;
  int in_min_depth;
  int in_max_depth;
  int in_min_size;
  int in_max_size;
  int in_min_vars;
  int in_max_vars;
  float in_average_length;
  float in_average_depth;
  int in_predicate_count;
  int in_funsymb_count;
  int in_extaxiom_count;
  int in_axiom_count;
  int in_assumption_count;
  int in_goal_count;
  int in_neg_goal_count;
  int in_pos_goal_count;
  int in_posunit_goal_count;
  int in_max_const_ucount;
  gint in_max_occ_const;

  int sin_clause_count;
  int sin_rule_clause_count;
  int sin_fact_clause_count;
  int sin_answer_clause_count;
  int sin_ground_clause_count;
  int sin_unit_clause_count;
  int sin_horn_clause_count;
  int sin_pos_clause_count;
  int sin_neg_clause_count;
  int sin_poseq_clause_count;
  int sin_negeq_clause_count;
  int sin_unitposeq_clause_count;
  int sin_chain_clause_count;
  int sin_min_length;
  int sin_max_length;
  int sin_min_depth;
  int sin_max_depth;
  int sin_min_size;
  int sin_max_size;
  int sin_min_vars;
  int sin_max_vars;
  float sin_average_length;
  float sin_average_depth;
  int sin_predicate_count;
  int sin_funsymb_count;
  int sin_extaxiom_count;
  int sin_axiom_count;
  int sin_assumption_count;
  int sin_goal_count;
  int sin_neg_goal_count;
  int sin_pos_goal_count;
  int sin_posunit_goal_count;
  int sin_max_const_ucount;
  gint sin_max_occ_const;
  int sin_has_nonground_arithmetic;

  /* run averages and measures */
  
  int current_run_nr;
  int current_fork_nr;
  double avg_kept_weight;
  float passed_ratio;
  int qwl_active;          // 1 when qwl_weight and qwl_fifo_reach hold limits
  int qwl_weight[4];       // weight limit of each queue block, -1: none
  gint qwl_fifo_reach[4];  // first-in positions below this are reached
  gint qwl_next_update;    // pick count of the next computation of the limits
  double* qwl_samples;     // ring of (run seconds, picks) pairs for the recent pick rate, or NULL
  int qwl_nsamples;        // pairs in qwl_samples
  int qwl_sample_next;     // next pair to write in qwl_samples
  double qwl_first_pick_time; // run seconds at the first pick, -1 before it
  void* qwb_cache;         // literal weights of the parent clauses for queue_weight_bound, or NULL
  void* fsi_index;         // index of clactivesubsume for the forward subsumption of the picked clause, or NULL
  void* sci_index;         // index of the stored units for the strong cut, NULL, or 1 after a failed allocation
  clock_t allruns_start_clock;
  //clock_t allruns_end_clock;
  clock_t run_start_clock;
  //clock_t run_end_clock;  
  char guidetext[MAX_GUIDETEXT_LEN];

  /* unit propagation (Task 20); at the end of the struct, so that the
     offsets of the fields above stay as before */
  int unit_propagate; // strategy key unit_propagate: 0 off, 1 cut two-literal clauses, 2 cut all clauses
  int unit_backsubsume; // strategy key unit_backsubsume: 1 a kept unit makes clauses with an equal literal of the same sign unusable
  int stat_up_cut; // unit propagation: clauses cut
  int stat_up_derived; // unit propagation: shorter clauses built
  int stat_up_backsubsumed; // unit propagation: clauses made unusable by backward subsumption
  void* up_index;          // tables of unit propagation, or NULL
  int para_into_ordered;   // strategy key para_into_ordered: 1 the ordering test after the substitution also for paramodulation into the given clause
  void* dmi_index;         // index of the demodulators of hash_rewrite_terms, NULL, or 1 after a failed allocation
  int rewrite_guard;       // 1: wr_rewrite_term does not use a demodulator that matches by a renaming
  gptr rewrite_self;       // a clause that wr_rewrite_term does not use as a demodulator, or NULL
  int ti_selftest;          // strategy key ti_selftest: 1 the self test of the term index on the input, then stop
  int stat_dmi_tree_stopped; // the tree of the demodulators stopped: 1 no memory or too many variables in an insert, 2 no memory in a lookup, 3 unify settings
  int demod_derived;       // strategy key demod_derived: 1 a derived clause is rewritten with the demodulators before the cuts
  int demod_source;        // strategy key demod_source: 1 (absent) an equation becomes a demodulator when picked, 2 when kept
  int demod_skip_active;   // 1: the activation of the picked clause does not file it as a demodulator again (demod_source 2)
  int demod_this_clause;   // 1: the derived clause in work was rewritten; its demodulators are in rewrite_clvec
  long stat_demod_derived_checked;   // demod_derived: derived clauses tested
  long stat_demod_derived_rewritten; // demod_derived: derived clauses with a literal rewritten
  long stat_demod_derived_dropped;   // demod_derived: rewritten clauses dropped (s=s, or the buffer full)
  long stat_demod_filed_kept;        // demod_source 2: demodulators filed when kept
  gint demod_rw_stop;      // demod_derived: wr_rewrite_term stops at this count of tmp_rewrites, 0: no limit
  int demod_rw_hit;        // 1: the limit of rewrites stopped the rewriting of the clause in work
  long stat_demod_rw_limit; // demod_derived: clauses that reached the limit of rewrites (a defect: expected 0)
  int rw_base;             // 0, or the stack position above which wr_rewrite_term binds and clears (the rewriting of a derived clause)
  int demod_oldform;       // the check build of the one-pass normalisation: 1 the full rebuild
  gptr dmi_rhsbuf;         // the chunks of the right-side copies of the demodulator tree, or NULL
  long stat_dmi_rhs_not_filed; // demodulators not filed in the tree: a variable of the right side not in the left side
  long stat_dmi_rhs_bytes; // the bytes of the right-side copies in the tree bank
  long stat_demod_parents_lost; // demod_derived: derivation records without their demodulators (no room in cut_clvec)
  long stat_demod_neg_trivial;  // demod_derived: rewritten derived clauses with a negative literal s!=s
  long stat_demod_neg_trivial_unit; // the same, as the only literal (an empty clause)
  int unit_cut_match;      // strategy key unit_cut_match: 1 a stored unit of the opposite sign that matches a literal cuts it
  int unit_subsume_match;  // strategy key unit_subsume_match: 1 a stored unit of the same sign that matches a literal subsumes the clause
  int unit_match_equal;    // strategy key unit_match_equal: 1 the unit trees accept only what the unit tables accept
  void* ucm_pos;           // the tree of the positive stored units, NULL, or 1 after a failure
  void* ucm_neg;           // the tree of the negative stored units, NULL, or 1 after a failure
  gptr ucm_scratch;        // a record for the swapped sides of an equality literal
  int ucm_scratch_cap;     // its size in gints
  long stat_ucm_cut;       // unit_cut_match: literals cut by a unit that the tables did not find
  long stat_usm_subsumed;  // unit_subsume_match: clauses subsumed by a unit that the tables did not find
  long stat_ucm_pick_cut;      // unit_cut_match 2: literals of picked clauses cut by the tree
  long stat_usm_pick_subsumed; // unit_subsume_match 2: picked clauses subsumed by the tree
  gptr ucm_pick_self;         // the picked clause during its simplification (the trees leave it out)
  long stat_ucm_inserted;  // units filed in the two trees
  long stat_ucm_lookups;   // walks of the unit trees (two for an equality literal without a match)
  int stat_ucm_stopped;    // the unit trees stopped: 1 no memory, 3 unify settings
  int tree_compress;       // strategy key tree_compress: 1 the discrimination trees have compressed paths
  long stat_sci_match_lookups; // the strong cut on the tree: lookups by matching
  long stat_sci_match_found;   // ... that found a cutter
  long stat_sci_unify_lookups; // the strong cut on the tree: lookups by unification (the filter)
  long stat_sci_unify_cands;   // ... candidates of the filter
  long stat_sci_unify_tests;   // ... candidates tested with wr_unify_term
  long stat_sci_unify_ok;      // ... tests that passed
} glb;



/* === Protos for funs in glb.c === */


glb* wr_glb_new_full(void* db);
glb* wr_glb_new_simple(void* db);

int wr_glb_free(glb* g);

int wr_glb_init_simple(glb* g);

int wr_glb_init_shared_simple(glb* g);
int wr_glb_init_shared_complex(glb* g);
int wr_glb_init_seq_shared_complex(glb* g);
int wr_glb_free_shared_simple(glb* g);
int wr_glb_free_shared_complex(glb* g);

int wr_glb_init_local_simple(glb* g);
int wr_glb_init_local_complex(glb* g);
int wr_glb_init_seq_local_complex(glb* g);
int wr_glb_free_local_simple(glb* g);
int wr_glb_free_local_complex(glb* g);

void wr_print_glb_memarea(glb* g, gptr p);
void wr_check_cl_memarea(glb* g, gptr xptr);

void wr_hashlists_explore(glb* g);

#endif
