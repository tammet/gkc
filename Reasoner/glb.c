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

 /** @file glb.c
 *  Reasoner globals.
 *
 */

/* ====== Includes =============== */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif


#include "rincludes.h"

  
/* ====== Private headers and defs ======== */



/* ======= Private protos ================ */


/* ====== Functions ============== */


/** Creates and fills in the glb structure.
*
*/

glb* wr_glb_new_full(void* db) {     
  glb* g;
  int tmp;  
   
  g=wr_glb_new_simple(db);
  if (g==NULL) return NULL;
  tmp=wr_glb_init_shared_complex(g); // creates and fills in shared tables, substructures, etc
  if (tmp) {
    wr_printf("\nerror: cannot init shared complex datastructures\n");
    wr_glb_free_shared_complex(g);
    sys_free(g);
    return NULL; 
  }  
  tmp=wr_glb_init_local_complex(g); // creates and fills in local tables, substructures, etc
  if (tmp) {
    wr_printf("\nerror: cannot init local complex datastructures\n");
    wr_glb_free_shared_complex(g);
    wr_glb_free_local_complex(g);
    sys_free(g);
    return NULL; 
  } 
  return g;
  
}  
  
glb* wr_glb_new_simple(void* db) {     
  glb* g;  
  
  g=sys_malloc(sizeof(glb)); // allocate space
  if (g==NULL) return NULL;  
  (g->db)=db;          // store database pointer to glb structure
  (g->kb_db)=NULL;          
  (g->child_db)=NULL;
  (g->inkb)=0;
  (g->db_offset)=0;
  (g->kb_g)=NULL;  
  wr_glb_init_simple(g);  // fills in simple values (ints, strings etc)  
  //printf("\ndbmemsegh(db)->tptp %ld\n",dbmemsegh(db)->tptp);
  if (dbmemsegh(db)->tptp) {
    (g->print_tptp)=1;        
    (g->print_fof_conversion_proof)=1;   
    (g->print_clauses_tptp)=1;
    (g->print_proof_tptp)=1;  
  } else {
    (g->print_tptp)=0;        
    (g->print_fof_conversion_proof)=0; 
    (g->print_clauses_tptp)=0;
    (g->print_proof_tptp)=0;  
  } 
  if (dbmemsegh(db)->json) {
    (g->print_json)=1; 
    (g->print_clauses_json)=1;    
  }
  return g;  
} 

/** Fills in simple slots of glb structure.
*
*/

