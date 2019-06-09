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
//#undef DEBUG
#define QUIET
//#undef QUIET

#define USE_RES_TERMS // loop over active clauses in wr_resolve_binary_all_active

/* ======= Private protos ================ */


/* ====== Functions ============== */


gptr wr_simplify_cl(glb* g, gptr cl, gptr cl_metablock) {
  
  //void* db=g->db;
  gptr res;
  gint history; //resmeta,
  gint initial_queue_termbuf_next;
  int i;
  int len, rlen;
  int tmp, cuts, calccuts, rewritten_atoms=0;
  cvec foundbucket;
  gptr rptr;
  int rpos;
  int ruleflag;
  gptr xcl;
  gint xatom,yatom;
  gint xmeta;  
  gint prev_rewrites=0;
  
#ifdef DEBUG
  printf("\nwr_simplify_cl called with ");
  wr_print_clause(g,cl); 
  printf("\n"); 
#endif

  (g->tmp_rewrites)=0;
  // do not process non-rule clauses for now
  if (! wg_rec_is_rule_clause(db,cl)) {

    // not a ruleclause 

    len=1;
    // here we could only find a contradiction or calc tautology
    // first try to compute
    (g->cut_clvec)[1]=(gint)NULL;    
    (g->build_buffer)=g->queue_termbuf;

    //meta=RECORD_META_FACT_CLAUSE;
    //blt=wr_build_calc_term(g,rptr[LIT_ATOM_POS]);
    if ((g->use_rewrite_terms_strat) && (g->have_rewrite_terms)) (g->build_rewrite)=1;
    if ((g->build_rewrite) ||
        ((g->use_comp_funs) && wr_computable_termptr(g,cl)) ) {
      // can rewrite    
      (g->rewrite_clvec)[1]=2; // first free elem 
      initial_queue_termbuf_next=CVEC_NEXT(g->build_buffer);
      prev_rewrites=(g->tmp_rewrites); // to check later if rewrites were done
      yatom=wr_build_calc_term(g,rpto(g,cl));      
      (g->build_rewrite)=0;
      if (g->print_litterm_selection) {                
        wr_printf("\nrewritten to: ");
        wr_print_term(g,yatom);       
      }   
      //CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored ??
      if (yatom==WG_ILLEGAL) {
        ++(g->stat_internlimit_discarded_cl);
        wr_alloc_err(g,"could not alloc first buffer in wr_simplify_cl ");
        CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
        return NULL;
      } else if (yatom==ACONST_TRUE) {
        // do not use, tautologically true          
        CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
        return NULL; 
      } else if (yatom==ACONST_FALSE) {
        // should cut
        //printf("\n proof founy \n");
        g->proof_found=1;    
        history=wr_build_simplify_history(g,cl,NULL,g->rewrite_clvec);
        g->proof_history=history; 
        return NULL;     
      } 
      if (prev_rewrites==(g->tmp_rewrites)) {
        // no rewrites done
        CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
        return cl;
      } else {
        // rewrites done
        res=rotp(g,yatom);        
        history=wr_build_simplify_history(g,cl,g->cut_clvec,g->rewrite_clvec);
        wr_set_history(g,res,history); 
        //resmeta=wr_calc_clause_meta(g,res,cl_metablock);
        //wr_add_cl_to_unithash(g,res,resmeta);
        return res;       
      } 
    } else {
      // no rewrites possible
      return cl; 
    }    
  } else {
    len=wg_count_clause_atoms(db,cl);
  } 

  // ruleclause

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
  // init vector for storing cutters and rewriters
  (g->cut_clvec)[1]=(gint)NULL;  
  (g->rewrite_clvec)[1]=2; // first free elem
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

    //printf("\nxatom before rewrite/compute\n");
    //wr_print_term(g,xatom);
    //printf("\n");
    
    // first try to rewrite and compute
    if ((g->use_rewrite_terms_strat) && (g->have_rewrite_terms)) (g->build_rewrite)=1;
    if ((g->build_rewrite) ||
        ((g->use_comp_funs) && wr_computable_termptr(g,rotp(g,xatom))) ) {
      prev_rewrites=(g->tmp_rewrites); // to check later if rewrites were done
      yatom=wr_build_calc_term(g,xatom);

      //printf("\nyatom after rewrite/compute %ld\n",yatom);
      //if (yatom) wr_print_term(g,yatom);
      //printf("\n");
     
      (g->build_rewrite)=0;
      //if (prev_rewrites==(g->tmp_rewrites)) {
      //  CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
      //}       
      if (yatom==WG_ILLEGAL) {
        ++(g->stat_internlimit_discarded_cl);
        wr_alloc_err(g,"could not alloc first buffer in wr_simplify_cl ");        
        return NULL;
      } else if (yatom==ACONST_TRUE) {       
        if (wg_atom_meta_is_neg(db,xmeta)) {
          calccuts++;  
          continue;
        } else {
          CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
          return NULL;        
        }  
      } else if (yatom==ACONST_FALSE) {
        if (!wg_atom_meta_is_neg(db,xmeta)) {
          calccuts++;  
          continue;
        } else {
          CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
          return NULL;            
        }  
      }        
    } else {
      yatom=xatom;
    }
    // second, try to cut and subsume with units
    // nb! should not subsume unit cl-s, otherwise cancel out
    /*
    printf("\nhash_neg_groundunits\n");
    wr_print_termhash(g,rotp(g,g->hash_neg_groundunits));
    printf("\nhash_pos_groundunits\n");
    wr_print_termhash(g,rotp(g,g->hash_pos_groundunits));
    */
    if (len<2 && (prev_rewrites==(g->tmp_rewrites))) 
      tmp=wr_atom_cut_and_subsume(g,yatom,xmeta,&foundbucket,0); // block subsume
    else 
      tmp=wr_atom_cut_and_subsume(g,yatom,xmeta,&foundbucket,1); // allow subsume
#ifdef DEBUG
    printf("\nyatom searched for cut:\n");
    wr_print_term(g,yatom);
    printf("\ntmp returned by wr_atom_cut_and_subsume: %d\n",tmp); 
#endif

    if (tmp<0) {
      // subsumed
      CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
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
    } else if (prev_rewrites!=(g->tmp_rewrites)) {
      // rewrites done
      //printf("\n rewrites done (g->cut_clvec)[0] is %d, cuts is %d \n",(g->cut_clvec)[0],cuts);
      rewritten_atoms++;
      rptr[(rpos*LIT_WIDTH)+LIT_META_POS]=xmeta;
      rptr[(rpos*LIT_WIDTH)+LIT_ATOM_POS]=yatom;      
      //printf("\nyatom stored:\n");      
      //wr_print_term(g,rptr[(rpos*LIT_WIDTH)+LIT_ATOM_POS]);      
      ++rpos;         
    } else {
      // atom preserved intact store lit
      rptr[(rpos*LIT_WIDTH)+LIT_META_POS]=xmeta;
      rptr[(rpos*LIT_WIDTH)+LIT_ATOM_POS]=yatom;
      /*
      printf("\nxatom:\n");
      wr_print_term(g,xatom);
      wr_print_term(g,rptr[(rpos*LIT_WIDTH)+LIT_ATOM_POS]);
      */
      ++rpos;   
    }
  } 
  
  //printf("\n(g->tmp_rewrites) %d\n",(g->tmp_rewrites));  
  //if (!rewritten_atoms) {
  //    CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
  //}   

  // if no cuts and rewrites, return the unchanged original clause
  if (!cuts && !calccuts && !rewritten_atoms) {
    //printf("\nno cuts or rewrites found\n");
    CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
    return cl;
  }

  // from now on we found some cuts or rewrites and will build and store a new clause

  (g->stat_simplified)++;
  if (cuts || calccuts) (g->stat_derived_cut)++;
  if (g->tmp_rewrites) (g->stat_derived_rewritten)++;
  (g->build_buffer)=g->queue_termbuf;
  if (rpos==0) {
    //printf("\n proof founx \n");
    g->proof_found=1;    
    history=wr_build_simplify_history(g,cl,g->cut_clvec,g->rewrite_clvec);
    g->proof_history=history; 
    return NULL;
  }
  // check whether should be stored as a ruleclause or not
  ruleflag=wr_process_resolve_result_isrulecl(g,rptr,rpos);
  // set up var rename params ?????
  //wr_process_resolve_result_setupsubst(g)
  wr_process_simp_setupquecopy(g);  
  // build history and a new clause
  history=wr_build_simplify_history(g,cl,g->cut_clvec,g->rewrite_clvec);
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
    printf("\n\nfound pure answer: ");
    wr_print_clause(g,res);
    g->proof_found=1;   
    g->proof_history=history;    
    return NULL;
  }  
  if (g->print_litterm_selection) {                
    wr_printf("\nsimplified to: ");
    wr_print_clause(g,res);       
  }  
  //resmeta=wr_calc_clause_meta(g,res,cl_metablock);
  //wr_add_cl_to_unithash(g,res,resmeta);
  return res;
} 


#ifdef __cplusplus
}
#endif
