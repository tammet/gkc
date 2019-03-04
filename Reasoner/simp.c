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
 *  Simplification of clauses.
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
#include "../Db/dballoc.h"  // for ACONST_TRUE/FALSE
  
/* ====== Private headers and defs ======== */

//#define DEBUG
#undef DEBUG
#define QUIET
//#undef QUIET

#define USE_RES_TERMS // loop over active clauses in wr_resolve_binary_all_active

/* ======= Private protos ================ */


/* ====== Functions ============== */


gptr wr_simplify_cl(glb* g, gptr cl, gptr cl_metablock) {
  
  //void* db=g->db;
  gptr res;
  gint resmeta,history;
  gint initial_queue_termbuf_next;
  int i;
  int len, rlen;
  int tmp, cuts, calccuts;
  cvec foundbucket;
  gptr rptr;
  int rpos;
  int ruleflag;
  gptr xcl;
  gint xatom,yatom;
  gint xmeta;  
  
#ifdef DEBUG
  printf("\nwr_simplify_cl called with ");
  wr_print_clause(g,cl); 
  printf("\n"); 
#endif

  // do not process non-rule clauses for now
  if (! wg_rec_is_rule_clause(db,cl)) {
    len=1;
    // here we could only find a contradiction or calc tautology
    // first try to compute
    (g->build_buffer)=g->queue_termbuf;

    //meta=RECORD_META_FACT_CLAUSE;
    //blt=wr_build_calc_term(g,rptr[LIT_ATOM_POS]);

    if ((g->use_comp_funs) && 
        wr_computable_termptr(g,cl) ) {
      initial_queue_termbuf_next=CVEC_NEXT(g->build_buffer);    // ??
      yatom=wr_build_calc_term(g,rpto(g,cl));
      //CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored ??
      if (yatom==WG_ILLEGAL) {
        ++(g->stat_internlimit_discarded_cl);
        wr_alloc_err(g,"could not alloc first buffer in wr_simplify_cl ");
        return NULL;
      } else if (yatom==ACONST_TRUE) {
        // do not use, tautologically true          
        return NULL; 
      } else if (yatom==ACONST_FALSE) {
        // should cut
        //printf("\n proof founy \n");
        g->proof_found=1;    
        history=wr_build_simplify_history(g,cl,NULL);
        g->proof_history=history; 
        return NULL;     
      }        
    }
    return cl; 
  } else {
    len=wg_count_clause_atoms(db,cl);
  }  
  // reserve sufficient space in termbuf for simple sequential store of atoms:
  // no top-level meta kept
  rlen=len*LIT_WIDTH;  
  (g->derived_termbuf)[1]=2; // init termbuf
  // rptr will hold the new, simplified (shorter) clause
  rptr=wr_alloc_from_cvec(g,g->derived_termbuf,rlen); 
  if (rptr==NULL) {
    ++(g->stat_internlimit_discarded_cl);
    wr_alloc_err(g,"could not alloc first buffer in wr_simplify_cl ");
    return NULL; // could not alloc memory, could not store clause
  }  
  rpos=0;
  // init vector for storing cutters
  (g->cut_clvec)[1]=(gint)NULL; 
  cuts=0; // by units
  calccuts=0; // by calc
  // set up subs parameters
  wr_process_resolve_result_setupquecopy(g);
  initial_queue_termbuf_next=CVEC_NEXT(g->build_buffer); // to be restored if not actually used
  if (g->use_comp_funs_strat) g->use_comp_funs=1;
  // loop over literals, storing the results of cuts as we go
  xcl=cl;  
  for(i=0; i<len; i++) {    
    xmeta=wg_get_rule_clause_atom_meta(db,xcl,i);
    xatom=wg_get_rule_clause_atom(db,xcl,i);        
    foundbucket=NULL;
    // first try to compute
    if ((g->use_comp_funs) && 
        wr_computable_termptr(g,rotp(g,xatom)) ) {
      yatom=wr_build_calc_term(g,xatom);
      CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
      if (yatom==WG_ILLEGAL) {
        ++(g->stat_internlimit_discarded_cl);
        wr_alloc_err(g,"could not alloc first buffer in wr_simplify_cl ");
        return NULL;
      } else if (yatom==ACONST_TRUE) {
        // do not use, tautologically true          
        return NULL; 
      } else if (yatom==ACONST_FALSE) {
        // should cut
        calccuts++;         
        //printf("\n calccuts %d, continuing\n",calccuts);
        continue;         
      }        
    }
    // second, try to cut and subsume with units
    // nb! should not subsume unit cl-s, otherwise cancel out
    /*
    printf("\nhash_neg_groundunits\n");
    wr_print_termhash(g,rotp(g,g->hash_neg_groundunits));
    printf("\nhash_pos_groundunits\n");
    wr_print_termhash(g,rotp(g,g->hash_pos_groundunits));
    */
    if (len<2) tmp=wr_atom_cut_and_subsume(g,xatom,xmeta,&foundbucket,0); // block subsume
    else tmp=wr_atom_cut_and_subsume(g,xatom,xmeta,&foundbucket,1); // allow subsume
#ifdef DEBUG
    printf("\nxatom searched for cut:\n");
    wr_print_term(g,xatom);
    printf("\ntmp returned by wr_atom_cut_and_subsume: %d\n",tmp); 
#endif

    if (tmp<0) {
      // subsumed
      return NULL;
    } else if (tmp==1) {
      // cut
      //printf("\n cut found (g->cut_clvec)[0] is %d, cuts is %d \n",(g->cut_clvec)[0],cuts);
      cuts++;
      if ((int)((g->cut_clvec)[0])>cuts+2) {
        (g->cut_clvec)[cuts]=(gint)foundbucket;
        (g->cut_clvec)[cuts+1]=(gint)NULL;
      }  
      (g->stat_lit_hash_cut_ok)++;
    } else {
      // atom preserved intact store lit
      rptr[(rpos*LIT_WIDTH)+LIT_META_POS]=xmeta;
      rptr[(rpos*LIT_WIDTH)+LIT_ATOM_POS]=xatom;
      /*
      printf("\nxatom:\n");
      wr_print_term(g,xatom);
      wr_print_term(g,rptr[(rpos*LIT_WIDTH)+LIT_ATOM_POS]);
      */
      ++rpos;   
    }
  } 
  // if no cuts, return the unchanged original clause
  if (!cuts && !calccuts) {
    //printf("\nno cuts found\n");
    return cl;
  }
  // from now on we found some cuts and will build and store a new clause
  (g->stat_simplified)++;
  (g->stat_derived_cut)++;
  (g->build_buffer)=g->queue_termbuf;
  if (rpos==0) {
    //printf("\n proof founx \n");
    g->proof_found=1;    
    history=wr_build_simplify_history(g,cl,g->cut_clvec);
    g->proof_history=history; 
    return NULL;
  }
  // check whether should be stored as a ruleclause or not
  ruleflag=wr_process_resolve_result_isrulecl(g,rptr,rpos);
  // set up var rename params ?????
  //wr_process_resolve_result_setupsubst(g)
  wr_process_simp_setupquecopy(g);  
  // build history and a new clause
  history=wr_build_simplify_history(g,cl,g->cut_clvec);
  // set up building params
  initial_queue_termbuf_next=CVEC_NEXT(g->build_buffer); // to be restored if not actually used
  res=wr_derived_build_cl_from_initial_cl(g,rptr,rpos,ruleflag,history);
  if (!history || !res) {
    // allocation failed
    ++(g->stat_internlimit_discarded_cl);
    wr_alloc_err(g,"could not alloc while building a simplified clause");
    return NULL;
  }  
  tmp=wr_cl_derived_is_answer(g,res);
  if (tmp>0) {
    printf("\nfound pure answer: ");
    wr_print_clause(g,res);
    g->proof_found=1;   
    g->proof_history=history;    
    return NULL;
  }  
  resmeta=wr_calc_clause_meta(g,res,cl_metablock);
  wr_add_cl_to_unithash(g,res,resmeta);
  return res;
} 


#ifdef __cplusplus
}
#endif