int wr_glb_init_simple(glb* g) {
  //printf("\nwr_glb_init_simple called\n");
  (g->filename)=NULL;
  (g->outfilename)=NULL;
  (g->proof_found)=0; // becomes 1 if proof found
  (g->proof_answer)=NULL; // becomes a ptr to pure answer clause
  (g->proof_history)=wg_encode_null(g->db,NULL); // becomes a gint encoding of the constructed history

  (g->alloc_err)=0; // 0 if ok, becomes 1 or larger if alloc error occurs:
                    // 3 out of varspace err
  
  (g->shared_clid_next)=0;
  (g->local_clid_next)=0;
  (g->local_db)=NULL; // must be set to the real local kb before used, done in rmain.c

  /* parser configuration */

  (g->parse_newpred_strat)=1; // NORMAL 1, TESTING 0
  (g->parse_caps_as_var)=1;
  (g->parse_question_as_var)=1;
  (g->store_fof_skolem_steps)=1;
  (g->store_fof_source)=1;

  /* parser temporary */

  (g->parse_is_included_file)=0;
  (g->parse_skolem_prefix)=NULL; 
  // (g->parse_skolem_nr)=0; // moved to dballoc.h
  (g->parse_newpred_prefix)=NULL; 
  // (g->parse_newpred_nr)=0; // moved to dballoc.h
  (g->parse_errmsg)=NULL;
  (g->parse_errflag)=0;

  /* unification/matching configuration */
  
  (g->unify_samelen)=1;
  (g->unify_maxuseterms)=0;
  (g->unify_firstuseterm)=1;
  (g->unify_funpos)=1;
  (g->unify_funarg1pos)=2; // rec pos of a fun/pred first arg
  (g->unify_funarg2pos)=3; // rec pos of a fun/pred second arg
  (g->unify_footerlen)=0; 
  
  /* strategy selection */    
  
  //(g->max_forks)=4; // default 2 forks
  (g->sine_strat)=0; // 0 if none, 1 if weak, 2 if strong
  (g->sine_strat_used)=0; // 0 if not actually used, 1 if used: set automatically by analyze if sine_strat!=0
  (g->sine_order_strat)=0; // 1 if sine order used for ordering initial clauses
  (g->required_answer_nr)=1;
  (g->pick_given_queue_ratio)=5;         // this is used for all queues to diff btw priority and simple
  (g->pick_given_queue_ratio_counter)=0; // this is not used for queues
  (g->next_pick_given_queue_block_nr)=0;

  (g->cl_depth_penalty)=0; // normal 0
  (g->cl_length_penalty)=1; // normal 1
  (g->var_weight)=5; // NORMAL 5
  (g->repeat_var_weight)=7; // NORMAL 7
  (g->atom_poseq_penalty)=0; // normal 0
  (g->use_max_ground_weight)=0; // normal 0
  (g->use_max_weight)=0; // normal 1

  /* pre-given limits */
  (g->max_run_seconds)=0; // one run max seconds
  (g->max_seconds)=0;     // total max seconds
  (g->cl_maxkeep_weightlimit)=0;
  (g->cl_maxkeep_sizelimit)=0;
  (g->cl_maxkeep_depthlimit)=0;
  (g->cl_maxkeep_lengthlimit)=0;  

  (g->cl_pick_queue_strategy)=0; // default a single 2nd level queue for all
  
  (g->reverse_clauselist_strat)=0;
  (g->queryfocus_strat)=0;
  (g->queryfocusneg_strat)=0;  

  (g->hyperres_strat)=0;
  (g->relaxed_hyperres_strat)=1;
  (g->goalweight_normal_strat)=0; // normal 0: if 1, normal initial weights are applied, else special low weights
  (g->unit_active_strat)=0;  // normal 0; if 1, all units are made active for sos strategies
  (g->weightorder_strat)=0;  
  (g->negpref_strat)=0;
  (g->pospref_strat)=0;
  (g->knuthbendixpref_strat)=0;
  (g->hardnesspref_strat)=0;
  (g->res_shortarglen_limit)=0; // max non-ans len of the shortest res argument (generalization of unit)
  (g->res_arglen_limit)=0; // if non-zero, do not resolve upon longer clauses and never para
  (g->res_strict_arglen_limit)=0; // if non-zero,  do para on units if res_arglen_limit<2
  (g->back_subsume)=0; // 1 uses back subsumption, 0 does not
  (g->propagate)=0;    // 1 does not work any more 
  (g->use_equality_strat)=1; // general strategy
  (g->use_equality)=1; // current principle
  (g->posunitpara_strat)=0; // only paramodulate from unit equalities
  (g->rewrite_only_strat)=0; // only store demodulators, do not store para terms
  (g->instgen_strat)=0;
  (g->propgen_strat)=0;
  (g->use_comp_funs_strat)=1;
  (g->use_comp_funs)=1;
  (g->use_comp_arithmetic)=1;
  (g->use_rewrite_terms_strat)=1; // general strategy // NORMAL 1
  (g->have_rewrite_terms)=0; // do we actually have rewrite terms
  (g->use_strong_unit_cutoff)=0; // if 1, then cut off also with unification, not just with hash equality
  (g->use_strong_duplicates)=0; // iff 1, then additional unique var based duplicate removal used
  (g->prohibit_nested_para)=0; // iff 1, paramodulation derivations cannot be directly nested
 
  (g->max_proofs)=1;
  (g->store_history)=1;
  (g->endgame_mode)=0; // set to 1 at the last phase of one run to look for contradiction directly
  //(g->cl_maxdepth)=1000000;
  //(g->cl_limitkept)=1;
  (g->sine_k_values)=NULL;
  (g->sine_uri_k_values)=NULL;
  (g->sine_k_bytes)=SINE_K_VALUES_SIZE;
  (g->backsubsume_bytes)=SINE_K_VALUES_SIZE;

  // prop

  (g->use_prop_constant)=0;
  (g->prop_constant)=encode_smallint(1);
  (g->prop_counter)=2; 
  
  /*  printout configuration */
  
  (g->print_flag)=1; // if 0: no printout except result: rmain sets other flags accordingly
  (g->print_json)=0; // if 1: non-log output is json
  (g->print_tptp)=1; // if 1: non-log output is tptp/casc format
  (g->print_clauses_json)=0; // if 1: clauses are printed as json lists
  (g->print_clauses_tptp)=1; // if 1: clauses are printed in tptp format
  (g->print_proof_tptp)=1; // if 1: tptp style proof printing
  (g->print_level_flag)=10; // rmain uses this to set other flags accordingly. Normal: 10
                           // -1: use default, 0: none, 10: normal, 20: medium, 30: detailed
  (g->print_derived)=0;                            
  (g->print_clause_history)=0; // 1 does not work well: prints double while deriving, ok for parsing res
  (g->print_history_extra)=0; // 1 prints clause group etc, 0 prints basics
  (g->print_fof_conversion_proof)=1; // 1 prints fof sources and some conversion steps in proof, 0 does not

  (g->parser_print_level)=1;
  (g->print_initial_parser_result)=0;
  (g->print_generic_parser_result)=0;
  
  (g->print_initial_active_list)=1;
  (g->print_initial_passive_list)=1;
  
  (g->print_given_interval_trace)=0;
  (g->print_initial_given_cl)=1;
  (g->print_final_given_cl)=1;
  (g->print_active_cl)=1;
  (g->print_litterm_selection)=1;
  (g->print_partial_derived_cl)=1;
  (g->print_derived_cl)=1;
  (g->print_derived_subsumed_cl)=1;
  (g->print_derived_precut_cl)=1;
  
  (g->print_clause_detaillevel)=1;
  (g->print_runs)=1;
  (g->print_stats)=1;  
  (g->print_datastructs)=1;
  (g->print_sine)=0;
  
  /* tmp variables */

  (g->tmp_printbuf)=NULL;

  /* dynamically changed limits: initialized to cl_maxkeep_* */

  (g->cl_keep_weightlimit)=0;
  (g->cl_keep_sizelimit)=0;
  (g->cl_keep_depthlimit)=0;
  (g->cl_keep_lengthlimit)=0;
  
  /* build control: changed in code */
  
  (g->build_rewrite)=0;  // to rewrite terms while building or not
  (g->build_subst)=1;    // subst var values into vars
  (g->build_calc)=1;     // do fun and pred calculations
  (g->build_dcopy)=0;    // copy nonimmediate data (vs return ptr)
  (g->build_buffer)=NULL; // build everything into local tmp buffer area (vs main area)
  
  /* statistics */
  
  (g->stat_wr_mallocs)=0;
  (g->stat_wr_callocs)=0;
  (g->stat_wr_reallocs)=0;
  (g->stat_wr_frees)=0;
  (g->stat_wr_malloc_bytes)=0;
  (g->stat_wr_calloc_bytes)=0;
  (g->stat_wr_realloc_bytes)=0;
  (g->stat_wr_realloc_freebytes)=0;
  
  (g->stat_built_cl)=0;
  (g->stat_derived_cl)=0;
  (g->stat_derived_partial_hyper_cl)=0;
  (g->stat_binres_derived_cl)=0;
  (g->stat_instgen_derived_cl)=0;
  (g->stat_prop_inst_derived_cl)=0;
  (g->stat_propagated_derived_cl)=0;
  (g->stat_factor_derived_cl)=0;
  (g->stat_para_derived_cl)=0;
  (g->stat_kept_cl)=0;
  (g->stat_made_rewriters)=0;
  (g->stat_tautologies_discarded)=0;
  (g->stat_hyperres_partial_cl)=0;
  (g->stat_weight_discarded_building)=0;
  (g->stat_weight_discarded_cl)=0;
  (g->stat_internlimit_discarded_cl)=0;
  (g->stat_internlimit_discarded_para)=0;
  (g->stat_given_candidates)=0;
  (g->stat_given_candidates_at_endgame)=0;
  (g->stat_given_candidates_hyper)=0;
  (g->stat_given_used)=0;
  (g->stat_given_used_at_endgame)=0;
  (g->stat_simplified)=0;
  (g->stat_simplified_given)=0;
  (g->stat_simplified_derived)=0;
  (g->stat_backward_subsumed)=0;
  (g->stat_propagated_subsumed)=0;
  (g->stat_forward_subsumed)=0;
  (g->stat_derived_cut)=0;
  (g->stat_derived_rewritten)=0;

  (g->stat_clsubs_attempted)=0;
  (g->stat_clsubs_top_meta_attempted)=0;
  (g->stat_clsubs_top_meta_failed)=0;
  (g->stat_clsubs_top_meta_nonpref_attempted)=0;
  (g->stat_clsubs_top_meta_nonpref_succeeded)=0;
  (g->stat_clsubs_top_meta_pref_attempted)=0;
  (g->stat_clsubs_top_meta_pref_succeeded)=0;
  (g->stat_clsubs_top_meta_pref1_succeeded)=0;
  (g->stat_clsubs_top_meta_pref2_succeeded)=0;
  (g->stat_clsubs_top_meta_pref3_succeeded)=0;
  (g->stat_clsubs_meta_attempted)=0;
  (g->stat_clsubs_meta_failed)=0;
  (g->stat_clsubs_predsymbs_attempted)=0;
  (g->stat_clsubs_unit_attempted)=0;
  (g->stat_clsubs_full_attempted)=0;
  (g->stat_clsubs_fact_groundunit_found)=0;
  (g->stat_clsubs_rule_groundunit_found)=0;

  (g->stat_forwardsubs_attempted)=0;
  
  (g->stat_lit_hash_added)=0;
  (g->stat_lit_hash_computed)=0;
  (g->stat_lit_hash_match_found)=0;
  (g->stat_lit_hash_match_miss)=0;
  (g->stat_prop_hash_match_miss)=0;
  (g->stat_lit_hash_cut_ok)=0;    
  (g->stat_lit_strong_cut_ok)=0;
  (g->stat_lit_hash_subsume_ok)=0;  

  (g->stat_atom_hash_added)=0;
  (g->stat_atom_hash_computed)=0;
  (g->stat_atom_hash_match_found)=0;
  (g->stat_atom_hash_match_miss)=0;

  (g->tmp1)=0;
  (g->tmp2)=0;
  (g->tmp3)=0;
  (g->tmp4)=0;
  (g->tmp5)=0;

  (g->tmp6)=0;
  (g->tmp7)=0;
  (g->tmp8)=0;
  (g->tmp9)=0;

  (g->in_has_fof)=0;

  (g->in_clause_count)=0;
  (g->in_rule_clause_count)=0;
  (g->in_fact_clause_count)=0;
  (g->in_answer_clause_count)=0;
  (g->in_ground_clause_count)=0;
  (g->in_unit_clause_count)=0;
  (g->in_horn_clause_count)=0;
  (g->in_pos_clause_count)=0;
  (g->in_neg_clause_count)=0;
  (g->in_poseq_clause_count)=0;
  (g->in_negeq_clause_count)=0;
  (g->in_unitposeq_clause_count)=0;
  (g->in_chain_clause_count)=0;
  (g->in_min_length)=100000;
  (g->in_max_length)=0;
  (g->in_min_depth)=100000;
  (g->in_max_depth)=0;
  (g->in_min_size)=100000;
  (g->in_max_size)=0;
  (g->in_min_vars)=100000;
  (g->in_max_vars)=0;
  (g->in_average_length)=0;
  (g->in_average_depth)=0;
  (g->in_predicate_count)=0;
  (g->in_funsymb_count)=0;
  (g->in_extaxiom_count)=0;
  (g->in_axiom_count)=0;
  (g->in_assumption_count)=0;
  (g->in_goal_count)=0;
  (g->in_neg_goal_count)=0;
  (g->in_pos_goal_count)=0;
  (g->in_posunit_goal_count)=0;
  (g->in_max_const_ucount)=0;
  (g->in_max_occ_const)=0;

  (g->sin_clause_count)=0;
  (g->sin_rule_clause_count)=0;
  (g->sin_fact_clause_count)=0;
  (g->sin_answer_clause_count)=0;
  (g->sin_ground_clause_count)=0;
  (g->sin_unit_clause_count)=0;
  (g->sin_horn_clause_count)=0;
  (g->sin_pos_clause_count)=0;
  (g->sin_neg_clause_count)=0;
  (g->sin_poseq_clause_count)=0;
  (g->sin_negeq_clause_count)=0;
  (g->sin_unitposeq_clause_count)=0;
  (g->sin_chain_clause_count)=0;
  (g->sin_min_length)=100000;
  (g->sin_max_length)=0;
  (g->sin_min_depth)=100000;
  (g->sin_max_depth)=0;
  (g->sin_min_size)=100000;
  (g->sin_max_size)=0;
  (g->sin_min_vars)=100000;
  (g->sin_max_vars)=0;
  (g->sin_average_length)=0;
  (g->sin_average_depth)=0;
  (g->sin_predicate_count)=0;
  (g->sin_funsymb_count)=0;
  (g->sin_extaxiom_count)=0;
  (g->sin_axiom_count)=0;
  (g->sin_assumption_count)=0;
  (g->sin_goal_count)=0;
  (g->sin_neg_goal_count)=0;
  (g->sin_pos_goal_count)=0;
  (g->sin_posunit_goal_count)=0;
  (g->sin_max_const_ucount)=0;
  (g->sin_max_occ_const)=0;

  (g->current_run_nr)=0;
  (g->current_fork_nr)=0;
  (g->avg_kept_weight)=0;
  (g->passed_ratio)=0;
  
  return 0;
}  


