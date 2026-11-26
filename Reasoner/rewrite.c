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

/** @file rewrite.c
 *  Term rewriting (demodulation) header.
 *
 */

/* ====== Includes =============== */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "rincludes.h"
 

/* ====== Private headers and defs ======== */

//#define DEBUG
//#undef DEBUG
//#define TDEBUG


/* ======= Private protos ================ */



/* ====== Functions ============== */


/* ====================================================

  index of the demodulators

  A perfect discrimination tree (the module termindex.c) over the greater
  sides of the demodulators in hash_rewrite_terms (the local table; an
  attached KB keeps its own table and is walked as before). Each entry is
  the node of the hash list (term, clause, path) and an insert number.

  wr_dmi_best walks the tree with the term to rewrite. The walk binds the
  variables of the tree and clears them on backtracking, as in E, and
  gives the newest demodulator that matches and passes the tests of the
  list loop (not blocked, not rewrite_self, and with rewrite_guard a
  substitution that is not a renaming): the demodulator that the list
  gives. Its bindings are then set on the variable stack, and
  wr_rewrite_term builds the right side from them without a second match.
  When bindings are already on the variable stack, the list answers (the
  list tests its first candidate with these bindings).

  The tree is built only for the local table, outside a KB build, with
  unify_samelen 1 and unify_maxuseterms 0; otherwise, after a failed
  allocation, and in a build with -DGKC_NO_DEMOD_INDEX or with 4-byte
  pointers, wr_rewrite_term walks the list.

================================================ */

#ifdef USE_DEMOD_INDEX

/* the tree is the module termindex.c; a payload is the node of the hash
   list */

void wr_dmi_free(glb* g) {
  ti_index* x=(ti_index*)(g->dmi_index);
  if (!x) return;
  if (x==(void*)1) { (g->dmi_index)=NULL; return; }
  ti_free(x);
  (g->dmi_index)=NULL;
}

static void wr_dmi_fail(glb* g) {
  wr_dmi_free(g);
  (g->dmi_index)=(void*)1; // the list answers for the rest of the run
}

/* file the greater side term of a demodulator; node is its new node of
   the hash list */

void wr_dmi_insert(glb* g, gint term, gint node) {
  ti_index* x=(ti_index*)(g->dmi_index);
  if (x==(void*)1 || (g->inkb) || !node) return;
  if (!(g->unify_samelen) || (g->unify_maxuseterms)) { (g->stat_dmi_tree_stopped)=3; wr_dmi_fail(g); return; }
  if (!x) {
    x=ti_new();
    if (!x) { (g->stat_dmi_tree_stopped)=1; wr_dmi_fail(g); return; }
    (g->dmi_index)=x;
  }
  if (!ti_insert(g,x,term,node) || ti_failed(x)) {
    (g->stat_dmi_tree_stopped)=1;
    wr_dmi_fail(g);
  }
}

/* a demodulator that the rewriting may use: not blocked, not the
   excluded clause */

static int wr_dmi_accept(glb* g, gint node) {
  gptr ycl=otp(g->db,(otp(g->db,node))[CLTERM_HASHNODE_CL_POS]);
  if ((g->back_subsume) && wr_blocked_clause(g,ycl)) return 0;
  if (ycl==(g->rewrite_self)) return 0;
  return 1;
}

/* the newest demodulator of the local list that rewrites the term x: the
   walk of the perfect discrimination tree finds it with its substitution,
   which is then set on the variable stack (the variables of the
   demodulator), so the caller builds the right side from these bindings.
   Returns 1 and sets *node, 0 when there is none, -1 when the list has to
   answer (no tree, or bindings already on the variable stack) */

int wr_dmi_best(glb* g, gint x, gint* node) {
  ti_index* d=(ti_index*)(g->dmi_index);
  gint payload, *vars, *vals;
  gptr vc;
  int32_t n, nv, k;
  if (d==(void*)1) return -1;
  if (!d) { *node=0; return 0; } // no demodulator filed yet
  if ((g->varstack)[1]>2) return -1; // the list tests the first candidate with these bindings
  n=ti_tokens(g,d,x);
  if (n<0) { (g->stat_dmi_tree_stopped)=2; wr_dmi_fail(g); return -1; }
  if (!ti_match_best(g,d,0,n,wr_dmi_accept,g->rewrite_guard,&payload,&nv,&vars,&vals)) {
    *node=0;
    return 0;
  }
  vc=((gptr)(g->varstack))+1;
  for(k=0;k<nv;k++) SETVAR(vars[k],vals[k],g->varbanks,g->varstack,vc);
  *node=payload;
  return 1;
}

