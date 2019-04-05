/*
*
* Copyright (c) Tanel Tammet 2019
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

 /** @file analyze.c
 * Analyze the clause set before search.
 */

/* ====== Includes =============== */


#ifdef __cplusplus
extern "C" {
#endif

#include "rincludes.h"   
 

/* ====== Private defs =========== */


//#define DEBUG
#undef DEBUG  


/* ====== Private headers ======== */

  
/* ====== Functions ============== */


/* =====================================================

 

====================================================== */


int wr_analyze_clause_list(glb* g, gint clauselist) {
  void* db=g->db;
  gint cell;
  gcell *cellptr;
  gptr rec;
  int n=0, tmp;

  // loop over clauses
  //cell=(dbmemsegh(db)->clauselist);

  //(g->varstack)=wr_cvec_new(g,NROF_VARBANKS*NROF_VARSINBANK); 
  //(g->varstack)[1]=2; // first free elem

  wr_clear_all_varbanks(g);
  cell=clauselist;
  while(cell) {         
    cellptr=(gcell *) offsettoptr(db, cell);
    rec=offsettoptr(db,cellptr->car);
    printf("\n\n*** rec nr %d\n",n);
    wg_print_record(db,rec);
    tmp=wr_analyze_clause(g,rec);
    if (!tmp) {
      // something wrong with the clause
      wr_clear_all_varbanks(g);
      return 0;
    }
    n++;
    cell=cellptr->cdr;
  }  

  wr_show_in_stats(g);

  return 1;  
}


int wr_analyze_clause(glb* g, gptr cl) {
  void* db=g->db;
  gint history;
  gptr historyptr;
  int i, hlen;
  //int htype,tag,dechead;
  //gint head, cl1;
  gint meta,atom=0;
  gint vc_tmp;
  int tmp;
  int ruleflag=0, len=0, anslit=0, neglit=0, poslit=0;
  int size=0,maxdepth=0,varcount=0;


  printf("\n");
  wr_print_clause(g,cl); 
  
  history=wr_get_history(g,cl);
  if (history!=(gint)NULL) {
    historyptr=otp(db,history);
    hlen=wg_get_record_len(db,historyptr);
    /* 
    if (len==HISTORY_PREFIX_LEN) return 0; // input
    head=wr_get_history_record_field(db,historyptr,HISTORY_DERIVATION_TAG_POS);
    htype=wg_get_encoded_type(db,head);
    if (htype!=WG_INTTYPE) return 0;
    dechead=wg_decode_int(db,head);
    tag=wr_get_history_tag(g,dechead);
    */
  }
  g->tmp_unify_vc=((gptr)(g->varstack))+1;
  vc_tmp=*(g->tmp_unify_vc);

  if (wg_rec_is_fact_clause(db,cl)) {
    printf("\nfact");
    ruleflag=0;
    len=1;
    neglit=0;
    poslit=1;
    if (wr_answer_lit(g,atom)) {
      printf("\n!!! Warning: single ans clause\n");
      anslit++;
    } 
    tmp=wr_analyze_term(g,pto(db,cl),0,&size,&maxdepth);
  } else if (wg_rec_is_rule_clause(db,cl)) {
    printf("\nrule");
    ruleflag=1;
    len=wg_count_clause_atoms(db, cl);
    for(i=0; i<len; i++) {
      meta=wg_get_rule_clause_atom_meta(db,cl,i);
      atom=wg_get_rule_clause_atom(db,cl,i);
      if (wg_atom_meta_is_neg(db,meta)) neglit++;
      else poslit++;
      if (wr_answer_lit(g,atom)) {
        printf("\n!!! answer lit");
        anslit++;
      }
      tmp=wr_analyze_term(g,atom,0,&size,&maxdepth);
      if (!tmp) {
        // something wrong with the atom
        printf("\nError: found an incorrectly encoded clause\n");
        wg_print_record(db,cl);
        return 0; 
      }
    }
  }

  varcount=*(g->tmp_unify_vc)-vc_tmp;

  if (vc_tmp!=*(g->tmp_unify_vc)) {
    //printf("\n clearing varstack topslice\n");
    wr_clear_varstack_topslice(g,g->varstack,vc_tmp);
  }  

  printf("\nruleflag %d len %d neglit %d poslit %d size %d maxdepth %d varcount %d",
     ruleflag,len,neglit,poslit,size,maxdepth,varcount);
  
  if (len==2 && poslit==1 && neglit==1 && !anslit && 
      varcount==1 && size==4 && maxdepth==1) {

    // this is likely a chain clause
    check_process_chain_clause(g,cl);
  }

  (g->in_clause_count)++;
  if (ruleflag) (g->in_rule_clause_count)++;
  else (g->in_fact_clause_count)++;  
  if (anslit) (g->in_answer_clause_count)++;
  if (!varcount) (g->in_ground_clause_count)++;
  if (len==1) (g->in_unit_clause_count)++;
  if (poslit<=1) (g->in_horn_clause_count)++;
  if (!poslit) (g->in_neg_clause_count)++;
  if (!neglit) (g->in_pos_clause_count)++;
  //(g->in_poseq_clause_count)++;
  //(g->in_negeq_clause_count)++;
  if (len<(g->in_min_length)) (g->in_min_length)=len;
  if (len>(g->in_max_length)) (g->in_max_length)=len;
  if (maxdepth<(g->in_min_depth)) (g->in_min_depth)=maxdepth;
  if (maxdepth>(g->in_max_depth)) (g->in_max_depth)=maxdepth;
  if (size<(g->in_min_size)) (g->in_min_size)=size;
  if (size>(g->in_max_size)) (g->in_max_size)=size;
  if (varcount<(g->in_min_vars)) (g->in_min_vars)=varcount;
  if (varcount>(g->in_max_vars)) (g->in_max_vars)=varcount;
  /*
  (g->in_average_length)=0;
  (g->in_average_depth)=0;
  (g->in_predicate_count)=0;
  (g->in_funsymb_count)=0;
  (g->in_axiom_count)=0;
  (g->in_assumption_count)=0;
  (g->in_goal_count)=0;
  */
  
  return 1;
}



int wr_analyze_term(glb* g, gint x, int depth, int* size, int* maxdepth) {
  void* db;
  gptr xptr;
  int i, start, end;  
  int w;
    
  //printf("wr_analyze_term called with x %d type %d depth %d size %d maxdepth %d\n",
  //       x,wg_get_encoded_type(g->db,x),depth,*size,*maxdepth);
  //wr_print_term(g,x);
  //printf("\n");

  if (!isdatarec(x)) {
    // now we have a simple value  
    (*size)++;
    if (!isvar(x)) return 10;
    // here x is a var    
    if (VARVAL_DIRECT(x,(g->varbanks))==UNASSIGNED) {
      // a new var 
      //printf("\n a new var %ld where *(g->tmp_unify_vc) %ld\n",x,*(g->tmp_unify_vc));
      SETVAR(x,encode_smallint(1),g->varbanks,g->varstack,g->tmp_unify_vc);
      //printf("\n after setvar *(g->tmp_unify_vc) %ld\n",*(g->tmp_unify_vc));
      //printf("\n new var\n");
      return 1;
    } else {
      // a var seen before
      //printf("\n old var\n");
      //printf("\n an old var %ld\n",x);
      return 2;
    }       
  }   
  // now we have a datarec
  db=g->db;
  xptr=decode_record(db,x);
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,xptr);
  w=0;    
  depth++;
  if (depth>(*maxdepth)) *maxdepth=depth;
  for(i=start;i<end;i++) {
    if (!xptr[i]) return 0; // should not have 0 in args
    w=w+wr_analyze_term(g,xptr[i],depth,size,maxdepth);      
  }   
  return 1;
}  


