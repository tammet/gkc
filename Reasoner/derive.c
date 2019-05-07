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
  //void* db=g->db;
  int xisrule,yisrule,xatomnr,yatomnr;
  int rlen;
  int tmp;
  gptr rptr;
  int rpos,cutpos1=0,cutpos2=0;
  gptr res;  
  gint resmeta,history, hash;
  int ruleflag;
  //int partialresflag;
  //int clstackflag;
  //gint given_termbuf_storednext=0;
  gint initial_queue_termbuf_next=0;
  int weight;
  double avg;
  gint cl_metablock[CLMETABLOCK_ELS];
  int size,depth,length;

  /*
  printf("\n+++ wr_process_resolve_result called\n");
  wr_print_clause(g,xcl); printf(" : ");wr_print_term(g,xatom);
  printf("\n");
  wr_print_clause(g,ycl);  printf(" : ");wr_print_term(g,yatom);
  printf("\n");
  wr_print_vardata(g); 
  */
#ifdef DEBUG
  printf("\n+++ wr_process_resolve_result called\n");
  wr_print_clause(g,xcl); printf(" : ");wr_print_term(g,xatom);
  printf("\n");
  wr_print_clause(g,ycl);  printf(" : ");wr_print_term(g,yatom);
  printf("\n");
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
    return;
  } 

  // check subsumption and cutoff
  
  if (g->print_derived_precut_cl) {
    printf("\nc pre-cut derived by mp: ");
    wr_print_halfbuilt_clause(g,rptr,rpos);
  }
  tmp=wr_derived_cl_cut_and_subsume(g,rptr,rpos);
  if (tmp<0) {
    // clause was subsumed
#ifdef DEBUG      
    printf("\nwr_process_resolve_result was subsumed with new subsumer\n");
#endif    
    if (g->print_derived_subsumed_cl) {
      printf("\n- subsumed derived by mp ");
      wr_print_halfbuilt_clause(g,rptr,rpos);
    }
    return;    
  } else if (tmp>0) {
    // there were cuts
    if (g->print_derived_precut_cl) {
      printf("\nc post-cut derived by mp ");
      wr_print_halfbuilt_clause(g,rptr,rpos);
    }
    wr_process_resolve_result_remove_cuts(g,rptr,&rpos,tmp);
    (g->stat_derived_cut)++;
    if (rpos==0) {
      g->proof_found=1;    
      g->proof_history=wr_build_resolve_history(g,xcl_as_active,ycl,cutpos1,cutpos2,g->cut_clvec); 
      return;
    } 
  }


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
  if (!history || !res) {
    // allocation failed
    return;
  }  

  // now the resulting clause is fully built
  /*
  if (g->print_derived_cl) {
    printf("\n+ derived by mp: ");
    wr_print_clause(g,res);
  }  
  */
  // check if result contains only ans predicates
  
  //printf("\n checking for pure answer clause\n");
  //wr_print_clause(g,res);
  tmp=wr_cl_derived_is_answer(g,res);
  if (tmp>0) {
    printf("\n\nfound pure answer: ");
    wr_print_clause(g,res);
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
      printf("\n+ partial derived by mp: ");
      wr_print_clause(g,res);
    }
    weight=wr_calc_clause_weight(g,res,&size,&depth,&length);
    avg=(g->avg_kept_weight);       
    //printf(" weight %d average %f count %d \n",weight,(g->avg_kept_weight),(g->stat_kept_cl));
    if (!wr_derived_weight_check(g,avg,weight,size,depth,length)) {
      (g->stat_weight_discarded_cl)++;
      CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
      if (g->print_derived_cl) printf("\nw discarded overweight");
      return;
    }
    tmp=wr_push_cl_hyper_queue(g,(g->hyper_queue),res,weight);     
    if (!tmp) {
      wr_alloc_err(g,"could not store into hyper_queue in wr_process_resolve_result ");
      return; // could not alloc memory, could not store clause
    }
  } else {           
    // ordinary case (not partial hyperres): resulting clause is finished
    if (g->print_derived_cl) {
      printf("\n+ derived by mp: ");
      wr_print_clause(g,res);    
    }  
    weight=wr_calc_clause_weight(g,res,&size,&depth,&length);
    avg=(g->avg_kept_weight);       
    //printf(" weight %d average %f count %d \n",weight,(g->avg_kept_weight),(g->stat_kept_cl));
    if (!wr_derived_weight_check(g,avg,weight,size,depth,length)) {
      (g->stat_weight_discarded_cl)++;
      CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
      if (g->print_derived_cl) printf("\nw discarded overweight");
      return;
    }
    ++(g->stat_kept_cl);
    avg+=(weight-avg)/((g->stat_kept_cl)+1);
    (g->avg_kept_weight)=avg;
    resmeta=wr_calc_clause_meta(g,res,cl_metablock);
    hash=wr_add_cl_to_unithash(g,res,resmeta); 
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
    wr_push_cl_clpick_queues(g,(g->clpick_queues),res,weight);    
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
  int tmp;
  gptr rptr;
  int rpos, cutpos1=0;
  gptr res;
  gint resmeta,history;
  int ruleflag;
  gint given_termbuf_storednext=0;
  gint initial_queue_termbuf_next=0;
  int weight;
  double avg;
  gint cl_metablock[CLMETABLOCK_ELS];
  int size,depth,length;
  
#ifdef DEBUG
  printf("+++ wr_process_factor_result called\n");
  wr_print_clause(g,xcl); printf(" %d, %d : ",x,y);
  wr_print_term(g,xatom);
  printf("\n");
  wr_print_term(g,yatom);
  printf("\n");
  wr_print_vardata(g);
#endif  
  //db=g->db;
  ++(g->stat_derived_cl);
  ++(g->stat_factor_derived_cl);
  // reserve sufficient space in derived_termbuf for simple sequential store of atoms:
  // no top-level meta kept
  rlen=(len-1)*LIT_WIDTH;
  if (rlen==0) { 
    g->proof_found=1;    
    g->proof_history=wr_build_factorial_history(g,xcl_as_active,x,y,NULL);
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
    return; 
  } 

  // check subsumption and cutoff

  if (g->print_derived_precut_cl) {
    printf("\nc pre-cut derived by merge: ");
    wr_print_halfbuilt_clause(g,rptr,rpos);
  }
  tmp=wr_derived_cl_cut_and_subsume(g,rptr,rpos);
  if (tmp<0) {
    // clause was subsumed
#ifdef DEBUG    
    printf("\nwr_process_resolve_result was subsumed with new subsumer\n");
#endif    
    if (g->print_derived_subsumed_cl) {
      printf("\n- subsumed derived by merge ");
      wr_print_halfbuilt_clause(g,rptr,rpos);
    }
    return;    
  } else if (tmp>0) {
    // there were cuts
    if (g->print_derived_precut_cl) {
      printf("\nc post-cut derived by merge ");
      wr_print_halfbuilt_clause(g,rptr,rpos);
    }
    wr_process_resolve_result_remove_cuts(g,rptr,&rpos,tmp);
    (g->stat_derived_cut)++;
    if (rpos==0) {
      g->proof_found=1; 
      g->proof_history=wr_build_factorial_history(g,xcl_as_active,x,y,g->cut_clvec);    
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

  history=wr_build_factorial_history(g,xcl_as_active,x,y,g->cut_clvec);
  res=wr_derived_build_cl_from_initial_cl(g,rptr,rpos,ruleflag,history);
  if (!history || !res) {
    // allocation failed
    return;
  }
  
  
  // now the resulting clause is fully built
  // try to subsume the derived clause
  /*
  if (g->print_derived_cl) {
    printf("\n+ derived by merge1: ");
    wr_print_clause(g,res);
  }  
  */
  /*
  tmp=wr_derived_fullcl_subsumed(g, res);
  if (tmp) {
    // clause was subsumed, free it up
    //printf("\nwr_process_factor_result was subsumed\n");
    CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
    return;
  }
  */

  // check if result contains only ans predicates
  
  //printf("\n checking for pure answer clause\n");
  //wr_print_clause(g,res);
  tmp=wr_cl_derived_is_answer(g,res);
  if (tmp>0) {
    printf("\n\nfound pure answer: ");
    wr_print_clause(g,res);
    g->proof_found=1;
    g->proof_history=history;     
    return;
  } 

  if (0) { //((g->hyperres_strat) &&  !wr_hyperres_satellite_cl(g,res)) { 
    // hyperresolution case: not a finished clause yet, must cut off some 
    ++(g->stat_hyperres_partial_cl);
    if (g->print_partial_derived_cl) {
      printf("\n+ partial derived: ");
      wr_print_clause(g,res);
    }  
    //wr_push_clpickstack_cl(g,res);
    wr_clear_varstack(g,g->varstack);
    //wr_clear_all_varbanks(g);
    //wr_print_vardata(g);
    wr_resolve_binary_all_active(g,res,xcl_as_active); ///!!!! problem with xcl_as_active
    // restore buffer pos to situation before building the current clause
    wr_vec_free(g,g->build_buffer);    // !!!!!!!!!! problem         
    g->build_buffer=(gptr)given_termbuf_storednext;      
    //CVEC_NEXT(g->given_termbuf)=given_termbuf_storednext;
  } else {       
    // ordinary case (not partial hyperres): resulting clause is finished
    if (g->print_derived_cl) {
      printf("\n+ derived by merge: ");
      wr_print_clause(g,res);    
    }  
    weight=wr_calc_clause_weight(g,res,&size,&depth,&length);
    avg=(g->avg_kept_weight);  
    if (!wr_derived_weight_check(g,avg,weight,size,depth,length)) {
      (g->stat_weight_discarded_cl)++;
      CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
      if (g->print_derived_cl) printf("\nw discarded overweight");
      return;
    }
    ++(g->stat_kept_cl);
    avg+=(weight-avg)/((g->stat_kept_cl)+1);
    (g->avg_kept_weight)=avg;
    resmeta=wr_calc_clause_meta(g,res,cl_metablock);
    wr_add_cl_to_unithash(g,res,resmeta);
    wr_push_cl_clpick_queues(g,(g->clpick_queues),res,weight);      
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
  //void* db=g->db;
  int xisrule,yisrule,xatomnr,yatomnr;
  int rlen;
  int tmp;
  gptr rptr;
  int rpos,cutpos1=0,cutpos2=0;
  gptr res;  
  gint resmeta,history;
  int ruleflag;
  //int partialresflag;
  //int clstackflag;
  gint given_termbuf_storednext=0;
  gint initial_queue_termbuf_next=0;
  int weight;
  double avg;
  gint cl_metablock[CLMETABLOCK_ELS];
  int size,depth,length;
  
  /*
  printf("\n+++ wr_process_paramodulate_result called\n");
  printf(" xcl: "); wr_print_clause(g,xcl); printf(" xatom: ");wr_print_term(g,xatom);
  printf("\n");
  printf(" ycl: "); wr_print_clause(g,ycl);  printf(" yatom: ");wr_print_term(g,yatom);
  printf("\n");
  printf("\naterm:"); wr_print_term(g,aterm);  
  printf("\nbterm:"); wr_print_term(g,bterm);  
  printf("\npath: %d\n",path); 
  printf("\leftflag: %d\n",leftflag); 
  wr_print_vardata(g); 
  */
  
#ifdef DEBUG
  printf("\n+++ wr_process_paramodulate_result called\n");
  wr_print_clause(g,xcl); printf(" : ");wr_print_term(g,xatom);
  printf("\n");
  wr_print_clause(g,ycl);  //printf(" : ");wr_print_term(g,yatom);
  printf("\naterm:");
  wr_print_term(g,aterm);  printf("\nbterm:");
  wr_print_term(g,bterm);  printf("\npath: %d\n",path); 
  printf("\leftflag: %d\n",leftflag); 
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
    if (fromflag) {
      //printf("\n fromflag %d cutpos1 %d cutpos2 %d leftflag %d\n",fromflag,cutpos1,cutpos2,leftflag);
      g->proof_history=wr_build_para_history(g,xcl_as_active,ycl,0,0,NULL,path,leftflag,fromflag);
    } else {
      //printf("\n fromflag %d cutpos1 %d cutpos2 %d leftflag %d\n",fromflag,cutpos1,cutpos2,leftflag);
      g->proof_history=wr_build_para_history(g,xcl,xcl_as_active,0,0,NULL,path,leftflag,fromflag);  
    }   
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
    //g->proof_history=wr_build_para_history(g,xcl_as_active,ycl,cutpos1,cutpos2,NULL,path,leftflag,fromflag);
    return;
  } 
  /*
  printf("\nin wr_process_paramodulate_result we got halfbuilt:\n");
  wr_print_halfbuilt_clause(g,rptr,rpos);
  printf("\n");
  */
  // check subsumption and cutoff
  if (g->print_derived_precut_cl) {    
    printf("\nc pre-cut derived by =: ");
    if (fromflag) printf("from: ");
    else printf("into: ");
    wr_print_halfbuilt_clause(g,rptr,rpos);
  }
  tmp=wr_derived_cl_cut_and_subsume(g,rptr,rpos);
  if (tmp<0) {
    // clause was subsumed
#ifdef DEBUG      
    printf("\nwr_process_paramodulate_result was subsumed with new subsumer\n");
#endif    
    if (g->print_derived_subsumed_cl) {
      printf("\n- subsumed derived by = ");
      if (fromflag) printf("from: ");
      else printf("into: ");
      wr_print_halfbuilt_clause(g,rptr,rpos);
    }    
    return;    
  } else if (tmp>0) {
    // there were cuts
    if (g->print_derived_precut_cl) {
      printf("\nc post-cut derived by = ");
      if (fromflag) printf("from: ");
      else printf("into: ");
      wr_print_halfbuilt_clause(g,rptr,rpos);
    }
    wr_process_resolve_result_remove_cuts(g,rptr,&rpos,tmp);
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
      //g->proof_history=wr_build_para_history(g,xcl_as_active,ycl,cutpos1,cutpos2,g->cut_clvec,path,leftflag,fromflag); 
      return;
    } 
  }


  // now we have stored all subst-into and renamed metas/atoms into rptr: build clause  

  //printf("filled meta/atom new vec, rpos %d\n",rpos);          
  // check whether should be stored as a ruleclause or not
  ruleflag=wr_process_resolve_result_isrulecl(g,rptr,rpos);  
  // create new record
  if (0) { // ((g->hyperres_strat) &&  !wr_hyperres_satellite_tmpres(g,rptr,rpos)){
    // must still cut something off: not fully finished hyperres
    //partialresflag=1;   
    //wr_process_resolve_result_setupclpickstackcopy(g); 
    wr_process_resolve_result_setupgivencopy(g);    
    // store buffer pos to be restored later
    given_termbuf_storednext=(gint)(g->build_buffer);
    //g->build_buffer=malloc(1024);
    //given_termbuf_storednext=CVEC_NEXT(g->given_termbuf); 
    // allocate a new vector
    g->build_buffer=wr_cvec_new(g,BUILD_BUFFER_SIZE);  // !!!!!!!!!! problem
  } else {
    // fully formed, storable result
    //partialresflag=0;      
    wr_process_resolve_result_setupquecopy(g); // use g->queue_termbuf as g->build_buffer  
    initial_queue_termbuf_next=CVEC_NEXT(g->build_buffer); // to be restored if not actually used
  }  
  if (fromflag) {
    //printf("\n fromflag %d cutpos1 %d cutpos2 %d leftflag %d\n",fromflag,cutpos1,cutpos2,leftflag);
    /*
    wr_print_clause(g,xcl_as_active);
    printf("\n");
    wr_print_clause(g,ycl);
    printf("\n");
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
  printf("\n+!+!+!+!+ derived by = ");
  if (fromflag) printf("from: ");
  else printf("into: ");
  wr_print_clause(g,res);
  */
  //printf("\n derived as raw record: \n");
  //wg_print_record(g->db,res);
  //printf("\n");
  /*
  if (g->print_derived_cl) {
    printf("\n+ derived by = ");
    if (fromflag) printf("from: ");
    else printf("into: ");
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
    printf("\n\nfound pure answer: ");
    wr_print_clause(g,res);
    g->proof_found=1;     
    g->proof_history=history;   
    return;
  }

  // start storing to queues and hashes
  if (0) { // ((g->hyperres_strat) &&  !wr_hyperres_satellite_cl(g,res)) { 
    // hyperresolution case: not a finished clause yet, must cut off some 
    printf("wr_process_paramodulate_result hyperres-strat and not satellite\n");
    ++(g->stat_hyperres_partial_cl);
    if (g->print_partial_derived_cl) {
      printf("\n+ partial derived: ");
      wr_print_clause(g,res);
    }  
    //wr_push_clpickstack_cl(g,res);
    wr_clear_varstack(g,g->varstack);
    //wr_clear_all_varbanks(g);
    //wr_print_vardata(g);
    wr_resolve_binary_all_active(g,res,xcl_as_active); // !!! problem with xcl_as_active      
    // restore buffer pos to situation before building the current clause
    wr_vec_free(g,g->build_buffer);    // !!!!!!!!!! problem         
    g->build_buffer=(gptr)given_termbuf_storednext;      
    //CVEC_NEXT(g->given_termbuf)=given_termbuf_storednext;
  } else {       
    // ordinary case (not partial hyperres): resulting clause is finished
    if (g->print_derived_cl) {
      printf("\n+ derived by = ");
      if (fromflag) printf("from: ");
      else printf("into: ");
      wr_print_clause(g,res);
      //printf("\n derived as raw record: \n");
      //wg_print_record(g->db,res);
      //printf("\n");
    }    
    weight=wr_calc_clause_weight(g,res,&size,&depth,&length);  
    avg=(g->avg_kept_weight);  
    //printf(" weight %d average %f count %d \n",weight,(g->avg_kept_weight),(g->stat_kept_cl));
    if (!wr_derived_weight_check(g,avg,weight,size,depth,length)) {
      (g->stat_weight_discarded_cl)++;
      CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
      if (g->print_derived_cl) printf("\nw discarded overweight");
      return;
    }
    ++(g->stat_kept_cl);
    avg+=(weight-avg)/((g->stat_kept_cl)+1);
    (g->avg_kept_weight)=avg;
    resmeta=wr_calc_clause_meta(g,res,cl_metablock);
    wr_add_cl_to_unithash(g,res,resmeta);
    wr_push_cl_clpick_queues(g,(g->clpick_queues),res,weight);     
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
  printf("\nwr_process_resolve_result_aux called on atomnr %d with clause and term:\n",atomnr);
  wr_print_clause(g,cl);
  wr_print_term(g,cutatom);
  printf("\n and atomnr %d *rpos %d *cutpos %d replpath %d and replterm:\n",atomnr,*rpos,*cutpos,replpath);
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
      printf("\n!! unexpected error: atom meta in wr_process_resolve_result_aux is 0\n ");
      return 0;
      //wg_set_rule_clause_atom_meta(db,rec,litnr,meta)
    }  
#ifdef DEBUG
    printf("\nwr_process_resolve_result_aux loop i %d built meta %d and term\n",i,(int)meta);    
    wr_print_term(g,newatom);          
#endif
    if (newatom==WG_ILLEGAL) {
      ++(g->stat_internlimit_discarded_cl);
      wr_alloc_err(g,"could not build subst newatom in wr_process_resolve_result ");
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
          printf("\nlooking for equal term at pos j %d for newatom: ",j);
          wr_print_term(g,newatom);
          printf(" and atom at pos j %d: ",j);
          wr_print_term(g,rptr[(j*LIT_WIDTH)+LIT_ATOM_POS]);
          printf("\n");
#endif       
      (g->tmp1)++;
      if (wr_equal_term(g,newatom,rptr[(j*LIT_WIDTH)+LIT_ATOM_POS],1)) {        
        rmeta=rptr[(j*LIT_WIDTH)+LIT_META_POS];
        if (!litmeta_negpolarities(meta,rmeta)) {
          //same sign, drop lit
          posfoundflag=1;          
#ifdef DEBUG          
          printf("\nequals found:\n");
          wr_print_term(g,newatom);
          printf("\n");
          wr_print_term(g,rptr[(j*LIT_WIDTH)+LIT_ATOM_POS]);
          printf("\n");
#endif          
          break;                                
        } else {
#ifdef DEBUG           
          printf("\nin wr_process_resolve_result_aux return 0\n");
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
  
#ifdef DEBUG  
  //printf("\nwr_process_resolve_result_aux generated clause\n");
  //wr_print_clause(g,rptr);  
#endif  
  return 1; // 1 means clause is still ok. 0 return means: drop clause 
}  



void wr_process_resolve_result_remove_cuts(glb* g, gptr rptr, int* rpos, int cuts) {
  int i,storepos,tmp;
  gint xatom,xatommeta;

  #ifdef DEBUG
    printf("\nwr_process_resolve_result_remove_cuts called with %d cuts\n",cuts);
  #endif        
          
  storepos=0;        
  for(i=0; i < *rpos; i++) { 
    tmp=i*LIT_WIDTH;
    xatom=rptr[tmp+LIT_ATOM_POS];
    xatommeta=rptr[tmp+LIT_META_POS];
    
    //printf("\n i %d storepos %d  xatom %d xatommeta %d atom:\n",i,storepos,xatom,xatommeta);
    //wr_print_term(g,xatom);    

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
    
    //printf("\nwr_derived_build_cl_from_initial_cl rule case \n");

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
      //blt=wr_build_calc_term(g,rptr[tmp+LIT_ATOM_POS]);
      //else blt=rptr[tmp+LIT_ATOM_POS];
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
    printf("\nwr_derived_build_cl_from_initial_cl notrule case \n");
    wr_print_term_otter(g,rptr[LIT_ATOM_POS],1000);
    printf("\n");
    //wg_print_record(db,rptr[LIT_ATOM_POS]);
    printf("\n");
    */

    meta=RECORD_META_FACT_CLAUSE;
    blt=wr_build_calc_term(g,rptr[LIT_ATOM_POS]);

    /*
    printf("\nwr_derived_build_cl_from_initial_cl notrule case built blt\n");
    wr_print_term_otter(g,blt,1000);
    printf("\n");
    //wg_print_record(db,rptr[LIT_ATOM_POS]);
    printf("\n");
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
    printf("\nwr_derived_build_cl_from_initial_cl notrule case built res\n");
    wr_print_clause(g,res) ;
    printf("\n");
    //wg_print_record(db,rptr[LIT_ATOM_POS]);
    printf("\n");
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


int wr_derived_weight_check(glb* g, double avg, int weight,  int size, int depth, int length) {
  //int res=1;
  // first immediate weight limits
  //printf(" avg %f ",avg);
  //printf("\n weight %d size %d depth %d length %d \n",weight,size,depth,length);

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
  if ((g->passed_ratio)>0) {
    if ((g->passed_ratio)<0.5) { 
      //wr_printf(" less than 0.5 "); 
      return 1; 
    }
    if ((g->passed_ratio)>0.90) { 
      //wr_printf(" over 90 %f ",(g->passed_ratio)); 
      return 0; 
    }
    if ((g->passed_ratio)>0.7 && (weight*2>=avg)) { 
      //wr_printf(" over 0.7 %f ",(g->passed_ratio)); 
      return 0;
    }
    if ((g->passed_ratio)>0.5 && (weight>=avg)) { 
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
  
  //printf("\nwr_add_cl_to_unithash:\n");
  //wr_print_clause(g,cl);
  //printf("\nlen %d wg_rec_is_rule_clause %d\n",len,wg_rec_is_rule_clause(db,cl));

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
    printf("\ng->hash_neg_groundunits after adding:");      
    wr_print_termhash(g,rotp(g,g->hash_neg_groundunits));
    //wr_clterm_hashlist_print(g,rotp(g,g->hash_neg_groundunits));
    printf("\ng->hash_pos_groundunits after adding:");   
    wr_print_termhash(g,rotp(g,g->hash_pos_groundunits));   
    //wr_clterm_hashlist_print(g,rotp(g,g->hash_pos_groundunits));
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
      //meta=wg_get_rule_clause_atom_meta(g->db,cl,i);
      //printf("\natom i %d:\n",i);
      //wr_print_term(g,atom);
      //printf("\n");
      if (wr_answer_lit(g,atom)) ansnr++;      
    }
  }
  //printf("\nwr_cl_derived_is_answer exiting with ansnr %d len %d \n",ansnr,len);
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
    if (g->negpref_strat) return 1;
    else return 0;    
  } else {
    // rule clause: check if contains only non-preferred
    len=wg_count_clause_atoms(g->db,cl);
    for(i=0;i<len;i++) {
      meta=wg_get_rule_clause_atom_meta(g->db,cl,i);
      if (wg_atom_meta_is_neg(g->db,meta)) {
        if (g->negpref_strat) return 0;
      } else {
        if (g->pospref_strat) return 0;  
      }        
    }
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
      if (g->negpref_strat) return 0;
    } else {
      if (g->pospref_strat) return 0;  
    }        
  }
  return 1;    
} 



/* -----------

setups for building

-------------- */

void wr_process_resolve_result_setupsubst(glb* g) {
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


/* ---------

temporary stuff

------------ */


/* 

  Process newly derived cutoff result

*/

/*
void wr_process_cutoff_result(glb* g, gptr xcl, int len, int x, gint xatom, int y, gint yatom) {
  void* db;
  int rlen;
  int tmp;
  gptr rptr;
  int rpos;
  gptr res;
  gint meta,resmeta;
  int ruleflag;
  gint given_termbuf_storednext;
  gint initial_queue_termbuf_next;
  int weight;
  double avg;
  
#ifdef DEBUG
  printf("+++ wr_process_factor_result called\n");
  wr_print_clause(g,xcl); printf(" %d, %d : ",x,y);
  wr_print_term(g,xatom);
  printf("\n");
  wr_print_term(g,yatom);
  printf("\n");
  wr_print_vardata(g);
#endif  
  db=g->db;
  ++(g->stat_derived_cl);
  ++(g->stat_factor_derived_cl);
  // reserve sufficient space in derived_termbuf for simple sequential store of atoms:
  // no top-level meta kept
  rlen=(len-1)*LIT_WIDTH;
  if (rlen==0) { g->proof_found=1; return; }  
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
  tmp=wr_process_resolve_result_aux(g,xcl,xatom,len,rptr,&rpos);
  if (!tmp) {
    wr_process_resolve_result_cleanupsubst(g);       
    return; 
  }    
  wr_process_resolve_result_cleanupsubst(g);
  if (rpos==0) { g->proof_found=1; return; } 
  // now we have stored all subst-into and renamed metas/atoms into rptr: build clause  
  //printf("filled meta/atom new vec, rpos %d\n",rpos);          
  // check whether should be stored as a ruleclause or not
  ruleflag=wr_process_resolve_result_isrulecl(g,rptr,rpos);  
  // create new record  
  wr_process_resolve_result_setupquecopy(g); // use g->queue_termbuf as g->build_buffer
  initial_queue_termbuf_next=CVEC_NEXT(g->build_buffer); // to be restored if not actually used 

  res=wr_derived_build_cl_from_initial_cl(g,rptr,rpos,ruleflag);
  if (res==NULL) {
    // allocation failed
    return;
  }
  
  // now the resulting clause is fully built
  // try to subsume the derived clause

  //printf("\n+ derived: ");
  //wr_print_clause(g,res);

  tmp=wr_derived_fullcl_subsumed(g, res);
  if (tmp<0) {
    // clause was subsumed, free it up
    //printf("\nwr_process_factor_result was subsumed\n");
    CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
    return;
  }

  if ((g->hyperres_strat) &&  !wr_hyperres_satellite_cl(g,res)) { 
    // hyperresolution case: not a finished clause yet, must cut off some 
    ++(g->stat_hyperres_partial_cl);
    if (g->print_partial_derived_cl) {
      printf("\n+ partial derived: ");
      wr_print_clause(g,res);
    }  
    //wr_push_clpickstack_cl(g,res);
    wr_clear_varstack(g,g->varstack);
    //wr_clear_all_varbanks(g);
    //wr_print_vardata(g);
    wr_resolve_binary_all_active(g,res);       
    // restore buffer pos to situation before building the current clause
    wr_vec_free(g,g->build_buffer);    // !!!!!!!!!! problem         
    g->build_buffer=given_termbuf_storednext;      
    //CVEC_NEXT(g->given_termbuf)=given_termbuf_storednext;
  } else {       
    // ordinary case (not partial hyperres): resulting clause is finished
    if (g->print_derived_cl) {
      printf("\n+ factor derived: ");
      wr_print_clause(g,res);    
    }  
    weight=wr_calc_clause_weight(g,res,&size,&depth,&length);
    if ((g->cl_maxweight) && weight>(g->cl_maxweight)) {
      CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
      return;
    }  
    avg=(g->avg_kept_weight);
    avg+=(weight-avg)/((g->stat_kept_cl)+1);   
    if (!wr_derived_weight_check(g,weight,avg)) {
      (g->stat_weight_discarded_cl)++;
      CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
      if (g->print_derived_cl) printf("\nw discarded overweight");
      return;
    }
    ++(g->stat_kept_cl);
    (g->avg_kept_weight)=avg;
    resmeta=wr_calc_clause_meta(g,res);
    wr_add_cl_to_unithash(g,res,resmeta);
    wr_push_cl_clpick_queues(g,(g->clpick_queues),res,weight); 
  }    
}  
*/


#ifdef __cplusplus
}
#endif
