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

  A discrimination tree over the greater sides of the demodulators in
  hash_rewrite_terms (the local table; an attached KB keeps its own table
  and is walked as before). The path of a side is the term in preorder
  over the positions of the unification: a record gives the token "record
  of n elements" and then its elements, a constant the token of its value
  hash (wr_term_basehash), a variable a star. A side can match a term only
  if the term has its tokens wherever the side has no variable, so the
  walk (the term's token or the star at each place) finds every side that
  wr_match_term can accept.

  Each entry is the node of the hash list (term, clause, path) and an
  insert number. wr_dmi_lookup returns the nodes in the order of the list
  (the newest first), so wr_rewrite_term tests the same nodes in the same
  order as with the list, and picks the same demodulator; the nodes of the
  list that the index leaves out cannot match.

  The tree is built only for the local table, outside a KB build, with
  unify_samelen 1 and unify_maxuseterms 0; otherwise, after a failed
  allocation, and in a build with -DGKC_NO_DEMOD_INDEX or with 4-byte
  pointers, wr_rewrite_term walks the list.

================================================ */

#ifdef USE_DEMOD_INDEX

typedef struct { uint64_t tok; int32_t parent, child, sibling, entries; } wr_dmi_node;
typedef struct { gint node; int32_t seq; int32_t next; } wr_dmi_entry;
typedef struct {
  wr_dmi_node* nodes; int32_t ncnt, ncap;
  int32_t* edges; int32_t ecap, ecnt;
  wr_dmi_entry* ents; int32_t encnt, encap;
  uint64_t* toks; int32_t* skip; int32_t tcap;
  int32_t* cand; int32_t ccnt, ccap;   // candidates: entry indices
  gint* out; int32_t ocap;              // the nodes returned by wr_dmi_lookup
} wr_dmi_t;

#define DMI_STAR UINT64_C(1)
#define DMI_REC(n) ((((uint64_t)(n))<<2)|UINT64_C(3))

static uint64_t wr_dmi_mix(uint64_t a, uint64_t b) {
  uint64_t k=(a^(b+UINT64_C(0x9E3779B97F4A7C15)+(a<<6)+(a>>2)))*UINT64_C(0xBF58476D1CE4E5B9);
  return k^(k>>31);
}

void wr_dmi_free(glb* g) {
  wr_dmi_t* x=(wr_dmi_t*)(g->dmi_index);
  if (!x) return;
  if (x==(void*)1) { (g->dmi_index)=NULL; return; }
  if (x->nodes) sys_free(x->nodes);
  if (x->edges) sys_free(x->edges);
  if (x->ents) sys_free(x->ents);
  if (x->toks) sys_free(x->toks);
  if (x->skip) sys_free(x->skip);
  if (x->cand) sys_free(x->cand);
  if (x->out) sys_free(x->out);
  sys_free(x);
  (g->dmi_index)=NULL;
}

static void wr_dmi_fail(glb* g) {
  wr_dmi_free(g);
  (g->dmi_index)=(void*)1; // the list answers for the rest of the run
}

static int32_t wr_dmi_tokens(glb* g, wr_dmi_t* x, gint t, int32_t n) {
  gptr tp;
  int i, start, end;
  int32_t me;
  if (n>=x->tcap) {
    int32_t nc=x->tcap*2;
    uint64_t* nt=sys_realloc(x->toks,sizeof(uint64_t)*nc);
    int32_t* ns;
    if (!nt) return -1;
    x->toks=nt;
    ns=sys_realloc(x->skip,sizeof(int32_t)*nc);
    if (!ns) return -1;
    x->skip=ns; x->tcap=nc;
  }
  me=n;
  if (isvar(t)) { x->toks[n]=DMI_STAR; x->skip[n]=n+1; return n+1; }
  if (!isdatarec(t)) {
    x->toks[n]=(wr_dmi_mix((uint64_t)wr_term_basehash(g,t),UINT64_C(7))<<2)|UINT64_C(2);
    x->skip[n]=n+1;
    return n+1;
  }
  tp=decode_record(g->db,t);
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,tp);
  x->toks[n]=DMI_REC(end-start);
  n++;
  for(i=start;i<end;i++) {
    n=wr_dmi_tokens(g,x,tp[i],n);
    if (n<0) return -1;
  }
  x->skip[me]=n;
  return n;
}

