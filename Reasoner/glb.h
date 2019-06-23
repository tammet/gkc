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


#define NROF_DYNALLOCINITIAL_ELS    10000000

#define NROF_GIVEN_TERMBUF_ELS      10000000
#define NROF_DERIVED_TERMBUF_ELS    10000000
#define NROF_SIMPLIFIED_TERMBUF_ELS 10000000

#ifdef _WIN32
#define NROF_QUEUE_TERMBUF_ELS    50000000
#define NROF_HYPER_TERMBUF_ELS    10000000                              
#define NROF_ACTIVE_TERMBUF_ELS   50000000 
#else
#define NROF_QUEUE_TERMBUF_ELS   100000000
#define NROF_HYPER_TERMBUF_ELS   100000000
#define NROF_ACTIVE_TERMBUF_ELS  100000000
#endif

#define NROF_CUT_CLVEC_ELS   100
#define NROF_REWRITE_CLVEC_ELS  10000
#define NROF_HYPER_QUEUE_ELS 100000

#define NROF_VARBANKS   5 
#define NROF_VARSINBANK 1000
#define FIRST_UNREAL_VAR_NR ((NROF_VARBANKS-1)*NROF_VARSINBANK)

#define NROF_WEIGHTQUEUE_ELS 200
#define NROF_CLTERM_HASHVEC_ELS       1000000 // was 1000000
#define NROF_ACTIVE_UNIT_HASHVEC_ELS  1000000 // was 1000000
#define NROF_CLTERM_HASHNODESVEC_ELS 10000000 // used if MALLOC_HASHNODES not set

#define NROF_ATOM_OCCURRENCES_HASHVEC_ELS 1000000

#define TERMHASH_INITIAL_BUCKETSIZE 4
#define ATOMHASH_INITIAL_BUCKETSIZE 4

#define MAX_CLAUSE_LEN 1000
#define INITIAL_SORTVEC_LEN 1000
#define INITIAL_CLTMPVEC_LEN 1000
#define INITIAL_VARTMPVEC_LEN 1000

#define INITIAL_ANSWERS_LEN 1000

#define MAX_CLPRIOR 10000 // priorities differentiated in a priority queue for picking clauses
/*
#define NROF_PRIORQUEUES 4
#define GOAL_
*/

#define MALLOC_HASHNODES // if defined, use malloc for hashnodes, else alloc from g->hash_nodes
//#undef MALLOC_HASHNODES


/* ======== Structures ========== */

/** glb contains global values for requests.
* 
* requests should use a single global variable g, which
* is a pointer to glb structure.
* 
*/

