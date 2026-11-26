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

 /** @file derive.c
 * Clause derivation functions.  
 */

/* ====== Includes =============== */


#ifdef __cplusplus
extern "C" {
#endif

#include "rincludes.h"  
#include "../Db/dballoc.h"  // for ACONST_TRUE/FALSE

/* ====== Private defs =========== */

#define HARD_DEPTH_LIMIT 10000
#define HARD_SIZE_LIMIT 10000

//#define NONLENGTHENING_ONLY

//#define GLOBALUNITS

//#define DEBUG
//#undef DEBUG
//#define DEBUGHASH


/* ====== Private headers ======== */

/* ====== Functions ============== */


/*
  Literal hashes of a derived clause. wr_derived_cl_cut_and_subsume computes
  wr_lit_hash of each literal of the half-built clause and keeps the pairs
  (atom, hash) in lithash_atoms and lithash_vals. The removal of cut literals
  and of strong duplicates keeps the order of the other literals, and the
  final clause copies the atoms in that order without a renaming of the
  variables. So wr_lithash_final finds the hash of each literal of the final
  clause, and the final steps need not compute it again.
*/

int wr_lithash_reserve(glb* g, int n) {
  gint *atoms, *vals, *final;
  int size;

  if (n<=(g->lithash_size)) return 1;
  size=(n<16 ? 16 : 2*n);
  atoms=sys_realloc(g->lithash_atoms,size*sizeof(gint));
  if (atoms==NULL) return 0;
  (g->lithash_atoms)=atoms;
  vals=sys_realloc(g->lithash_vals,size*sizeof(gint));
  if (vals==NULL) return 0;
  (g->lithash_vals)=vals;
  final=sys_realloc(g->lithash_final,size*sizeof(gint));
  if (final==NULL) return 0;
  (g->lithash_final)=final;
  (g->lithash_size)=size;
  return 1;
}

/* the hashes of the rpos literals of rptr by position, or NULL if unknown */

static gint* wr_lithash_final(glb* g, gptr rptr, int rpos) {
  int i, j=0;
  gint atom;

  for(i=0;i<rpos;i++) {
    atom=rptr[(i*LIT_WIDTH)+LIT_ATOM_POS];
    while (j<(g->lithash_n) && (g->lithash_atoms)[j]!=atom) j++;
    if (j>=(g->lithash_n)) return NULL;
    (g->lithash_final)[i]=(g->lithash_vals)[j];
    j++;
  }
  return g->lithash_final;
}

/*
  The weight check of the final steps, run on the half-built clause before
  the history record and the final clause are built. Only factoring and
  paramodulation call it: they have no double cut, which can lower the
  weight of a resolvent before the late check. wr_derived_weight_check reads
  the fill level of queue_termbuf, which is lower here than after the copy;
  a higher fill level only adds reasons to drop. So a clause dropped here
  would be dropped by the late check as well, and a clause kept here goes
  the old way: copy, history record, late check. Returns 1 if dropped.
*/

/*
  GKC_WEIGHT_DROP_PLACE (compile time, plan item W7): where the weight
  rules drop a derived clause.
  0 (default): after the cuts, the duplicate removal and (for resolution)
    the double cut, at the late check in wr_process_derived_final, and in
    the early drop of factoring and paramodulation.
  1: in addition, right after the build of the half-built clause (with its
    merge of equal literals and its tautology test), before the unit cut,
    the unit subsumption and the strong cut: the present weight rules and,
    with queue_weight_limit, the queue limit. The later checks stay.
*/

#ifndef GKC_WEIGHT_DROP_PLACE
#define GKC_WEIGHT_DROP_PLACE 0
#endif

#if GKC_WEIGHT_DROP_PLACE==1
/*
  The weight rules on the half-built clause before the cuts. h1, h2: the
  parents that give the queue block. Returns 1 if dropped.
*/

static int wr_derived_weight_drop_precut(glb* g, gptr rptr, int rpos,
                                         int wxatomnr, int wyatomnr, gptr h1, gptr h2) {
  int i, ansnr=0, weight, size, depth, length, ruleflag;
  double avg;

  if ((g->hyperres_strat) || (g->print_derived_cl) || (g->print_derived)) return 0;
  for(i=0;i<rpos;i++) {
    if (wr_answer_lit(g,rptr[(i*LIT_WIDTH)+LIT_ATOM_POS])) ansnr++;
  }
  if (ansnr) return 0; // the final steps may register an answer
  ruleflag=wr_process_resolve_result_isrulecl(g,rptr,rpos);
  weight=wr_calc_lits_weight(g,ruleflag,rptr[LIT_ATOM_POS],rptr,rpos,&size,&depth,&length);
  if (!wr_derived_weight_check(g,g->avg_kept_weight,weight,size,depth,length,
                               wxatomnr,wyatomnr)) {
    (g->stat_weight_discarded_cl)++;
  } else if ((g->queue_weight_limit) && (g->qwl_active) &&
             wr_qwl_drop_weight(g,wr_qwl_block_from_parents(g,h1,h2),weight,length)) {
    (g->stat_queue_limit_dropped)++;
    if ((g->queue_weight_limit)==4) {
      avg=(g->avg_kept_weight);
      avg+=(weight-avg)/((g->stat_kept_cl)+(g->stat_queue_limit_dropped)+1);
      (g->avg_kept_weight)=avg;
    }
  } else {
    return 0;
  }
  // as the early drop: stat_built_cl and the clause id of the skipped
  // history record
  ++(g->stat_built_cl);
  if (g->store_history) (g->local_clid_next)++;
  return 1;
}
#endif

static int wr_derived_weight_drop_early(glb* g, gptr rptr, int rpos, int ruleflag,
                                        int wxatomnr, int wyatomnr) {
  int i, ansnr=0, weight, size, depth, length;

  // the final steps send these clauses past the weight check or print them
  if ((g->hyperres_strat) || (g->print_derived_cl) || (g->print_derived)) return 0;
  for(i=0;i<rpos;i++) {
    if (wr_answer_lit(g,rptr[(i*LIT_WIDTH)+LIT_ATOM_POS])) ansnr++;
  }
  if (ansnr==rpos) return 0; // an answer: the final steps register it
  weight=wr_calc_lits_weight(g,ruleflag,rptr[LIT_ATOM_POS],rptr,rpos,&size,&depth,&length);
  if (wr_derived_weight_check(g,g->avg_kept_weight,weight,size,depth,length,
                              wxatomnr,wyatomnr)) {
    // kept: the final clause has the same literals, so the final steps reuse
    // these values instead of computing them again
    (g->early_weight)[0]=weight;
    (g->early_weight)[1]=size;
    (g->early_weight)[2]=depth;
    (g->early_weight)[3]=length;
    (g->early_weight_known)=1;
    return 0;
  }
  (g->stat_weight_discarded_cl)++;
  // stat_built_cl counts the clauses that reach the final build or this
  // early weight check, as in the order before the check moved
  ++(g->stat_built_cl);
  // the skipped history record would have taken a clause id
  if (g->store_history) (g->local_clid_next)++;
  return 1;
}

/*
  The half-built clause of a derived clause is built in derived_termbuf. When
  it does not fit, wr_alloc_from_cvec sets derived_build_overflow instead of
  an allocation error, the build returns WG_ILLEGAL, and the caller drops the
  clause as it drops a tautology. wr_derived_build_end counts the drop and,
  when a time limit has passed, sets derived_overflow_timeout: the four
  wr_process_*_result functions then return at once, the inference loops in
  resolve.c end, and wr_genloop stops with its time-limit result.
*/

static void wr_derived_build_begin(glb* g) {
  (g->derived_build_active)=1;
  (g->derived_build_overflow)=0;
  (g->lithash_n)=0; // no literal hash of this clause is known yet
  (g->early_weight_known)=0; // no weight of this clause is known yet
}

static void wr_derived_build_end(glb* g) {
  clock_t curclock;
  float run_seconds, run_dseconds, total_seconds;

  if (g->derived_build_overflow) {
    (g->stat_derived_overflow_dropped)++;
    // a drop costs a full derived_termbuf: test the time limits of
    // wr_genloop here, since the inference loops do not test max_seconds
    curclock=clock();
    run_seconds=(float)(curclock-(g->run_start_clock)) / CLOCKS_PER_SEC;
    run_dseconds=(float)(curclock-(g->run_start_clock)) / ((float)CLOCKS_PER_SEC / 10.0);
    total_seconds=(float)(curclock-(dbmemsegh(g->db)->allruns_start_clock)) / CLOCKS_PER_SEC;
    if (((g->max_run_seconds) && (run_seconds>(g->max_run_seconds))) ||
        ((g->max_run_dseconds) && (run_dseconds>(g->max_run_dseconds))) ||
        ((g->max_seconds) && (total_seconds>(g->max_seconds)))) {
      (g->derived_overflow_timeout)=1;
    }
  }
  (g->derived_build_active)=0;
  (g->derived_build_overflow)=0;
}

/*
  Final steps of a derived clause, shared by wr_process_resolve_result,
  wr_process_factor_result, wr_process_paramodulate_result and
  wr_process_instgen_result. They start after res and its history are built.

  The flags keep each difference between the four former copies:
    sortflag         sort the literals of res first (instgen)
    doublecutflag    double cut with wr_simplify_doublecut_cl (resolve); a
                     NULL result returns without a rollback of the buffer
    hyperflag        partial hyperresolution clauses go to hyper_queue
                     (resolve, factor, paramodulate)
    weightcheckflag  wr_derived_weight_check with the parent lengths
                     wxatomnr, wyatomnr (resolve, factor with 0,0,
                     paramodulate; instgen has no weight check)
    doublehashflag   wr_add_cl_to_doublehash (resolve, factor, paramodulate)
    propclhashflag   non-unit clauses go to prop_hash_clauses with clhash
                     (instgen)
    propagateflag    unit propagation when g->propagate is set (resolve,
                     instgen)
  unittext, derivedtext and derivedtext2 are the print texts of the caller;
  derivedtext2 is the "from: " or "into: " of paramodulation, else NULL.
  The callers pass constant flags, so the compiler can specialize each call.
*/

static void wr_process_derived_final(glb* g, gptr res, gint* lithash, gint history,
      gint initial_queue_termbuf_next, gptr xcl_as_active,
      int sortflag, int doublecutflag, int hyperflag,
      int weightcheckflag, int wxatomnr, int wyatomnr,
      int doublehashflag, int propclhashflag, gint clhash, int propagateflag,
      char* unittext, char* derivedtext, char* derivedtext2) {
  int tmp;
  gint resmeta, hash;
  int weight;
  double avg;
  gint cl_metablock[CLMETABLOCK_ELS];
  int size,depth,length;

  UNUSED(unittext);
  if (sortflag) {
    wr_sort_cl(g,res);
    lithash=NULL; // the literal order may have changed
  }
  // check if result contains only ans predicates
  tmp=wr_cl_derived_is_answer(g,res);
  if (tmp>0) {
    wr_register_answer(g,res,history);
    g->proof_found=1;
    g->proof_history=history;
    return;
  }
  if (doublecutflag) {
    gptr simp2=NULL;
    gint cl_metablock2[CLMETABLOCK_ELS];
    simp2=wr_simplify_doublecut_cl(g,res,cl_metablock2,lithash);
    if (!simp2) {
      return;
    }
    if (simp2!=res) {
      res=simp2;
      lithash=NULL; // a literal was cut: positions changed
    }
  }
  // start storing to queues and hashes
  if (hyperflag && (g->hyperres_strat) &&  !wr_hyperres_satellite_cl(g,res)) {
    // hyperresolution case: not a finished clause yet, must cut off some
    ++(g->stat_hyperres_partial_cl);
    ++(g->stat_derived_partial_hyper_cl);
    if (g->print_partial_derived_cl) {
      wr_printf("\n+ partial derived by mp: ");
      wr_print_clause(g,res);
    }
    weight=wr_calc_clause_weight(g,res,&size,&depth,&length);
    tmp=wr_push_cl_hyper_queue(g,(g->hyper_queue),res,weight);
    if (!tmp) {
      wr_alloc_err(g,"could not store into hyper_queue in wr_process_resolve_result ");
      return; // could not alloc memory, could not store clause
    }
  } else {
    // ordinary case (not partial hyperres): resulting clause is finished
#ifdef GLOBALUNITS
    if (unittext) {
      int rflag=wg_rec_is_rule_clause(db,res);
      int rlen;
      if (rflag) rlen = wg_count_clause_atoms(db,res);
      else rlen=1;
      if (rlen==1) {
        wr_printf("%s",unittext);
        wr_print_clause(g,res);
      }
    }
#endif
    if (g->print_derived_cl) {
      wr_printf("%s",derivedtext);
      if (derivedtext2) wr_printf("%s",derivedtext2);
      wr_print_clause(g,res);
    } else if (g->print_derived) {
      wr_print_clause_via_buf(g,res);
    }
    if (g->early_weight_known) {
      // the early weight check computed these on the same literals
      weight=(g->early_weight)[0];
      size=(g->early_weight)[1];
      depth=(g->early_weight)[2];
      length=(g->early_weight)[3];
      (g->early_weight_known)=0;
    } else {
      weight=wr_calc_clause_weight(g,res,&size,&depth,&length);
    }
    avg=(g->avg_kept_weight);
    if (weightcheckflag &&
        !wr_derived_weight_check(g,avg,weight,size,depth,length,wxatomnr,wyatomnr)) {
      (g->stat_weight_discarded_cl)++;
      CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
      if (g->print_derived_cl) wr_printf("\nw discarded overweight");
      return;
    }
    // experimental: the clause lies beyond the reach of its queue block
    if ((g->queue_weight_limit) && weightcheckflag && wr_qwl_drop(g,res,weight)) {
      (g->stat_queue_limit_dropped)++;
      if ((g->queue_weight_limit)==4) {
        // the mean weight of the fill-level rule counts this clause as kept
        avg+=(weight-avg)/((g->stat_kept_cl)+(g->stat_queue_limit_dropped)+1);
        (g->avg_kept_weight)=avg;
      }
      CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
      if (g->print_derived_cl) wr_printf("\nw discarded beyond the queue limit");
      return;
    }
    ++(g->stat_kept_cl);
    if ((g->queue_weight_limit)==4) avg+=(weight-avg)/((g->stat_kept_cl)+(g->stat_queue_limit_dropped)+1);
    else avg+=(weight-avg)/((g->stat_kept_cl)+1);
    (g->avg_kept_weight)=avg;
    resmeta=wr_calc_clause_meta(g,res,cl_metablock);
    hash=wr_add_cl_to_unithash_h(g,res,resmeta,lithash);
    if (doublehashflag) wr_add_cl_to_doublehash_h(g,res,lithash);
    if (propclhashflag && hash<0) {
      // nonunit clause: add to clause hash
      wr_push_prop_clausehash(g,rotp(g,g->prop_hash_clauses),clhash,res);
    }
    if (propagateflag) {
      if (g->propagate) {
        if (hash>=0)  {
          // unit, propagate
          wr_propagate_unit(g,res,resmeta,hash,xcl_as_active);
          if (g->proof_found || g->alloc_err) {
            return;
          }
        } else {
          // nonunit, store occurrences
          wr_store_atom_occurrences(g,res);
        }
      }
#ifdef DEBUG
      wr_print_atomhash(g,rotp(g,(g->hash_atom_occurrences)));
#endif
    }
    if (g->use_strong_unit_cutoff) wr_cl_store_res_units(g,res);
    wr_push_cl_clpick_queues(g,(g->clpick_queues),res,weight);
    tmp=wr_cl_create_propinst(g,res);
    if (tmp==2) {  return;  }
  }
}

/* 

  Process binary res result

*/

/*

Where is the clause built / data blocks used:

the initial temp clause is built in: g->derived_termbuf
the final kept clause is built in:   g->queue_termbuf

* first phase, temp clause building in g->derived_termbuf

  wr_process_resolve_result_setupsubst(g); is set up for the next step only:

  first 
    wr_process_resolve_result_aux 
  is called for both parent clauses, it calls wr_build_calc_term for each lit        

* second phase is creating the final kept clause in g->queue_termbuf

  wr_process_resolve_result_setupquecopy(g); is set up for this step
 
  clause building is done in
    wr_derived_build_cl_from_initial_cl(g,rptr,rpos,ruleflag) 
  which again uses
    wr_build_calc_term(g,rptr[tmp+LIT_ATOM_POS]);

* third phase is storing the clause (not copying) to differen queues, hashes, lists etc.

*/


/*
  Experimental: a derived clause dropped before its build by the lower
  bound of queue_weight_bound. It takes the clause id of the history
  record that it skips, as the early weight drop does.
*/

static void wr_qwb_dropped(glb* g) {
  (g->stat_queue_bound_dropped)++;
  if (g->store_history) (g->local_clid_next)++;
}

void wr_process_resolve_result
      (glb* g, gint xatom, gptr xcl, gint yatom, gptr ycl, gptr xcl_as_active) {
  int xisrule,yisrule,xatomnr,yatomnr;
  int rlen;
  int tmp,tmp2;
  gptr rptr;
  int rpos,cutpos1=0,cutpos2=0;
  gptr res;  
  gint history;
  int ruleflag;
  gint initial_queue_termbuf_next=0;
  
#ifdef DEBUG
  wr_printf("\n+++ wr_process_resolve_result called\n");
  wr_print_clause(g,xcl); wr_printf(" : ");wr_print_term(g,xatom);
  wr_printf("\n");
  wr_print_clause(g,ycl);  wr_printf(" : ");wr_print_term(g,yatom);
  wr_printf("\n");
  wr_print_vardata(g);  
#endif  
  if (g->derived_overflow_timeout) return; // time limit passed at a drop
  ++(g->stat_derived_cl);
  ++(g->stat_binres_derived_cl);  
  // get basic info about clauses
  xisrule=wg_rec_is_rule_clause(db,xcl);
  yisrule=wg_rec_is_rule_clause(db,ycl);
  if (xisrule) xatomnr=wg_count_clause_atoms(db,xcl);
  else xatomnr=1;
  if (yisrule) yatomnr=wg_count_clause_atoms(db,ycl);
  else yatomnr=1;
  // reserve sufficient space in derived_termbuf for simple sequential store of atoms:
  // no top-level meta kept
  rlen=(xatomnr+yatomnr-2)*LIT_WIDTH;
  if (rlen==0) {
    g->proof_found=1;
    g->proof_history=wr_build_resolve_history(g,xcl_as_active,ycl,0,0,NULL);
    wr_register_answer(g,NULL,g->proof_history);
    return;
  }  
  // experimental: a lower bound of the weight above the queue limit
  if ((g->queue_weight_bound) && (g->qwl_active) &&
      wr_qwb_drop(g,xcl,xatom,-1,ycl,yatom,-1,xcl_as_active,ycl)) {
    wr_qwb_dropped(g);
    return;
  }
  (g->derived_termbuf)[1]=2; // init termbuf
  rptr=wr_alloc_from_cvec(g,g->derived_termbuf,rlen); 
  if (rptr==NULL) {
    ++(g->stat_internlimit_discarded_cl);
    wr_alloc_err(g,"could not alloc first buffer in wr_process_resolve_result ");
    return; // could not alloc memory, could not store clause
  }  
  //printf("xisrule %d yisrule %d xatomnr %d yatomnr %d rlen %d\n",
  //        xisrule,yisrule,xatomnr,yatomnr,rlen);
  // set up var rename params
  wr_process_resolve_result_setupsubst(g);  
  wr_derived_build_begin(g);
  // store all ready-built atoms sequentially, excluding duplicates
  // and looking for tautology: only for rule clauses needed    
  rpos=0;
  if (xatomnr>1) {
    tmp=wr_process_resolve_result_aux(g,xcl,xatom,xatomnr,rptr,&rpos,&cutpos1,-1,0);
    if (!tmp) {
      wr_process_resolve_result_cleanupsubst(g);       
      wr_derived_build_end(g);
      return; 
    }  
  }
  if (yatomnr>1) { 
    tmp=wr_process_resolve_result_aux(g,ycl,yatom,yatomnr,rptr,&rpos,&cutpos2,-1,0);
    if (!tmp) {
      wr_process_resolve_result_cleanupsubst(g);   
      wr_derived_build_end(g);
      return;
    }  
  }
  wr_process_resolve_result_cleanupsubst(g);  
  wr_derived_build_end(g);
  if (rpos==0) {
    g->proof_found=1;     
    g->proof_history=wr_build_resolve_history(g,xcl_as_active,ycl,cutpos1,cutpos2,NULL);
    wr_register_answer(g,NULL,g->proof_history);
    return;
  } 

  // check subsumption and cutoff
  if (g->print_derived_precut_cl) {
    wr_printf("\nc pre-cut derived by mp: ");
    wr_print_halfbuilt_clause(g,rptr,rpos);
  }
#if GKC_WEIGHT_DROP_PLACE==1
  if (wr_derived_weight_drop_precut(g,rptr,rpos,xatomnr,yatomnr,xcl_as_active,ycl)) return;
#endif
  tmp=wr_derived_cl_cut_and_subsume(g,rptr,rpos,NULL);
  if (tmp<0) {
    // clause was subsumed
#ifdef DEBUG      
    wr_printf("\nwr_process_resolve_result was subsumed with new subsumer\n");
#endif    
    if (g->print_derived_subsumed_cl) {
      wr_printf("\n- subsumed derived by mp: ");
      wr_print_halfbuilt_clause(g,rptr,rpos);
    }
    return;    
  } else {
    tmp2=0;
    if ((g->use_strong_unit_cutoff) && rpos) {
      tmp2=wr_derived_cl_strong_cut(g,rptr,rpos,tmp);
    }
    if (tmp>0 || tmp2>0) {
      // there were cuts
      if (g->print_derived_precut_cl) {
        wr_printf("\nc post-cut derived by mp: ");
        wr_print_halfbuilt_clause(g,rptr,rpos);
      }

      
      //printf("\ntmp %d tmp2 %d\n",tmp,tmp2);
      //CP0

      wr_process_resolve_result_remove_cuts(g,rptr,&rpos,tmp+tmp2);
      (g->stat_derived_cut)++;
      if (rpos==0) {
        g->proof_found=1;    
        g->proof_history=wr_build_resolve_history(g,xcl_as_active,ycl,cutpos1,cutpos2,g->cut_clvec); 
        wr_register_answer(g,NULL,g->proof_history);
        return;
      } 
    }  
  }
  // remove strong duplicates  
  //printf("\nrpos starts with %d\n",rpos);
  if ((rpos>1) && (g->use_strong_duplicates)) {    
    //printf("\nrpos before %d\n",rpos);
    wr_find_strong_duplicates(g,rptr,&rpos);
    //printf("\nrpos after %d\n",rpos);
  }  
  /* 
  } else if (tmp>0) {
    // there were cuts
    if (g->print_derived_precut_cl) {
      wr_printf("\nc post-cut derived by mp ");
      wr_print_halfbuilt_clause(g,rptr,rpos);
    }
    wr_process_resolve_result_remove_cuts(g,rptr,&rpos,tmp);
    (g->stat_derived_cut)++;
    if (rpos==0) {
      g->proof_found=1;    
      g->proof_history=wr_build_resolve_history(g,xcl_as_active,ycl,cutpos1,cutpos2,g->cut_clvec); 
      wr_register_answer(g,NULL,g->proof_history);
      return;
    } 
  } else {
    if (g->use_strong_unit_cutoff) wr_derived_cl_strong_cut(g,rptr,rpos); 
  }
  */

  // now we have stored all subst-into and renamed metas/atoms into rptr: build clause  

  //printf("filled meta/atom new vec, rpos %d\n",rpos);          

  // check whether should be stored as a ruleclause or not
  ruleflag=wr_process_resolve_result_isrulecl(g,rptr,rpos);  
  // create new record
  if ((g->hyperres_strat) &&  !wr_hyperres_satellite_tmpres(g,rptr,rpos)){
    // must still cut something off: not fully finished hyperres
    //partialresflag=1;      
    wr_process_resolve_result_setuphypercopy(g); // use g->queue_hyperbuf as g->build_buffer
    initial_queue_termbuf_next=CVEC_NEXT(g->build_buffer); // to be restored if not actually used
  } else {
    // fully formed, storable result
    //partialresflag=0;
    wr_process_resolve_result_setupquecopy(g); // use g->queue_termbuf as g->build_buffer
    initial_queue_termbuf_next=CVEC_NEXT(g->build_buffer); // to be restored if not actually used
  }  

  history=wr_build_resolve_history(g,xcl_as_active,ycl,cutpos1,cutpos2,g->cut_clvec);
  res=wr_derived_build_cl_from_initial_cl(g,rptr,rpos,ruleflag,history);

  //CP1  
  //printf("\nres\n");
  //wr_print_clause(g,res);

  if (!history || !res) {
    // allocation failed
    return;
  }  

  wr_process_derived_final(g,res,wr_lithash_final(g,rptr,rpos),history,initial_queue_termbuf_next,xcl_as_active,
    0,1,1, 1,xatomnr,yatomnr, 1,0,0,1, "\n+ unit by mp: ","\n+ derived by mp: ",NULL);
}  


void wr_process_instgen_result
      (glb* g, gint xatom, gptr xcl, gint yatom, gptr ycl, gptr xcl_as_active, int usexcl) {
  int isrule,atomnr;
  int rlen;
  int tmp;
  gptr cl;
  gint atom;
  gptr rptr;
  int rpos,cutpos1=0,cutpos2=0;
  gptr res;  
  gint history;
  int ruleflag;
  gint initial_queue_termbuf_next=0;
  gint clhash;
  
#ifdef DEBUG
  wr_printf("\n+++ wr_process_instgen_result called\n");
  wr_print_clause(g,xcl); wr_printf(" : ");wr_print_term(g,xatom);
  wr_printf("\n");
  wr_print_clause(g,ycl);  wr_printf(" : ");wr_print_term(g,yatom);
  wr_printf("\n");
  wr_print_vardata(g);  
#endif    
  if (usexcl) {
    cl=xcl;
    atom=xatom;    
  } else {
    cl=ycl;
    atom=yatom;
  }  
  if (wr_ground_term(g,atom)) return;
  if (g->derived_overflow_timeout) return; // time limit passed at a drop
  ++(g->stat_derived_cl);
  ++(g->stat_instgen_derived_cl); 
  // get basic info about clauses
  isrule=wg_rec_is_rule_clause(db,cl); 
  if (isrule) atomnr=wg_count_clause_atoms(db,cl);
  else atomnr=1; 
  // reserve sufficient space in derived_termbuf for simple sequential store of atoms:
  // no top-level meta kept
  rlen=(atomnr)*LIT_WIDTH;  
  (g->derived_termbuf)[1]=2; // init termbuf
  rptr=wr_alloc_from_cvec(g,g->derived_termbuf,rlen); 
  if (rptr==NULL) {
    ++(g->stat_internlimit_discarded_cl);
    wr_alloc_err(g,"could not alloc first buffer in wr_process_instgen_result ");
    return; // could not alloc memory, could not store clause
  }  
  //printf("xisrule %d yisrule %d xatomnr %d yatomnr %d rlen %d\n",
  //        xisrule,yisrule,xatomnr,yatomnr,rlen);
  // set up var rename params
  wr_process_resolve_result_setupsubst(g);  
  wr_derived_build_begin(g);
  // store all ready-built atoms sequentially, excluding duplicates
  // and looking for tautology: only for rule clauses needed    
  rpos=0;
  tmp=wr_process_instgen_result_aux(g,cl,atom,atomnr,rptr,&rpos,&cutpos1,-1,0);
  if (!tmp) {
    wr_process_resolve_result_cleanupsubst(g);       
    wr_derived_build_end(g);
    return; 
  }
  wr_process_resolve_result_cleanupsubst(g);  
  wr_derived_build_end(g);
  if (rpos==0) {
    g->proof_found=1;   
    if (usexcl) g->proof_history=wr_build_instgen_history(g,xcl_as_active,ycl,cutpos1,cutpos2,NULL);
    else g->proof_history=wr_build_instgen_history(g,ycl,xcl_as_active,cutpos2,cutpos1,NULL);
    wr_register_answer(g,NULL,g->proof_history);
    return;
  } 

  // check subsumption and cutoff
  
  if (g->print_derived_precut_cl) {
    wr_printf("\nc pre-cut derived by instgen: ");
    wr_print_halfbuilt_clause(g,rptr,rpos);
  }
  tmp=wr_derived_cl_cut_and_subsume(g,rptr,rpos,&clhash);
  
  /* 
  printf("\nhalfbuilt clause:\n");
  wr_print_halfbuilt_clause(g,rptr,rpos);
  printf("\n");
  */

  if (tmp<0) {
    // clause was subsumed
#ifdef DEBUG      
    wr_printf("\nwr_process_instgen_result was subsumed with new subsumer\n");
#endif    
    if (g->print_derived_subsumed_cl) {
      wr_printf("\n- subsumed derived by instgen: ");
      wr_print_halfbuilt_clause(g,rptr,rpos);
    }
    return;    
  } else if (tmp>0) {
    // there were cuts
    if (g->print_derived_precut_cl) {
      wr_printf("\nc post-cut derived by instgen: ");
      wr_print_halfbuilt_clause(g,rptr,rpos);
    }
    wr_process_resolve_result_remove_cuts(g,rptr,&rpos,tmp);
    (g->stat_derived_cut)++;
    if (rpos==0) {
      g->proof_found=1;   
      if (usexcl) g->proof_history=wr_build_instgen_history(g,xcl_as_active,ycl,cutpos1,cutpos2,g->cut_clvec);
      else g->proof_history=wr_build_instgen_history(g,ycl,xcl_as_active,cutpos2,cutpos1,g->cut_clvec); 
      wr_register_answer(g,NULL,g->proof_history);
      return;
    } 
  }


  // now we have stored all subst-into and renamed metas/atoms into rptr: build clause  

  //printf("filled meta/atom new vec, rpos %d\n",rpos);          

  // check whether should be stored as a ruleclause or not
  ruleflag=wr_process_resolve_result_isrulecl(g,rptr,rpos);  
  // create new record
 
  wr_process_resolve_result_setupquecopy(g); // use g->queue_termbuf as g->build_buffer
  initial_queue_termbuf_next=CVEC_NEXT(g->build_buffer); // to be restored if not actually used    
  
  if (usexcl) history=wr_build_instgen_history(g,xcl_as_active,ycl,cutpos1,cutpos2,g->cut_clvec);
  else history=wr_build_instgen_history(g,ycl,xcl_as_active,cutpos2,cutpos1,g->cut_clvec); 
  res=wr_derived_build_cl_from_initial_cl(g,rptr,rpos,ruleflag,history);
  if (!history || !res) {
    // allocation failed
    return;
  }  
  wr_process_derived_final(g,res,wr_lithash_final(g,rptr,rpos),history,initial_queue_termbuf_next,xcl_as_active,
    1,0,0, 0,0,0, 0,1,clhash,1, NULL,"\n+ derived by instgen: ",NULL);
}  


/* 

  Process factorization and equality reflexive resolution result 
  (in the latter case y == -1 and yatom == 0)

*/


void wr_process_factor_result
      (glb* g, gptr xcl, int len, int x, gint xatom, int y, gint yatom, gptr xcl_as_active) {
  //void* db;
  int rlen;
  int tmp,tmp2;
  gptr rptr;
  int rpos, cutpos1=0;
  gptr res;
  gint history;
  int ruleflag;
  gint initial_queue_termbuf_next=0;
  
#ifdef DEBUG
  wr_printf("+++ wr_process_factor_result called\n");
  wr_print_clause(g,xcl); wr_printf(" %d, %d : ",x,y);
  wr_print_term(g,xatom);
  wr_printf("\n");
  wr_print_term(g,yatom);
  wr_printf("\n");
  wr_print_vardata(g);
#endif  

  if (g->derived_overflow_timeout) return; // time limit passed at a drop
  ++(g->stat_derived_cl);
  ++(g->stat_factor_derived_cl);
  // reserve sufficient space in derived_termbuf for simple sequential store of atoms:
  // no top-level meta kept
  rlen=(len-1)*LIT_WIDTH;
  if (rlen==0) {     
    g->proof_found=1;    
    g->proof_history=wr_build_factorial_history(g,xcl_as_active,x,y,NULL);
    wr_register_answer(g,NULL,g->proof_history);
    return;
  }  
  // experimental: a lower bound of the weight above the queue limit
  if ((g->queue_weight_bound) && (g->qwl_active) &&
      wr_qwb_drop(g,xcl,0,y,NULL,0,-1,xcl_as_active,NULL)) {
    wr_qwb_dropped(g);
    return;
  }
  (g->derived_termbuf)[1]=2; // init termbuf
  rptr=wr_alloc_from_cvec(g,g->derived_termbuf,rlen); 
  if (rptr==NULL) {
    ++(g->stat_internlimit_discarded_cl);
    wr_alloc_err(g,"could not alloc first buffer in wr_process_resolve_result ");
    return; // could not alloc memory, could not store clause
  }  
  // set up var rename params
  wr_process_resolve_result_setupsubst(g);  
  wr_derived_build_begin(g);
  // store all ready-built atoms sequentially, excluding duplicates
  // and looking for tautology: only for rule clauses needed    
  rpos=0;
  tmp=wr_process_resolve_result_aux(g,xcl,xatom,len,rptr,&rpos, &cutpos1,-1,0);
  if (!tmp) {
    wr_process_resolve_result_cleanupsubst(g);       
    wr_derived_build_end(g);
    return; 
  }    
  wr_process_resolve_result_cleanupsubst(g);
  wr_derived_build_end(g);
  if (rpos==0) { 
    g->proof_found=1; 
    g->proof_history=wr_build_factorial_history(g,xcl_as_active,x,y,NULL);
    wr_register_answer(g,NULL,g->proof_history);
    return; 
  } 

  // check subsumption and cutoff

  if (g->print_derived_precut_cl) {
    wr_printf("\nc pre-cut derived by merge: ");
    wr_print_halfbuilt_clause(g,rptr,rpos);
  }
  /* 
  tmp=wr_derived_cl_cut_and_subsume(g,rptr,rpos,NULL);
  if (tmp<0) {
    // clause was subsumed
#ifdef DEBUG    
    wr_printf("\nwr_process_resolve_result was subsumed with new subsumer\n");
#endif    
    if (g->print_derived_subsumed_cl) {
      wr_printf("\n- subsumed derived by merge ");
      wr_print_halfbuilt_clause(g,rptr,rpos);
    }
    return;    
  } else if (tmp>0) {
    // there were cuts
    if (g->print_derived_precut_cl) {
      wr_printf("\nc post-cut derived by merge ");
      wr_print_halfbuilt_clause(g,rptr,rpos);
    }
    wr_process_resolve_result_remove_cuts(g,rptr,&rpos,tmp);
    (g->stat_derived_cut)++;
    if (rpos==0) {
      g->proof_found=1; 
      g->proof_history=wr_build_factorial_history(g,xcl_as_active,x,y,g->cut_clvec);    
      wr_register_answer(g,NULL,g->proof_history);
      return;
    } 
  }
  */

#if GKC_WEIGHT_DROP_PLACE==1
  if (wr_derived_weight_drop_precut(g,rptr,rpos,0,0,xcl_as_active,NULL)) return;
#endif
  tmp=wr_derived_cl_cut_and_subsume(g,rptr,rpos,NULL);
  if (tmp<0) {
    // clause was subsumed
#ifdef DEBUG      
    wr_printf("\nwr_process_factor_result was subsumed with new subsumer\n");
#endif    
    if (g->print_derived_subsumed_cl) {
      wr_printf("\n- subsumed derived by merge: ");
      wr_print_halfbuilt_clause(g,rptr,rpos);
    }
    return;    
  } else {
    tmp2=0;
    if ((g->use_strong_unit_cutoff) && rpos) {
      tmp2=wr_derived_cl_strong_cut(g,rptr,rpos,tmp);
    }
    if (tmp>0 || tmp2>0) {
      // there were cuts
      if (g->print_derived_precut_cl) {
        wr_printf("\nc post-cut derived by merge: ");
        wr_print_halfbuilt_clause(g,rptr,rpos);
      }
      wr_process_resolve_result_remove_cuts(g,rptr,&rpos,tmp+tmp2);
      (g->stat_derived_cut)++;
      if (rpos==0) {
        g->proof_found=1;    
        g->proof_history=wr_build_factorial_history(g,xcl_as_active,x,y,g->cut_clvec);  
        wr_register_answer(g,NULL,g->proof_history);
        return;
      } 
    }  
  }




  // now we have stored all subst-into and renamed metas/atoms into rptr: build clause  
  //printf("filled meta/atom new vec, rpos %d\n",rpos);          
  // check whether should be stored as a ruleclause or not
  ruleflag=wr_process_resolve_result_isrulecl(g,rptr,rpos);  
  // create new record  
  wr_process_resolve_result_setupquecopy(g); // use g->queue_termbuf as g->build_buffer
  initial_queue_termbuf_next=CVEC_NEXT(g->build_buffer); // to be restored if not actually used 
  if (wr_derived_weight_drop_early(g,rptr,rpos,ruleflag,0,0)) return;

  history=wr_build_factorial_history(g,xcl_as_active,x,y,g->cut_clvec);
  res=wr_derived_build_cl_from_initial_cl(g,rptr,rpos,ruleflag,history);
  if (!history || !res) {
    // allocation failed
    return;
  }
  wr_process_derived_final(g,res,wr_lithash_final(g,rptr,rpos),history,initial_queue_termbuf_next,xcl_as_active,
    0,0,1, 1,0,0, 1,0,0,0, "\n+ unit by factor: ","\n+ derived by merge: ",NULL);
}  

/*

 combine and store equality-containing clause with another clause 
 after successful equality/subterm unification as a paramodulation result 

 xatom: equality literal
 xcl: clause containing equality
 yatom: literal substituted into
 ycl: clause containing literal substituted into
 xcl_as_active: equality clause as stored in the active list
 aterm: equality arg used for unification: here used only for printing
 bterm: other arg of equality: term substituted as as replacement into yatom
 path:  already decoded int 
        use as repllitnr=wr_decode_para_termpath_litnr(g,replpath); replpos=wr_decode_para_termpath_pos(g,replpath);  
 leftflag: which eq arg used for unification: non-0 if left eq arg, 0 if right eq arg
 

*/

void wr_process_paramodulate_result
      (glb* g, gint xatom, gptr xcl, gint yatom, gptr ycl, gptr xcl_as_active, 
      gint aterm, gint bterm, gint path, int leftflag, int fromflag) {
  int xisrule,yisrule,xatomnr,yatomnr;
  int rlen;
  int tmp,tmp2;
  gptr rptr;
  int rpos,cutpos1=0,cutpos2=0;
  gptr res;  
  gint history;
  int ruleflag;
  gint initial_queue_termbuf_next=0;
    
#ifdef DEBUG
  wr_printf("\n+++ wr_process_paramodulate_result called\n");
  wr_print_clause(g,xcl); wr_printf(" : ");wr_print_term(g,xatom);
  wr_printf("\n");
  wr_print_clause(g,ycl);  //printf(" : ");wr_print_term(g,yatom);
  wr_printf("\naterm:");
  wr_print_term(g,aterm);  wr_printf("\nbterm:");
  wr_print_term(g,bterm);  wr_printf("\npath: %d\n",path); 
  wr_printf("\leftflag: %d\n",leftflag); 
  wr_print_vardata(g);
#endif  
  if (g->derived_overflow_timeout) return; // time limit passed at a drop
  ++(g->stat_derived_cl);
  ++(g->stat_para_derived_cl);
  // get basic info about clauses
  xisrule=wg_rec_is_rule_clause(db,xcl);
  yisrule=wg_rec_is_rule_clause(db,ycl);
  if (xisrule) xatomnr=wg_count_clause_atoms(db,xcl);
  else xatomnr=1;
  if (yisrule) yatomnr=wg_count_clause_atoms(db,ycl);
  else yatomnr=1;
  // reserve sufficient space in derived_termbuf for simple sequential store of atoms:
  // no top-level meta kept
  rlen=(xatomnr+yatomnr-1)*LIT_WIDTH;
  if (rlen==0) {
    g->proof_found=1;
    printf("\nwr_process_paramodulate_result found proof1\n");
    printf("xcl ");
    wr_print_clause(g,xcl);
    printf("\nxcl_as_active ");
    wr_print_clause(g,xcl_as_active);
    printf("\nycl ");
    wr_print_clause(g,ycl);
    printf("\n");
    if (fromflag) {
      printf("\n fromflag %d cutpos1 %d cutpos2 %d leftflag %d\n",fromflag,cutpos1,cutpos2,leftflag);     
      g->proof_history=wr_build_para_history(g,xcl_as_active,ycl,0,0,NULL,path,leftflag,fromflag);
    } else {
      printf("\n fromflag %d cutpos1 %d cutpos2 %d leftflag %d\n",fromflag,cutpos1,cutpos2,leftflag);
      g->proof_history=wr_build_para_history(g,xcl,xcl_as_active,0,0,NULL,path,leftflag,fromflag);  
    }   
    wr_register_answer(g,NULL,g->proof_history);
    //g->proof_history=wr_build_para_history(g,xcl_as_active,ycl,0,0,NULL,path,leftflag,fromflag);
    return;
  }  
  // experimental: a lower bound of the weight above the queue limit
  if ((g->queue_weight_bound) && (g->qwl_active) &&
      wr_qwb_drop(g,xcl,xatom,-1,ycl,0,wr_decode_para_termpath_litnr(g,path),
                                (fromflag ? xcl_as_active : xcl),(fromflag ? ycl : xcl_as_active))) {
    wr_qwb_dropped(g);
    return;
  }
  (g->derived_termbuf)[1]=2; // init termbuf
  rptr=wr_alloc_from_cvec(g,g->derived_termbuf,rlen); 
  if (rptr==NULL) {
    ++(g->stat_internlimit_discarded_cl);
    wr_alloc_err(g,"could not alloc first buffer in wr_process_paramodulate_result ");
    return; // could not alloc memory, could not store clause
  }  
  //printf("xisrule %d yisrule %d xatomnr %d yatomnr %d rlen %d\n",
  //        xisrule,yisrule,xatomnr,yatomnr,rlen);
  // set up var rename params
  wr_process_resolve_result_setupsubst(g);  
  wr_derived_build_begin(g);
  // store all ready-built atoms sequentially, excluding duplicates
  // and looking for tautology: only for rule clauses needed    
  rpos=0;
  if (xatomnr>1) {    
    tmp=wr_process_resolve_result_aux(g,xcl,xatom,xatomnr,rptr,&rpos,&cutpos1,-1,0);    
    if (!tmp) {
      wr_process_resolve_result_cleanupsubst(g);       
      wr_derived_build_end(g);
      return; 
    }  
  }
  //if (yatomnr>1) { // ???
  tmp=wr_process_resolve_result_aux(g,ycl,0,yatomnr,rptr,&rpos,&cutpos2,path,bterm);
  if (!tmp) {
    wr_process_resolve_result_cleanupsubst(g);   
    wr_derived_build_end(g);
    return;
  }  
  //}
  wr_process_resolve_result_cleanupsubst(g);  
  wr_derived_build_end(g);
  if (rpos==0) {
    g->proof_found=1; 
    if (fromflag) {
      //printf("\n fromflag %d cutpos1 %d cutpos2 %d leftflag %d\n",fromflag,cutpos1,cutpos2,leftflag);
      g->proof_history=wr_build_para_history(g,xcl_as_active,ycl,cutpos1,cutpos2,NULL,path,leftflag,fromflag);
    } else {
      //printf("\n fromflag %d cutpos1 %d cutpos2 %d leftflag %d\n",fromflag,cutpos1,cutpos2,leftflag);
      g->proof_history=wr_build_para_history(g,xcl,xcl_as_active,cutpos2,cutpos1,NULL,path,leftflag,fromflag);  
    }   
    wr_register_answer(g,NULL,g->proof_history);
    //g->proof_history=wr_build_para_history(g,xcl_as_active,ycl,cutpos1,cutpos2,NULL,path,leftflag,fromflag);
    return;
  } 
  /*
  wr_printf("\nin wr_process_paramodulate_result we got halfbuilt:\n");
  wr_print_halfbuilt_clause(g,rptr,rpos);
  wr_printf("\n");
  */
  // check subsumption and cutoff
  if (g->print_derived_precut_cl) {    
    wr_printf("\nc pre-cut derived by =: ");

    //wr_printf("\n+++ wr_process_paramodulate_result called\n");
    wr_printf("\nxcl and term:\n");
    wr_print_clause(g,xcl); wr_printf(" : ");wr_print_term(g,xatom);
    wr_printf("\nycl:\n");
    wr_print_clause(g,ycl);  //printf(" : ");wr_print_term(g,yatom);
    wr_printf("\naterm:");
    wr_print_term(g,aterm);  wr_printf("\nbterm:");
    wr_print_term(g,bterm);  wr_printf("\npath: %ld\n",path); 
    wr_printf("\nleftflag: %d\n",leftflag); 
    //wr_print_vardata(g);

    if (fromflag) {
      wr_printf("from: ");
    } else {
      wr_printf("into: ");
    }
    wr_printf("\nhalfbuilt res:\n");
    wr_print_halfbuilt_clause(g,rptr,rpos);
  }

  // --
#if GKC_WEIGHT_DROP_PLACE==1
  if (wr_derived_weight_drop_precut(g,rptr,rpos,xatomnr,yatomnr,
                                    (fromflag ? xcl_as_active : xcl),(fromflag ? ycl : xcl_as_active))) return;
#endif
  tmp=wr_derived_cl_cut_and_subsume(g,rptr,rpos,NULL);
  if (tmp<0) {
    // clause was subsumed
#ifdef DEBUG      
    wr_printf("\nwr_process_paramodulate_result was subsumed with new subsumer\n");
#endif    
    if (g->print_derived_subsumed_cl) {
      wr_printf("\n- subsumed derived by = ");
      if (fromflag) {
        wr_printf("from: ");
      } else {
        wr_printf("into: ");
      } wr_print_halfbuilt_clause(g,rptr,rpos);
    } 
    return;    
  } else {
    tmp2=0;
    if ((g->use_strong_unit_cutoff) && rpos) {
      tmp2=wr_derived_cl_strong_cut(g,rptr,rpos,tmp);
    }
    if (tmp>0 || tmp2>0) {
      // there were cuts
      if (g->print_derived_precut_cl) {
        wr_printf("\nc post-cut derived by =: ");
        if (fromflag) {
          wr_printf("from: ");
        } else {
          wr_printf("into: ");
        } 
        printf("\n(g->cut_clvec)[0]: %ld\n",(g->cut_clvec)[0]);
        wr_print_clause(g,(gptr)((g->cut_clvec)[1]));
        printf("\n");        
        wr_print_halfbuilt_clause(g,rptr,rpos);
      }
      wr_process_resolve_result_remove_cuts(g,rptr,&rpos,tmp+tmp2);
      (g->stat_derived_cut)++;
      if (rpos==0) {
        g->proof_found=1;  
        if (fromflag) {
          //printf("\n fromflag %d cutpos1 %d cutpos2 %d leftflag %d\n",fromflag,cutpos1,cutpos2,leftflag);
          g->proof_history=wr_build_para_history(g,xcl_as_active,ycl,cutpos1,cutpos2,g->cut_clvec,path,leftflag,fromflag);
        } else {
          //printf("\n fromflag %d cutpos1 %d cutpos2 %d leftflag %d\n",fromflag,cutpos1,cutpos2,leftflag);
          g->proof_history=wr_build_para_history(g,xcl,xcl_as_active,cutpos2,cutpos1,g->cut_clvec,path,leftflag,fromflag);  
        }
        wr_register_answer(g,NULL,g->proof_history);
        //g->proof_history=wr_build_para_history(g,xcl_as_active,ycl,cutpos1,cutpos2,g->cut_clvec,path,leftflag,fromflag); 
        return;
      }
    }  
  }
  // --
  

  // remove strong duplicates  
  //printf("\nrpos starts with %d\n",rpos);
  if ((rpos>1) && (g->use_strong_duplicates)) {    
    //printf("\nrpos before %d\n",rpos);
    wr_find_strong_duplicates(g,rptr,&rpos);
    //printf("\nrpos after %d\n",rpos);
  } 

  // now we have stored all subst-into and renamed metas/atoms into rptr: build clause  

  //printf("filled meta/atom new vec, rpos %d\n",rpos);          
  // check whether should be stored as a ruleclause or not
  ruleflag=wr_process_resolve_result_isrulecl(g,rptr,rpos);  
  // create new record
  // fully formed, storable result
  //partialresflag=0;      
  wr_process_resolve_result_setupquecopy(g); // use g->queue_termbuf as g->build_buffer  
  initial_queue_termbuf_next=CVEC_NEXT(g->build_buffer); // to be restored if not actually used
  if (wr_derived_weight_drop_early(g,rptr,rpos,ruleflag,xatomnr,yatomnr)) return;
  if (fromflag) {
    //printf("\n fromflag %d cutpos1 %d cutpos2 %d leftflag %d\n",fromflag,cutpos1,cutpos2,leftflag);
    /*
    wr_print_clause(g,xcl_as_active);
    wr_printf("\n");
    wr_print_clause(g,ycl);
    wr_printf("\n");
    */
    history=wr_build_para_history(g,xcl_as_active,ycl,cutpos1,cutpos2,g->cut_clvec,path,leftflag,fromflag);
  } else {
    //printf("\n fromflag %d cutpos1 %d cutpos2 %d leftflag %d\n",fromflag,cutpos1,cutpos2,leftflag);
    history=wr_build_para_history(g,xcl,xcl_as_active,cutpos2,cutpos1,g->cut_clvec,path,leftflag,fromflag);  
  }
  res=wr_derived_build_cl_from_initial_cl(g,rptr,rpos,ruleflag,history);
  if (!history || !res) {
    // allocation failed
    return;
  }  
  wr_process_derived_final(g,res,wr_lithash_final(g,rptr,rpos),history,initial_queue_termbuf_next,xcl_as_active,
    0,0,1, 1,xatomnr,yatomnr, 1,0,0,0, "\n+ unit by factor: ","\n+ derived by = ",
    (fromflag ? "from: " : "into: "));
}  

/* -----------

new clause builders 

-------------- */


int wr_process_resolve_result_aux
      (glb* g, gptr cl, gint cutatom, int atomnr, gptr rptr, int* rpos, int* cutpos, 
       gint replpath, gint replterm){
  void *db=g->db;
  int i,j;
  int ruleflag,posfoundflag;
  gint meta,atom,newatom,rmeta;
  int repllitnr=-1,replpos;
  int termpath;

#ifdef DEBUG
  wr_printf("\nwr_process_resolve_result_aux called on atomnr %d with clause and term:\n",atomnr);
  wr_print_clause(g,cl);
  wr_print_term(g,cutatom);
  wr_printf("\n and atomnr %d *rpos %d *cutpos %d replpath %d and replterm:\n",atomnr,*rpos,*cutpos,replpath);
  wr_print_term(g,replterm);
#endif        
  (g->tmp3)++;
  ruleflag=wg_rec_is_rule_clause(db,cl);      
  if (replpath>=0) {
    repllitnr=wr_decode_para_termpath_litnr(g,replpath);  
    *cutpos=repllitnr;
  }  
  for(i=0;i<atomnr;i++) {
    if (!ruleflag) {
      meta=4; // dummy!!!
      atom=encode_record(db,cl);
    } else {
      meta=wg_get_rule_clause_atom_meta(db,cl,i);
      atom=wg_get_rule_clause_atom(db,cl,i);
    }  
    if (atom==cutatom) {
      *cutpos=i;
      continue; // drop cut atoms
    }  
    // subst into xatom 
    if (i==repllitnr) {
      replpos=wr_decode_para_termpath_pos(g,replpath);
      termpath=0;
      newatom=wr_build_calc_term_replace(g,atom,replpos,replterm,&termpath);
    } else {
      newatom=wr_build_calc_term(g,atom);
    }  
    if (!meta) {
      // check that no meta is actual 0: we will mark cutoff literals with meta being actual 0
      wr_printf("\n!! unexpected error: atom meta in wr_process_resolve_result_aux is 0\n ");
      return 0;     
    }  
#ifdef DEBUG
    wr_printf("\nwr_process_resolve_result_aux loop i %d built meta %d and term\n",i,(int)meta);    
    wr_print_term(g,newatom);          
#endif
    if (newatom==WG_ILLEGAL) {
      ++(g->stat_internlimit_discarded_cl);
      //wr_alloc_err(g,"could not build subst newatom in wr_process_resolve_result ");
      return 0; // could not alloc memory, could not store clause
    }  
    if (newatom==ACONST_TRUE) {
      if (wg_atom_meta_is_neg(db,meta)) continue;
      else return 0;
    } 
    if (newatom==ACONST_FALSE) {
      if (wg_atom_meta_is_neg(db,meta)) return 0;
      else continue;      
    }      
    posfoundflag=0;

    // check if xatom present somewhere earlier      
    for(j=0;j < *rpos;j++){
#ifdef DEBUG          
          wr_printf("\nlooking for equal term at pos j %d for newatom: ",j);
          wr_print_term(g,newatom);
          wr_printf(" and atom at pos j %d: ",j);
          wr_print_term(g,rptr[(j*LIT_WIDTH)+LIT_ATOM_POS]);
          wr_printf("\n");
#endif       
      (g->tmp1)++;
      if (wr_equal_term(g,newatom,rptr[(j*LIT_WIDTH)+LIT_ATOM_POS],1)) {        
        rmeta=rptr[(j*LIT_WIDTH)+LIT_META_POS];
        if (!litmeta_negpolarities(meta,rmeta)) {
          //same sign, drop lit
          posfoundflag=1;          
#ifdef DEBUG          
          wr_printf("\nequals found:\n");
          wr_print_term(g,newatom);
          wr_printf("\n");
          wr_print_term(g,rptr[(j*LIT_WIDTH)+LIT_ATOM_POS]);
          wr_printf("\n");
#endif          
          break;                                
        } else {
#ifdef DEBUG           
          wr_printf("\nin wr_process_resolve_result_aux return 0\n");
#endif           
          // negative sign, tautology, drop clause          
          (g->stat_tautologies_discarded)++;
          return 0;
        }            
      }         
    }
    if (!posfoundflag) {  
      // store lit
      rptr[((*rpos)*LIT_WIDTH)+LIT_META_POS]=meta;
      rptr[((*rpos)*LIT_WIDTH)+LIT_ATOM_POS]=newatom;
      ++(*rpos);      
    }       
  }     
 

  return 1; // 1 means clause is still ok. 0 return means: drop clause 
}  

int wr_tautology_cl(glb* g, gptr cl){
  void *db=g->db;
  int i,j,len,xisneg; 
  gint xmeta,xatom,ymeta,yatom;
  
  UNUSED(db); 
  //return 0;
#ifdef DEBUG
  wr_printf("\nwr_tautology_cl called on clause:\n");
  wr_print_clause(g,cl);
  wr_printf("\n"); 
#endif        
  if (cl==NULL) return 0;
  if (!wg_rec_is_rule_clause(db,cl)) return 0;
  len=wg_count_clause_atoms(db,cl);  
  for(i=0;i<len-1;i++) {   
    xmeta=wg_get_rule_clause_atom_meta(db,cl,i);
    xatom=wg_get_rule_clause_atom(db,cl,i);
    xisneg=wg_atom_meta_is_neg(g->db,xmeta);
    //printf("\nxmeta %ld\n",xmeta);
    //printf("\nxmeta masked %ld\n",xmeta&(ATOM_META_CANSUBS_MASK<<SMALLINTSHFT));
    for(j=i+1;j<len;j++) {     
      ymeta=wg_get_rule_clause_atom_meta(db,cl,j);
      //printf("\nymeta %ld\n",ymeta); 
      //printf("\nymeta masked %ld\n",ymeta&(ATOM_META_CANSUBS_MASK<<SMALLINTSHFT));  
      if ((xmeta&(ATOM_META_CANSUBS_MASK<<SMALLINTSHFT))!=(ymeta&(ATOM_META_CANSUBS_MASK<<SMALLINTSHFT))) {
        continue;
      } 
      if ((xisneg && wg_atom_meta_is_neg(g->db,ymeta)) ||
          (!xisneg && !wg_atom_meta_is_neg(g->db,ymeta)))
        continue;
      yatom=wg_get_rule_clause_atom(db,cl,j);  
      if (wr_equal_term(g,xatom,yatom,1)) {
        //printf("\ntautology!!\n");
        return 1; 
      }   
    }
  }  
  return 0; // 0 means clause is ok, 1: tautology
} 


// check if xatom can subsume another atom without instantiating repeating vars
// if yes, drop the atom
// example 1: p(c,X) | p(c,Y) | r(X)  gives  p(c,X) | r(X)
// example 2: p(c,d) | p(c,Y) | r(X)  gives  p(c,d) | r(X)

#define MAX_BLOCKED_LITNR 40

int wr_find_strong_duplicates(glb* g, gptr rptr, int* rpos) {
  void* db;
  int varoccs,x,y,maxpos,foundunique;
  int blockedcount,keepcount,shift,newpos;
  gptr vx,maxpt;
  gint mres,maxnr,xatom,xmeta,yatom,ymeta; //lit1,lit2;
  char blocked[MAX_BLOCKED_LITNR];
    
  UNUSED(db);   
  maxpos=*rpos; 
  wr_clear_varstack(g,g->varstack);
  //vb=g->varbanks; // start from the beginning of varbanks
  g->tmp_unify_vc=((gptr)(g->varstack))+1;
  // first mark repeated vars    
  varoccs=0;
  //wr_print_vardata(g);
  for(x=0; x<maxpos; x++) {
    xatom=rptr[(x*LIT_WIDTH)+LIT_ATOM_POS];
    varoccs+=wr_mark_repeated_vars(g,xatom);
  }
  //wr_print_vardata(g);
  // check if there were any vars
  //printf("\nvaroccs %d\n",varoccs);
  if (!varoccs) {
    // no vars, nothing to clean
    return 0;
  }
   
  // now all non-repeated vars are set to NULL
  // check if there were any nonrepeated vars 
  foundunique=0;  
  vx=(g->varstack);
  ++vx;
  maxnr= *vx;
  //printf("\nmaxnr %d\n",maxnr);
  if (maxnr>2) {    
    maxpt=(g->varstack)+maxnr;    
    for(++vx; vx<maxpt; ++vx) { 
      //printf("\n vx %d var %d val %d ",vx,*vx,decode_var(*vx),*((gptr)(*vx)));
      //wr_print_term(g,*((gptr)(*vx)));
      //printf("\n");       
      if(*((gptr)(*vx))==(gint)NULL) {
        foundunique=1;
        break;
      }      
    }  
  }
  //printf("\nfoundunique %d\n",foundunique);  
  if (!foundunique) {
    wr_clear_varstack(g,g->varstack);
    return 0;
  }
  // now there were some unique vars
  // loop over atoms
   
  // zero blocked array
  memset(blocked,0,MAX_BLOCKED_LITNR);
  blockedcount=0;
  db=g->db;
  // loop over atoms, trying to mark them as blocked to be dropped later
  for(x=0; (x < maxpos) && (x < MAX_BLOCKED_LITNR) ;x++){  
    if (blocked[x]) continue;  
    xmeta=rptr[(x*LIT_WIDTH)+LIT_META_POS];
    xatom=rptr[(x*LIT_WIDTH)+LIT_ATOM_POS];
    //lit1=rpto(g,xatom);
    for(y=x+1; (y < maxpos) && (y < MAX_BLOCKED_LITNR); y++) {
      if (blocked[y]) continue;
      ymeta=rptr[(y*LIT_WIDTH)+LIT_META_POS];      
      if (wg_atom_meta_is_neg(db,xmeta)==wg_atom_meta_is_neg(db,ymeta)) {
        yatom=rptr[(y*LIT_WIDTH)+LIT_ATOM_POS];
        //lit2=rpto(g,yatom);      
        mres=wr_nomatch_term_uniquevars(g,xatom,yatom,1); // uniquestrflag=1
        //printf("\n mres1 %d\n",mres);
        // mres=0 if success, 1 if fails because of vars, 2 if fails because of const/fun
        if (!mres) {
          // match successful, lit1 is more general, drop lit1          
          blocked[x]=(char)1;
          blockedcount++;
        } else if (mres==1) {
          // failed because of vars: try another way
          mres=wr_nomatch_term_uniquevars(g,yatom,xatom,1);
          //printf("\n mres2 %d\n",mres);
          if (!mres) {
            // match successful, lit2 is more general, drop lit2
            blocked[y]=(char)1;
            blockedcount++;
          }  
        } else {
          // failed because of const/fun: do not try more
        }
      }                 
    }
  }  
  wr_clear_varstack(g,g->varstack);
  if (!blockedcount) {
    return 0;
  } 
  /*
  printf("\nfound blockedcount %d and blocked els:\n",blockedcount);
  for(x=0;x<*rpos;x++) {
    printf("el %d blocked %d\n",x,(int)(blocked[x]));
  }
  printf("for halfbuilt cl:\n");
  wr_print_halfbuilt_clause(g,rptr,*rpos);
  printf("\n");
  */

  // here we found a blocked lit
  keepcount=0;
  shift=0;
  for(x=0; (x < *rpos) ; x++){
    if ((x < MAX_BLOCKED_LITNR) && blocked[x]) {
      shift++;
    } else {
      keepcount++;
      if (shift) {
        newpos=x-shift;
        rptr[(newpos*LIT_WIDTH)+LIT_META_POS]=rptr[(x*LIT_WIDTH)+LIT_META_POS];
        rptr[(newpos*LIT_WIDTH)+LIT_ATOM_POS]=rptr[(x*LIT_WIDTH)+LIT_ATOM_POS];
      } 
    }
  }  
  // finally change count
  //*rpos=(*rpos)-blockedcount;
  *rpos=keepcount;
  /*
  printf("simplified halfbuilt cl with *rpos %d:\n",*rpos);
  wr_print_halfbuilt_clause(g,rptr,*rpos);
  printf("\n");
  */
  return blockedcount;  
}

/*
    push all vars to (g->varstack)
    mark repeated vars in as having themselves as value
    mark non-repeated vars as having NULL as value
    return nr of overall var occurrences (0 if none)

    *vc is a pointer to stack counter var
*/

int wr_mark_repeated_vars(glb* g, gint x) {
  void* db;
  gptr xptr;
  int i, start, end, varoccs;  
  gint tmp; // used by VARVAL_F
  gint val;  

#ifdef DEBUG
  wr_printf("wr_mark_repeated_vars called with x %ld ",x);
  wr_print_term(g,x);
  wr_printf("\n");
#endif      
  if (!isdatarec(x)) {
    // now we have a simple value     
    if (isvar(x)) {
      val=(g->varbanks)[decode_var(x)];
      if (val==UNASSIGNED) {
        // var has not been seen before
        // initially, vars are set UNASSIGNED 
        // set it to NULL and push to stack
        SETVAR(x,(gint)NULL,(g->varbanks),(g->varstack),(g->tmp_unify_vc)); 
      } else {
        // var has been seen before
        // set it to itself and do not push to stack
        (g->varbanks)[decode_var(x)]=x;         
#ifdef DEBUG        
        wr_printf("\nvar %d %d marked as repeated\n",x,decode_var(x));            
#endif        
      }
      return 1; // if var found, return 1 
    } 
    return 0;  // no var found
  }   
  // now we have a datarec
  db=g->db;
  xptr=decode_record(db,x);
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,xptr); 
  varoccs=0;
  for(i=start;i<end;i++) {
    tmp=wr_mark_repeated_vars(g,xptr[i]);      
    varoccs=varoccs+tmp;
  }
  return varoccs;
} 