static int32_t wr_dmi_edge_slot(wr_dmi_t* x, int32_t parent, uint64_t tok) {
  uint64_t h=wr_dmi_mix((uint64_t)parent,tok);
  int32_t i=(int32_t)(h&(uint64_t)(x->ecap-1));
  int32_t e;
  while((e=x->edges[i])>=0) {
    if (x->nodes[e].parent==parent && x->nodes[e].tok==tok) return i;
    i=(i+1)&(x->ecap-1);
  }
  return i;
}

static int wr_dmi_grow_edges(wr_dmi_t* x) {
  int32_t* oe=x->edges; int32_t oc=x->ecap, i, j;
  int32_t* ne=sys_malloc(sizeof(int32_t)*oc*2);
  if (!ne) return 0;
  for(i=0;i<oc*2;i++) ne[i]=-1;
  x->edges=ne; x->ecap=oc*2;
  for(i=0;i<oc;i++) if (oe[i]>=0) {
    j=wr_dmi_edge_slot(x,x->nodes[oe[i]].parent,x->nodes[oe[i]].tok);
    ne[j]=oe[i];
  }
  sys_free(oe);
  return 1;
}

static wr_dmi_t* wr_dmi_make(glb* g) {
  wr_dmi_t* x=sys_malloc(sizeof(wr_dmi_t));
  int i;
  if (!x) return NULL;
  memset(x,0,sizeof(wr_dmi_t));
  (g->dmi_index)=x;
  x->ncap=1<<10; x->ecap=1<<11; x->encap=1<<9; x->tcap=1<<8; x->ccap=1<<8; x->ocap=1<<8;
  x->nodes=sys_malloc(sizeof(wr_dmi_node)*x->ncap);
  x->edges=sys_malloc(sizeof(int32_t)*x->ecap);
  x->ents=sys_malloc(sizeof(wr_dmi_entry)*x->encap);
  x->toks=sys_malloc(sizeof(uint64_t)*x->tcap);
  x->skip=sys_malloc(sizeof(int32_t)*x->tcap);
  x->cand=sys_malloc(sizeof(int32_t)*x->ccap);
  x->out=sys_malloc(sizeof(gint)*x->ocap);
  if (!x->nodes || !x->edges || !x->ents || !x->toks || !x->skip || !x->cand || !x->out) return NULL;
  for(i=0;i<x->ecap;i++) x->edges[i]=-1;
  x->nodes[0].tok=0; x->nodes[0].parent=-1; x->nodes[0].child=-1;
  x->nodes[0].sibling=-1; x->nodes[0].entries=-1;
  x->ncnt=1;
  return x;
}

/* file the greater side term of a demodulator; node is its new node of
   the hash list */