/** Fills in shared complex slots of glb structure.
*
*/

int wr_glb_init_shared_complex(glb* g) {     
 
  // first NULL all vars
  
  (g->initial_cl_list)=0;

  (g->clbuilt)=(gint)NULL;
  (g->clqueue)=(gint)NULL;
  (g->clqueue_given)=(gint)NULL; 
  (g->clpickstack)=(gint)NULL;  
  (g->clactive)=(gint)NULL;
  (g->clactivesubsume)=(gint)NULL;
  (g->clpickpriorqueue)=(gint)NULL;
  (g->clpick_queues)=(gint)NULL;


  //(g->clweightqueue)=(gint)NULL;  
  (g->hash_neg_atoms)=(gint)NULL; 
  (g->hash_pos_atoms)=(gint)NULL; 
  (g->hash_neg_units)=(gint)NULL; 
  (g->hash_pos_units)=(gint)NULL; 
  //(g->hash_units)=(gint)NULL; 
  (g->hash_para_terms)=(gint)NULL; 
  (g->hash_eq_terms)=(gint)NULL; 
  (g->hash_rewrite_terms)=(gint)NULL;
#ifndef MALLOC_HASHNODES  
  (g->hash_nodes)=(gint)NULL;
#endif

  // prop

  (g->prop_hash_atoms)=(gint)NULL; 
  (g->prop_hash_clauses)=(gint)NULL;
  (g->prop_varvals)=(gint)NULL;
  (g->prop_clauses)=(gint)NULL;
  (g->prop_varval_clauses)=(gint)NULL;

  // then create space 
  
  (g->clbuilt)=rpto(g,wr_cvec_new(g,NROF_DYNALLOCINITIAL_ELS));  
  (g->clactive)=rpto(g,wr_cvec_new(g,NROF_DYNALLOCINITIAL_ELS));
  (g->clactivesubsume)=rpto(g,wr_cvec_new(g,NROF_DYNALLOCINITIAL_ELS));
  (g->clpickstack)=rpto(g,wr_cvec_new(g,NROF_DYNALLOCINITIAL_ELS));  
  (g->clqueue)=rpto(g,wr_cvec_new(g,NROF_DYNALLOCINITIAL_ELS));
  (g->clqueue_given)=1;  
  //(g->clweightqueue)=rpto(g,wr_vec_new(g,NROF_WEIGHTQUEUE_ELS));
  (g->clpickpriorqueue)=rpto(g,wr_make_priorqueue(g,MAX_CLPRIOR));   

  (g->clpick_queues)=wr_create_clpick_queues(g, 4);  
  (g->clpick_given)=1; // first queue block in the queues vector 

  // hash vectors  must be zeroed

  (g->hash_neg_groundunits)=rpto(g,wr_vec_new_zero(g,NROF_CLTERM_HASHVEC_ELS));
  //wr_vec_zero(rotp(g,g->hash_neg_groundunits)); 
  (g->hash_pos_groundunits)=rpto(g,wr_vec_new_zero(g,NROF_CLTERM_HASHVEC_ELS));
  //wr_vec_zero(rotp(g,g->hash_pos_groundunits)); 

  (g->hash_neg_active_groundunits)=rpto(g,wr_vec_new_zero(g,NROF_ACTIVE_UNIT_HASHVEC_ELS));
  //wr_vec_zero(rotp(g,g->hash_neg_active_groundunits)); 
  (g->hash_pos_active_groundunits)=rpto(g,wr_vec_new(g,NROF_ACTIVE_UNIT_HASHVEC_ELS));
  //wr_vec_zero(rotp(g,g->hash_pos_active_groundunits)); 

  (g->hash_atom_occurrences)=rpto(g,wr_vec_new_zero(g,NROF_ATOM_OCCURRENCES_HASHVEC_ELS));
  //wr_vec_zero(rotp(g,g->hash_atom_occurrences));

  (g->hash_neg_atoms)=rpto(g,wr_vec_new_zero(g,NROF_CLTERM_HASHVEC_ELS));
  //wr_vec_zero(rotp(g,g->hash_neg_atoms));  
  (g->hash_pos_atoms)=rpto(g,wr_vec_new_zero(g,NROF_CLTERM_HASHVEC_ELS)); 
  //wr_vec_zero(rotp(g,g->hash_pos_atoms)); 

  if (g->use_strong_unit_cutoff) {
    (g->hash_neg_units)=rpto(g,wr_vec_new_zero(g,NROF_CLTERM_HASHVEC_ELS));
    //wr_vec_zero(rotp(g,g->hash_neg_atoms));  
    (g->hash_pos_units)=rpto(g,wr_vec_new_zero(g,NROF_CLTERM_HASHVEC_ELS)); 
    //wr_vec_zero(rotp(g,g->hash_pos_atoms)); 
  }
 
  //(g->hash_units)=rpto(g,wr_vec_new_zero(g,NROF_CLTERM_HASHVEC_ELS)); 
  //wr_vec_zero(rotp(g,g->hash_units)); 
 
  (g->hash_para_terms)=rpto(g,wr_vec_new_zero(g,NROF_CLTERM_HASHVEC_ELS)); 
  //wr_vec_zero(rotp(g,g->hash_para_terms)); 
  (g->hash_eq_terms)=rpto(g,wr_vec_new_zero(g,NROF_CLTERM_HASHVEC_ELS)); 
  //wr_vec_zero(rotp(g,g->hash_eq_terms)); 
  (g->hash_rewrite_terms)=rpto(g,wr_vec_new_zero(g,NROF_CLTERM_HASHVEC_ELS)); 
  //wr_vec_zero(rotp(g,hash_rewrite_terms)); 

  //(g->hash_nodes)=rpto(g,wr_vec_new(g,NROF_CLTERM_HASHNODESVEC_ELS));
  
  // prop

  if ((g->instgen_strat) || (g->propgen_strat)) {
    (g->prop_hash_atoms)=rpto(g,wr_vec_new_zero(g,NROF_CLTERM_HASHVEC_ELS)); 
    (g->prop_hash_clauses)=rpto(g,wr_vec_new_zero(g,NROF_CLTERM_HASHVEC_ELS));
    (g->prop_varvals)=rpto(g,wr_cvec_new_zero(g,NROF_PROP_VARVALS_ELS));
    (g->prop_groundings)=rpto(g,wr_cvec_new_zero(g,2*NROF_PROP_VARVALS_ELS));
    (g->prop_clauses)=rpto(g,wr_cvec_new(g,NROF_PROP_CLAUSES_ELS));
    (g->prop_varval_clauses)=rpto(g,wr_cvec_new_zero(g,NROF_PROP_VARVALS_ELS));
  }

  // sine
  //(g->tmp_uriinfo)=rpto(g,wr_cvec_new(g,INITIAL_URITMPVEC_LEN));
  //(g->tmp_uriinfo)=NULL;

  if (g->alloc_err) {
    return 1;
  }  
      
  return 0; 
}  



