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



/*
  Experimental weight limit per queue block (strategy key
  queue_weight_limit): the budget of the coming picks, and a new
  computation of the limits every QWL_UPDATE_PICKS picks.

  Budgets: with max_given, (max_given - given clauses so far) x picks per
  given clause so far, from counters only. With a time limit, from the
  time left of the limit that ends first: value 1, picks so far x time
  left / time spent since the first pick; values 2 to 4, the picks of
  the last tenth of the time limit x time left / the length of that
  stretch. The smaller budget counts. No limit before 10 picks, and no
  time budget before 5% of the time.
*/

#define QWL_UPDATE_PICKS 32
#define QWL_SAMPLES 2048

// g->qwl_samples: QWL_SAMPLES pairs (run seconds, picks), allocated at the
// first run with values 2 to 4; without it the window starts at the first pick

static void wr_qwl_reset(glb* g) {
  if (!(g->qwl_samples) && (g->queue_weight_limit)>=2)
    (g->qwl_samples)=sys_malloc(sizeof(double)*2*QWL_SAMPLES);
  (g->qwl_nsamples)=0;
  (g->qwl_sample_next)=0;
  (g->qwl_first_pick_time)=-1;
  (g->qwl_active)=0;
  (g->qwl_next_update)=0;
}

static void wr_qwl_update(glb* g, float run_seconds, float total_seconds) {
  gint picks=(g->stat_given_candidates);
  double budget=-1, b, limit=0, elapsed=0, left=-1, spent, window, t0;
  gint p0;
  int i, k;

  if ((g->qwl_first_pick_time)<0 && picks>0) (g->qwl_first_pick_time)=run_seconds;
  if (picks<10 || picks<(g->qwl_next_update)) return;
  (g->qwl_next_update)=picks+QWL_UPDATE_PICKS;
  if (g->qwl_samples) {
    (g->qwl_samples)[2*(g->qwl_sample_next)]=run_seconds;
    (g->qwl_samples)[2*(g->qwl_sample_next)+1]=(double)picks;
    (g->qwl_sample_next)=((g->qwl_sample_next)+1)%QWL_SAMPLES;
    if ((g->qwl_nsamples)<QWL_SAMPLES) (g->qwl_nsamples)++;
  }
  // the budget from max_given: counters only
  if ((g->max_given)>0 && (g->stat_given_used)>0) {
    b=(double)((g->max_given)-(g->stat_given_used))*(double)picks/(double)(g->stat_given_used);
    if (b<0) b=0;
    budget=b;
  }
  // the time limit that ends first
  if (g->max_run_seconds) {
    limit=(double)(g->max_run_seconds); elapsed=run_seconds; left=limit-elapsed;
  }
  if ((g->max_run_dseconds) && (left<0 || (g->max_run_dseconds)/10.0-run_seconds<left)) {
    limit=(g->max_run_dseconds)/10.0; elapsed=run_seconds; left=limit-elapsed;
  }
  if ((g->max_seconds) && (left<0 || (double)(g->max_seconds)-total_seconds<left)) {
    limit=(double)(g->max_seconds); elapsed=total_seconds; left=limit-elapsed;
  }
  if (limit>0 && elapsed>=0.05*limit && (g->qwl_first_pick_time)>=0) {
    if (left<0) left=0;
    b=-1;
    if ((g->queue_weight_limit)==1) {
      spent=run_seconds-(g->qwl_first_pick_time);
      if (spent>0) b=(double)picks*left/spent;
    } else {
      // the oldest sample inside the window of a tenth of the limit
      window=0.1*limit;
      t0=(g->qwl_first_pick_time); p0=0;
      for(i=1;i<=(g->qwl_nsamples);i++) {
        k=((g->qwl_sample_next)-i+QWL_SAMPLES)%QWL_SAMPLES;
        if ((g->qwl_samples)[2*k]<run_seconds-window) break;
        t0=(g->qwl_samples)[2*k]; p0=(gint)((g->qwl_samples)[2*k+1]);
        if (i==(g->qwl_nsamples) && (g->qwl_nsamples)<QWL_SAMPLES) { t0=(g->qwl_first_pick_time); p0=0; }
      }
      if (run_seconds-t0>0 && picks>p0) b=(double)(picks-p0)*left/(run_seconds-t0);
    }
    if (b>=0 && (budget<0 || b<budget)) budget=b;
  }
  if (budget<0) { (g->qwl_active)=0; return; }
  wr_qwl_compute(g,rotp(g,g->clpick_queues),budget);
}

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
  float run_seconds,run_dseconds,total_seconds,fullness; // passed_ratio
  int given_from_hyperqueue_flag,tmp,origlen,simplen;
  int demod_same=0; // demod_source 2: the pick did not change the picked clause