void wr_dmi_insert(glb* g, gint term, gint node) {
  wr_dmi_t* x=(wr_dmi_t*)(g->dmi_index);
  int32_t n, i, cur, nx, slot, e;
  if (x==(void*)1 || (g->inkb) || !node) return;
  if (!(g->unify_samelen) || (g->unify_maxuseterms)) { wr_dmi_fail(g); return; }
  if (!x) {
    x=wr_dmi_make(g);
    if (!x) { wr_dmi_fail(g); return; }
  }
  n=wr_dmi_tokens(g,x,term,0);
  if (n<0) { wr_dmi_fail(g); return; }
  cur=0;
  for(i=0;i<n;i++) {
    slot=wr_dmi_edge_slot(x,cur,x->toks[i]);
    nx=x->edges[slot];
    if (nx<0) {
      if (x->ncnt>=x->ncap) {
        wr_dmi_node* nn=sys_realloc(x->nodes,sizeof(wr_dmi_node)*(x->ncap)*2);
        if (!nn) { wr_dmi_fail(g); return; }
        x->nodes=nn; x->ncap*=2;
      }
      nx=x->ncnt++;
      x->nodes[nx].tok=x->toks[i]; x->nodes[nx].parent=cur;
      x->nodes[nx].child=-1; x->nodes[nx].entries=-1;
      x->nodes[nx].sibling=x->nodes[cur].child;
      x->nodes[cur].child=nx;
      x->edges[slot]=nx;
      (x->ecnt)++;
      if (2*(x->ecnt)>x->ecap && !wr_dmi_grow_edges(x)) { wr_dmi_fail(g); return; }
    }
    cur=nx;
  }
  if (x->encnt>=x->encap) {
    wr_dmi_entry* ne=sys_realloc(x->ents,sizeof(wr_dmi_entry)*(x->encap)*2);
    if (!ne) { wr_dmi_fail(g); return; }
    x->ents=ne; x->encap*=2;
  }
  e=x->encnt++;
  x->ents[e].node=node;
  x->ents[e].seq=e;
  x->ents[e].next=x->nodes[cur].entries;
  x->nodes[cur].entries=e;
}

static int wr_dmi_collect(wr_dmi_t* x, int32_t node) {
  int32_t e;
  for(e=x->nodes[node].entries; e>=0; e=x->ents[e].next) {
    if (x->ccnt>=x->ccap) {
      int32_t* c=sys_realloc(x->cand,sizeof(int32_t)*(x->ccap)*2);
      if (!c) return 0;
      x->cand=c; x->ccap*=2;
    }
    x->cand[(x->ccnt)++]=e;
  }
  return 1;
}

static int wr_dmi_walk(wr_dmi_t* x, int32_t node, int32_t i, int32_t n) {
  int32_t c;
  uint64_t t;
  if (i>=n) return wr_dmi_collect(x,node);
  t=x->toks[i];
  if (t!=DMI_STAR) {
    c=x->edges[wr_dmi_edge_slot(x,node,t)];
    if (c>=0 && !wr_dmi_walk(x,c,i+1,n)) return 0;
  }
  c=x->edges[wr_dmi_edge_slot(x,node,DMI_STAR)];
  if (c>=0 && !wr_dmi_walk(x,c,x->skip[i],n)) return 0;
  return 1;
}

static int wr_dmi_cmp(const void* a, const void* b) {
  int32_t x=*(const int32_t*)a, y=*(const int32_t*)b;
  return (x>y) ? -1 : ((x<y) ? 1 : 0); // the newest first, as in the list
}

/* the nodes of the hash list whose greater side can match the term x, in
   the order of the list; returns their count and sets *nodes, or -1 when
   the list has to answer. The entry point for any term. */