void wr_show_in_stats(glb* g) {

  if (!(g->print_stats)) return;
    
  printf("\ninput clause set statistics:\n");
  printf("----------------------------------\n");

  printf("in_clause_count:        %13d\n",g->in_clause_count);  
  printf("in_rule_clause_count:   %13d\n",g->in_rule_clause_count);
  printf("in_fact_clause_count:   %13d\n",g->in_fact_clause_count);
  printf("in_answer_clause_count: %13d\n",g->in_answer_clause_count);
  printf("in_ground_clause_count: %13d\n",g->in_ground_clause_count);
  printf("in_unit_clause_count:   %13d\n",g->in_unit_clause_count);
  printf("in_horn_clause_count:   %13d\n",g->in_horn_clause_count);
  printf("in_pos_clause_count:    %13d\n",g->in_pos_clause_count);
  printf("in_neg_clause_count:    %13d\n",g->in_neg_clause_count);
  printf("in_poseq_clause_count:  %13d\n",g->in_poseq_clause_count);
  printf("in_negeq_clause_count:  %13d\n",g->in_negeq_clause_count);
  printf("in_min_length: %13d\n",g->in_min_length);
  printf("in_max_length: %13d\n",g->in_max_length);
  printf("in_min_depth:  %13d\n",g->in_min_depth);
  printf("in_max_depth:  %13d\n",g->in_max_depth);
  printf("in_min_size:   %13d\n",g->in_min_size);
  printf("in_max_size:   %13d\n",g->in_max_size);
  printf("in_min_vars:   %13d\n",g->in_min_vars);
  printf("in_max_vars:   %13d\n",g->in_max_vars);
  printf("in_average_length:   %f\n",g->in_average_length);
  printf("in_average_depth:    %f\n",g->in_average_depth);
  printf("in_predicate_count:  %13d\n",g->in_predicate_count);
  printf("in_funsymb_count:    %13d\n",g->in_funsymb_count);
  printf("in_axiom_count:      %13d\n",g->in_axiom_count);
  printf("in_assumption_count: %13d\n",g->in_assumption_count);
  printf("in_goal_count:       %13d\n",g->in_goal_count);
  //printf(": %13d\n",g->);
}

#ifdef __cplusplus
}
#endif