#ifndef USE_RES_TERMS  
  gint ipassive;
  gint iactive;
  gptr activecl;
#endif  
  
#ifndef QUIET    
  wr_printf("========= rwr_genloop starting ========= \n");   
#endif  
  UNUSED(db);

  //wr_print_active_clauses(g);
  //printf("\n** clactivesubsume data from external:\n"); 
  //wr_show_clactivesubsume(r_kb_g(g));

  if (g->ti_selftest) {
    wr_ti_selftest(g);
    fflush(stdout);
    exit(0);
  }
  (g->run_start_clock)=clock();  
  (g->derived_overflow_timeout)=0;
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
  //wr_print_strat_flags(g);

  if (g->back_subsume) {
    if ((g->backsubsume_values) && (g->backsubsume_bytes)) {     
      // setup
      memset((g->backsubsume_values),0,(g->backsubsume_bytes));    
    }  
  }  

  if (g->queue_weight_limit) wr_qwl_reset(g);
  for(;;) {     
    /*
    if (g->hardnesspref_strat) {
      (g->print_initial_given_cl)=1;
      (g->print_final_given_cl)=1;
      printf("(g->stat_given_candidates) %d\n",(g->stat_given_candidates));
      if ((g->stat_given_used)>200) break;
    }
    */

    //printf("\n %d %d %d %d\n",(g->queryfocus_strat), (g->res_shortarglen_limit),(g->negpref_strat),(g->pospref_strat));
    // first check time
    curclock=clock();
    run_seconds = (float)(curclock - (g->run_start_clock)) / CLOCKS_PER_SEC;
    run_dseconds = (float)(curclock - (g->run_start_clock)) / ((float)CLOCKS_PER_SEC / 10.0);
    total_seconds = (float)(curclock - (dbmemsegh(db)->allruns_start_clock)) / CLOCKS_PER_SEC;
    if ((g->max_run_seconds) && (run_seconds>(g->max_run_seconds))) return 2;
    if ((g->max_run_dseconds) && (run_dseconds>(g->max_run_dseconds))) return 2;
    if ((g->max_seconds) && (total_seconds>(g->max_seconds))) return 2;
    // stop after max_given given clauses with the return code of a time limit
    if ((g->max_given)>0 && (g->stat_given_used)>=(g->max_given)) return 2;
    if (g->max_run_dseconds) {
      if (g->max_run_dseconds) g->passed_ratio=(float)run_dseconds / (float)(g->max_run_dseconds);
      else g->passed_ratio=0; 
    } else {  
      if (g->max_run_seconds) g->passed_ratio=(float)run_seconds / (float)(g->max_run_seconds);
      else g->passed_ratio=0; 
    }
    
    //printf("\n run_seconds %f total_seconds %f (g->max_run_seconds) %d (g->max_seconds) %d g->passed_ratio %f\n",
    //  (float)run_seconds,total_seconds,(g->max_run_seconds),(g->max_seconds),g->passed_ratio);
    /*  
    printf("(g->pick_given_queue_ratio) %ld (g->res_shortarglen_limit) %ld (g->res_arglen_limit) %ld\n", 
      (g->pick_given_queue_ratio), (g->res_shortarglen_limit), (g->res_arglen_limit));
    printf("(g->stat_given_candidates) %ld (g->stat_para_derived_cl) %ld\n",
      (g->stat_given_candidates),(g->stat_para_derived_cl));  
    */  
    // change strategy for endgame
    /*
    if ((g->stat_given_used)>=1050) {
      printf("\n (g->stat_given_used) %d\n", (g->stat_given_used));
      (g->print_level_flag)=50;
    }
    */
    if (g->queue_weight_limit) wr_qwl_update(g,run_seconds,total_seconds);
    float endgcoeff=1.0;
    if ((g->instgen_strat) &&  (int)((g->queue_termbuf)[0])<(int)((g->queue_termbuf)[1]+1000000)) {
       return 1;
    } else if (((g->passed_ratio)>(0.9*endgcoeff)) && !(g->endgame_mode)) {
               //&& (g->max_run_seconds)>3) {
      if (g->print_given_interval_trace) {
        wr_printf("\nfork %d: passed time ratio %.2f",g->current_fork_nr,g->passed_ratio);
      }
      (g->res_arglen_limit)=1;           
      (g->endgame_mode)=1;
      (g->stat_given_candidates_at_endgame)=(g->stat_given_candidates);
      (g->stat_given_used_at_endgame)=(g->stat_given_used);
    } else if (((g->passed_ratio)>(0.85*endgcoeff)) &&  (g->res_shortarglen_limit)!=1) {
      if (g->print_given_interval_trace) {
        wr_printf("\nfork %d: passed time ratio %.2f",g->current_fork_nr,g->passed_ratio);
      }
      (g->res_shortarglen_limit)=1;
    } else if (((g->passed_ratio)>(0.7*endgcoeff)) && 
               (!(g->res_shortarglen_limit) || (g->res_shortarglen_limit)>2) ) {
      if (g->print_given_interval_trace) {
        wr_printf("\nfork %d: passed time ratio %.2f",g->current_fork_nr,g->passed_ratio);      
      }
      (g->res_shortarglen_limit)=2;
    } else if (((g->passed_ratio)>(0.5*endgcoeff)) &&  (g->pick_given_queue_ratio)!=100) {
      if (g->print_given_interval_trace) {
        wr_printf("\nfork %d: passed time ratio %.2f",g->current_fork_nr,g->passed_ratio);      
      }
      (g->pick_given_queue_ratio)=100;

      // exp_2
      /*
      if (!(g->hardnesspref_strat)) {
        (g->negpref_strat)=0;
        (g->pospref_strat)=0;
        (g->hardnesspref_strat)=1;
      }
      (g->posunitpara_strat)=1;
      */ 
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
    if ((g->unit_propagate) || (g->unit_backsubsume)) {
      // unit propagation of the units kept since the last pick
      wr_up_run(g);
      if (g->alloc_err) return -1;
      if ((g->proof_found) && wr_enough_answers(g)) return 0;
    }
    given_from_hyperqueue_flag=0;  

    //if ((g->print_level_flag)==50) CP0;

    picked_given_cl_cand=wr_pick_from_hyper_queue(g,(g->hyper_queue),given_cl_metablock);  
    if (picked_given_cl_cand==NULL) {
      //printf("\ncandidate NOT from hyper_queue:\n");
      picked_given_cl_cand=wr_pick_from_clpick_queues(g,rotp(g,g->clpick_queues),given_cl_metablock);
    } else {
      //printf("\ncandidate from hyper_queue:\n");
      given_from_hyperqueue_flag=1;
      (g->stat_given_candidates_hyper)++;
    }
    //if ((g->print_level_flag)==50) CP1;
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
    //if ((g->print_level_flag)==50) CP2;
    if (picked_given_cl_cand==NULL) {
      return 1;
    }
    if ((g->res_arglen_limit) && (wg_rec_is_rule_clause(db,picked_given_cl_cand))) {
      if (wr_count_cl_nonans_atoms(g,picked_given_cl_cand) > (g->res_arglen_limit)) {
        continue;
      }
    }
    if ((g->endgame_mode) && (wr_count_cl_nonans_atoms(g,picked_given_cl_cand)>1)) {
      continue;
    }
    //if ((g->print_level_flag)==50) CP3;
    (g->stat_given_candidates)++; //stats 
    if (wr_tautology_cl(g,picked_given_cl_cand)) {
      continue;
    }
    /*
    handling picked given_cl_cand:
      maybe should not happen: check if True or False: either drop or proof found
      demodulate, calculate and cut/subsume result with hash units:
        if True or subsumed, drop
        else if any of demod/calc/cut succeeds, create a fully new clause with history        
          and use that in the following
      fully subsume with active clauses: if subsumed, drop      
    */  
    if ((g->back_subsume) && !(g->endgame_mode) && wr_get_cl_backsubsumed(g,picked_given_cl_cand)) {
      //printf("\n given is blocked\n");
      continue;
    } else if ((g->propagate) && wg_rec_is_blocked_clause(db,picked_given_cl_cand)) {     
      //printf("\n given is blocked\n");
      continue;      
    } else if ((g->up_index) && wr_up_unusable(g,picked_given_cl_cand)) {
      // made unusable by unit propagation: its shorter clause takes its place
      gptr r=picked_given_cl_cand; int steps=0;
      while(r && wr_up_unusable(g,r) && steps<64) { r=wr_up_get_repl(g,r); steps++; }
      if (!r || wr_up_unusable(g,r)) continue;
      picked_given_cl_cand=r;
    }
    origlen=wg_count_clause_atoms(db,picked_given_cl_cand);
    //if ((g->print_level_flag)==50) CP4;
    wr_process_given_cl_setupsubst(g,g->given_termbuf,1,1); // !!!!! new try      
    /*
    printf("\nbefore ");
    wr_print_clause(g,picked_given_cl_cand);
    printf("\nhardn ");
    wr_print_clause_hardnesses(g,picked_given_cl_cand);
    */
    if (!wg_in_attached_kb(picked_given_cl_cand)) { // never sort a shared kb clause in place
      wr_sort_cl(g, picked_given_cl_cand);
    }
    /*
    printf("\nafter ");
    wr_print_clause(g,picked_given_cl_cand);
    printf("\n");
    */
    //if ((g->print_level_flag)==50) CP5;
    // demod_source 2: the picked clause is a filed demodulator itself; it
    // must not rewrite itself (into s=s) at the pick
    if ((g->demod_source)==2) {
      (g->rewrite_self)=picked_given_cl_cand;
      given_cl_cand=wr_simplify_cl(g, picked_given_cl_cand, given_cl_metablock);
      (g->rewrite_self)=NULL;
      demod_same=(given_cl_cand==picked_given_cl_cand);
    } else {
      given_cl_cand=wr_simplify_cl(g, picked_given_cl_cand, given_cl_metablock);
    }
    //if ((g->print_level_flag)==50) CP6;
    //wr_print_cl_literals_meta(g, picked_given_cl_cand);
    //wg_print_record(db,picked_given_cl_cand);
    wr_process_given_cl_cleanupsubst(g);   
    //if ((g->print_level_flag)==50) CP7;
    if (given_cl_cand==NULL) {
      //printf("\nwr_simplify_cl returned null for given_cl_cand\n");
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
    if (wr_tautology_cl(g,given_cl_cand)) {
      continue;
    }
    simplen=wg_count_clause_atoms(db,given_cl_cand);
    if (!wg_in_attached_kb(given_cl_cand)) { // kb clause metas were stored at build time
      wr_calc_clause_meta(g,given_cl_cand,given_cl_metablock);
    }
    // -- check part 1 starts ---
    if ((gint)given_cl_cand==ACONST_FALSE) {
      wr_printf("\ngiven_cl_cand is ACONST_FALSE\n");
      continue;
    }
    if ((gint)given_cl_cand==ACONST_TRUE) {
      wr_printf("\ngiven_cl_cand is ACONST_TRUE\n");
      continue;
    }
    tmp=wr_prop_truefalsity_clause(g,given_cl_cand);
    if (tmp==1) {        
      continue;
    } else if (tmp==(0-1)) {       
      g->proof_found=1;
      g->proof_history=wr_get_history(g,given_cl_cand);
      wr_register_answer(g,NULL,g->proof_history);
      if (wr_enough_answers(g)) { return 0; }
      else { continue; }  
    }     
    // -- check part 1 ends ---

    if (!(g->endgame_mode) && wr_given_cl_subsumed(g,given_cl_cand,given_cl_metablock)) {      
#ifdef DEBUG
      wr_printf("\ngiven cl is subsumed\n");
#endif    
      continue;
    }
    if (g->arith_instantiation && g->use_comp_funs_strat &&
        g->use_comp_arithmetic && g->have_nonground_arithmetic &&
        g->stat_arithinst_probes < g->arith_inst_global_limit) {
      wr_generate_arith_instances(g,given_cl_cand);
      if (g->alloc_err) return -1;
      if (g->proof_found && wr_enough_answers(g)) return 0;
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
      if ((g->stat_given_used)>=10000 &&
          (g->stat_given_used)%(GIVEN_INTERVAL_TRACE*10)==0 && (g->stat_given_used)) {        
        wr_printf("\nfork %d: %d given",g->current_fork_nr,(g->stat_given_used));
        if (((g->queue_termbuf)[1]-1) > 0) {
          fullness=((float)((g->queue_termbuf)[1]-1) / (float)((g->queue_termbuf)[0])); 
          wr_printf(" filled %.0f%% ",100*fullness);
        }
      } else if ((g->stat_given_used)<10000 &&
                (g->stat_given_used)%GIVEN_INTERVAL_TRACE==0 && (g->stat_given_used)) {        
        wr_printf("\nfork %d: %d given",g->current_fork_nr,(g->stat_given_used));
        if (((g->queue_termbuf)[1]-1) > 0) {
          fullness=((float)((g->queue_termbuf)[1]-1) / (float)((g->queue_termbuf)[0])); 
          wr_printf(" filled %.0f%% ",100*fullness);
        }
      }
    }
    //if ((g->stat_given_used)>233) return; //223
    //printf("\nmetablock2 %d %d %d %d \n",*given_cl_metablock,*(given_cl_metablock+1),*(given_cl_metablock+2),*(given_cl_metablock+3));
    // optionally do backsubsumption
    if (!(g->endgame_mode) && (g->back_subsume)) wr_given_cl_backsubsume(g,given_cl,given_cl_metablock);
    // calculate resolvability: (g->tmp_resolvability_vec)

    if (given_from_hyperqueue_flag) {
      wr_calc_clause_resolvability(g,given_cl,0,1);
    } else {
      wr_calc_clause_resolvability(g,given_cl,0,0);
    }  
    if (g->print_initial_given_cl) {
      wr_print_clause_resolvability(g,given_cl);
    }  

    /*
    printf("\nresolvability ");
    wr_print_clause_resolvability(g,given_cl);
    printf("\nhardnesses    ");
    wr_print_clause_hardnesses(g,given_cl); 
    printf("\n");
    */
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
    } else { //if (!(g->endgame_mode)) {
      // add to active list
      //printf("\n from normal queue\n");
      // demod_source 2: the picked clause was filed when it was kept; a
      // clause that the pick changed is a new clause and is filed now
      if ((g->demod_source)==2) {
        (g->demod_skip_active)=demod_same;
        given_cl_as_active=wr_add_given_cl_active_list(g,given_cl,given_cl_metablock,1,
                                      g->active_termbuf,(g->tmp_resolvability_vec));
        (g->demod_skip_active)=0;
      } else {
        given_cl_as_active=wr_add_given_cl_active_list(g,given_cl,given_cl_metablock,1,
                                      g->active_termbuf,(g->tmp_resolvability_vec));
      }
    //printf("\nmetablock3 %d %d %d %d \n",*given_cl_metablock,*(given_cl_metablock+1),*(given_cl_metablock+2),*(given_cl_metablock+3));
    }
    if (given_cl_as_active==NULL) {
      if (g->alloc_err) return -1;
      continue; 
    }    
    if ((g->unit_propagate) || (g->unit_backsubsume)) {
      // the picked clause leaves the queues; its active copy can be cut
      if (!wr_up_mark(g,picked_given_cl_cand)) { wr_alloc_err(g,"could not mark a picked clause for unit propagation"); return -1; }
      wr_up_register(g,given_cl_as_active);
    }
    if (simplen!=origlen && simplen==2) {
      // insert the active copy: given_cl lies in given_termbuf, which is
      // reused for the next given clause
      wr_add_cl_to_doublehash(g,given_cl_as_active);
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
    /*
    if (g->endgame_mode) {
      printf("endgame given_cl ");
      wr_print_clause(g,given_cl);
      printf("\n");
    }
    */
    wr_resolve_binary_all_active(g,given_cl,given_cl_as_active,(g->tmp_resolvability_vec)); 
    if ((g->proof_found) && wr_enough_answers(g)) return 0;
    if (g->alloc_err) return -1;        
    // next one blocks para for endgame
    //
    // SWW
    /*
     if ((g->res_arglen_limit) && !(g->res_strict_arglen_limit) && (g->res_arglen_limit)<2) {
      continue;
    } 
    */
    /*
    if ((g->res_arglen_limit) && !(g->res_strict_arglen_limit) && (g->res_arglen_limit)<2) {
      continue;
    }       
    */


    if ((g->use_equality) && (!(g->rewrite_only_strat))) {  
      if (!(g->endgame_mode) && 
          (!(g->prohibit_nested_para) || (wr_get_cl_history_tag(g,given_cl)!=WR_HISTORY_TAG_PARA)) ) {
        wr_paramodulate_from_all_active(g,given_cl,given_cl_as_active,(g->tmp_resolvability_vec));    
        if ((g->proof_found) && wr_enough_answers(g)) {
          //printf("\nrgenloop processing proof found by wr_paramodulate_from_all_active\n");
          return 0;
        }  
        if (g->alloc_err) return -1;                  
        wr_paramodulate_into_all_active(g,given_cl,given_cl_as_active,(g->tmp_resolvability_vec));    
        if ((g->proof_found) && wr_enough_answers(g)) return 0;
        if (g->alloc_err) return -1;          
      }      
    }       
  }
  // loop ended: either nothing to do or timeout
  if (wr_have_answers(g)) {
    return 0;
  } else { 
    //printf("\nnot found\n");   
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
    if (!wg_in_attached_kb(cl)) { // kb clause metas were stored at build time
      wr_calc_clause_meta(g,cl,given_cl_metablock);
    }
    return cl;
  }
  // fndidates for given found 
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
  if (g->endgame_mode) (g->build_rewrite)=0; 
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
#ifdef USE_LIT_HASH64
      gint fsipos=CVEC_NEXT(rotp(g,(g->clactivesubsume))); // position of this meta block
#endif
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
#ifdef USE_LIT_HASH64
      wr_fsi_insert(g,active_cl,fsipos);
#endif
      //printf("\n cl offset stored in (g->clactivesubsume): %ld", rpto(g,(gint)active_cl));     
    }  
  }
  //wr_show_clactivesubsume(g);

  //  store neg and pos preds to hash_neg/pos_atoms and store para terms 
  wr_cl_store_res_terms(g,active_cl,resolvability);  
  if (!(g->endgame_mode) && (g->use_equality) && (g->use_equality_strat)) {   
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
    hash=wr_lit_hash_unit(g,rpto(g,xatom),wr_lit_hash(g,rpto(g,xatom))); // the hash of the unit tables
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
