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
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "rincludes.h"  
 

//#define DEBUG  
//#undef DEBUG

  
/* ====== Private headers and defs ======== */


/* ======= Private protos ================ */



/* ====== Functions ============== */


void wr_propagate_unit(glb* g, gptr cl, gint clmeta, gint hash, gptr cl_as_active) {
  int len, negflag;
  gint atom,meta;  
  gptr bucketnode;
  int clvecpos,blockvecpos,i;
  cvec clvec,blockvec;

#ifdef DEBUG
  wr_printf("\nwr_propagate_unit called with clmeta %d, hash %d, cl: ",(int)clmeta,(int)hash);
  wr_print_clause(g,cl);
  wr_printf("\n");
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
  wr_printf("\n should propagate, negflag is %d \n ",negflag);
#endif
  bucketnode=wr_find_atomhash_bucketnode(g,rotp(g,(g->hash_atom_occurrences)),atom,hash);  
  if (bucketnode==NULL) {
#ifdef DEBUG    
    wr_printf("\n no bucketnode found \n");
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
    wr_printf("\n no clauses in the suitable clvec\n");
#endif    
    //return;
  } else {
#ifdef DEBUG    
    wr_printf("\n !!!!!! some clauses in the suitable clvec:\n");    
#endif    
    // loop over clauses to be cut
    for(i=2;i<clvec[0]+2 && i<clvec[1];i++) {
#ifdef DEBUG      
      wr_printf("\n prop i %d with ",i);      
      wr_print_clause(g,cl); 
      wr_printf(" and ");     
      wr_print_clause(g,(gptr)(clvec[i]));
      wr_printf("\n");
#endif      
      wr_resolve_propagated_clause(g,atom,cl,(gptr)(clvec[i]),negflag,cl);
      if (g->proof_found || g->alloc_err) {
        //wr_clear_varstack(g,g->varstack); 
        wr_printf("\nproof found in propagate\n");
        return;          
      }  
    }
  }  
  if (1) { //} (blockvec==NULL) {
    // no clauses in the suitable clvec
#ifdef DEBUG    
    wr_printf("\n no clauses in blockvec\n");
#endif    
    //return;
  } else {
#ifdef DEBUG    
    wr_printf("\n !!!!!! some clauses in blockvec:\n");    
#endif    
    // loop over clauses to be blocked
    for(i=2;i<blockvec[0]+2 && i<blockvec[1];i++) {
#ifdef DEBUG      
      wr_printf("\n prop i %d with ",i);      
      wr_print_clause(g,cl); 
      wr_printf(" and ");     
      wr_print_clause(g,(gptr)(blockvec[i]));
      wr_printf("\n");
#endif      
      wr_mark_clause_blocked(g,(gptr)(blockvec[i]));    
      (g->stat_propagated_subsumed)++;
    }
  }
#ifdef DEBUG  
  wr_printf("\n propagating ended for: ");
  wr_print_clause(g,cl);
  wr_printf("\n");
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
  int foundflag=0;

  
  UNUSED(db);
  UNUSED(foundflag);
#ifdef DEBUG
  wr_printf("\n!!! wr_resolve_propagated_clause called for clauses \n");
  wr_print_clause(g,cl);
  wr_printf("\n"); 
  wr_print_clause(g,cutcl);
  wr_printf("\n");   
  wr_print_vardata(g);
#endif  
  // just to be sure we are not given fact clauses
  if (! wg_rec_is_rule_clause(db,cutcl)) return;
  len = wg_count_clause_atoms(db,cutcl);                       
  // loop over literals to find the atom to be cut
  for(i=0; i<len; i++) {   
    xmeta=wg_get_rule_clause_atom_meta(db,cutcl,i);   
    xatom=wg_get_rule_clause_atom(db,cutcl,i);       
    /* 
    printf("\natom and xatom:\n");
    wr_print_term(g,atom);
    printf("\n");
    wr_print_term(g,xatom);
    printf("\n");
    */
    if (wr_equal_term(g,atom,xatom,1) &&
        ((negflag && !wg_atom_meta_is_neg(db,xmeta)) ||
         (!negflag && wg_atom_meta_is_neg(db,xmeta)) )) {
      // found the right atom
      foundflag=1;
      wr_process_propagated_result(g,atom,cl,xatom,cutcl,cl_as_active,i);
      if (g->proof_found || g->alloc_err) {
        //wr_clear_varstack(g,g->varstack);          
        return;          
      }
      break;
    }  
  }
#ifdef DEBUG 
  if (!foundflag) {
    wr_printf("\n something wrong: atom not found in wr_resolve_propagated_clause\n");
  }
#endif  
}     


