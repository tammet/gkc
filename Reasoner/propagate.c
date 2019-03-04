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

 /** @file propagate.c
 *  Propagating units and setting up required datastructures.
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
 

//#define DEBUG  
#undef DEBUG

  
/* ====== Private headers and defs ======== */


/* ======= Private protos ================ */



/* ====== Functions ============== */


void wr_propagate_unit(glb* g, gptr cl, gint clmeta, gint hash, gptr cl_as_active) {
  //void* db=g->db;
  int len, negflag;
  gint atom,meta;  
  gptr bucketnode;
  int clvecpos,blockvecpos,i;
  cvec clvec,blockvec,s;

#ifdef DEBUG
  printf("\nwr_propagate_unit called with clmeta %d, hash %d, cl: ",(int)clmeta,(int)hash);
  wr_print_clause(g,cl);
  printf("\n");
#endif
  if (wg_rec_is_fact_clause(db,cl)) len=1;
  else len=wg_count_clause_atoms(db,cl);
  if (len!=1) return;
  if (wg_rec_is_rule_clause(db,cl)) {
    atom=wg_get_rule_clause_atom(db,cl,0);  
    meta=wg_get_rule_clause_atom_meta(db,cl,0);  
    if (wg_atom_meta_is_neg(db,meta)) negflag=1;
    else negflag=0;      
  } else {
    atom=rpto(g,cl);
    negflag=0;
  }    
#ifdef DEBUG
  printf("\n should propagate, negflag is %d \n ",negflag);
#endif
  bucketnode=wr_find_atomhash_bucketnode(g,rotp(g,(g->hash_atom_occurrences)),atom,hash);  
  if (bucketnode==NULL) {
#ifdef DEBUG    
    printf("\n no bucketnode found \n");
#endif    
    return;
  }
  // here we have a bucketnode, look for clause vectors with the opposite polarity
  if (negflag) {
    clvecpos=ATOMHASH_POSCLAUSES_POS;
    blockvecpos=ATOMHASH_NEGCLAUSES_POS;
  } else {
    clvecpos=ATOMHASH_NEGCLAUSES_POS;
    blockvecpos=ATOMHASH_POSCLAUSES_POS;
  }  
  clvec=(gptr)(bucketnode[clvecpos]);
  blockvec=(gptr)(bucketnode[blockvecpos]);
  if (clvec==NULL) {
    // no clauses in the suitable clvec
#ifdef DEBUG    
    printf("\n no clauses in the suitable clvec\n");
#endif    
    //return;
  } else {
#ifdef DEBUG    
    printf("\n !!!!!! some clauses in the suitable clvec:\n");    
#endif    
    // loop over clauses to be cut
    for(i=2;i<clvec[0]+2 && i<clvec[1];i++) {
#ifdef DEBUG      
      printf("\n prop i %d with ",i);      
      wr_print_clause(g,cl); 
      printf(" and ");     
      wr_print_clause(g,(gptr)(clvec[i]));
      printf("\n");
#endif      
      wr_resolve_propagated_clause(g,atom,cl,(gptr)(clvec[i]),negflag,cl);
      if (g->proof_found || g->alloc_err) {
        //wr_clear_varstack(g,g->varstack); 
        printf("\nproof found in propagate\n");
        return;          
      }  
    }
  }  
  if (blockvec==NULL) {
    // no clauses in the suitable clvec
#ifdef DEBUG    
    printf("\n no clauses in blockvec\n");
#endif    
    //return;
  } else {
#ifdef DEBUG    
    printf("\n !!!!!! some clauses in blockvec:\n");    
#endif    
    // loop over clauses to be blocked
    for(i=2;i<blockvec[0]+2 && i<blockvec[1];i++) {
#ifdef DEBUG      
      printf("\n prop i %d with ",i);      
      wr_print_clause(g,cl); 
      printf(" and ");     
      wr_print_clause(g,(gptr)(blockvec[i]));
      printf("\n");
#endif      
      wr_mark_clause_blocked(g,(gptr)(blockvec[i]));    
      (g->stat_propagated_subsumed)++;
    }
  }
#ifdef DEBUG  
  printf("\n propagating ended for: ");
  wr_print_clause(g,cl);
  printf("\n");
#endif  
}