/** Fills in local complex slots of glb structure.
*
*/

int wr_glb_init_local_complex(glb* g) {     
  // first NULL all vars
  
  (g->filename)=NULL;
  (g->outfilename)=NULL;
  (g->varbanks)=NULL;
  (g->varstack)=NULL;         
  (g->given_termbuf)=NULL;
  (g->simplified_termbuf)=NULL;
  (g->derived_termbuf)=NULL;
  (g->queue_termbuf)=NULL;
  (g->hyper_termbuf)=NULL;
  (g->active_termbuf)=NULL;
  (g->cut_clvec)=NULL;
  (g->rewrite_clvec)=NULL;
  (g->hyper_queue)=NULL;
  (g->answers)=NULL;
  (g->tmp_litinf_vec)=NULL; 
  (g->tmp_hardnessinf_vec)=NULL;
  (g->tmp_resolvability_vec)=NULL;
  (g->tmp_sort_vec)=NULL;
  (g->prop_file_name)=NULL;  
  (g->prop_solver_name)=NULL;
  (g->prop_solver_outfile_name)=NULL;
  

  // then create space
  
 
  (g->filename)=wr_str_new(g,MAX_FILENAME_LEN);
  //(g->outfilename)=wr_str_new(g,MAX_FILENAME_LEN);

  (g->varbanks)=wr_vec_new(g,NROF_VARBANKS*NROF_VARSINBANK);
  //(g->varbankrepl)=wr_vec_new(g,3*NROF_VARSINBANK);
  (g->varstack)=wr_cvec_new(g,NROF_VARBANKS*NROF_VARSINBANK); 
  (g->varstack)[1]=2; // first free elem

  (g->xcountedvarlist)=wr_cvec_new(g,4+(NROF_VARSINBANK*2)); 
  (g->xcountedvarlist)[1]=2; // first free elem
 
  (g->ycountedvarlist)=wr_cvec_new(g,4+(NROF_VARSINBANK*2)); 
  (g->ycountedvarlist)[1]=2; // first free elem

  //(g->tmp1_cl_vec)=wr_vec_new(g,100);    
  //(g->tmp2_cl_vec)=wr_vec_new(g,100); 

  //(g->tmp_litinf_vec)=wr_vec_new(g,100); 
        
  (g->given_termbuf)=wr_cvec_new(g,NROF_GIVEN_TERMBUF_ELS);
  (g->given_termbuf)[1]=2;
  //(g->given_termbuf_freeindex)=2;

  (g->simplified_termbuf)=wr_cvec_new(g,NROF_SIMPLIFIED_TERMBUF_ELS);
  (g->simplified_termbuf)[1]=2;
  
  (g->derived_termbuf)=wr_cvec_new(g,NROF_DERIVED_TERMBUF_ELS);
  (g->derived_termbuf)[1]=2;
 
  (g->queue_termbuf)=wr_cvec_new(g,NROF_QUEUE_TERMBUF_ELS);
  (g->queue_termbuf)[1]=2;

  (g->hyper_termbuf)=wr_cvec_new(g,NROF_HYPER_TERMBUF_ELS);
  (g->hyper_termbuf)[1]=2; 
  
  (g->active_termbuf)=wr_cvec_new(g,NROF_ACTIVE_TERMBUF_ELS);
  (g->active_termbuf)[1]=2;

#ifndef MALLOC_HASHNODES
  (g->hash_nodes)=wr_cvec_new(g,NROF_CLTERM_HASHNODESVEC_ELS);
  (g->hash_nodes)[1]=2;
#endif

  (g->cut_clvec)=wr_vec_new(g,NROF_CUT_CLVEC_ELS);
  (g->rewrite_clvec)=wr_cvec_new(g,NROF_REWRITE_CLVEC_ELS);
  (g->rewrite_clvec)[1]=2; // first free elem

  (g->hyper_queue)=wr_cvec_new(g,NROF_HYPER_QUEUE_ELS);
  (g->hyper_queue)[1]=3; // next free pos in the queue (initially for empty queue 3)
  (g->hyper_queue)[2]=3; // next pos to pick for given (initially for empty queue 3)

  (g->answers)=wr_cvec_new(g,INITIAL_ANSWERS_LEN);
  (g->answers)[1]=2; // next free pos in the queue (initially for empty queue 2)

  (g->sine_k_values)=sys_malloc(SINE_K_VALUES_SIZE); // bytestring allocated by malloc
  if (!(g->sine_k_values)) {
    wr_printf("\nerror: cannot init sine_k_values\n");   
    (g->sine_k_bytes)=0;
  } else {
    (g->sine_k_bytes)=SINE_K_VALUES_SIZE;
  }    
  (g->backsubsume_values)=sys_malloc(SINE_K_VALUES_SIZE); // bytestring allocated by malloc
  if (!(g->backsubsume_values)) {
    wr_printf("\nerror: cannot init backsubsume_values\n");   
    (g->backsubsume_bytes)=0;
  } else {
    (g->backsubsume_bytes)=SINE_K_VALUES_SIZE;
  }  


  (g->sine_uri_k_values)=sys_malloc(SINE_K_VALUES_SIZE); // bytestring allocated by malloc
  if (!(g->sine_uri_k_values)) {
    wr_printf("\nerror: cannot init sine_uri_k_values\n");   
    (g->sine_uri_k_bytes)=0;
  } else {
    (g->sine_uri_k_bytes)=SINE_K_VALUES_SIZE;
  }
    
  (g->tmp_litinf_vec)=wr_vec_new(g,MAX_CLAUSE_LEN); // used by subsumption
  (g->tmp_hardnessinf_vec)=wr_vec_new(g,MAX_CLAUSE_LEN); // used for resolvability
  
  (g->tmp_resolvability_vec)=wr_vec_new(g,MAX_CLAUSE_LEN); // used for resolvability
  (g->tmp_sort_vec)=wr_vec_new(g,INITIAL_SORTVEC_LEN); // used for sorting the initial clause list

  (g->tmp_clinfo)=wr_cvec_new(g,INITIAL_CLTMPVEC_LEN);
  (g->tmp_varinfo)=wr_cvec_new(g,INITIAL_VARTMPVEC_LEN);  

  //(g->derived_termbuf_freeindex)=2;
  
  //(g->use_termbuf)=0;
  
  //(g->pick_given_queue_ratio)=4;
  //(g->pick_given_queue_ratio_counter)=0;
  
  (g->prop_file_name)=wr_str_new(g,100);
  if (g->prop_file_name) strncpy((g->prop_file_name),DEFAULT_PROP_FILE_NAME,99);
 
  (g->prop_solver_name)=wr_str_new(g,100);
  if (g->prop_solver_name) strncpy((g->prop_solver_name),DEFAULT_PROP_SOLVER_NAME,99);

  (g->prop_solver_outfile_name)=wr_str_new(g,100);
  if (g->prop_solver_outfile_name) strncpy((g->prop_solver_outfile_name),DEFAULT_PROP_SOLVER_OUTFILE_NAME,99);

  if ((g->alloc_err)==1) {
    return 1;
  }   
  return 0; 
}  