void wr_process_propagated_result
      (glb* g, gint xatom, gptr xcl, gint yatom, gptr ycl, gptr xcl_as_active, int cutpos2) {
  int yatomnr, datalen;
  int tmp;
  gptr res;  
  gint resmeta,history, hash;
  gint initial_queue_termbuf_next;
  int weight;
  double avg;
  gint cl_metablock[CLMETABLOCK_ELS];
  int size,depth,length;

  int i,j;
  gint atom, meta; // gint: atom offsets and metas are 64-bit values

#ifdef DEBUG
  wr_printf("\n+++ wr_process_propagated_result called\n");
  wr_print_clause(g,xcl); wr_printf(" : ");wr_print_term(g,xatom);
  wr_printf("\n");
  wr_print_clause(g,ycl);  wr_printf(" : ");wr_print_term(g,yatom);
  wr_printf("\n");
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
    wr_register_answer(g,res,history);
    //wr_printf("\n\nfound pure answer: ");
    //wr_print_clause(g,res);
    g->proof_found=1;   
    g->proof_history=history;    
    return;
  } 
  // resulting clause is finished

  //wr_printf("\n+ derived by fmp: ");
  //wr_print_clause(g,res);    

  if (g->print_derived_cl) {
    wr_printf("\n+ derived by fmp: ");
    wr_print_clause(g,res);    
  }  
  weight=wr_calc_clause_weight(g,res,&size,&depth,&length);
  avg=(g->avg_kept_weight);       
  //printf(" weight %d average %f count %d \n",weight,(g->avg_kept_weight),(g->stat_kept_cl));
  if (!wr_derived_weight_check(g,avg,weight,size,depth,length,0,0)) {
    (g->stat_weight_discarded_cl)++;
    CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
    if (g->print_derived_cl) wr_printf("\nw discarded overweight");
    return;
  }
  //wr_mark_clause_blocked(g,ycl); // mark long parent blocked
  ++(g->stat_kept_cl);
  avg+=(weight-avg)/((g->stat_kept_cl)+1);
  (g->avg_kept_weight)=avg;
  resmeta=wr_calc_clause_meta(g,res,cl_metablock);
  hash=wr_add_cl_to_unithash(g,res,resmeta);  
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
#ifdef DEBUG
  //wr_print_atomhash(g,rotp(g,(g->hash_atom_occurrences)));
#endif
  wr_push_cl_clpick_queues(g,(g->clpick_queues),res,weight);
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
  wr_printf("\nwr_store_atom_occurrences called with cl: ");
  wr_print_clause(g,cl);
  wr_printf("\n");
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
  wr_printf("\nwr_store_atom_occurrences returns with count %d ",count);
#endif  

  //printf("\nhash_atom_occurrences\n");
  //wr_print_atomhash(g,rotp(g,(g->hash_atom_occurrences)));

  return count; 
}  





/* ====================================================

  eager unit propagation (strategy keys unit_propagate, unit_backsubsume)

  A table files the literals of the clauses of 2 or more literals that
  enter the queues (unit_propagate 1: two-literal clauses only) and of the
  active copies of the picked clauses, under the hash of the unit tables
  (wr_lit_hash_unit). When gkc keeps a unit, the unit goes into a pending
  list. Before each pick, wr_up_run takes the pending units one by one and
  walks the filed literals with the same hash:

  * a literal of the opposite sign that is equal to the unit (a variant
    with unit_variant_hash) is cut: gkc derives the clause without it
    (wr_up_derive_shorter, a unit resolution with the history of a
    resolution step) and makes the longer clause unusable;
  * with unit_backsubsume, a literal of the same sign that is equal to the
    unit makes its clause unusable (the unit subsumes it).

  An unusable clause is in a local set: the pick skips it, and the loops
  of resolution and paramodulation over the active clauses skip it. No
  record is written, so a clause of an attached KB is never changed; KB
  clauses are not filed. A new unit made by the propagation is filed in
  the unit tables as any kept unit and goes into the pending list too.

================================================ */

typedef struct { gint cl; int32_t idx; int32_t len; int32_t next; } wr_up_ent;
typedef struct { gint key; int32_t first; } wr_up_slot;
typedef struct {
  wr_up_slot* tab; gint cap, cnt;      // hash of a literal -> first entry
  wr_up_ent* ents; int32_t ecnt, ecap;
  gint* uset; gint ucap, ucnt;         // the unusable clauses (record offsets)
  gint* pend; int32_t pcnt, pcap;      // the pending units (record offsets)
} wr_up_t;

static uint64_t wr_up_mixkey(gint k) {
  uint64_t x=(uint64_t)k; x^=x>>33; x*=UINT64_C(0xff51afd7ed558ccd); x^=x>>29; return x;
}