/*


*/

void wr_resolve_propagated_clause(glb* g, gint atom, gptr cl, gptr cutcl, int negflag, gptr cl_as_active) {
  void* db=g->db;
  int i;
  int len;      
  gint xmeta; 
  gint xatom;
  //gptr xcl;
  //int ures;  
  //gint a,b;
  //gptr tptr;
  //int atomlen;

#ifdef DEBUG
  printf("\n!!! wr_resolve_propagated_clause called for clauses \n");
  wr_print_clause(g,cl);
  printf("\n"); 
  wr_print_clause(g,cutcl);
  printf("\n");   
  wr_print_vardata(g);
#endif  
  // just to be sure we are not given fact clauses
  if (! wg_rec_is_rule_clause(db,cutcl)) return;
  len = wg_count_clause_atoms(db,cutcl);                       
  // loop over literals to find the atom to be cut
  for(i=0; i<len; i++) {   
    xmeta=wg_get_rule_clause_atom_meta(db,cutcl,i);   
    xatom=wg_get_rule_clause_atom(db,cutcl,i);        
    if (wr_equal_term(g,atom,xatom,1) &&
        ((negflag && !wg_atom_meta_is_neg(db,xmeta)) ||
         (!negflag && wg_atom_meta_is_neg(db,xmeta)) )) {
      // found the right atom
      wr_process_propagated_result(g,atom,cl,xatom,cutcl,cl_as_active,i);
           if (g->proof_found || g->alloc_err) {
        //wr_clear_varstack(g,g->varstack);          
        return;          
      }
      break;
    }  
  }
#ifdef DEBUG  
  printf("\n something wrong: atom not found in wr_resolve_propagated_clause\n");
#endif  
}     