int wr_process_instgen_result_aux
      (glb* g, gptr cl, gint cutatom, int atomnr, gptr rptr, int* rpos, int* cutpos, 
       gint replpath, gint replterm){
  void *db=g->db;
  int i,j;
  int ruleflag,posfoundflag;
  gint meta,atom,newatom,rmeta;
  int repllitnr=-1,replpos;
  int termpath;

#ifdef DEBUG
  wr_printf("\nwr_process_instgen_result_aux called on atomnr %d with clause and term:\n",atomnr);
  wr_print_clause(g,cl);
  wr_print_term(g,cutatom);
  wr_printf("\n and atomnr %d *rpos %d *cutpos %d replpath %d and replterm:\n",atomnr,*rpos,*cutpos,replpath);
  wr_print_term(g,replterm);
#endif        
  (g->tmp3)++;
  ruleflag=wg_rec_is_rule_clause(db,cl);      
  if (replpath>=0) {
    repllitnr=wr_decode_para_termpath_litnr(g,replpath);  
    *cutpos=repllitnr;
  }  
  for(i=0;i<atomnr;i++) {
    if (!ruleflag) {
      meta=4; // dummy!!!
      atom=encode_record(db,cl);
    } else {
      meta=wg_get_rule_clause_atom_meta(db,cl,i);
      atom=wg_get_rule_clause_atom(db,cl,i);
    }  
    // subst into xatom 
    if (i==repllitnr) {
      replpos=wr_decode_para_termpath_pos(g,replpath);
      termpath=0;
      newatom=wr_build_calc_term_replace(g,atom,replpos,replterm,&termpath);
    } else {
      newatom=wr_build_calc_term(g,atom);
    }  
    if (!meta) {
      // check that no meta is actual 0: we will mark cutoff literals with meta being actual 0
      wr_printf("\n!! unexpected error: atom meta in wr_process_instgen_result_aux is 0\n ");
      return 0;     
    }  
#ifdef DEBUG
    wr_printf("\nwr_process_instgen_result_aux loop i %d built meta %d and term\n",i,(int)meta);    
    wr_print_term(g,newatom);          
#endif
    if (newatom==WG_ILLEGAL) {
      ++(g->stat_internlimit_discarded_cl);
      //wr_alloc_err(g,"could not build subst newatom in wr_process_resolve_result ");
      return 0; // could not alloc memory, could not store clause
    }  
    if (newatom==ACONST_TRUE) {
      if (wg_atom_meta_is_neg(db,meta)) {
        if (g->arith_inst_probe_active) ++(g->tmp_arithinst_deleted_lits);
        continue;
      } else {
        if (g->arith_inst_probe_active) g->tmp_arithinst_tautology=1;
        return 0;
      }
    } 
    if (newatom==ACONST_FALSE) {
      if (wg_atom_meta_is_neg(db,meta)) {
        if (g->arith_inst_probe_active) g->tmp_arithinst_tautology=1;
        return 0;
      } else {
        if (g->arith_inst_probe_active) ++(g->tmp_arithinst_deleted_lits);
        continue;
      }
    }      
    posfoundflag=0;

    // check if xatom present somewhere earlier      
    for(j=0;j < *rpos;j++){
#ifdef DEBUG          
          wr_printf("\nlooking for equal term at pos j %d for newatom: ",j);
          wr_print_term(g,newatom);
          wr_printf(" and atom at pos j %d: ",j);
          wr_print_term(g,rptr[(j*LIT_WIDTH)+LIT_ATOM_POS]);
          wr_printf("\n");
#endif       
      (g->tmp1)++;
      if (wr_equal_term(g,newatom,rptr[(j*LIT_WIDTH)+LIT_ATOM_POS],1)) {        
        rmeta=rptr[(j*LIT_WIDTH)+LIT_META_POS];
        if (!litmeta_negpolarities(meta,rmeta)) {
          //same sign, drop lit
          posfoundflag=1;          
#ifdef DEBUG          
          wr_printf("\nequals found:\n");
          wr_print_term(g,newatom);
          wr_printf("\n");
          wr_print_term(g,rptr[(j*LIT_WIDTH)+LIT_ATOM_POS]);
          wr_printf("\n");
#endif          
          break;                                
        } else {
#ifdef DEBUG           
          wr_printf("\nin wr_process_resolve_result_aux return 0\n");
#endif           
          // negative sign, tautology, drop clause          
          (g->stat_tautologies_discarded)++;
          if (g->arith_inst_probe_active) g->tmp_arithinst_tautology=1;
          return 0;
        }            
      }         
    }
    if (!posfoundflag) {  
      // store lit
      rptr[((*rpos)*LIT_WIDTH)+LIT_META_POS]=meta;
      rptr[((*rpos)*LIT_WIDTH)+LIT_ATOM_POS]=newatom;
      ++(*rpos);      
    }       
  }     
  
  return 1; // 1 means clause is still ok. 0 return means: drop clause 
}  