/** Frees the glb structure and subitems in glb.
*
*/  

int wr_glb_free(glb* g) {
  // first free subitems
  wr_glb_free_shared_complex(g);
  wr_glb_free_local_complex(g);
  wr_glb_free_shared_simple(g); 
  wr_glb_free_local_simple(g);

  sys_free(g); // free whole spaces
  return 0;
}  

/** Frees the glb shared simple subitems.
*
*/  

int wr_glb_free_shared_simple(glb* g) {
  
  //str_freeref(g,&(g->info)); 

  return 0;  
}

/** Frees the glb local simple subitems.
*
*/  

int wr_glb_free_local_simple(glb* g) {
  
  //str_freeref(g,&(g->info)); 
  if (g->tmp_printbuf) wr_free(g,g->tmp_printbuf);
  return 0;  
}  


/** Frees the glb shared complex subitems.
*
*/  



int wr_glb_free_shared_complex(glb* g) {
  wr_vec_free(g,rotp(g,g->clbuilt)); 
  wr_vec_free(g,rotp(g,g->clactive));  
  wr_vec_free(g,rotp(g,g->clactivesubsume));  
  wr_vec_free(g,rotp(g,g->clpickstack));  
  wr_vec_free(g,rotp(g,g->clqueue));     
  //wr_vec_free(g,rotp(g,g->clweightqueue)); 
  wr_free_priorqueue(g,rotp(g,g->clpickpriorqueue)); 
  wr_free_clpick_queues(g,rotp(g,g->clpick_queues));
  wr_free_termhash(g,rotp(g,g->hash_pos_groundunits));
  wr_free_termhash(g,rotp(g,g->hash_neg_groundunits));

  wr_free_termhash(g,rotp(g,g->hash_pos_active_groundunits));
  wr_free_termhash(g,rotp(g,g->hash_neg_active_groundunits));

  wr_free_atomhash(g,rotp(g,g->hash_atom_occurrences));
  wr_clterm_hashlist_free(g,rotp(g,g->hash_neg_atoms));    
  wr_clterm_hashlist_free(g,rotp(g,g->hash_pos_atoms)); 
  if (g->use_strong_unit_cutoff) {    
    if (g->hash_neg_units) wr_clterm_hashlist_free(g,rotp(g,g->hash_neg_units));    
    if (g->hash_pos_units) wr_clterm_hashlist_free(g,rotp(g,g->hash_pos_units)); 
  }
  //wr_clterm_hashlist_free(g,rotp(g,g->hash_units)); CP7
  wr_clterm_hashlist_free(g,rotp(g,g->hash_para_terms));
  wr_clterm_hashlist_free(g,rotp(g,g->hash_eq_terms));
  wr_clterm_hashlist_free(g,rotp(g,g->hash_rewrite_terms));
  // prop
  if ((g->instgen_strat) || (g->propgen_strat)) {
    wr_free_prop_termhash(g,rotp(g,g->prop_hash_atoms)); 
    wr_free_prop_termhash(g,rotp(g,g->prop_hash_clauses));
    wr_vec_free(g,rotp(g,g->prop_varvals));
    wr_vec_free(g,rotp(g,g->prop_groundings));
    wr_free_prop_clauses(g,rotp(g,g->prop_clauses));
    wr_vec_free(g,rotp(g,g->prop_varval_clauses));
  }  
  // sine
  //wr_vec_free(g,rotp(g,(g->tmp_uriinfo)));
  return 0;
}  

