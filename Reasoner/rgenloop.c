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

 /** @file wr_genloop.c
 *  Procedures for reasoner top level search loops: given-clause, usable, sos etc.
 *
 */

/* ====== Includes =============== */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif


#ifdef _WIN32
#include "../config-w32.h"
#else
#include "../config.h"
#endif
#include "../Db/dballoc.h"
#include "../Db/dbdata.h"
#include "../Db/dbhash.h"
#include "../Db/dblog.h"
#include "../Db/dbindex.h"
#include "../Db/dbcompare.h"
  
#include "rincludes.h"

#ifdef __cplusplus
extern "C" {
#endif
  
/* ====== Private headers and defs ======== */

//#define DEBUG
//#undef DEBUG

//#define DEBUGHASH
//#undef DEBUGHASH

#define QUIET
//#undef QUIET

#define USE_RES_TERMS // loop over active clauses in wr_resolve_binary_all_active
#define GIVEN_INTERVAL_TRACE 1000 // interval at which minimal trace is printed

/* ======= Private protos ================ */



/* ====== Functions ============== */



int wr_genloop(glb* g) {
  void* db=g->db;
  gptr picked_given_cl_cand;
  gptr given_cl_cand; 
  gptr given_cl;  
  //gptr simp_given_cl=NULL;
  gptr given_cl_as_active=NULL;
  //int i;
  //gptr cl;
  //gint clmeta; 
  //int propres;
  gint given_cl_metablock[CLMETABLOCK_ELS];
  clock_t curclock;
  float run_seconds,total_seconds,fullness; // passed_ratio
  int given_from_hyperqueue_flag;

#ifndef USE_RES_TERMS  
  gint ipassive;
  gint iactive;
  gptr activecl;
#endif  
  
#ifndef QUIET    
  wr_printf("========= rwr_genloop starting ========= \n");   
#endif  
  UNUSED(db);

  //printf("\n** clactivesubsume data from external:\n"); 
  //wr_show_clactivesubsume(r_kb_g(g));

  (g->run_start_clock)=clock();  
  //clear_active_cl_list(); // ???
  wr_clear_all_varbanks(g);  
  // loop until no more passive clauses available
  g->proof_found=0;
  g->clqueue_given=CVEC_START;
  
  (g->stat_given_candidates)=0;
  (g->stat_given_used)=0;

  //wr_make_prop_file(g);
  //propres=wr_prop_solve_current(g);
  //if (propres==2) {
  //    return 0;
  //} 
  for(;;) {     
    //printf("\n %d %d %d %d\n",(g->queryfocus_strat), (g->res_shortarglen_limit),(g->negpref_strat),(g->pospref_strat));
    // first check time
    curclock=clock();
    run_seconds = (float)(curclock - (g->run_start_clock)) / CLOCKS_PER_SEC;
    total_seconds = (float)(curclock - (g->allruns_start_clock)) / CLOCKS_PER_SEC;
    if ((g->max_run_seconds) && (run_seconds>(g->max_run_seconds))) return 2;
    if ((g->max_seconds) && (total_seconds>(g->max_seconds))) return 2; 
    if (g->max_run_seconds) g->passed_ratio=(float)run_seconds / (float)(g->max_run_seconds);
    else g->passed_ratio=0; 
    //wr_printf("\n run_seconds %f (g->max_run_seconds) %d g->passed_ratio %f\n",
    //  (float)run_seconds,(g->max_run_seconds),g->passed_ratio);
    // change strategy for endgame
      
    if ((g->instgen_strat) &&  (int)((g->queue_termbuf)[0])<(int)((g->queue_termbuf)[1]+1000000)) {
       return 1;
    } else if (((g->passed_ratio)>0.95) && (g->max_run_seconds)>3 && (g->res_arglen_limit)!=1) {
      if (g->print_given_interval_trace) {
        wr_printf("\npassed time ratio %.2f",g->passed_ratio);
      }
      (g->res_arglen_limit)=1;           
    } else if (((g->passed_ratio)>0.85) &&  (g->res_shortarglen_limit)!=1) {
      if (g->print_given_interval_trace) {
        wr_printf("\npassed time ratio %.2f",g->passed_ratio);
      }
      (g->res_shortarglen_limit)=1;
    } else if (((g->passed_ratio)>0.7) && 
               (!(g->res_shortarglen_limit) || (g->res_shortarglen_limit)>2) ) {
      if (g->print_given_interval_trace) {
        wr_printf("\npassed time ratio %.2f",g->passed_ratio);      
      }
      (g->res_shortarglen_limit)=2;
    } else if (((g->passed_ratio)>0.5) &&  (g->pick_given_queue_ratio)!=100) {
      if (g->print_given_interval_trace) {
        wr_printf("\npassed time ratio %.2f",g->passed_ratio);      
      }
      (g->pick_given_queue_ratio)=100;
    }
    

#ifdef DEBUG
    wr_printf("wr_genloop for loop beginning queue is\n");
    //wr_show_clqueue(g);
    //printf("queue ended\n");
#endif     
    if (g->alloc_err) {
      wr_alloc_err(g,"Unhandled alloc_err detected in the main wr_genloop\n");
      return -1;
    }         
    //wr_show_clactive(g);
    //picked_given_cl_cand=wr_pick_given_cl(g,given_cl_metablock); 
    given_from_hyperqueue_flag=0;  
    picked_given_cl_cand=wr_pick_from_hyper_queue(g,(g->hyper_queue),given_cl_metablock);  
    if (picked_given_cl_cand==NULL) {
      //printf("\ncandidate NOT from hyper_queue:\n");
      picked_given_cl_cand=wr_pick_from_clpick_queues(g,rotp(g,g->clpick_queues),given_cl_metablock);
    } else {
      //printf("\ncandidate from hyper_queue:\n");
      given_from_hyperqueue_flag=1;
      (g->stat_given_candidates_hyper)++;
    }
    if (!picked_given_cl_cand && !(g->stat_given_candidates)) {
       if ((g->print_flag) && (g->print_runs)) wr_printf("No candidate clauses found.\n");
       if (wr_have_answers(g)) return 0;
       else return 2; // code for no candidates at all.
    }
    // given_kept_flag will now indicate whether to add to active list or not
    if (g->print_initial_given_cl) {    
      wr_printf("\n**** given candidate %d: ",(g->stat_given_candidates));
      if (g->print_flag) {
        //wr_print_clause_name_history(g,wr_get_history(g,picked_given_cl_cand));
        wr_print_clause(g,picked_given_cl_cand);          
      }  
    }       
    if (picked_given_cl_cand==NULL) {
      return 1;
    }
    if ((g->res_arglen_limit) && (wg_rec_is_rule_clause(db,picked_given_cl_cand))) {
      if (wr_count_cl_nonans_atoms(g,picked_given_cl_cand) > (g->res_arglen_limit)) {
        continue;
      }
    }
    (g->stat_given_candidates)++; //stats 
    /*
    handling picked given_cl_cand:
      maybe should not happen: check if True or False: either drop or proof found
      demodulate, calculate and cut/subsume result with hash units:
        if True or subsumed, drop
        else if any of demod/calc/cut succeeds, create a fully new clause with history        
          and use that in the following
      fully subsume with active clauses: if subsumed, drop      
    */  
    if ((g->back_subsume) && wr_blocked_clause(g,picked_given_cl_cand)) {
      //printf("\n given is blocked\n");
      continue;
    } else if ((g->propagate) && wg_rec_is_blocked_clause(db,picked_given_cl_cand)) {     
      //printf("\n given is blocked\n");
      continue;      
    }
    wr_process_given_cl_setupsubst(g,g->given_termbuf,1,1); // !!!!! new try      
    wr_sort_cl(g, picked_given_cl_cand);
    given_cl_cand=wr_simplify_cl(g, picked_given_cl_cand, given_cl_metablock);
    //wr_print_cl_literals_meta(g, picked_given_cl_cand);
    wr_process_given_cl_cleanupsubst(g);
    if (given_cl_cand==NULL) {
      //printf("\nwr_activate_passive_cl returned null\n");
      if (g->alloc_err) return -1;
      else if (g->proof_found) {
#ifdef DEBUG
        wr_printf("\nproof found, returning 0\n");
#endif         
        if (wr_enough_answers(g)) { return 0; }
        else { continue; }
      }
      // otherwise the candidate was subsumed or otherwise useless
      continue; 
    }   
    wr_calc_clause_meta(g,given_cl_cand,given_cl_metablock);
    // -- check part 1 starts ---
    if ((gint)given_cl_cand==ACONST_FALSE) {
      wr_printf("\nggiven_cl_cand is ACONST_FALSE\n");
      continue;
    }
    if ((gint)given_cl_cand==ACONST_TRUE) {
      wr_printf("\ngiven_cl_cand is ACONST_TRUE\n");
      continue;
    }

    // -- check part 1 ends ---

    if (wr_given_cl_subsumed(g,given_cl_cand,given_cl_metablock)) {
#ifdef DEBUG
      wr_printf("\ngiven cl is subsumed\n");
#endif    
      continue;
    }
    given_cl=wr_process_given_cl(g,given_cl_cand,g->given_termbuf); 
    if (given_cl==NULL) {
      if (g->alloc_err) return -1;
      continue; 
    }  
    // -- check part 2 starts ---
    if ((gint)given_cl==ACONST_FALSE) {
      //printf("\ngiven cl is ACONST_FALSE\n");
      continue;
    }
    if ((gint)given_cl==ACONST_TRUE) {
      //printf("\ngiven cl is ACONST_TRUE\n");
      continue;
    }
    // -- check part 2 ends ---
#ifdef RECORD_HISTORY_ORDER
    wr_set_history_record_given_order(g,
      rotp(g,wr_get_history(g,given_cl)));  
#endif   

    //if ((g->stat_given_used)>119) exit(0);

    if (g->print_final_given_cl) {
      wr_printf("\n*** given %d: ",(g->stat_given_used));
      wr_print_clause(g,given_cl);   
    } else if (g->print_given_interval_trace) {
      if ((g->stat_given_used)%GIVEN_INTERVAL_TRACE==0 && (g->stat_given_used)) {        
        wr_printf("\n%d given",(g->stat_given_used));
        if (((g->queue_termbuf)[1]-1) > 0) {
          fullness=((float)((g->queue_termbuf)[1]-1) / (float)((g->queue_termbuf)[0])); 
          wr_printf(" filled %.0f%% ",100*fullness);
        }
      }
    }
    //if ((g->stat_given_used)>233) return; //223
    //printf("\nmetablock2 %d %d %d %d \n",*given_cl_metablock,*(given_cl_metablock+1),*(given_cl_metablock+2),*(given_cl_metablock+3));
    // optionally do backsubsumption
    if (g->back_subsume) wr_given_cl_backsubsume(g,given_cl,given_cl_metablock);
    // calculate resolvability: (g->tmp_resolvability_vec)
    if (given_from_hyperqueue_flag) {
      wr_calc_clause_resolvability(g,given_cl,0,1);
    } else {
      wr_calc_clause_resolvability(g,given_cl,0,0);
    }  
    if (g->print_initial_given_cl) {
      wr_print_clause_resolvability(g,given_cl);
    }  
    //wr_print_clause_hardnesses(g,given_cl); 

    if (given_from_hyperqueue_flag) {
      // make an active clause from partial hyper
      //printf("\n from hyperqueue\n");
      wr_process_given_cl_setupsubst(g,g->active_termbuf,2,0); 
      given_cl_as_active=wr_build_calc_cl(g,given_cl);      
      wr_process_given_cl_cleanupsubst(g); 
      if (given_cl_as_active==NULL) {
        if (g->alloc_err) return -1;
        continue; 
      }        
    } else {
      // add to active list
      //printf("\n from normal queue\n");
      given_cl_as_active=wr_add_given_cl_active_list(g,given_cl,given_cl_metablock,1,
                                    g->active_termbuf,(g->tmp_resolvability_vec));
    //printf("\nmetablock3 %d %d %d %d \n",*given_cl_metablock,*(given_cl_metablock+1),*(given_cl_metablock+2),*(given_cl_metablock+3));
    }
    if (given_cl_as_active==NULL) {
      if (g->alloc_err) return -1;
      continue; 
    }    
    // do factorizations with the given clause
    if (!(g->hyperres_strat) || (g->relaxed_hyperres_strat) || wr_hyperres_satellite_cl(g,given_cl)) { //(!(g->instgen_strat))  {
      wr_factor(g,given_cl,given_cl_as_active);
    }  
    if (g->alloc_err) return -1;
    // resolve with equality reflexive atom X=X

    if (g->use_equality) {
      wr_resolve_equality_reflexive(g,given_cl,given_cl_as_active); 
    }      
    if ((g->proof_found) && wr_enough_answers(g)) return 0;
    if (g->alloc_err) return -1;  

    // do all resolutions with the given clause
    // normal case: active loop is done inside the wr_resolve_binary_all_active    
    wr_resolve_binary_all_active(g,given_cl,given_cl_as_active,(g->tmp_resolvability_vec)); 
    if ((g->proof_found) && wr_enough_answers(g)) return 0;
    if (g->alloc_err) return -1;    
    
    if ((g->res_arglen_limit) && (g->res_arglen_limit)<2) {
      continue;
    }
    if (g->use_equality) {
      wr_paramodulate_from_all_active(g,given_cl,given_cl_as_active,(g->tmp_resolvability_vec));    
      if ((g->proof_found) && wr_enough_answers(g)) return 0;
      if (g->alloc_err) return -1;          
      wr_paramodulate_into_all_active(g,given_cl,given_cl_as_active,(g->tmp_resolvability_vec));    
      if ((g->proof_found) && wr_enough_answers(g)) return 0;
      if (g->alloc_err) return -1;       
    }
    
  }
  // loop ended: either nothing to do or timeout
  if (wr_have_answers(g)) {
    return 0;
  } else {    
    return 1;      
  }
}  


gptr wr_pick_given_cl(glb* g, gptr given_cl_metablock) {
  gptr cl;
  int next;
  
#ifdef DEBUG  
  wr_printf("picking cl nr %d as given\n",g->clqueue_given);
#endif  
  //if (g->clqueue_given>=4) exit(0);
  // first try stack
#ifdef DEBUG  
  wr_printf("trying stack\n");
#endif  

#ifdef DEBUG  
  wr_printf("trying priority queue:\n");
  //wr_print_priorqueue(g,rotp(g,g->clpickpriorqueue));
#endif
  // then try priority queue
  if ((g->pick_given_queue_ratio_counter)<(g->pick_given_queue_ratio)) {
    cl=wr_pick_priorqueue(g, rotp(g,(g->clpickpriorqueue)));
    if (cl!=NULL) {     
      (g->pick_given_queue_ratio_counter)++;
      //printf("\n**** prior *******\n");
      return cl;
    } 
  } else {
    (g->pick_given_queue_ratio_counter)=0;
  }
#ifdef DEBUG  
  wr_printf("trying queue\n");
#endif
  // then try queue
  next=CVEC_NEXT(rotp(g,g->clqueue));
  if (next>(g->clqueue_given)) {
    cl=(gptr)((rotp(g,g->clqueue))[g->clqueue_given]);           
    ++(g->clqueue_given); 
    wr_calc_clause_meta(g,cl,given_cl_metablock);
    return cl;
  }
  // no candidates for given found 
  return NULL;
}



/*

  buf: new clause will be built in buf: if NULL, wg_create_raw_record used
       standard value to pass for buf is g->given_termbuf

*/

gptr wr_process_given_cl(glb* g, gptr given_cl_cand, gptr buf) {  
  gptr given_cl; 

#ifdef DEBUG
  void* db=g->db;
  wr_printf("\nwr_process_given_cl called with \n");
  wr_printf("int %d type %d\n",(int)given_cl_cand,(int)(wg_get_encoded_type(db,given_cl_cand)));
  wr_print_record(g,given_cl_cand);
  wr_printf("\n");
  wr_print_clause(g,given_cl_cand);  
  wr_printf("\n");
#endif    
#ifdef DEBUG
  wr_printf("\nwr_process_given_cl to do wr_process_given_cl_setupsubst \n");
#endif  
  wr_process_given_cl_setupsubst(g,buf,1,1);
#ifdef DEBUG
  wr_printf("\nwr_process_given_cl to do wr_build_calc_cl \n");
#endif    
  given_cl=wr_build_calc_cl(g,given_cl_cand);
  if (!given_cl) return NULL;
#ifdef DEBUG
  wr_printf("\nwr_process_given_cl got a given_cl \n");
  wr_print_clause(g,given_cl); 
  wr_printf("\n");
#endif    

  // -- check 3 starts --
  if ((gint)given_cl==ACONST_FALSE) {
    //printf("\nrebuilt as false\n");
    return given_cl;
  } 
  if ((gint)given_cl==ACONST_TRUE) {
    //printf("\nrebuilt as true\n");
    return (gptr)ACONST_TRUE;
  } 
  if (given_cl==NULL) {
    //printf("\nrebuilt as NULL\n");
    return given_cl;
  }
  //printf("\nrebuilt as \n");
  //wr_print_record(g,given_cl);
  //wr_print_clause(g,given_cl);    
  
  // -- check 3 ends --

  wr_process_given_cl_cleanupsubst(g);
  if (given_cl==NULL) return NULL; // could be memory err  
  //wr_print_varbank(g,g->varbanks);
#ifdef DEBUG
  wr_printf("\nrebuilt as \n");
  wr_print_record(g,given_cl);
  wr_printf("\n");
  wr_print_clause(g,given_cl);  
  wr_printf("\n");
#endif  
  return given_cl;
} 

/*
  if subsflag==0, do not add to active subsumption structures (used for query focus)
*/

gptr wr_add_given_cl_active_list(glb* g, gptr given_cl, gptr given_cl_metablock, 
    int subsflag, gptr build_buffer, cvec resolvability) {  
  gptr active_cl;
  int hashadded;

#ifdef DEBUG
  void* db=g->db;
  wr_printf("\nwr_add_given_cl_active_list called with \n");
  wr_printf("int %d type %d\n",(int)given_cl,(int)(wg_get_encoded_type(db,given_cl)));
  wr_printf("\nwg_rec_is_fact_clause(db,rec): %d \n",wg_rec_is_fact_clause(g->db,given_cl));
  wr_printf("\nwg_count_clause_atoms(db,clause): %d \n",wg_count_clause_atoms(g->db,given_cl));
  wr_print_record(g,given_cl);
  wr_printf("\n");
  wr_print_clause(g,given_cl);  
  wr_printf("\n");
#endif          
  wr_process_given_cl_setupsubst(g,build_buffer,2,0); 
  active_cl=wr_build_calc_cl(g,given_cl);
  if (!active_cl) return NULL;
  wr_process_given_cl_cleanupsubst(g); 
  if (active_cl==NULL) return NULL; // could be memory err
#ifdef DEBUG
  wr_printf("\nwr_add_given_cl_active_list generated for storage \n");
  wr_printf("int %d type %d\n",(int)given_cl,(int)(wg_get_encoded_type(db,active_cl)));
  wr_print_record(g,active_cl);
  wr_printf("\n");
  wr_print_clause(g,active_cl);
  wr_printf("\n");
#endif    
  // add to a list of all active clauses
 
  if ((g->clactive)!=(gint)NULL) {
#ifdef DEBUG  
    wr_printf("\npushing to clactive pos %d\n",(int)((rotp(g,g->clactive))[1]));    
#endif     
    (g->clactive)=rpto(g,wr_cvec_push(g,rotp(g,(g->clactive)),(gint)active_cl));
  }  
  // add ground units to unithash  
  if (1) {  //(subsflag) {
    // only add to active subsumption structures if subsflag==1 (ie not for queryfocus big sos list)
    hashadded=wr_add_cl_to_active_unithash(g,active_cl);
    if (!hashadded && subsflag && (g->clactivesubsume)!=(gint)NULL) {    
  #ifdef DEBUG  
      wr_printf("\npushing to clactivesubsume pos %d\n",(int)((rotp(g,g->clactivesubsume))[1]));
  #endif  
      (g->clactivesubsume)=rpto(g,wr_cvec_push(g,rotp(g,(g->clactivesubsume)),given_cl_metablock[0]));
      (g->clactivesubsume)=rpto(g,wr_cvec_push(g,rotp(g,(g->clactivesubsume)),given_cl_metablock[1]));
      (g->clactivesubsume)=rpto(g,wr_cvec_push(g,rotp(g,(g->clactivesubsume)),given_cl_metablock[2]));
      (g->clactivesubsume)=rpto(g,wr_cvec_push(g,rotp(g,(g->clactivesubsume)),given_cl_metablock[3]));
      (g->clactivesubsume)=rpto(g,wr_cvec_push(g,rotp(g,(g->clactivesubsume)),given_cl_metablock[4]));
      (g->clactivesubsume)=rpto(g,wr_cvec_push(g,rotp(g,(g->clactivesubsume)),
          rpto(g,(gint)active_cl))); 
      //printf("\n cl offset stored in (g->clactivesubsume): %ld", rpto(g,(gint)active_cl));     
    }  
  }
  //wr_show_clactivesubsume(g);

  //  store neg and pos preds to hash_neg/pos_atoms and store para terms
  wr_cl_store_res_terms(g,active_cl,resolvability);  
  if ((g->use_equality) && (g->use_equality_strat)) {   
    wr_cl_store_para_terms(g,active_cl,resolvability);
  } 
  (g->stat_given_used)++;  // stats   
  return active_cl;
} 


int wr_add_cl_to_active_unithash(glb* g, gptr cl) {
  void* db;
  int len;
  gint clmeta;
  gptr xatom;
  gint hash;  
  int ruleflag;

  db=g->db;   
  UNUSED(db);
 
  ruleflag=wg_rec_is_rule_clause(db,cl);
  if (ruleflag) len = wg_count_clause_atoms(db, cl);
  else len=1;
#ifdef DEBUGHASH
  wr_printf("\nwr_add_cl_to_active_unithash:\n");
  wr_print_clause(g,cl);
  wr_printf("\nlen %d ruleflag %d\n",len,ruleflag);
#endif
  if (len==1) { 
    if (ruleflag) {
      xatom=rotp(g,wg_get_rule_clause_atom(db,cl,0));        
      clmeta=wg_get_rule_clause_atom_meta(db,cl,0);
/* 
#ifdef DEBUGHASH
      printf("\nNB! NB! len==1, is rule clause, meta: %ld, is ground: %d\n",
        clmeta,wg_atom_meta_is_ground(db,clmeta));
#endif      
*/
      if (!wg_atom_meta_is_ground(db,clmeta)) return 0;
    } else {
      // not a rule clause
      xatom=cl; 
      //xatom=encode_record(db,cl);           
    }   
    hash=wr_lit_hash(g,rpto(g,xatom));
    //printf("\nhash for storing is %d\n",(int)hash);
    if (ruleflag && wg_atom_meta_is_neg(db,clmeta)) {
      //printf("\nneg\n");
      wr_push_termhash(g,rotp(g,g->hash_neg_active_groundunits),hash,xatom,cl);
      //wr_print_termhash(g,rotp(g,g->hash_neg_groundunits));
    } else {
      //printf("\npos\n");
      wr_push_termhash(g,rotp(g,g->hash_pos_active_groundunits),hash,xatom,cl); 
      //wr_print_termhash(g,rotp(g,g->hash_pos_groundunits));
    }    
/*
#ifdef DEBUGHASH    
    wr_printf("\ng->hash_neg_active_groundunits after adding:");      
    wr_print_termhash(g,rotp(g,g->hash_neg_active_groundunits));
    //wr_clterm_hashlist_print(g,rotp(g,g->hash_neg_groundunits));
    wr_printf("\ng->hash_pos_active_groundunits after adding:");   
    wr_print_termhash(g,rotp(g,g->hash_pos_active_groundunits));   
    //wr_clterm_hashlist_print(g,rotp(g,g->hash_pos_groundunits));
#endif
*/
    return 1;
  }
  return 0;
}

void wr_process_given_cl_setupsubst(glb* g, gptr buf, gint banknr, int reuseflag) {
  g->build_rewrite=0;   // default is no rewriting during building
  g->build_subst=0;     // subst var values into vars
  g->build_calc=0;      // do fun and pred calculations
  g->build_dcopy=0;     // copy nonimmediate data (vs return ptr)
  //g->build_buffer=NULL; // build everything into tmp buffer (vs main area)
  if (reuseflag && buf!=NULL) buf[1]=2; // reuse given_termbuf
  g->build_buffer=buf;
  g->build_rename=1;   // do var renaming
  g->build_rename_maxseenvnr=-1; // tmp var for var renaming
  g->build_rename_vc=0;    // tmp var for var renaming 
  g->build_rename_banknr=banknr; // nr of bank of created vars
  // points to bank of created vars
  g->build_rename_bank=(g->varbanks)+((g->build_rename_banknr)*NROF_VARSINBANK);  
  g->tmp_unify_vc=((gptr)(g->varstack))+1;
}

void wr_process_given_cl_cleanupsubst(glb* g) {
  
  wr_clear_varstack(g,g->varstack);
  //for(i=0;i<g->build_rename_vc;i++) {
  //  (g->build_rename_bank)[i]=UNASSIGNED;
  //}  
}  



#ifdef __cplusplus
}
#endif