void wr_process_resolve_result_remove_cuts(glb* g, gptr rptr, int* rpos, int cuts) {
  int i,storepos,tmp;
  gint xatom,xatommeta;

  #ifdef DEBUG
    wr_printf("\nwr_process_resolve_result_remove_cuts called with %d cuts\n",cuts);
  #endif        
          
  storepos=0;        
  for(i=0; i < *rpos; i++) { 
    tmp=i*LIT_WIDTH;
    xatom=rptr[tmp+LIT_ATOM_POS];
    xatommeta=rptr[tmp+LIT_META_POS];    

    if (xatommeta==0) {
      // marked as cut, do nothing  
    } else {
      // not marked as cut
      if (i!=storepos) {
        // shift
        rptr[(storepos*LIT_WIDTH)+LIT_ATOM_POS]=xatom;
        rptr[(storepos*LIT_WIDTH)+LIT_META_POS]=xatommeta;
      }
      storepos++;
    }  
  }
  // store a shorter vec info
  *rpos = *rpos - cuts;
  return;
}



gptr wr_derived_build_cl_from_initial_cl(glb* g, gptr rptr, int rpos, int ruleflag, gint history) {
  gint meta;
  gptr res;
  int i,datalen,tmp;
  gint blt;
  void* db;

  db=g->db;
  if (ruleflag) {   
    // resulting clause was a rule    

    meta=RECORD_META_RULE_CLAUSE;
    datalen=rpos*LIT_WIDTH;
    res=wr_create_raw_record(g,CLAUSE_EXTRAHEADERLEN+datalen,meta,g->build_buffer);
    if (res==NULL) {
      ++(g->stat_internlimit_discarded_cl);
      wr_alloc_err(g,"could not alloc raw record in wr_process_resolve_result ");
      return NULL;
    }  
    for(i=RECORD_HEADER_GINTS;i<(RECORD_HEADER_GINTS+CLAUSE_EXTRAHEADERLEN);i++) {
      res[i]=0;     
    }       
    for(i=0;i<rpos;i++) { 
      tmp=i*LIT_WIDTH; 
      res[tmp+RECORD_HEADER_GINTS+CLAUSE_EXTRAHEADERLEN+LIT_META_POS]=rptr[tmp+LIT_META_POS];
      blt=wr_build_calc_term(g,rptr[tmp+LIT_ATOM_POS]);
      if (blt==WG_ILLEGAL) {
        ++(g->stat_internlimit_discarded_cl);       
        wr_alloc_err(g,"could not build new atom blt in wr_process_resolve_result ");
        return NULL;
      }
      res[tmp+RECORD_HEADER_GINTS+CLAUSE_EXTRAHEADERLEN+LIT_ATOM_POS]=blt;  
      wr_set_history(g,res,history);               
    }
    ++(g->stat_built_cl);    
  } else {
    // resulting clause was not a rule

    /*
    wr_printf("\nwr_derived_build_cl_from_initial_cl notrule case \n");
    wr_print_term_otter(g,rptr[LIT_ATOM_POS],1000);
    wr_printf("\n");
    //wg_print_record(db,rptr[LIT_ATOM_POS]);
    wr_printf("\n");
    */

    meta=RECORD_META_FACT_CLAUSE;
    blt=wr_build_calc_term(g,rptr[LIT_ATOM_POS]);

    /*
    wr_printf("\nwr_derived_build_cl_from_initial_cl notrule case built blt\n");
    wr_print_term_otter(g,blt,1000);
    wr_printf("\n");
    //wg_print_record(db,rptr[LIT_ATOM_POS]);
    wr_printf("\n");
    */

    if (blt==WG_ILLEGAL) {
      ++(g->stat_internlimit_discarded_cl);
      wr_alloc_err(g,"could not build new atom blt in wr_process_resolve_result ");
      return NULL;
    }
    res=otp(db,blt);
    wr_set_history(g,res,history); 
    res[RECORD_META_POS]=meta;
    ++(g->stat_built_cl);

    /*
    wr_printf("\nwr_derived_build_cl_from_initial_cl notrule case built res\n");
    wr_print_clause(g,res) ;
    wr_printf("\n");
    //wg_print_record(db,rptr[LIT_ATOM_POS]);
    wr_printf("\n");
    */

  } 
  return res;
}