void wr_process_propagated_result
      (glb* g, gint xatom, gptr xcl, gint yatom, gptr ycl, gptr xcl_as_active, int cutpos2) {
  //void* db=g->db;
  //int xisrule,yisrule,xatomnr,yatomnr
  int yatomnr, datalen;
  //int rlen;
  int tmp;
  //gptr rptr;
  //int rpos,cutpos1=0,cutpos2=0;
  gptr res;  
  gint resmeta,history, hash;
  //int ruleflag;
  //int partialresflag;
  //int clstackflag;
  //gint given_termbuf_storednext;
  gint initial_queue_termbuf_next;
  int weight;
  double avg;
  gint cl_metablock[CLMETABLOCK_ELS];
  int size,depth,length;

  int i,j, atom, meta;

#ifdef DEBUG
  printf("\n+++ wr_process_propagated_result called\n");
  wr_print_clause(g,xcl); printf(" : ");wr_print_term(g,xatom);
  printf("\n");
  wr_print_clause(g,ycl);  printf(" : ");wr_print_term(g,yatom);
  printf("\n");
  wr_print_vardata(g);  
#endif  
  ++(g->stat_derived_cl);
  ++(g->stat_binres_derived_cl);
  ++(g->stat_propagated_derived_cl);
  // get basic info about clauses
  // we assume xcl is unit and ycl is a rule clause  
  yatomnr=wg_count_clause_atoms(db,ycl);
  wr_process_resolve_result_setupquecopy(g); // use g->queue_termbuf as g->build_buffer
  initial_queue_termbuf_next=CVEC_NEXT(g->build_buffer); // to be restored if not actually used
  // build history and reserve new record space
  //history=wr_build_resolve_history(g,xcl_as_active,ycl,0,cutpos2,NULL);  
  history=wr_build_propagate_history(g,xcl_as_active,ycl,0,cutpos2);  
  resmeta=RECORD_META_RULE_CLAUSE;
  datalen=(yatomnr-1)*LIT_WIDTH;
  res=wr_create_raw_record(g,CLAUSE_EXTRAHEADERLEN+datalen,resmeta,g->build_buffer);
  if (history==(gint)NULL || res==NULL) {
    ++(g->stat_internlimit_discarded_cl);
    wr_alloc_err(g,"could not alloc history or raw record in wr_process_propagated_result ");
    return;
  }  
  // zero header
  for(i=RECORD_HEADER_GINTS;i<(RECORD_HEADER_GINTS+CLAUSE_EXTRAHEADERLEN);i++) {
    res[i]=0;     
  }       
  // copy atoms and metas
  j=0; // j is index in the result, i is index in the original
  for(i=0;i<yatomnr;i++) {   
    meta=wg_get_rule_clause_atom_meta(db,ycl,i);
    atom=wg_get_rule_clause_atom(db,ycl,i);     
    if (i==cutpos2) {
      // drop cut atoms, do not increase j
      continue; 
    }  
    // else copy meta and atom and increase j
    tmp=j*LIT_WIDTH; 
    res[tmp+RECORD_HEADER_GINTS+CLAUSE_EXTRAHEADERLEN+LIT_META_POS]=meta;
    res[tmp+RECORD_HEADER_GINTS+CLAUSE_EXTRAHEADERLEN+LIT_ATOM_POS]=atom;
    j++;
  }
  // store history built before
  wr_set_history(g,res,history);      
  // now the resulting clause is fully built
  ++(g->stat_built_cl);
  // check if result contains only ans predicates  
  tmp=wr_cl_derived_is_answer(g,res);
  if (tmp>0) {
    printf("\nfound pure answer: ");
    wr_print_clause(g,res);
    g->proof_found=1;   
    g->proof_history=history;    
    return;
  }
  // resulting clause is finished
  if (g->print_derived_cl) {
    printf("\n+ derived by fmp: ");
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
  wr_mark_clause_blocked(g,ycl); // mark long parent blocked
  ++(g->stat_kept_cl);
  avg+=(weight-avg)/((g->stat_kept_cl)+1);
  (g->avg_kept_weight)=avg;
  resmeta=wr_calc_clause_meta(g,res,cl_metablock);
  hash=wr_add_cl_to_unithash(g,res,resmeta);  
  if (hash>=0)  {
    // unit, propagate
    // wr_propagate_unit(g,res,resmeta,hash,res); //// !!!!!
  } else {  
    // nonunit, store occurrences
    // wr_store_atom_occurrences(g,res); // !!!!!!
  }  
#ifdef DEBUG
  //wr_print_atomhash(g,rotp(g,(g->hash_atom_occurrences)));
#endif
  wr_push_priorqueue(g,rotp(g,g->clpickpriorqueue),res,weight);
  //printf("pushed, to print g->clpickpriorqueue\n");
  //wr_print_priorqueue(g,rotp(g,g->clpickpriorqueue));  
  // push built clause into suitable list
  wr_push_clqueue_cl(g,res); 
}  
   






/* ---------------------------------------------

  Storing occurrences

 ---------------------------------------------- */

int wr_store_atom_occurrences(glb* g, gptr cl) {
  //void* db=g->db;
  int ruleflag, len, i, count, negflag;
  gint atom, meta, hash;

#ifdef DEBUG
  printf("\nwr_store_atom_occurrences called with cl: ");
  wr_print_clause(g,cl);
  printf("\n");
#endif

  ruleflag=wg_rec_is_rule_clause(db,cl);
  if (!ruleflag) return 0;
  len=wg_count_clause_atoms(db,cl);
  if (len<2) return 0;   
  count=0;
#ifdef DEBUG  
  //printf("\n len %d count %d\n",len,count);
#endif  
  for(i=0;i<len;i++) {   
    meta=wg_get_rule_clause_atom_meta(db,cl,i);  
    atom=wg_get_rule_clause_atom(db,cl,i);
    if (wr_answer_lit(g,atom)) continue;
    hash=wr_lit_hash(g,atom);
    //printf("\n hash %d\n",hash);
    if (wg_atom_meta_is_neg(db,meta)) negflag=1;
    else negflag=0;
    wr_push_atomhash(g,rotp(g,(g->hash_atom_occurrences)),hash,atom,cl,i,negflag);
    //printf("\n pushed \n");    
    count++;
  } 
#ifdef DEBUG
  printf("\nwr_store_atom_occurrences returns with count %d ",count);
#endif  
  return count; 
}  




#ifdef __cplusplus
}
#endif