int wr_dmi_lookup(glb* g, gint x, gint** nodes) {
  wr_dmi_t* d=(wr_dmi_t*)(g->dmi_index);
  int32_t n, i;
  if (d==(void*)1) return -1;
  if (!d) { *nodes=NULL; return 0; } // no demodulator filed yet
  n=wr_dmi_tokens(g,d,x,0);
  if (n<0) { wr_dmi_fail(g); return -1; }
  d->ccnt=0;
  if (!wr_dmi_walk(d,0,0,n)) { wr_dmi_fail(g); return -1; }
  if (d->ccnt>1) qsort(d->cand,(size_t)(d->ccnt),sizeof(int32_t),wr_dmi_cmp);
  if (d->ccnt>d->ocap) {
    int32_t nc=d->ocap; gint* o;
    while(nc<d->ccnt) nc*=2;
    o=sys_realloc(d->out,sizeof(gint)*nc);
    if (!o) { wr_dmi_fail(g); return -1; }
    d->out=o; d->ocap=nc;
  }
  for(i=0;i<d->ccnt;i++) d->out[i]=d->ents[d->cand[i]].node;
  *nodes=d->out;
  return d->ccnt;
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
static gint dmi_check_first(glb* g, gint x, gint* nodes, int n, vec hashvec, gint hash) {
  void* db=g->db; gint node, yterm; gptr nodeptr, ycl; int k=0, ures;
  node=(n>=0) ? (n>0 ? nodes[0] : 0) : wr_clterm_hashlist_start(g,hashvec,hash);
  while(node!=0) {
    nodeptr=otp(db,node); yterm=nodeptr[CLTERM_HASHNODE_TERM_POS]; ycl=otp(db,nodeptr[CLTERM_HASHNODE_CL_POS]);
    if (!((g->back_subsume) && wr_blocked_clause(g,ycl))) {
      ures=wr_match_term(g,yterm,x,1);
      wr_clear_varstack(g,g->varstack);
      dmi_check_tests++;
      if (ures) return node;
    }
    k++;
    node=(n>=0) ? (k<n ? nodes[k] : 0) : wr_clterm_hashlist_next(g,hashvec,node);
  }
  return 0;
}
static void dmi_check(glb* g, gint x, vec hashvec, gint hash) {
  static gint* sv=NULL; static gint svcap=0;
  gint svn=(g->varstack)[1], q, a, b;
  gint* nodes; int n;
  if (svn>2) dmi_check_dirty++;
  if (svn>svcap) { svcap=svn*2; sv=sys_realloc(sv,sizeof(gint)*2*svcap); }
  for(q=2;q<svn;q++) { sv[2*q]=(g->varstack)[q]; sv[2*q+1]=*((gptr)((g->varstack)[q])); }
  dmi_check_tests=0;
  a=dmi_check_first(g,x,NULL,-1,hashvec,hash);
  dmi_check_ltests+=dmi_check_tests;
  wr_clear_varstack(g,g->varstack);
  for(q=2;q<svn;q++) { (g->varstack)[q]=sv[2*q]; *((gptr)(sv[2*q]))=sv[2*q+1]; }
  (g->varstack)[1]=svn;
  n=wr_dmi_lookup(g,x,&nodes);
  if (n<0) return;
  dmi_check_tests=0;
  b=dmi_check_first(g,x,nodes,n,hashvec,hash);
  dmi_check_itests+=dmi_check_tests;
  if (b) dmi_check_hits++;
  wr_clear_varstack(g,g->varstack);
  for(q=2;q<svn;q++) { (g->varstack)[q]=sv[2*q]; *((gptr)(sv[2*q]))=sv[2*q+1]; }
  (g->varstack)[1]=svn;
  dmi_check_calls++;
  if (a!=b) dmi_check_diff++;
}
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
  gint* dmi_nodes=NULL;
  int dmi_n=-1, dmi_k=1;
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
    dmi_n=-1; dmi_k=1;
#ifdef GKC_DEMOD_CHECK
    if (dbused==0) dmi_check(g,x,hashvec,hash);
#else
    if (dbused==0) {
      dmi_n=wr_dmi_lookup(g,x,&dmi_nodes);
      if (dmi_n>=0) node=(dmi_n>0 ? dmi_nodes[0] : 0);
    }
#endif
#define WR_DMI_NEXT(node) ((dmi_n>=0) ? (dmi_k<dmi_n ? dmi_nodes[dmi_k++] : 0) : wr_clterm_hashlist_next(g,hashvec,(node)))
#else
#define WR_DMI_NEXT(node) wr_clterm_hashlist_next(g,hashvec,(node))
#endif
    // suitable node was found    
    while(node!=0) {  
      nodeptr=otp(db,node);  
      yterm=nodeptr[CLTERM_HASHNODE_TERM_POS];      
      ycl=otp(db,nodeptr[CLTERM_HASHNODE_CL_POS]);        
      if ((g->back_subsume) && wr_blocked_clause(g,ycl)) {
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
      ures=wr_match_term(g,yterm,x,1); // uniquestrflag=1
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