/* -----------

helpers

-------------- */


/* ---------------------

 checks

 ------------------------ */


int wr_derived_weight_check(glb* g, double avg, int weight,  int size, int depth, int length, int xatomnr, int yatomnr) {
  //int res=1;
  // first immediate weight limits
  //printf(" avg %f ",avg);
  //printf("\n weight %d size %d depth %d length %d \n",weight,size,depth,length);

  if (depth>HARD_DEPTH_LIMIT) return 0;
  if (size>HARD_SIZE_LIMIT) return 0;

  if ((g->cl_keep_weightlimit) && weight>(g->cl_keep_weightlimit)) return 0;
  if ((g->cl_keep_sizelimit) && size>(g->cl_keep_sizelimit)) return 0;
  if ((g->cl_keep_depthlimit) && depth>(g->cl_keep_depthlimit)) return 0;
  if ((g->cl_keep_lengthlimit) && length>(g->cl_keep_lengthlimit)) return 0;

  //printf("\npassed 1\n");
  // next dynamic extra limits from space use

  
  // if more than half of the space used, do not store big clauses
  if (((g->queue_termbuf)[1]-1) * 2 > ((g->queue_termbuf)[0]) ) {      
    if ( ((g->queue_termbuf)[1]-1) > ((g->queue_termbuf)[0]) * (7.0/8.0) ) {
      // if more than 7/8 used, do not store anything
      if (1) {
        //printf("\n--- cur %d restored %d c %d lim %f \n",CVEC_NEXT(g->build_buffer),initial_queue_termbuf_next,((g->queue_termbuf)[0]),((g->queue_termbuf)[0]) * (7.0/8.0)); 
        return 0;
      }     
    } else if ( ((g->queue_termbuf)[1]-1) > ((g->queue_termbuf)[0]) * (3.0/4.0) ) {
      // if more than 3/4 used, store only twice smaller than average weight
      if (weight*2>=avg) {
        //printf("\n-- cur %d restored %d c %d lim %f \n",CVEC_NEXT(g->build_buffer),initial_queue_termbuf_next,((g->queue_termbuf)[0]),((g->queue_termbuf)[0]) * (3.0/4.0)); 
        return 0;     
      }
    } else if (weight>=avg) {
      // more than half but less than 3/4 used: store only smaller than average height
      //printf("\n- cur %d restored %d \n",CVEC_NEXT(g->build_buffer),initial_queue_termbuf_next); 
      return 0;
    } 
  }
  
  //printf("\npassed 2\n");
  // similarly, do not store if little time left
  
  //printf(" pratio %f ",g->passed_ratio);

#ifdef NONLENGTHENING_ONLY
// EXPERIMENTAL TEMPORARY!!!
    if ((g->passed_ratio)>0) {
      if (xatomnr && yatomnr) {        
        if ((g->passed_ratio)>0.5) { 
          //wr_printf(" over 0.5 ");          
          if (length>xatomnr || length>yatomnr) {
            (g->stat_weight_discarded_cl)++;
            if (g->print_derived_cl) wr_printf("\n -lengthlimit for over 0.5 %d by parents %d %d ",length,xatomnr,yatomnr);       
            return 0;
          }
        } else if ((g->passed_ratio)>0.1) { 
          //wr_printf(" over 0.1 "); 
          if (length>xatomnr && length>yatomnr) {
            (g->stat_weight_discarded_cl)++;
            if (g->print_derived_cl) wr_printf("\n -lengthlimit for over 0.1  %d by parents %d %d ",length,xatomnr,yatomnr);       
            return 0;
          }
        }      
      }    
    }
    /*
    if (length>xatomnr && length>yatomnr & xatomnr & yatomnr) {
      (g->stat_weight_discarded_cl)++;
      if (g->print_derived_cl) wr_printf("\n -lengthlimit %d by parents %d %d ",length,xatomnr,yatomnr);       
      return;
    } 
    */ 
#endif
  float endgcoeff=1.0;

  if ((g->passed_ratio)>0) {
    if ((g->passed_ratio)<(0.5*endgcoeff)) { 
      //wr_printf(" less than 0.5 "); 
      return 1; 
    }
    /*
    if (length>xatomnr && length>yatomnr & xatomnr & yatomnr) {
      (g->stat_weight_discarded_cl)++;
      if (g->print_derived_cl) wr_printf("\n -lengthlimit %d by parents %d %d ",length,xatomnr,yatomnr);       
      return;
    }  
    */
    if ((g->passed_ratio)>(0.90*endgcoeff)) { 
      //wr_printf(" over 90 %f ",(g->passed_ratio)); 
      return 0; 
    }
    if ((g->passed_ratio)>(0.7*endgcoeff) && (weight*2>=avg)) { 
      //wr_printf(" over 0.7 %f ",(g->passed_ratio)); 
      return 0;
    }
    if ((g->passed_ratio)>(0.5*endgcoeff) && (weight>=avg)) { 
      //wr_printf(" over 0.5 %f ",(g->passed_ratio)); 
      return 0; 
    }
  }
  //printf("\npassed 3\n");
  return 1; 
}  