#else

void wr_dmi_free(glb* g) { UNUSED(g); }

#endif

#ifdef GKC_DEMOD_CHECK
/* the check build: both lookups from the same variable stack, the answer of the list */
static long dmi_check_calls=0, dmi_check_diff=0, dmi_check_dirty=0, dmi_check_hits=0, dmi_check_ltests=0, dmi_check_itests=0, dmi_check_tests;
static void dmi_check_report(void) {
  fprintf(stderr,"DMI_CHECK lookups %ld differences %ld dirty_varstack %ld rewritten %ld list_tests %ld index_tests %ld\n",dmi_check_calls,dmi_check_diff,dmi_check_dirty,dmi_check_hits,dmi_check_ltests,dmi_check_itests);
}
__attribute__((constructor)) static void dmi_check_init(void) { atexit(dmi_check_report); }
/* the list: the first node that passes and matches, with the bindings on the stack */
static gint dmi_check_first(glb* g, gint x, vec hashvec, gint hash) {
  void* db=g->db; gint node, yterm; gptr nodeptr; int ures;
  node=wr_clterm_hashlist_start(g,hashvec,hash);
  while(node!=0) {
    nodeptr=otp(db,node); yterm=nodeptr[CLTERM_HASHNODE_TERM_POS];
    if (wr_dmi_accept(g,node)) {
      ures=wr_match_term(g,yterm,x,1);
      if (ures && (g->rewrite_guard) && wr_match_is_renaming(g)) ures=0;
      dmi_check_tests++;
      if (ures) return node;
      wr_clear_varstack(g,g->varstack);
    }
    node=wr_clterm_hashlist_next(g,hashvec,node);
  }
  return 0;
}
static void dmi_check(glb* g, gint x, vec hashvec, gint hash) {
  static gint* sv=NULL; static gint svcap=0;
  gint svn=(g->varstack)[1], q, a, b, an, bn;
  if (svn>2) { dmi_check_dirty++; return; } // the tree is not used then
  if (!(g->dmi_index) || (g->dmi_index)==(void*)1) return;
  dmi_check_tests=0;
  a=dmi_check_first(g,x,hashvec,hash);
  dmi_check_ltests+=dmi_check_tests;
  an=(g->varstack)[1];
  if (an*2>svcap) { svcap=an*4; sv=sys_realloc(sv,sizeof(gint)*svcap); }
  for(q=2;q<an;q++) { sv[2*q]=(g->varstack)[q]; sv[2*q+1]=*((gptr)((g->varstack)[q])); }
  wr_clear_varstack(g,g->varstack);
  if (wr_dmi_best(g,x,&b)<0) { wr_clear_varstack(g,g->varstack); return; }
  dmi_check_itests++;
  if (b) dmi_check_hits++;
  bn=(g->varstack)[1];
  dmi_check_calls++;
  if (a!=b || an!=bn) dmi_check_diff++;
  else {
    for(q=2;q<an;q++) {
      if (sv[2*q]!=(g->varstack)[q] || sv[2*q+1]!=*((gptr)((g->varstack)[q]))) { dmi_check_diff++; break; }
    }
  }
  wr_clear_varstack(g,g->varstack);
}
#endif

/* is the substitution of the last match a renaming: every bound variable
   bound to a variable, no two to the same one */

int wr_match_is_renaming(glb* g) {
  gptr vs=g->varstack;
  gint i, j, n=vs[1], v;
  for(i=2;i<n;i++) {
    v=*((gptr)(vs[i]));
    if (!isvar(v)) return 0;
    for(j=2;j<i;j++) if (*((gptr)(vs[j]))==v) return 0;
  }
  return 1;
}

#ifdef USE_DEMOD_INDEX
#define WR_DMI_FOUND (dmi_n>0)
#else
#define WR_DMI_FOUND 0
#endif