static wr_up_t* wr_up_get(glb* g) {
  wr_up_t* x=(wr_up_t*)(g->up_index);
  if (x) return x;
  x=sys_malloc(sizeof(wr_up_t));
  if (!x) return NULL;
  memset(x,0,sizeof(wr_up_t));
  x->cap=1<<12; x->ecap=1<<12; x->ucap=1<<10; x->pcap=1<<8;
  x->tab=sys_malloc(sizeof(wr_up_slot)*x->cap);
  x->ents=sys_malloc(sizeof(wr_up_ent)*x->ecap);
  x->uset=sys_malloc(sizeof(gint)*x->ucap);
  x->pend=sys_malloc(sizeof(gint)*x->pcap);
  if (!x->tab || !x->ents || !x->uset || !x->pend) {
    if (x->tab) sys_free(x->tab);
    if (x->ents) sys_free(x->ents);
    if (x->uset) sys_free(x->uset);
    if (x->pend) sys_free(x->pend);
    sys_free(x);
    return NULL;
  }
  memset(x->tab,0,sizeof(wr_up_slot)*x->cap);
  memset(x->uset,0,sizeof(gint)*x->ucap);
  (g->up_index)=x;
  return x;
}

void wr_up_free(glb* g) {
  wr_up_t* x=(wr_up_t*)(g->up_index);
  if (!x) return;
  sys_free(x->tab); sys_free(x->ents); sys_free(x->uset); sys_free(x->pend);
  sys_free(x);
  (g->up_index)=NULL;
}

/* the slot of key in the literal table; the key 0 is stored as 1 */

static wr_up_slot* wr_up_slot_of(wr_up_t* x, gint key, int ins) {
  gint i;
  if (!key) key=1;
  if (ins && 2*(x->cnt+1)>x->cap) {
    wr_up_slot* ot=x->tab; gint oc=x->cap, j;
    wr_up_slot* nt=sys_malloc(sizeof(wr_up_slot)*oc*2);
    if (!nt) return NULL;
    memset(nt,0,sizeof(wr_up_slot)*oc*2);
    x->tab=nt; x->cap=oc*2;
    for(j=0;j<oc;j++) if (ot[j].key) {
      i=(gint)(wr_up_mixkey(ot[j].key)&(uint64_t)(x->cap-1));
      while(nt[i].key) i=(i+1)&(x->cap-1);
      nt[i]=ot[j];
    }
    sys_free(ot);
  }
  i=(gint)(wr_up_mixkey(key)&(uint64_t)(x->cap-1));
  while(x->tab[i].key) {
    if (x->tab[i].key==key) return &(x->tab[i]);
    i=(i+1)&(x->cap-1);
  }
  if (!ins) return NULL;
  x->tab[i].key=key; x->tab[i].first=-1; (x->cnt)++;
  return &(x->tab[i]);
}

static gint wr_up_litkey(glb* g, gint atom) {
  return wr_lit_hash_unit(g,atom,wr_lit_hash(g,atom));
}

/* is the clause cl unusable */

int wr_up_unusable(glb* g, gptr cl) {
  wr_up_t* x=(wr_up_t*)(g->up_index);
  gint p, i;
  if (!x || !(x->ucnt)) return 0;
  p=pto(g->db,cl);
  i=(gint)(wr_up_mixkey(p)&(uint64_t)(x->ucap-1));
  while(x->uset[i]) {
    if (x->uset[i]==p) return 1;
    i=(i+1)&(x->ucap-1);
  }
  return 0;
}

/* make the clause cl unusable; returns 0 on a failed allocation */

int wr_up_mark(glb* g, gptr cl) {
  wr_up_t* x=wr_up_get(g);
  gint p, i;
  if (!x) return 0;
  p=pto(g->db,cl);
  if (2*(x->ucnt+1)>x->ucap) {
    gint* ou=x->uset; gint oc=x->ucap, j;
    gint* nu=sys_malloc(sizeof(gint)*oc*2);
    if (!nu) return 0;
    memset(nu,0,sizeof(gint)*oc*2);
    x->uset=nu; x->ucap=oc*2;
    for(j=0;j<oc;j++) if (ou[j]) {
      i=(gint)(wr_up_mixkey(ou[j])&(uint64_t)(x->ucap-1));
      while(nu[i]) i=(i+1)&(x->ucap-1);
      nu[i]=ou[j];
    }
    sys_free(ou);
  }
  i=(gint)(wr_up_mixkey(p)&(uint64_t)(x->ucap-1));
  while(x->uset[i]) {
    if (x->uset[i]==p) return 1;
    i=(i+1)&(x->ucap-1);
  }
  x->uset[i]=p; (x->ucnt)++;
  return 1;
}

/* file the literals of cl (a clause that enters the queues, or the active
   copy of a picked clause) */