gint wr_add_cl_to_unithash(glb* g, gptr cl, gint clmeta) {
  return wr_add_cl_to_unithash_h(g,cl,clmeta,NULL);
}

/* wr_add_cl_to_unithash; lithash: the literal hashes of cl by position, or NULL */

gint wr_add_cl_to_unithash_h(glb* g, gptr cl, gint clmeta, gint* lithash) {
  int len;
  gptr xatom;
  gint hash;
  void* db;

  db=g->db;   
  UNUSED(db);
  if (wg_rec_is_fact_clause(db,cl)) len=1;
  else len=wg_count_clause_atoms(db,cl);
  
  //printf("\nwr_add_cl_to_unithash:\n");
  //wr_print_clause(g,cl);
  //printf("\nlen %d wg_rec_is_rule_clause %d\n",len,wg_rec_is_rule_clause(db,cl));

  if (len==1) { //} && wg_atom_meta_is_ground(db,clmeta)) { //} && wg_rec_is_rule_clause(db,cl)) {
    if (wg_rec_is_rule_clause(db,cl)) {
      xatom=rotp(g,wg_get_rule_clause_atom(db,cl,0));        
    } else {
      xatom=cl;
    }  
    if (lithash) hash=lithash[0];
    else hash=wr_lit_hash(g,rpto(g,xatom));
    //printf("\nhash for storing is %d\n",(int)hash);
    if (wg_atom_meta_is_neg(db,clmeta)) {
      //printf("\nneg\n");
      wr_push_termhash(g,rotp(g,g->hash_neg_groundunits),hash,xatom,cl);
#ifdef SHARED_DERIVED      
      printf("\nstarting to copy,cl is:\n");
      wr_print_clause(g,cl);
      wg_print_record(db,cl);
      gptr newcl,newxatom,tmp_buffer;
      tmp_buffer=g->build_buffer;
      g->build_buffer=NULL;
      newcl=rotp(g,wr_copy_record(g,rpto(g,cl))); 
      printf("\ncopied, result is:\n");
      wr_print_clause(g,newcl);
      printf("\n");
      wg_print_record(db,newcl);
      printf("\n");
      g->build_buffer=tmp_buffer;
      if (wg_rec_is_rule_clause(db,newcl)) {
        newxatom=rotp(g,wg_get_rule_clause_atom(db,newcl,0));        
      } else {
        newxatom=newcl;
      } 
      printf("\nstarting to push to termhash\n"); 
      wr_push_termhash(g,rotp(g,g->shared_hash_neg_groundunits),hash,newxatom,newcl);
      printf("\npushed to termhash\n"); 
      wr_print_termhash(g,rotp(g,g->shared_hash_neg_groundunits));
      printf("\nsharing finished ok\n");
#endif      
      //wr_print_termhash(g,rotp(g,g->hash_neg_groundunits));
    } else {
      //printf("\npos\n");
      wr_push_termhash(g,rotp(g,g->hash_pos_groundunits),hash,xatom,cl); 
#ifdef SHARED_DERIVED      
      //wr_push_termhash(g,rotp(g,g->shared_hash_pos_groundunits),hash,xatom,cl);
#endif      
      //wr_print_termhash(g,rotp(g,g->hash_pos_groundunits));      
    }      
#ifdef DEBUGHASH    
    wr_printf("\ng->hash_neg_groundunits after adding:");      
    wr_print_termhash(g,rotp(g,g->hash_neg_groundunits));
    //wr_clterm_hashlist_print(g,rotp(g,g->hash_neg_groundunits));
    wr_printf("\ng->hash_pos_groundunits after adding:");   
    wr_print_termhash(g,rotp(g,g->hash_pos_groundunits));   
    //wr_clterm_hashlist_print(g,rotp(g,g->hash_pos_groundunits));
#ifdef SHARED_DERIVED    
    wr_printf("\ng->shared_hash_neg_groundunits after adding:");      
    wr_print_termhash(g,rotp(g,g->shared_hash_neg_groundunits));
    //wr_clterm_hashlist_print(g,rotp(g,g->hash_neg_groundunits));
    wr_printf("\ng->shared_hash_pos_groundunits after adding:");   
    wr_print_termhash(g,rotp(g,g->shared_hash_pos_groundunits));   
    //wr_clterm_hashlist_print(g,rotp(g,g->hash_pos_groundunits));
#endif    
#endif
    // for pushed units return hash of the unit
    return hash;
  }
  // if not pushed, i.e. not unit, return -1 to mark this
  return -1;
}