typedef struct {    
  
  void* db;             /**< mem database used as a base for offset: either kb_db or child_db here */
  void* kb_db;          /**< shared-mem knowledge base */
  void* child_db;       /**< local child db: if kb_db present, then child_db->kb_db is set to this */
  gint db_offset;       /**< offset of the db ptr */
  gint inkb;            /**< 1 if g struct is inside shared kb, 0 otherwise */
  void* kb_g;            /**< if external shared mem kb_db is present, then the g ptr inside kb_db, else NULL */ 

  /* === shared data block === */
  
  gint initial_cl_list; /**< offset of the initial clause list in a shared db */

  cveco clbuilt;        /**< vector containing built clauses, newest last. 0: vec len, 1: index of next unused vec elem */
  cveco clactive;      /**< vector containing active clauses, newest last.  */
  cveco clactivesubsume; /**< vector containing active clauses with metainfo for subsumption */
  
  cveco clpickstack;   /**< vector containing built clause stack to be selected as given before using queue (hyperres eg) */
  cveco clqueue;        /**< vector containing kept clauses, newest last. 0: vec len, 1: index of next unused vec elem */
  gint clqueue_given;  /**< index of next clause to be taken from clqueue */     
  gint clpickpriorqueue;  /**< priority queue for picking clauses as as given, maybe interleaved with clqueue */

  gint clpick_queues;   /**< vec of several queue structures */
  gint clpick_given;    /**< index of next queue to be taken from vec of queues */

  gint hash_neg_groundunits; /**< hash structure for cutting off with negative ground unit clauses */
  gint hash_pos_groundunits; /**< hash structure for cutting off with positive ground unit clauses */

  gint hash_neg_active_groundunits; /**< hash structure for subsuming with negative ground unit clauses in active */
  gint hash_pos_active_groundunits; /**< hash structure for subsuming with positive ground unit clauses in active */

  gint hash_atom_occurrences; /**< hash structure for atom occurrences, used for unit propagation */

  //veco clweightqueue;
  veco hash_neg_atoms;  /**< hash structure for all negative literals in active clauses inside all clauses  */
  veco hash_pos_atoms;  /**< hash structure for all positive literals in active clauses inside all clauses  */
  veco hash_units;      /**< hash structure for: initialized, but not really used */
  veco hash_para_terms; /**< hash structure for paramodulating from: terms here will be unified with newly given eq clauses */
  veco hash_eq_terms;   /**< hash structure for paramodulation into: equality args here will be unified with given clause subterms */
  veco hash_rewrite_terms; /**< hash structure for left sides of rewrite rules (oriented equalities) */
#ifndef MALLOC_HASHNODES  
  cvec hash_nodes;      /**< area for allocating hash nodes from **/
#endif
  /* == local data block === */
       
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
  
  vec tmp_litinf_vec;  // used by subsumption  
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

  cvec tmp_clinfo; // used for storing additional clause info while ordering for knuth-bendix
  cvec tmp_varinfo; // used for storing additional var info while ordering for knuth-bendix

  /* parser configuration */
  
  int parse_caps_as_var;
  int parse_question_as_var;
  int parse_newpred_strat; // introduce new predicates during distribution phase of clausification?

  /* parser temporary vars set by parser */

  int parse_is_included_file;
  char* parse_skolem_prefix;  
  int parse_skolem_nr;
  char* parse_newpred_prefix; 
  int parse_newpred_nr; 
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
  
  int pick_given_queue_ratio;
  int pick_given_queue_ratio_counter;
  int next_pick_given_queue_block_nr;

  int cl_depth_penalty;
  int cl_length_penalty;

  int cl_maxkeep_weightlimit;
  int cl_maxkeep_sizelimit;
  int cl_maxkeep_depthlimit;
  int cl_maxkeep_lengthlimit;

  int cl_pick_queue_strategy; // how to spread clauses between different queues: 0 
                              // 0: one only,
                              // 1: exactly as marked, 
                              // 2: make non-included axioms assumptions and positive conjecture part assumptions
                              // 3: only fully negative clauses of goal made goal and no assumptions (ie made axioms)

  /* dynamically changed limits */

  int max_run_seconds;
  int max_seconds;
  int cl_keep_weightlimit;
  int cl_keep_sizelimit;
  int cl_keep_depthlimit;
  int cl_keep_lengthlimit;
  
  int reverse_clauselist_strat; // initially reverse clause list, do not sort (only for non-query)
  int queryfocus_strat;
  int queryfocusneg_strat;
  int hyperres_strat;  
  int weightorder_strat;          // clause ordering for resolvability of literals
  int negpref_strat;          // clause ordering for resolvability of literals
  int pospref_strat;          // clause ordering for resolvability of literals
  int knuthbendixpref_strat;  // clause ordering for resolvability of literals
  int hardnesspref_strat;     // clause ordering for resolvability of literals
  int res_shortarglen_limit; // max non-ans len of the shortest res argument (generalization of unit)
  int posunitpara_strat;
  int back_subsume;
  int propagate;
  int use_equality_strat; // general strategy
  int use_equality; // current principle
  int use_comp_funs_strat; // general strategy
  int use_comp_funs; // current principle
  int use_rewrite_terms_strat; // general strategy
  int have_rewrite_terms; // observation  
  
  int max_proofs;
  int store_history;
  
  /*  printout configuration */
  
  int print_flag;
  int print_level_flag;
  int print_clause_history;
  int print_history_extra;
  
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
  
  /* tmp variables */
  
  gint* tmp_unify_vc;       // var count in unification
  gint  tmp_unify_occcheck; // occcheck necessity in unification (changes)
  gint  tmp_unify_do_occcheck;
  gint  tmp_rewrites; // count of rewrites during one clause simplification
  
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
  int stat_propagated_derived_cl;
  int stat_factor_derived_cl;
  int stat_para_derived_cl;
  int stat_kept_cl;
  
  int stat_hyperres_partial_cl;
  int stat_tautologies_discarded;
  int stat_weight_discarded_building;
  int stat_weight_discarded_cl;
  int stat_internlimit_discarded_cl;
  int stat_internlimit_discarded_para;
  int stat_given_candidates;
  int stat_given_candidates_hyper;
  int stat_given_used;  
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
  int stat_lit_hash_cut_ok;  
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

  /* run averages and measures */
  
  int current_run_nr;
  double avg_kept_weight;
  float passed_ratio;
  clock_t allruns_start_clock;
  //clock_t allruns_end_clock;
  clock_t run_start_clock;
  //clock_t run_end_clock;  

} glb;



/* === Protos for funs in glb.c === */


glb* wr_glb_new_full(void* db);
glb* wr_glb_new_simple(void* db);

int wr_glb_free(glb* g);

int wr_glb_init_simple(glb* g);

int wr_glb_init_shared_simple(glb* g);
int wr_glb_init_shared_complex(glb* g);
int wr_glb_free_shared_simple(glb* g);
int wr_glb_free_shared_complex(glb* g);

int wr_glb_init_local_simple(glb* g);
int wr_glb_init_local_complex(glb* g);
int wr_glb_free_local_simple(glb* g);
int wr_glb_free_local_complex(glb* g);

void wr_print_glb_memarea(glb* g, gptr p);
void wr_check_cl_memarea(glb* g, gptr xptr);

#endif