/** Frees the local glb complex subitems.
*
*/  

int wr_glb_free_local_complex(glb* g) {  
  wr_str_free(g,(g->filename));

  wr_vec_free(g,g->queue_termbuf);
  wr_vec_free(g,g->hyper_termbuf);
  wr_vec_free(g,g->active_termbuf);  
  wr_vec_free(g,g->cut_clvec); 
  wr_vec_free(g,g->rewrite_clvec);
  wr_vec_free(g,g->varstack); 
  wr_vec_free(g,g->xcountedvarlist);
  wr_vec_free(g,g->ycountedvarlist);
  wr_vec_free(g,g->varbanks);       
  wr_vec_free(g,g->given_termbuf);
  wr_vec_free(g,g->simplified_termbuf);
  wr_vec_free(g,g->derived_termbuf);        
  wr_vec_free(g,g->tmp_litinf_vec);  
  wr_vec_free(g,(g->tmp_hardnessinf_vec));
  wr_vec_free(g,(g->tmp_resolvability_vec));
  wr_vec_free(g,(g->tmp_sort_vec));
  wr_vec_free(g,g->tmp_clinfo);
  wr_vec_free(g,g->tmp_varinfo);
  wr_vec_free(g,g->hyper_queue);
  wr_vec_free(g,g->answers);
  if (g->sine_k_values) sys_free(g->sine_k_values); // bytestring
  if (g->sine_uri_k_values) sys_free(g->sine_uri_k_values); // bytestring
  if (g->backsubsume_values) sys_free(g->backsubsume_values); // bytestring

  wr_str_free(g,(g->parse_skolem_prefix));
  (g->parse_skolem_prefix)=NULL;
  wr_str_free(g,(g->parse_newpred_prefix));
  (g->parse_newpred_prefix)=NULL;
  wr_str_free(g,(g->parse_errmsg));
  (g->parse_errmsg)=NULL;

  wr_str_free(g,(g->prop_file_name));
  (g->prop_file_name)=NULL;
  wr_str_free(g,(g->prop_solver_name));
  (g->prop_solver_name)=NULL;
  wr_str_free(g,(g->prop_solver_outfile_name));
  (g->prop_solver_outfile_name)=NULL;

  return 0;
}  