gint wr_add_cl_to_doublehash(glb* g, gptr cl) {
  return wr_add_cl_to_doublehash_h(g,cl,NULL);
}

/* wr_add_cl_to_doublehash; lithash: the literal hashes of cl by position, or NULL */

gint wr_add_cl_to_doublehash_h(glb* g, gptr cl, gint* lithash) {
  int len,pos;
  gptr xatom;
  gint hash;
  gint litmeta;
  void* db;

  db=g->db;   
  UNUSED(db);
#ifdef DEBUG
  // an entry must stay valid for the whole run: given_termbuf and
  // derived_termbuf are reused
  if ((g->given_termbuf && cl>=(g->given_termbuf) &&
       cl<(g->given_termbuf)+CVEC_LEN(g->given_termbuf)) ||
      (g->derived_termbuf && cl>=(g->derived_termbuf) &&
       cl<(g->derived_termbuf)+CVEC_LEN(g->derived_termbuf))) {
    wr_printf("\nerror: wr_add_cl_to_doublehash got a clause in a reused buffer\n");
  }
#endif
  if (wg_rec_is_fact_clause(db,cl)) return -1;
  else len=wg_count_clause_atoms(db,cl);
  
  //printf("\nwr_add_cl_to_doublehash:\n");
  //wr_print_clause(g,cl);
  //printf("\nlen %d wg_rec_is_rule_clause %d\n",len,wg_rec_is_rule_clause(db,cl));

  if (len==2) { //} && wg_atom_meta_is_ground(db,clmeta)) { //} && wg_rec_is_rule_clause(db,cl)) {
#ifdef USE_LIT_HASH64
    gint ph[2];   // literal hashes and signs for the pair table
    int pneg[2];
#endif
    for(pos=0; pos<2; pos++) {     
      xatom=rotp(g,wg_get_rule_clause_atom(db,cl,pos));
      litmeta=wg_get_rule_clause_atom_meta(db,cl,pos);
      if (lithash) hash=lithash[pos];
      else hash=wr_lit_hash(g,rpto(g,xatom));
#ifdef USE_LIT_HASH64
      ph[pos]=hash;
      pneg[pos]=wg_atom_meta_is_neg(db,litmeta);
#endif
      //printf("\nhash for storing is %d\n",(int)hash);
      if (wg_atom_meta_is_neg(db,litmeta)) {
        //printf("\nneg\n");
        wr_push_termhash(g,rotp(g,g->hash_neg_grounddoubles),hash,xatom,cl); 
        //wr_print_termhash(g,rotp(g,g->hash_neg_groundunits));
      } else {
        //printf("\npos\n");
        wr_push_termhash(g,rotp(g,g->hash_pos_grounddoubles),hash,xatom,cl); 
  #ifdef SHARED_DERIVED      
        //wr_push_termhash(g,rotp(g,g->shared_hash_pos_groundunits),hash,xatom,cl);
  #endif      
        //wr_print_termhash(g,rotp(g,g->hash_pos_groundunits));      
      }     
    }   
#ifdef DEBUGHASH    
    wr_printf("\ng->hash_neg_grounddoubles after adding:");      
    wr_print_termhash(g,rotp(g,g->hash_neg_grounddoubles));
    //wr_clterm_hashlist_print(g,rotp(g,g->hash_neg_grounddoubles));
    wr_printf("\ng->hash_pos_grounddoubles after adding:");   
    wr_print_termhash(g,rotp(g,g->hash_pos_grounddoubles));   
    //wr_clterm_hashlist_print(g,rotp(g,g->hash_pos_grounddoubles));
#ifdef SHARED_DERIVED    
    wr_printf("\ng->shared_hash_neg_grounddoubles after adding:");      
    wr_print_termhash(g,rotp(g,g->shared_hash_neg_grounddoubles));
    //wr_clterm_hashlist_print(g,rotp(g,g->hash_neg_grounddoubles));
    wr_printf("\ng->shared_hash_pos_grounddoubles after adding:");   
    wr_print_termhash(g,rotp(g,g->shared_hash_pos_grounddoubles));   
    //wr_clterm_hashlist_print(g,rotp(g,g->hash_pos_grounddoubles));
#endif    
#endif
#ifdef USE_LIT_HASH64
    wr_push_doublepair(g,cl,ph[0],pneg[0],ph[1],pneg[1]);
#endif
    // for pushed units return hash of the unit
    return hash;
  }
  // if not pushed, i.e. not unit, return -1 to mark this
  return -1;
}