void wr_up_register(glb* g, gptr cl) {
  void* db=g->db;
  wr_up_t* x;
  wr_up_slot* s;
  int i, len;
  gint atom;
  if (!(g->unit_propagate) && !(g->unit_backsubsume)) return;
  if (!wg_rec_is_rule_clause(db,cl) || wg_in_attached_kb(cl)) return;
  len=wg_count_clause_atoms(db,cl);
  if (len<2) return;
  if ((g->unit_propagate)==1 && !(g->unit_backsubsume) && len!=2) return;
  x=wr_up_get(g);
  if (!x) { wr_alloc_err(g,"could not allocate the table of unit propagation"); return; }
  for(i=0;i<len;i++) {
    atom=wg_get_rule_clause_atom(db,cl,i);
    if (!isdatarec(atom) || wr_answer_lit(g,atom)) continue;
    s=wr_up_slot_of(x,wr_up_litkey(g,atom),1);
    if (!s) { wr_alloc_err(g,"could not grow the table of unit propagation"); return; }
    if (x->ecnt>=x->ecap) {
      wr_up_ent* ne=sys_realloc(x->ents,sizeof(wr_up_ent)*(x->ecap)*2);
      if (!ne) { wr_alloc_err(g,"could not grow the table of unit propagation"); return; }
      x->ents=ne; x->ecap*=2;
    }
    x->ents[x->ecnt].cl=pto(db,cl);
    x->ents[x->ecnt].idx=i;
    x->ents[x->ecnt].len=len;
    x->ents[x->ecnt].next=s->first;
    s->first=x->ecnt;
    (x->ecnt)++;
  }
}

/* a unit was kept: put it into the pending list */

void wr_up_unit(glb* g, gptr cl) {
  wr_up_t* x;
  if (!(g->unit_propagate) && !(g->unit_backsubsume)) return;
  x=wr_up_get(g);
  if (!x) { wr_alloc_err(g,"could not allocate the table of unit propagation"); return; }
  if (x->pcnt>=x->pcap) {
    gint* np=sys_realloc(x->pend,sizeof(gint)*(x->pcap)*2);
    if (!np) { wr_alloc_err(g,"could not grow the pending units of unit propagation"); return; }
    x->pend=np; x->pcap*=2;
  }
  x->pend[(x->pcnt)++]=pto(g->db,cl);
}

/* propagate the pending units; called before a pick, with no bindings
   of a caller on the variable stack */

void wr_up_run(glb* g) {
  void* db=g->db;
  wr_up_t* x=(wr_up_t*)(g->up_index);
  wr_up_slot* s;
  gptr ucl, ycl;
  gint uatom, umeta, yatom, ymeta, ypos;
  int uneg, yneg, same, k;
  int32_t e;
  if (!x) return;
  while(x->pcnt>0 && !(g->proof_found) && !(g->alloc_err)) {
    ucl=otp(db,x->pend[--(x->pcnt)]);
    if (wr_up_unusable(g,ucl)) continue;
    if (wg_rec_is_rule_clause(db,ucl)) {
      uatom=wg_get_rule_clause_atom(db,ucl,0);
      umeta=wg_get_rule_clause_atom_meta(db,ucl,0);
      uneg=wg_atom_meta_is_neg(db,umeta) ? 1 : 0;
    } else {
      uatom=encode_record(db,ucl);
      uneg=0;
    }
    if (!isdatarec(uatom) || wr_answer_lit(g,uatom)) continue;
    s=wr_up_slot_of(x,wr_up_litkey(g,uatom),0);
    if (!s) continue;
    for(e=s->first; e>=0 && !(g->proof_found) && !(g->alloc_err); e=x->ents[e].next) {
      ypos=x->ents[e].cl;
      ycl=otp(db,ypos);
      if (wr_up_unusable(g,ycl)) continue;
      k=x->ents[e].idx;
      yatom=wg_get_rule_clause_atom(db,ycl,k);
      ymeta=wg_get_rule_clause_atom_meta(db,ycl,k);
      yneg=wg_atom_meta_is_neg(db,ymeta) ? 1 : 0;
      if ((g->unit_variant_hash) ? !wr_variant_term(g,uatom,yatom) : !wr_equal_term(g,uatom,yatom,1)) continue;
      same=(uneg==yneg);
      if (same) {
        if (!(g->unit_backsubsume)) continue;
        if (!wr_up_mark(g,ycl)) { wr_alloc_err(g,"could not grow the unusable clauses of unit propagation"); return; }
        (g->stat_up_backsubsumed)++;
      } else {
        if (!(g->unit_propagate)) continue;
        if ((g->unit_propagate)==1 && x->ents[e].len!=2) continue;
        if (!wr_up_mark(g,ycl)) { wr_alloc_err(g,"could not grow the unusable clauses of unit propagation"); return; }
        (g->stat_up_cut)++;
        wr_up_derive_shorter(g,ucl,ycl,k);
        // wr_up_derive_shorter can grow x->ents through wr_up_register
        x=(wr_up_t*)(g->up_index);
      }
    }
  }
}

#ifdef __cplusplus
}
#endif