void wr_print_glb_memarea(glb* g, gptr p) {
  wr_printf("\nmemarea for %x: ",(int)(gint)p);
  if (!p) {
    wr_printf("NULL\n");
    return;
  }
  if ( p>=(g->given_termbuf)+2 &&
       p<=(g->given_termbuf)+((g->given_termbuf)[1]) ) {
    wr_printf("given_termbuf\n");
    return;
  }
  if ( p>=(g->derived_termbuf)+2 &&
       p<=(g->derived_termbuf)+((g->derived_termbuf)[1]) ) {
    wr_printf("derived_termbuf\n");
    return;
  }
  if ( p>=(g->simplified_termbuf)+2 &&
       p<=(g->simplified_termbuf)+((g->simplified_termbuf)[1]) ) {
    wr_printf("simplified_termbuf\n");
    return;
  }
  if ( p>=(g->queue_termbuf)+2 &&
       p<=(g->queue_termbuf)+((g->queue_termbuf)[1]) ) {
    wr_printf("queue_termbuf\n");
    return;
  }
  if ( p>=(g->hyper_termbuf)+2 &&
       p<=(g->hyper_termbuf)+((g->hyper_termbuf)[1]) ) {
    wr_printf("hyper_termbuf\n");
    return;
  }
  if ( p>=(g->answers)+2 &&
       p<=(g->answers)+((g->answers)[1]) ) {
    wr_printf("answers\n");
    return;
  }
  if ( p>=(g->active_termbuf)+2 &&
       p<=(g->active_termbuf)+((g->active_termbuf)[1]) ) {
    wr_printf("active_termbuf\n");
    return;
  }    
  /*
  if ( p>=(g->cut_clvec)+2 &&
       p<=(g->cut_clvec)+((g->cut_clvec)[1]) ) {
    wr_printf("cut_clvec\n");
    return;
  }
  */
  wr_printf("unknown\n");
}