int wr_process_resolve_result_isrulecl(glb* g, gptr rptr, int rpos) {
  void* db;
  int stopflag,ruleflag, len, i;
  gint meta, atom, term;
  gptr atomptr;
  
  if (rpos!=1) {
    return 1;
  } else {  
    // only clauses of len 1 check further
    db=g->db;
    stopflag=0;
    ruleflag=1;
    meta=rptr[LIT_META_POS];
    atom=rptr[LIT_ATOM_POS];     
    if (isdatarec(atom) && !wg_atom_meta_is_neg(db,meta)) {
      atomptr=decode_record(db,atom);
      len=get_record_len(atomptr);
      for(i=(g->unify_firstuseterm); i<len; i++) {
        term = get_field(atomptr,i);
        if (isdatarec(term) || isvar(term)) {
          stopflag=1;
          break;
        }        
      }
      if (!stopflag) {
        // really factclause 
        ruleflag=0;
      }  
    }
    return ruleflag;
  } 
}  

/**
 return -1 if not pure answer-clause, nr of ans literals otherwise

 */ 

int wr_cl_derived_is_answer(glb* g,gptr cl) {
  int len;
  int i;
  int ansnr;
  gint atom; //,meta;

  if (cl==NULL) return -1;
  ansnr=0;
  if (!wg_rec_is_rule_clause(g->db,cl)) {
    // fact clause
    len=1;
    atom=rpto(g,cl);
    //wr_print_term(g,atom);
    //printf("\n");
    if (wr_answer_lit(g,atom)) ansnr=1;    
  } else {
    // rule clause
    len=wg_count_clause_atoms(g->db,cl);   
    for(i=0;i<len;i++) {
      atom=wg_get_rule_clause_atom(g->db,cl,i);
      if (wr_answer_lit(g,atom)) ansnr++;      
    }
  } 
  if (ansnr==len) return ansnr;
  else return -1;    
}