gint wr_rewrite_term(glb* g, gint x) {
  void* db=g->db;
  gint hash,node,res,yterm,path,yatom,b,fun;
  int dbused, ures, hlen;
  gptr hashvec,nodeptr,ycl,tptr;
  int tmp_subst,tmp_rename;
  //gint tmp;
  int tmp;
#ifdef USE_DEMOD_INDEX
  int dmi_n=-1;
#endif

  //UNUSED(tmp);
#ifdef DEBUG
  wr_printf("\nwr_rewrite_term called on ");
  wr_print_term(g,x);
  wr_printf("\n");
#endif  

  // loop over local termhash (dbused==0) and external termhash (dbused==1) 
  
  if (isdatarec(x)) {
    tptr=rotp(g,x);
    fun=tptr[RECORD_HEADER_GINTS+(g->unify_funpos)];
  } else {
    fun=x;
  }
  hash=wr_term_basehash(g,fun);

  for(dbused=0; dbused<2; dbused++) {            
    if (dbused==0) {
      // local termhash             
      hashvec=rotp(g,g->hash_rewrite_terms);          
      hlen=wr_clterm_hashlist_len(g,hashvec,hash);
      if (hlen==0) {
  #ifdef DEBUG        
        wr_printf("no matching atoms in local hash\n");
  #endif        
        continue;
      }   
    } else {   
      // external kb termhash           
      if (!(r_kb_g(g))) continue;             
      hashvec=rotp(g,r_kb_g(g)->hash_rewrite_terms);       
      hlen=wr_clterm_hashlist_len(g,hashvec,hash);
      if (hlen==0) {
  #ifdef DEBUG        
        wr_printf("no matching atoms in external hash\n");
  #endif        
        continue;
      }               
    }   

    node=wr_clterm_hashlist_start(g,hashvec,hash);
    if (!node)  {      
      wr_printf("\nnode was null \n");
      continue; 
    }  
#ifdef USE_DEMOD_INDEX
    // the index gives the nodes of this list that can match x, in the same order
    // the tree gives the demodulator of the list that rewrites x, with its
    // bindings on the variable stack; dmi_n is then 1 (found) or 0 (none)
    dmi_n=-1;
#ifdef GKC_DEMOD_CHECK
    if (dbused==0) dmi_check(g,x,hashvec,hash);
#else
    if (dbused==0) {
      dmi_n=wr_dmi_best(g,x,&node);
    }
#endif
#define WR_DMI_NEXT(node) ((dmi_n>=0) ? 0 : wr_clterm_hashlist_next(g,hashvec,(node)))
#else
#define WR_DMI_NEXT(node) wr_clterm_hashlist_next(g,hashvec,(node))
#endif
    // suitable node was found    
    while(node!=0) {  
      nodeptr=otp(db,node);  
      yterm=nodeptr[CLTERM_HASHNODE_TERM_POS];      
      ycl=otp(db,nodeptr[CLTERM_HASHNODE_CL_POS]);        
      if (WR_DMI_FOUND) {
        // the walk of the tree has tested the clause and set the bindings
      } else if (((g->back_subsume) && wr_blocked_clause(g,ycl)) || ycl==(g->rewrite_self)) {
        node=WR_DMI_NEXT(node);
        continue;
      }

#ifdef DEBUG   
      wr_printf("\n before matching with rewriter:\n");
      wr_print_term(g,yterm);
      printf(" -> ");

      // get right side of the rewrite rule
        path=wg_decode_int(db,nodeptr[CLTERM_HASHNODE_PATH_POS]); 
        if (wg_rec_is_rule_clause(db,ycl)) {
          yatom=wg_get_rule_clause_atom(db,ycl,0); // eq literal
        } else {
          yatom=encode_record(db,ycl);
        }        
        if (path==1) {
          // right arg is replacement
          b=(gint)(((gint *)(otp(db,yatom)))[RECORD_HEADER_GINTS+(g->unify_funarg2pos)]);
        } else {
          // left arg is replacement
          b=(gint)(((gint *)(otp(db,yatom)))[RECORD_HEADER_GINTS+(g->unify_funarg1pos)]);
        }


      wr_print_term(g,b);
      wr_printf("\n and input term x:\n");
      wr_print_term(g,x);
      wr_printf("\n and vardata:\n");
      wr_print_vardata(g); ;
#endif      
      if (WR_DMI_FOUND) {
        ures=1; // the bindings of the walk are on the variable stack
      } else {
        ures=wr_match_term(g,yterm,x,1); // uniquestrflag=1
        if (ures && (g->rewrite_guard) && wr_match_is_renaming(g)) ures=0; // the completeness condition
      }
#ifdef DEBUG        
      wr_printf("match check res: %d\n",ures);
#endif        
      if (!ures) {
        // matching failed     
        wr_clear_varstack(g,g->varstack); 
        // get next node;
        node=WR_DMI_NEXT(node);
      } else {
        // term rewriter found  
        (g->tmp_rewrites)++;                  
        // get right side of the rewrite rule
        path=wg_decode_int(db,nodeptr[CLTERM_HASHNODE_PATH_POS]); 
        if (wg_rec_is_rule_clause(db,ycl)) {
          yatom=wg_get_rule_clause_atom(db,ycl,0); // eq literal
        } else {
          yatom=encode_record(db,ycl);
        }        
        if (path==1) {
          // right arg is replacement
          b=(gint)(((gint *)(otp(db,yatom)))[RECORD_HEADER_GINTS+(g->unify_funarg2pos)]);
        } else {
          // left arg is replacement
          b=(gint)(((gint *)(otp(db,yatom)))[RECORD_HEADER_GINTS+(g->unify_funarg1pos)]);
        }
#ifdef TDEBUG   
      wr_printf("\n successful match with rewriter:\n");
      wr_print_term(g,yterm);
      printf(" -> ");
      wr_print_term(g,b);
      wr_printf("\n and input term ");
      wr_print_term(g,x);
      printf("\n");
      //wr_printf("\n and vardata:\n");
      //wr_print_vardata(g); ;
#endif      
      /* 
      if ((g->print_level_flag)==50) {
        wr_printf("\n successful match with rewriter:\n");
        wr_print_term(g,yterm);
        printf(" -> ");
        wr_print_term(g,b);
        wr_printf("\n and input term ");
        wr_print_term(g,x);
        printf("\n");
      }
      */
        // store rewriter 
        tmp=(int)((g->rewrite_clvec)[1]);
        if (tmp<NROF_REWRITE_CLVEC_ELS-4) {
          (g->rewrite_clvec)[tmp]=(gint)ycl;
          ((g->rewrite_clvec)[1])++;          
        }  
        // build the result
        // b is now the raw right side of rewrite rule before rebuilding
#ifdef DEBUG 
        wr_printf("\n right side of rewrite rule before rebuilding ");
        wr_print_term(g,b);
        wr_printf("\n");
#endif 
        if (isvar(b)) {
          b=VARVAL_DIRECT(b,(g->varbanks));          
        } else if (isdatarec(b)) {
          tmp_subst=(g->build_subst);
          tmp_rename=(g->build_rename);
          (g->build_subst)=1;
          (g->build_rename)=0;
          (g->build_rewrite)=0; // do not rewrite in the next step
          b=wr_build_calc_term(g,b);
          (g->build_subst)=tmp_subst;
          (g->build_rename)=tmp_rename;
          (g->build_rewrite)=1; // restore rewriting
        } else { //if (wg_get_encoded_type(db,b)==WG_URITYPE)) {
          // rewritable constant
          wr_clear_varstack(g,g->varstack);
          return wr_rewrite_constant(g,b); // no need to further process constants            
        } /* else {
          // non-rewritable constant
          wr_clear_varstack(g,g->varstack);
          return b;
        }
        */

        // b is now the fully built (subst into) right side of rewrite rule
        // we have to rewrite again until chain terminates

        wr_clear_varstack(g,g->varstack);
        if (isvar(b)) {
          return b;
        } else if (isdatarec(b)) {
          res=wr_build_calc_term(g,b); 
          return res;
        } else {
          return wr_rewrite_constant(g,b);
        }                           
      } // end of match cases: unsuccesful/successful       
    } // end loop over nodes in the hash bucket
  }  // end loop over local and external termhashes
#ifdef DEBUG
  wr_printf("\nwr_rewrite_term returns unchanged term ");
  wr_print_term(g,x);
  wr_printf("\n");
#endif    
  return x;
}