void wr_check_cl_memarea(glb* g, gptr xptr) {
  //void* db=g->db;
  int ruleflag,xlen,i,xatomnr;
  gint tmp=0;

  wr_printf("\nwr_check_cl_memarea for ptr %x is ",(int)(gint)xptr);
  if (!xptr) {
    wr_printf("NULL\n");
    return;
  }
  if (isdatarec(rpto(g,tmp))) {   
    wr_printf("record in termarea ");
    wr_print_glb_memarea(g,xptr);
    wr_print_record(g,xptr);
    wr_printf("\n");
  } else {
    wr_printf(" atomic \n");
    return;
  }  
  wr_printf("with atoms:\n");
  ruleflag=wg_rec_is_rule_clause(db,xptr);  
  if (!ruleflag) {
    xlen=get_record_len(xptr);
    for(i=RECORD_HEADER_GINTS+CLAUSE_EXTRAHEADERLEN;i<xlen+RECORD_HEADER_GINTS;i++) {
      tmp=xptr[i];
      if(isdatarec(tmp)) {
        wr_print_glb_memarea(g,rotp(g,tmp));      
      }  
    }
  } else {    
    xatomnr=wg_count_clause_atoms(db,xptr);
    for(i=0;i<xatomnr;i++) {
      tmp=wg_get_rule_clause_atom(db,xptr,i);
      if(isdatarec(tmp)) {
        wr_print_glb_memarea(g,rotp(g,tmp));      
      } 
    }   
  }  
}


#ifdef __cplusplus
}
#endif