/**
  satellite is a fully-built hypperesolution result, not temporary result
  
*/

int wr_hyperres_satellite_cl(glb* g,gptr cl) {
  int len;
  int i;
  gint meta;
  
  if (cl==NULL) return 0;
  if (!wg_rec_is_rule_clause(g->db,cl)) {
    // fact clause (hence always positive)
    //if (g->negpref_strat) return 1;
    //else return 0;    
    return 1;
  } else {
    // rule clause: check if contains only non-preferred
    len=wg_count_clause_atoms(g->db,cl);
    for(i=0;i<len;i++) {
      meta=wg_get_rule_clause_atom_meta(g->db,cl,i);
      if (wg_atom_meta_is_neg(g->db,meta)) {
        //if (g->negpref_strat) return 0;
        return 0;
      } else {
        //if (g->pospref_strat) return 0;  
      }        
    }
    //printf("\n wr_hyperres_satellite_cl returns 1\n");
    return 1;    
  }      
} 

/**
  satellite is a fully-built hypperesolution result, not temporary result
  

*/  


int wr_hyperres_satellite_tmpres(glb* g,gptr tmpres, int respos) {
  int i;
  gint tmeta;
    
  for(i=0;i<respos;i++) {
    tmeta=tmpres[(i*LIT_WIDTH)+LIT_META_POS];
    if (wg_atom_meta_is_neg(g->db,tmeta)) {
      //if (g->negpref_strat) return 0;
      return 0;
    } else {
      //if (g->pospref_strat) return 0;  
      //return 1;
    }        
  }
  return 1;    
} 



/* -----------

setups for building

-------------- */

void wr_process_resolve_result_setupsubst(glb* g) {
  g->build_rewrite=0;   // default is no rewriting during building
  g->build_subst=1;     // subst var values into vars
  g->build_calc=0;      // do fun and pred calculations
  g->build_dcopy=0;     // copy nonimmediate data (vs return ptr)
  //g->build_buffer=NULL; // build everything into tmp buffer (vs main area)
  (g->given_termbuf)[1]=2; // reuse given_termbuf
  g->build_buffer=g->derived_termbuf;
  //g->build_buffer=g->queue_termbuf;
  g->build_rename=1;   // do var renaming
  g->build_rename_maxseenvnr=-1; // tmp var for var renaming
  g->build_rename_vc=0;    // tmp var for var renaming 
  g->build_rename_banknr=3; // nr of bank of created vars
  // points to bank of created vars
  g->build_rename_bank=(g->varbanks)+((g->build_rename_banknr)*NROF_VARSINBANK);  
  g->use_comp_funs=g->use_comp_funs_strat;
}

void wr_process_resolve_result_cleanupsubst(glb* g) {
  int i;
  
  for(i=0;i<g->build_rename_vc;i++) {
    (g->build_rename_bank)[i]=UNASSIGNED;
  }  
}  

void wr_process_resolve_result_setupgivencopy(glb* g) {
  g->build_rewrite=0;   // default is no rewriting during building
  g->build_subst=0;     // subst var values into vars
  g->build_calc=0;      // do fun and pred calculations
  g->build_dcopy=0;     // copy nonimmediate data (vs return ptr)
  //g->build_buffer=NULL; // build everything into tmp buffer (vs main area)
  //(g->given_termbuf)[1]=2; // reuse given_termbuf
  //g->build_buffer=g->given_termbuf;
  //g->build_buffer=g->given_termbuf;
  //g->build_buffer=NULL;  // PROBLEM WAS HERE: given_termbuf not ok here  
  g->build_rename=0;   // do var renaming   
  g->use_comp_funs=0;
}


void wr_process_resolve_result_setupquecopy(glb* g) {
  g->build_subst=0;     // subst var values into vars
  g->build_calc=0;      // do fun and pred calculations
  g->build_dcopy=0;     // copy nonimmediate data (vs return ptr)
  //g->build_buffer=NULL; // build everything into tmp buffer (vs main area)
  //(g->given_termbuf)[1]=2; // reuse given_termbuf
  g->build_buffer=g->queue_termbuf;
  g->build_rename=0;   // do var renaming   
  g->use_comp_funs=0;
}

void wr_process_resolve_result_setuphypercopy(glb* g) {
  g->build_rewrite=0;   // default is no rewriting during building
  g->build_subst=0;     // subst var values into vars
  g->build_calc=0;      // do fun and pred calculations
  g->build_dcopy=0;     // copy nonimmediate data (vs return ptr)
  //g->build_buffer=NULL; // build everything into tmp buffer (vs main area)
  //(g->given_termbuf)[1]=2; // reuse given_termbuf
  g->build_buffer=g->hyper_termbuf;
  g->build_rename=0;   // do var renaming   
  g->use_comp_funs=0;
}

void wr_process_resolve_result_setupclpickstackcopy(glb* g) {
  g->build_rewrite=0;   // default is no rewriting during building
  g->build_subst=0;     // subst var values into vars
  g->build_calc=0;      // do fun and pred calculations
  g->build_dcopy=0;     // copy nonimmediate data (vs return ptr)
  //g->build_buffer=NULL; // build everything into tmp buffer (vs main area)
  //(g->given_termbuf)[1]=2; // reuse given_termbuf
  g->build_buffer=g->queue_termbuf;
  g->build_rename=0;   // do var renaming   
  g->use_comp_funs=0;  
}  

void wr_process_simp_setupquecopy(glb* g) {
  g->build_rewrite=0;   // default is no rewriting during building  
  g->build_subst=0;     // subst var values into vars
  g->build_calc=0;      // do fun and pred calculations
  g->build_dcopy=0;     // copy nonimmediate data (vs return ptr)
  //g->build_buffer=NULL; // build everything into tmp buffer (vs main area)
  //(g->given_termbuf)[1]=2; // reuse given_termbuf
  g->build_buffer=g->queue_termbuf;
  g->build_rename=1;   // do var renaming   
  g->use_comp_funs=0;
  /* 
  g->build_rename=1;   // do var renaming
  g->build_rename_maxseenvnr=-1; // tmp var for var renaming
  g->build_rename_vc=0;    // tmp var for var renaming 
  g->build_rename_banknr=3; // nr of bank of created vars
  // points to bank of created vars
  g->build_rename_bank=(g->varbanks)+((g->build_rename_banknr)*NROF_VARSINBANK);
  */
}




#ifdef __cplusplus
}
#endif