#undef WR_DMI_NEXT
#undef WR_DMI_FOUND
gint wr_rewrite_constant(glb* g, gint x) {
  void* db=g->db;
  gint hash,node,yterm,path,yatom,b;
  int dbused,hlen,tmp;
  gptr hashvec,nodeptr,ycl;
  
#ifdef DEBUG
  wr_printf("\nwr_rewrite_constant called on ");
  wr_print_term(g,x);
  wr_printf("\n");
#endif  

  // loop over local termhash (dbused==0) and external termhash (dbused==1)   
  hash=wr_term_basehash(g,x);
  for(dbused=0; dbused<2; dbused++) {            
    if (dbused==0) {
      // local termhash           
      hashvec=rotp(g,g->hash_rewrite_terms);         
      hlen=wr_clterm_hashlist_len(g,hashvec,hash);
      if (hlen==0) {
  #ifdef DEBUG        
        wr_printf("no matching atoms in local hash\n");
  #endif        
        continue;
      }   
    } else {   
      // external kb termhash  
      if (!(r_kb_g(g))) continue;             
      //printf("\nr_kb_g(g) used\n");
      hashvec=rotp(g,r_kb_g(g)->hash_rewrite_terms);       
      hlen=wr_clterm_hashlist_len(g,hashvec,hash);
      if (hlen==0) {
  #ifdef DEBUG        
        wr_printf("no matching atoms in external hash\n");
  #endif        
        continue;
      }               
    }   

    node=wr_clterm_hashlist_start(g,hashvec,hash);
    if (!node)  {      
      wr_printf("\nnode was null \n");
      continue; 
    }  
    // suitable node was found    
    while(node!=0) {  
      nodeptr=otp(db,node);  
      yterm=nodeptr[CLTERM_HASHNODE_TERM_POS];      
      ycl=otp(db,nodeptr[CLTERM_HASHNODE_CL_POS]);             
      if ((g->back_subsume) && wr_blocked_clause(g,ycl)) {
        node=wr_clterm_hashlist_next(g,hashvec,node);
        continue;
      }

#ifdef DEBUG   
      wr_printf("\n before matching yterm (left side of rewrite rule):\n");
      wr_print_term(g,yterm);
      wr_printf("\n and input term x:\n");
      wr_print_term(g,x);
      wr_printf("\n and vardata:\n");
      wr_print_vardata(g); 
#endif     
      if (!wr_equal_term(g,yterm,x,1)) { // uniquestrflag=1
        // matching failed            
        // get next node;
        node=wr_clterm_hashlist_next(g,hashvec,node);
      } else {
        // term rewriter found  
        (g->tmp_rewrites)++;                                  
        // get right side of the rewrite rule
        path=wg_decode_int(db,nodeptr[CLTERM_HASHNODE_PATH_POS]); 
        if (wg_rec_is_rule_clause(db,ycl)) {
          yatom=wg_get_rule_clause_atom(db,ycl,0); // eq literal
        } else {
          yatom=encode_record(db,ycl);
        }
        if (path==1) {
          // right arg is replacement
          b=(gint)(((gint *)(otp(db,yatom)))[RECORD_HEADER_GINTS+(g->unify_funarg2pos)]);
        } else {
          // left arg is replacement
          b=(gint)(((gint *)(otp(db,yatom)))[RECORD_HEADER_GINTS+(g->unify_funarg1pos)]);
        }        
        // b is now the raw right side of rewrite rule before rebuilding
#ifdef DEBUG 
        wr_printf("\n right side of rewrite rule before rebuilding ");
        wr_print_term(g,b);
        wr_printf("\n");
#endif         
        // store rewriter 
        tmp=(int)((g->rewrite_clvec)[1]);
        if (tmp<NROF_REWRITE_CLVEC_ELS-4) {
          (g->rewrite_clvec)[tmp]=(gint)ycl;
          ((g->rewrite_clvec)[1])++;          
        }  
        // rewrite again until rewrite chain terminates
        return wr_rewrite_constant(g,b); 
      } // end of match cases: unsuccesful/successful       
    } // end loop over nodes in the hash bucket
  }  // end loop over local and external termhashes
#ifdef DEBUG
  wr_printf("\nwr_rewrite_constant returns unchanged term ");
  wr_print_term(g,x);
  wr_printf("\n");
#endif    
  return x;
}


#ifdef __cplusplus
}
#endif

