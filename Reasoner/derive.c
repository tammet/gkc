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


void wr_process_resolve_result
      (glb* g, gint xatom, gptr xcl, gint yatom, gptr ycl, gptr xcl_as_active) {
  int xisrule,yisrule,xatomnr,yatomnr;
  int rlen;
  int tmp,tmp2;
  gptr rptr;
  int rpos,cutpos1=0,cutpos2=0;
  gptr res;  
  gint resmeta,history, hash;
  int ruleflag;
  gint initial_queue_termbuf_next=0;
  int weight;
  double avg;
  gint cl_metablock[CLMETABLOCK_ELS];
  int size,depth,length; 
  
#ifdef DEBUG
  wr_printf("\n+++ wr_process_resolve_result called\n");
  wr_print_clause(g,xcl); wr_printf(" : ");wr_print_term(g,xatom);
  wr_printf("\n");
  wr_print_clause(g,ycl);  wr_printf(" : ");wr_print_term(g,yatom);
  wr_printf("\n");
  wr_print_vardata(g);  
#endif  
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
  // store all ready-built atoms sequentially, excluding duplicates
  // and looking for tautology: only for rule clauses needed    
  rpos=0;
  if (xatomnr>1) {
    tmp=wr_process_resolve_result_aux(g,xcl,xatom,xatomnr,rptr,&rpos,&cutpos1,-1,0);
    if (!tmp) {
      wr_process_resolve_result_cleanupsubst(g);       
      return; 
    }  
  }
  if (yatomnr>1) { 
    tmp=wr_process_resolve_result_aux(g,ycl,yatom,yatomnr,rptr,&rpos,&cutpos2,-1,0);
    if (!tmp) {
      wr_process_resolve_result_cleanupsubst(g);   
      return;
    }  
  }
  wr_process_resolve_result_cleanupsubst(g);  
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

  // now the resulting clause is fully built
  /*
  if (g->print_derived_cl) {
    wr_printf("\n+ derived by mp: ");
    wr_print_clause(g,res);
  }  
  */
  // check if result contains only ans predicates
  
  //printf("\n checking for pure answer clause\n");
  //wr_print_clause(g,res);
  tmp=wr_cl_derived_is_answer(g,res);
  if (tmp>0) {
    wr_register_answer(g,res,history);
    //wr_printf("\n\nfound pure answer: ");
    //wr_print_clause(g,res);
    g->proof_found=1;   
    g->proof_history=history;    
    return;
  }
  gptr simp2=NULL;
  gint cl_metablock2[CLMETABLOCK_ELS];
  simp2=wr_simplify_doublecut_cl(g,res,cl_metablock2);
  if (!simp2) {
    //printf("\ndoublecut found answer\n");
    return;
  }  
  if (simp2!=res) {
    //printf("+");
    /*
    printf("\ndoublecut made a simplification\n");
    printf("\norig ");
    wr_print_clause(g,res);
    printf("\nsimp ");
    wr_print_clause(g,simp2);
    printf("\n");
    */
    res=simp2;
  }


  // start storing to queues and hashes

  if ((g->hyperres_strat) &&  !wr_hyperres_satellite_cl(g,res)) { 
    // hyperresolution case: not a finished clause yet, must cut off some 
    //printf("\nwr_process_resolve_result hyperres-strat and not satellite\n");
    ++(g->stat_hyperres_partial_cl);
    ++(g->stat_derived_partial_hyper_cl);
    if (g->print_partial_derived_cl) {
      wr_printf("\n+ partial derived by mp: ");
      wr_print_clause(g,res);
    }
    weight=wr_calc_clause_weight(g,res,&size,&depth,&length);
    //avg=(g->avg_kept_weight);       
    //printf(" weight %d average %f count %d \n",weight,(g->avg_kept_weight),(g->stat_kept_cl));
    /* 
    if (!wr_derived_weight_check(g,avg,weight,size,depth,length,0,0)) {
      (g->stat_weight_discarded_cl)++;
      CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
      if (g->print_derived_cl) wr_printf("\nw discarded overweight");
      return;
    }
    */
    tmp=wr_push_cl_hyper_queue(g,(g->hyper_queue),res,weight);     
    if (!tmp) {
      wr_alloc_err(g,"could not store into hyper_queue in wr_process_resolve_result ");
      return; // could not alloc memory, could not store clause
    }
  } else {           
    // ordinary case (not partial hyperres): resulting clause is finished
#ifdef GLOBALUNITS
      int rflag=wg_rec_is_rule_clause(db,res);
      int rlen;
      if (rflag) rlen = wg_count_clause_atoms(db,res);
      else rlen=1;
      if (rlen==1) {
        wr_printf("\n+ unit by mp: ");
        wr_print_clause(g,res);
      }
#endif
    if (g->print_derived_cl) {
      wr_printf("\n+ derived by mp: ");
      wr_print_clause(g,res);    
    } else if (g->print_derived) {  
      wr_print_clause_via_buf(g,res);
    }  
    weight=wr_calc_clause_weight(g,res,&size,&depth,&length);
    avg=(g->avg_kept_weight);       
    //printf(" weight %d average %f count %d \n",weight,(g->avg_kept_weight),(g->stat_kept_cl));
    if (!wr_derived_weight_check(g,avg,weight,size,depth,length,xatomnr,yatomnr)) {
      (g->stat_weight_discarded_cl)++;
      CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
      if (g->print_derived_cl) wr_printf("\nw discarded overweight");
      return;
    }
    ++(g->stat_kept_cl);
    avg+=(weight-avg)/((g->stat_kept_cl)+1);
    (g->avg_kept_weight)=avg;
    resmeta=wr_calc_clause_meta(g,res,cl_metablock);
    hash=wr_add_cl_to_unithash(g,res,resmeta); 
    wr_add_cl_to_doublehash(g,res);
    if (g->propagate) {
      if (hash>=0)  {
        // unit, propagate
        wr_propagate_unit(g,res,resmeta,hash,xcl_as_active);
        if (g->proof_found || g->alloc_err) {
          //wr_clear_varstack(g,g->varstack);          
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
    if (g->use_strong_unit_cutoff) wr_cl_store_res_units(g,res);
    wr_push_cl_clpick_queues(g,(g->clpick_queues),res,weight);
    tmp=wr_cl_create_propinst(g,res);
    if (tmp==2) {  return;  }
  }    
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
  gint resmeta,history, hash;
  int ruleflag;
  gint initial_queue_termbuf_next=0;
  int weight;
  double avg;
  gint cl_metablock[CLMETABLOCK_ELS];
  int size,depth,length; 
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
  // store all ready-built atoms sequentially, excluding duplicates
  // and looking for tautology: only for rule clauses needed    
  rpos=0;
  tmp=wr_process_instgen_result_aux(g,cl,atom,atomnr,rptr,&rpos,&cutpos1,-1,0);
  if (!tmp) {
    wr_process_resolve_result_cleanupsubst(g);       
    return; 
  }
  wr_process_resolve_result_cleanupsubst(g);  
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
  wr_sort_cl(g,res); // note that sorting here is not done for resolution results

  // now the resulting clause is fully built
  /*
  if (g->print_derived_cl) {
    wr_printf("\n+ derived by mp: ");
    wr_print_clause(g,res);
  }  
  */
  // check if result contains only ans predicates
  
  //printf("\n checking for pure answer clause\n");
  //wr_print_clause(g,res);
  tmp=wr_cl_derived_is_answer(g,res);
  if (tmp>0) {
    wr_register_answer(g,res,history);
    //wr_printf("\n\nfound pure answer: ");
    //wr_print_clause(g,res);
    g->proof_found=1;   
    g->proof_history=history;    
    return;
  }

  // start storing to queues and hashes

  if (1) {           
    // ordinary case (not partial hyperres): resulting clause is finished
    if (g->print_derived_cl) {
      wr_printf("\n+ derived by instgen: ");
      wr_print_clause(g,res);    
    } else if (g->print_derived) {  
      wr_print_clause_via_buf(g,res);
    } 
    weight=wr_calc_clause_weight(g,res,&size,&depth,&length);
    avg=(g->avg_kept_weight);       
    //printf(" weight %d average %f count %d \n",weight,(g->avg_kept_weight),(g->stat_kept_cl));
    if (0) { //!wr_derived_weight_check(g,avg,weight,size,depth,length,xatomnr,yatomnr)) {
      (g->stat_weight_discarded_cl)++;
      CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
      if (g->print_derived_cl) wr_printf("\nw discarded overweight");
      return;
    }
    ++(g->stat_kept_cl);
    avg+=(weight-avg)/((g->stat_kept_cl)+1);
    (g->avg_kept_weight)=avg;
    resmeta=wr_calc_clause_meta(g,res,cl_metablock);
    hash=wr_add_cl_to_unithash(g,res,resmeta); 
  
    if (hash<0) { // length is 2 or bigger
    //if (!(wg_rec_is_fact_clause(db,cl)) && wg_count_clause_atoms(db,cl)>1) {
      // nonunit clause: add to clause hash
      wr_push_prop_clausehash(g,rotp(g,g->prop_hash_clauses),clhash,res);
    }

    if (g->propagate) {
      if (hash>=0)  {
        // unit, propagate
        wr_propagate_unit(g,res,resmeta,hash,xcl_as_active);
        if (g->proof_found || g->alloc_err) {
          //wr_clear_varstack(g,g->varstack);          
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
    if (g->use_strong_unit_cutoff) wr_cl_store_res_units(g,res);
    wr_push_cl_clpick_queues(g,(g->clpick_queues),res,weight);
    tmp=wr_cl_create_propinst(g,res);
    if (tmp==2) {  return;  }
  }    
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
  gint resmeta,history;
  int ruleflag;
  gint initial_queue_termbuf_next=0;
  int weight;
  double avg;
  gint cl_metablock[CLMETABLOCK_ELS];
  int size,depth,length;
  
#ifdef DEBUG
  wr_printf("+++ wr_process_factor_result called\n");
  wr_print_clause(g,xcl); wr_printf(" %d, %d : ",x,y);
  wr_print_term(g,xatom);
  wr_printf("\n");
  wr_print_term(g,yatom);
  wr_printf("\n");
  wr_print_vardata(g);
#endif  

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
  (g->derived_termbuf)[1]=2; // init termbuf
  rptr=wr_alloc_from_cvec(g,g->derived_termbuf,rlen); 
  if (rptr==NULL) {
    ++(g->stat_internlimit_discarded_cl);
    wr_alloc_err(g,"could not alloc first buffer in wr_process_resolve_result ");
    return; // could not alloc memory, could not store clause
  }  
  // set up var rename params
  wr_process_resolve_result_setupsubst(g);  
  // store all ready-built atoms sequentially, excluding duplicates
  // and looking for tautology: only for rule clauses needed    
  rpos=0;
  tmp=wr_process_resolve_result_aux(g,xcl,xatom,len,rptr,&rpos, &cutpos1,-1,0);
  if (!tmp) {
    wr_process_resolve_result_cleanupsubst(g);       
    return; 
  }    
  wr_process_resolve_result_cleanupsubst(g);
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

  history=wr_build_factorial_history(g,xcl_as_active,x,y,g->cut_clvec);
  res=wr_derived_build_cl_from_initial_cl(g,rptr,rpos,ruleflag,history);
  if (!history || !res) {
    // allocation failed
    return;
  }

  // check if result contains only ans predicates
  
  //printf("\n checking for pure answer clause\n");
  //wr_print_clause(g,res);
  tmp=wr_cl_derived_is_answer(g,res);
  if (tmp>0) {
    wr_register_answer(g,res,history);
    //wr_printf("\n\nfound pure answer: ");
    //wr_print_clause(g,res);
    g->proof_found=1;
    g->proof_history=history;     
    return;
  }   

  if ((g->hyperres_strat) &&  !wr_hyperres_satellite_cl(g,res)) { 
    // hyperresolution case: not a finished clause yet, must cut off some 
    //printf("\nwr_process_resolve_result hyperres-strat and not satellite\n");
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
    // resulting clause is finished
#ifdef GLOBALUNITS
      int rflag=wg_rec_is_rule_clause(db,res);
      int rlen;
      if (rflag) rlen = wg_count_clause_atoms(db,res);
      else rlen=1;
      if (rlen==1) {
        wr_printf("\n+ unit by factor: ");
        wr_print_clause(g,res);
      }
#endif    
    if (g->print_derived_cl) {
      wr_printf("\n+ derived by merge: ");
      wr_print_clause(g,res);    
    } else if (g->print_derived) {  
      wr_print_clause_via_buf(g,res);
    }  
    weight=wr_calc_clause_weight(g,res,&size,&depth,&length);
    avg=(g->avg_kept_weight);  
    if (!wr_derived_weight_check(g,avg,weight,size,depth,length,0,0)) {
      (g->stat_weight_discarded_cl)++;
      CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
      if (g->print_derived_cl) wr_printf("\nw discarded overweight");
      return;
    }
    ++(g->stat_kept_cl);
    avg+=(weight-avg)/((g->stat_kept_cl)+1);
    (g->avg_kept_weight)=avg;
    resmeta=wr_calc_clause_meta(g,res,cl_metablock);
    wr_add_cl_to_unithash(g,res,resmeta);
    wr_add_cl_to_doublehash(g,res);
    if (g->use_strong_unit_cutoff) wr_cl_store_res_units(g,res);
    wr_push_cl_clpick_queues(g,(g->clpick_queues),res,weight);   
    tmp=wr_cl_create_propinst(g,res);    
    if (tmp==2) {  return;  }
  }  
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
  gint resmeta,history;
  int ruleflag;
  gint initial_queue_termbuf_next=0;
  int weight;
  double avg;
  gint cl_metablock[CLMETABLOCK_ELS];
  int size,depth,length;
    
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
  // store all ready-built atoms sequentially, excluding duplicates
  // and looking for tautology: only for rule clauses needed    
  rpos=0;
  if (xatomnr>1) {    
    tmp=wr_process_resolve_result_aux(g,xcl,xatom,xatomnr,rptr,&rpos,&cutpos1,-1,0);    
    if (!tmp) {
      wr_process_resolve_result_cleanupsubst(g);       
      return; 
    }  
  }
  //if (yatomnr>1) { // ???
  tmp=wr_process_resolve_result_aux(g,ycl,0,yatomnr,rptr,&rpos,&cutpos2,path,bterm);
  if (!tmp) {
    wr_process_resolve_result_cleanupsubst(g);   
    return;
  }  
  //}
  wr_process_resolve_result_cleanupsubst(g);  
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

  // now the resulting clause is fully built

  /*
  wr_printf("\n+!+!+!+!+ derived by = ");
  if (fromflag) wr_printf("from: ");
  else wr_printf("into: ");
  wr_print_clause(g,res);
  */
  //printf("\n derived as raw record: \n");
  //wg_print_record(g->db,res);
  //printf("\n");
  /*
  if (g->print_derived_cl) {
    wr_printf("\n+ derived by = ");
    if (fromflag) wr_printf("from: ");
    else wr_printf("into: ");
    wr_print_clause(g,res);
    //printf("\n derived as raw record: \n");
    //wg_print_record(g->db,res);
    //printf("\n");
  }
  */

  // check if result contains only ans predicates
  
  //printf("\n checking for pure answer clause\n");
  //wr_print_clause(g,res);
  tmp=wr_cl_derived_is_answer(g,res);
  if (tmp>0) {
    wr_register_answer(g,res,history);
    //wr_printf("\n\nfound pure answer: ");
    //wr_print_clause(g,res);
    g->proof_found=1;     
    g->proof_history=history;   
    return;
  }

  // start storing to queues and hashes

  if ((g->hyperres_strat) &&  !wr_hyperres_satellite_cl(g,res)) { 
    // hyperresolution case: not a finished clause yet, must cut off some 
    //printf("\nwr_process_resolve_result hyperres-strat and not satellite\n");
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
    //  resulting clause is finished
#ifdef GLOBALUNITS
      int rflag=wg_rec_is_rule_clause(db,res);
      int rlen;
      if (rflag) rlen = wg_count_clause_atoms(db,res);
      else rlen=1;
      if (rlen==1) {
        wr_printf("\n+ unit by factor: ");
        wr_print_clause(g,res);
      }
#endif    
    if (g->print_derived_cl) {
      wr_printf("\n+ derived by = ");
      if (fromflag) {
        wr_printf("from: ");
      } else {
        wr_printf("into: ");
      } 
      wr_print_clause(g,res);
    } else if (g->print_derived) {  
      wr_print_clause_via_buf(g,res);
    }    
    weight=wr_calc_clause_weight(g,res,&size,&depth,&length);  
    avg=(g->avg_kept_weight);  
    //printf(" weight %d average %f count %d \n",weight,(g->avg_kept_weight),(g->stat_kept_cl));
    if (!wr_derived_weight_check(g,avg,weight,size,depth,length,xatomnr,yatomnr)) {
      (g->stat_weight_discarded_cl)++;
      CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
      if (g->print_derived_cl) wr_printf("\nw discarded overweight");
      return;
    }
    ++(g->stat_kept_cl);
    avg+=(weight-avg)/((g->stat_kept_cl)+1);
    (g->avg_kept_weight)=avg;
    resmeta=wr_calc_clause_meta(g,res,cl_metablock);
    wr_add_cl_to_unithash(g,res,resmeta);
    wr_add_cl_to_doublehash(g,res);
    if (g->use_strong_unit_cutoff) wr_cl_store_res_units(g,res);
    wr_push_cl_clpick_queues(g,(g->clpick_queues),res,weight); 
    tmp=wr_cl_create_propinst(g,res);    
    if (tmp==2) {  return;  }
  }  
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
  int len;
  gptr xatom;
  gint hash;
  void* db;

  db=g->db;   
  UNUSED(db);
  if (wg_rec_is_fact_clause(db,cl)) len=1;
  else len=wg_count_clause_atoms(db,cl);
  
  /*
  printf("\nwr_add_cl_to_unithash:\n");
  wr_print_clause(g,cl);
  printf("\nlen %d wg_rec_is_rule_clause %d\n",len,wg_rec_is_rule_clause(db,cl));
  printf("\ng %d g->db %d g->kb_db %ld\n",(gint)g,(gint)(g->db),(gint)(g->kb_db));
  */

  if (len==1) { //} && wg_atom_meta_is_ground(db,clmeta)) { //} && wg_rec_is_rule_clause(db,cl)) {
    if (wg_rec_is_rule_clause(db,cl)) {
      xatom=rotp(g,wg_get_rule_clause_atom(db,cl,0));        
    } else {
      xatom=cl;
    }     
    hash=wr_lit_hash(g,rpto(g,xatom));
    //printf("\nhash for storing is %d\n",(int)hash);
    if (wg_atom_meta_is_neg(db,clmeta)) {
      //printf("\nneg\n");
      wr_push_termhash(g,rotp(g,g->hash_neg_groundunits),hash,xatom,cl);
      //wr_print_termhash(g,rotp(g,g->hash_neg_groundunits));
    } else {
      //printf("\npos\n");
      wr_push_termhash(g,rotp(g,g->hash_pos_groundunits),hash,xatom,cl);     
      //wr_print_termhash(g,rotp(g,g->hash_pos_groundunits));      
    }      
#ifdef DEBUGHASH    
    wr_printf("\ng->hash_neg_groundunits after adding:");      
    wr_print_termhash(g,rotp(g,g->hash_neg_groundunits));
    //wr_clterm_hashlist_print(g,rotp(g,g->hash_neg_groundunits));
    wr_printf("\ng->hash_pos_groundunits after adding:");   
    wr_print_termhash(g,rotp(g,g->hash_pos_groundunits));   
    //wr_clterm_hashlist_print(g,rotp(g,g->hash_pos_groundunits));   
#endif
    //wr_print_termhash(g,rotp(g,g->hash_pos_groundunits));  
    // for pushed units return hash of the unit
    return hash;
  }
  // if not pushed, i.e. not unit, return -1 to mark this
  return -1;
}


gint wr_add_cl_to_shared_unithash(glb* g, glb* shared_g, gptr xatom, gptr cl, gint clmeta, 
       int* storecount, int pos, int type, int bucketnr) {
  int len, histlen, atomlen;
  //gptr xatom;
  gint hash, tmp, history;
  void* db;
  void* shared_db;
  gptr historyptr,newcl,newxatom,tmp_buffer,oldptr; // clptr,

  db=g->db;   
  shared_db=shared_g->db;
  UNUSED(db);
  UNUSED(shared_db);
  if (!cl) return -1;
  if (wg_rec_is_fact_clause(db,cl)) len=1;
  else len=wg_count_clause_atoms(db,cl);
  
  /*
  printf("\n!!! wr_add_cl_to_shared_unithash pos %d type %d, xatom:\n",pos,type);
  wr_print_term(g,rpto(g,xatom));
  printf("\n clause ");
  wr_print_clause(g,cl);
  printf("\n record");
  wg_print_record(db,cl);
  printf("\n");
  */
  
  /*
  printf("\nwr_add_cl_to_shared_unithash:\n");
  printf("\ng %ld shared_g %ld shared_db %ld shared_g->db %ld\n",(gint)g,(gint)shared_g,(gint)shared_db,(gint)(shared_g->db));
  wr_print_clause(g,cl);
  printf("\nlen %d wg_rec_is_rule_clause %d\n",len,wg_rec_is_rule_clause(db,cl));
  wr_print_termhash(g,rotp(g,g->hash_neg_groundunits)); 
  */
  if (type==0 && len!=1) return -1;
  if (type==1 && len!=2) return -1; 
  if (xatom==NULL) return -1;
  atomlen=wg_get_record_len(db,xatom);
  if (atomlen<1 || atomlen>100) {
      /*
      printf("\natomlen %d",atomlen);
      printf("M xatom %ld\n",(gint)xatom);  
      wr_print_term(g,rpto(g,xatom));
      printf("\nclause");
      wr_print_clause(g,cl);
      printf("\n record");
      wg_print_record(db,cl);
      printf("\n record el 0 %lx %ld type %d\n",wg_decode_record(db, wg_get_field(db, cl,0)),wg_get_field(db,cl,0),wg_get_encoded_type(db,wg_get_field(db,cl,0)));
      //wg_print_record(db,wg_get_field(db,cl,0));
      printf("\n record el 1 %lx  %ld type %d\n",wg_get_field(db,cl,1),wg_decode_int(db,wg_get_field(db,cl,1)),wg_get_encoded_type(db,wg_get_field(db,cl,1)));
      //wg_print_record(db,wg_get_field(db,cl,1));
      printf("\n record el 2 %lx  %ld type %d\n",wg_decode_record(db,wg_get_field(db,cl,2)),wg_get_field(db,cl,2),wg_get_encoded_type(db,wg_get_field(db,cl,2)));
      wg_print_record(db,wg_decode_record(db,wg_get_field(db,cl,2)));
      printf("\nxatom again\n");
      //wg_print_record(db,wg_decode_record(db,xatom)) ;
      wg_print_record(db,xatom) ;

      printf("\nM2\n");
      */
      return -1;
      //exit(0);
  }

  if (1) { //} && wg_atom_meta_is_ground(db,clmeta)) { //} && wg_rec_is_rule_clause(db,cl)) {           
    history=wr_get_history(g,cl);
    historyptr=otp(db,history);    
    histlen=wg_get_record_len(db,historyptr); 
    /*
    printf("\nhistlen %d history:\n",histlen);
    wg_print_record(db,historyptr);
    printf("\n");
    */   
    if (histlen==HISTORY_PREFIX_LEN) return -1; // input clause     
    if (type==1) {
      if (!ok_double_hash_elem(g,xatom,cl)) {
        /*
        printf("\n!!!atomlen %d",atomlen);
        printf("M xatom %ld\n",(gint)xatom);  
        wr_print_term(g,rpto(g,xatom));
        printf("\nclause");
        wr_print_clause(g,cl);
        printf("\n record");
        wg_print_record(db,cl);
        printf("\n record el 0 %lx %ld type %d\n",wg_decode_record(db, wg_get_field(db, cl,0)),wg_get_field(db,cl,0),wg_get_encoded_type(db,wg_get_field(db,cl,0)));
        //wg_print_record(db,wg_get_field(db,cl,0));
        printf("\n record el 1 %lx  %ld type %d\n",wg_get_field(db,cl,1),wg_decode_int(db,wg_get_field(db,cl,1)),wg_get_encoded_type(db,wg_get_field(db,cl,1)));
        //wg_print_record(db,wg_get_field(db,cl,1));
        printf("\n record el 2 %lx  %ld type %d\n",wg_decode_record(db,wg_get_field(db,cl,2)),wg_get_field(db,cl,2),wg_get_encoded_type(db,wg_get_field(db,cl,2)));
        wg_print_record(db,wg_decode_record(db,wg_get_field(db,cl,2)));
        printf("\nxatom again\n");
        //wg_print_record(db,wg_decode_record(db,xatom)) ;
        wg_print_record(db,xatom) ;

        printf("\n!!M2\n");
        */
        return -1;
        //exit(0);
      }
      /*
      printf("M1\n");
      wr_print_term(g,rpto(g,xatom));
      printf("\n atom as record");
      wg_print_record(db,xatom);
      printf("\nclause");
      wr_print_clause(g,cl);
      printf("\n record");
      wg_print_record(db,cl);
      printf("\nM2\n");
      */
    } 
    /*
    if (wg_rec_is_rule_clause(db,cl)) {
      xatom=rotp(g,wg_get_rule_clause_atom(db,cl,0));   
      //clmeta=wg_get_rule_clause_meta(db,cl,0);     
    } else {
      xatom=cl;
    } 
    */ 
   
    hash=wr_lit_hash(g,rpto(g,xatom));   
    //printf("# %d %d %ld ",type,bucketnr,hash);
    if (!pos) { //(wg_atom_meta_is_neg(db,clmeta)) {
      //CP10
      if (type==1) {       
        oldptr=wr_find_shared_offset_termhash(g,rotp(shared_g,shared_g->shared_hash_neg_grounddoubles), xatom, hash,type,cl);      
      } else {
        oldptr=wr_find_shared_offset_termhash(g,rotp(shared_g,shared_g->shared_hash_neg_groundunits), xatom, hash,type,NULL);
      }  
      //CP11
      //printf("\n oldptr: %ld oldptr==NULL: %d\n",(gint)oldptr,oldptr==NULL);
      if (oldptr==NULL) {
        //CP12
        tmp_buffer=shared_g->build_buffer;
        shared_g->build_buffer=NULL;     
        tmp=wr_copy_clause(g,shared_g,rpto(g,cl));
        if (tmp) {
          newcl=rotp(shared_g,tmp);         
          //exit(0);
          (*storecount)++;
          /*
          printf("store: ");
          wr_print_clause(shared_g,newcl);
          printf("\n");
          */
          shared_g->build_buffer=tmp_buffer;
          if (wg_rec_is_rule_clause(shared_db,newcl)) {
            if (type==0) {
              newxatom=rotp(shared_g,wg_get_rule_clause_atom(shared_db,newcl,0));        
            } else {
              if (rpto(g,xatom)==wg_get_rule_clause_atom(shared_db,cl,0)) {
                newxatom=rotp(shared_g,wg_get_rule_clause_atom(shared_db,newcl,0));
              } else {
                newxatom=rotp(shared_g,wg_get_rule_clause_atom(shared_db,newcl,1));
              }
            }  
          } else {
            newxatom=newcl;
          }         
          if (type==1)
            wr_push_termhash(shared_g,rotp(shared_g,shared_g->shared_hash_neg_grounddoubles),hash,newxatom,newcl);
          else
            wr_push_termhash(shared_g,rotp(shared_g,shared_g->shared_hash_neg_groundunits),hash,newxatom,newcl);
          //printf("\npushed to neg termhash\n");
          //printf("\npushed to termhash shared_db %ld, shared_g %ld, shared_g->shared_hash_neg_groundunits %ld \n",
          //  (gint)shared_db,(gint)shared_g,(gint)(shared_g->shared_hash_neg_groundunits)); 
          //wr_print_termhash(shared_g,rotp(shared_g,shared_g->shared_hash_neg_groundunits));
          //printf("\nsharing finished ok\n");    
        }  
      }  
    } else {
      //CP15     
      if (type==1) {
        oldptr=wr_find_shared_offset_termhash(g,rotp(shared_g,shared_g->shared_hash_pos_grounddoubles), xatom, hash,type,cl);
      } else {
        oldptr=wr_find_shared_offset_termhash(g,rotp(shared_g,shared_g->shared_hash_pos_groundunits), xatom, hash,type,NULL);   
      }  
      //CP16
      //printf("\n oldptr: %ld oldptr==NULL: %d\n",(gint)oldptr,oldptr==NULL);
      if (oldptr==NULL) {           
        //CP17
        tmp_buffer=shared_g->build_buffer;
        shared_g->build_buffer=NULL;
        //CP18
        tmp=wr_copy_clause(g,shared_g,rpto(g,cl));
        //CP19
        //printf("\ntmp: %ld\n",(gint)tmp);
        if (tmp) {
          newcl=rotp(shared_g,tmp);               
          //exit(0);
          //wg_print_record(shared_db,newcl);
          (*storecount)++;
          /*
          printf("store: ");
          wr_print_clause(shared_g,newcl);
          printf("\n");
          */
          //CP1
          shared_g->build_buffer=tmp_buffer;
          if (wg_rec_is_rule_clause(shared_db,newcl)) {                 
            if (type==0) {
              newxatom=rotp(shared_g,wg_get_rule_clause_atom(shared_db,newcl,0));        
            } else {
              if (rpto(g,xatom)==wg_get_rule_clause_atom(shared_db,cl,0)) {
                newxatom=rotp(shared_g,wg_get_rule_clause_atom(shared_db,newcl,0));
              } else {
                newxatom=rotp(shared_g,wg_get_rule_clause_atom(shared_db,newcl,1));
              }
            }  
          } else {            
            newxatom=newcl;
          } 
          if (type==1)
            wr_push_termhash(shared_g,rotp(shared_g,shared_g->shared_hash_pos_grounddoubles),hash,newxatom,newcl);
          else
            wr_push_termhash(shared_g,rotp(shared_g,shared_g->shared_hash_pos_groundunits),hash,newxatom,newcl);        
          //printf("\npushed to pos termhash\n"); 
          //wr_print_termhash(shared_g,rotp(shared_g,shared_g->shared_hash_pos_groundunits));
          //printf("\nsharing finished ok\n");       
        }       
      }  
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
    /*
    printf("\nto wr_print_termhash for shared_g\n");
    wr_print_termhash(shared_g,rotp(shared_g,shared_g->hash_pos_groundunits));  
    printf("\nended wr_print_termhash for shared_g\n");
    */
    // for pushed units return hash of the unit
    return hash;
  }
  // if not pushed, i.e. not unit, return -1 to mark this
  return -1;
}

int ok_double_hash_elem(glb* g,gptr xatom, gptr cl) {
  int pos;
  gptr clatom;
  void* db=g->db;

  if (!xatom || !cl) return 0;
  for(pos=0; pos<2; pos++) {
    clatom=rotp(g,wg_get_rule_clause_atom(db,cl,pos));
    if (xatom==clatom) return 1;    
  }  
  return 0;
}

gint wr_add_cl_to_doublehash(glb* g, gptr cl) {
  int len,pos;
  gptr xatom;
  gint hash;
  gint litmeta;
  void* db;

  db=g->db;   
  UNUSED(db);
  if (wg_rec_is_fact_clause(db,cl)) return -1;
  else len=wg_count_clause_atoms(db,cl);
  
  

  if (len==2) { //} && wg_atom_meta_is_ground(db,clmeta)) { //} && wg_rec_is_rule_clause(db,cl)) {
    /*
    printf("\nwr_add_cl_to_doublehash:\n");
    wr_print_clause(g,cl);
    printf("\nlen %d wg_rec_is_rule_clause %d\n",len,wg_rec_is_rule_clause(db,cl));
    */

    for(pos=0; pos<2; pos++) {     
      xatom=rotp(g,wg_get_rule_clause_atom(db,cl,pos));
      /*
      printf("\n xatom ");
      wr_print_term(g,rpto(g,xatom));
      printf("\n");
      */
      litmeta=wg_get_rule_clause_atom_meta(db,cl,pos);
      hash=wr_lit_hash(g,rpto(g,xatom));
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
