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

/* the buffers of the right-side copies: a chain of chunks of the layout of
   a cvec ([0] length, [1] next free, [2] the previous chunk), apart from
   queue_termbuf, since the weight check of the derived clauses reads the
   fill level of queue_termbuf */

#define DMI_RHS_CHUNK (1<<20)

static void wr_dmi_rhs_free(glb* g) {
  gptr c=(g->dmi_rhsbuf), p;
  while(c) { p=(gptr)(c[2]); sys_free(c); c=p; }
  (g->dmi_rhsbuf)=NULL;
}

/* a chunk with room for need gints (records with their alignment), or NULL */

static gptr wr_dmi_rhs_room(glb* g, gint need) {
  gptr c=(g->dmi_rhsbuf), nb;
  gint len;
  if (c && CVEC_NEXT(c)+need+2<CVEC_LEN(c)) return c;
  len=(need*2+16>DMI_RHS_CHUNK) ? need*2+16 : DMI_RHS_CHUNK;
  nb=sys_malloc((len+2)*sizeof(gint));
  if (!nb) return NULL;
  nb[0]=len; nb[1]=3; nb[2]=(gint)c;
  (g->dmi_rhsbuf)=nb;
  return nb;
}

/* the gints that a copy of the term needs, with one for the alignment of each record */

static gint wr_dmi_term_gints(glb* g, gint t) {
  gptr tp; gint n; int i, start, end;
  if (!isdatarec(t)) return 0;
  tp=decode_record(g->db,t);
  n=RECORD_HEADER_GINTS+get_record_len(tp)+1;
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,tp);
  for(i=start;i<end;i++) n+=wr_dmi_term_gints(g,tp[i]);
  return n;
}

/* demod_unoriented: the tree of the sides of unorientable equations shares
   the buffer of the right-side copies, so it goes when that buffer goes */

static void wr_dmu_drop(glb* g, void* to) {
  ti_index* u=(ti_index*)(g->dmu_index);
  if (u && u!=(void*)1) ti_free(u);
  (g->dmu_index)=to;
}

void wr_dmi_free(glb* g) {
  ti_index* x=(ti_index*)(g->dmi_index);
  wr_dmu_drop(g,NULL);
  wr_dmi_rhs_free(g);
  if (!x) return;
  if (x==(void*)1) { (g->dmi_index)=NULL; return; }
  ti_free(x);
  (g->dmi_index)=NULL;
}

static void wr_dmi_fail(glb* g) {
  wr_dmi_free(g);
  (g->dmi_index)=(void*)1; // the list answers for the rest of the run
  if (g->demod_unoriented) (g->dmu_index)=(void*)1; // no list holds the unorientable equations
}

/* file the greater side term of a demodulator; node is its new node of
   the hash list */

/* the bytes of the records of a term */

static long wr_dmi_term_bytes(glb* g, gint t) {
  gptr tp; long n; int i, start, end;
  if (!isdatarec(t)) return 0;
  tp=decode_record(g->db,t);
  n=(long)sizeof(gint)*(RECORD_HEADER_GINTS+get_record_len(tp));
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,tp);
  for(i=start;i<end;i++) n+=wr_dmi_term_bytes(g,tp[i]);
  return n;
}

/* the right side of the demodulator of the hash list node */

static gint wr_dmi_rhs(glb* g, gint node) {
  void* db=g->db;
  gptr nodeptr=otp(db,node), ycl;
  gint yatom, path;
  ycl=otp(db,nodeptr[CLTERM_HASHNODE_CL_POS]);
  path=wg_decode_int(db,nodeptr[CLTERM_HASHNODE_PATH_POS]);
  if (wg_rec_is_rule_clause(db,ycl)) yatom=wg_get_rule_clause_atom(db,ycl,0);
  else yatom=encode_record(db,ycl);
  if (path==1) return ((gint *)(otp(db,yatom)))[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];
  return ((gint *)(otp(db,yatom)))[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
}

/* file the greater side term of a demodulator; node is its new node of
   the hash list. The entry gets a copy of the right side in a buffer of the tree
   with its variables renamed to the tree bank by the slot numbers of the
   left side; a right side with a variable that is not in the left side is
   not filed (counted; the ordering never orients such an equation) */

void wr_dmi_insert(glb* g, gint term, gint node) {
  ti_index* x=(ti_index*)(g->dmi_index);
  gint *vars, rc, rhs;
  gptr buf;
  int32_t nv;
  if (x==(void*)1 || (g->inkb) || !node) return;
  if (!(g->unify_samelen) || (g->unify_maxuseterms)) { (g->stat_dmi_tree_stopped)=3; wr_dmi_fail(g); return; }
  if (!x) {
    x=ti_new((g->varbanks)+TREE_VARBANK*NROF_VARSINBANK,g->tree_compress);
    if (!x) { (g->stat_dmi_tree_stopped)=1; wr_dmi_fail(g); return; }
    (g->dmi_index)=x;
  }
  nv=ti_term_vars(g,x,term,&vars);
  if (nv<0) { (g->stat_dmi_tree_stopped)=1; wr_dmi_fail(g); return; }
  rhs=wr_dmi_rhs(g,node);
  buf=wr_dmi_rhs_room(g,wr_dmi_term_gints(g,rhs));
  if (!buf) { (g->stat_dmi_tree_stopped)=1; wr_dmi_fail(g); return; }
  rc=ti_rename_copy(g,rhs,vars,nv,TREE_VARBANK*NROF_VARSINBANK,buf);
  if (rc==WG_ILLEGAL) {
    // a variable of the right side not in the left side
    (g->stat_dmi_rhs_not_filed)++;
    return;
  }
  (g->stat_dmi_rhs_bytes)+=wr_dmi_term_bytes(g,rc);
  if (!ti_insert(g,x,term,node,rc) || ti_failed(x)) {
    (g->stat_dmi_tree_stopped)=1;
    wr_dmi_fail(g);
  }
}

/* demod_unoriented: file a side of an unorientable equation, as
   wr_dmi_insert does for an oriented one, in the tree dmu_index; returns 1
   when it is filed */

int wr_dmu_insert(glb* g, gint term, gint node) {
  ti_index* x=(ti_index*)(g->dmu_index);
  gint *vars, rc, rhs;
  gptr buf;
  int32_t nv;
  if (x==(void*)1 || (g->inkb) || !node) return 0;
  if (!(g->unify_samelen) || (g->unify_maxuseterms)) { wr_dmu_drop(g,(void*)1); return 0; }
  if (!x) {
    x=ti_new((g->varbanks)+TREE_VARBANK*NROF_VARSINBANK,g->tree_compress);
    if (!x) { wr_dmu_drop(g,(void*)1); return 0; }
    (g->dmu_index)=x;
  }
  nv=ti_term_vars(g,x,term,&vars);
  if (nv<0) { wr_dmu_drop(g,(void*)1); return 0; }
  rhs=wr_dmi_rhs(g,node);
  buf=wr_dmi_rhs_room(g,wr_dmi_term_gints(g,rhs));
  if (!buf) { wr_dmu_drop(g,(void*)1); return 0; }
  rc=ti_rename_copy(g,rhs,vars,nv,TREE_VARBANK*NROF_VARSINBANK,buf);
  if (rc==WG_ILLEGAL) {
    // a variable of the other side not in this side: never an instance to rewrite with
    (g->stat_dmi_rhs_not_filed)++;
    return 0;
  }
  (g->stat_dmi_rhs_bytes)+=wr_dmi_term_bytes(g,rc);
  if (!ti_insert(g,x,term,node,rc) || ti_failed(x)) { wr_dmu_drop(g,(void*)1); return 0; }
  (g->stat_dmu_filed)++;
  return 1;
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

/* the newest demodulator of the local list that rewrites the term x: the
   walk of the perfect discrimination tree finds it with its substitution
   in the cells of the tree bank. The bindings are then set on the variable
   stack (SETVAR on the variables of the tree bank), and *rhs is the copy
   of its right side in the tree bank, so the caller builds the new term
   with the existing build (build_subst 1) from these bindings. Returns 1
   and sets *node, 0 when there is none, -1 when the list has to answer (no
   tree, or bindings already on the variable stack outside a top-slice
   rewriting) */

/* demod_unoriented: a match of a side of an unorientable equation is
   accepted when the term (the instance of the side) is greater than the
   instance of the other side under the bindings of the match (they are in
   the cells of the tree); the walk has already refused a renaming */

static gint dmu_query=0;
#define WR_DMU_ON(g) ((g)->dmu_index && (g)->dmu_index!=(void*)1)

static int wr_dmu_accept(glb* g, gint node) {
  if (!wr_dmi_accept(g,node)) return 0;
  (g->stat_dmu_tests)++;
  return wr_order_eqterms(g,dmu_query,ti_cur_aux((ti_index*)(g->dmu_index)),g->varbanks)==1;
}

/* demod_unoriented: the newest side of an unorientable equation with an
   oriented instance that rewrites x, as wr_dmi_best gives it; 0 when there
   is none */

static int wr_dmu_best(glb* g, gint x, gint* node, gint* rhs) {
  ti_index* u=(ti_index*)(g->dmu_index);
  gint payload, aux, *vals;
  gptr vc;
  int32_t n, nv, k;
  if (!u || u==(void*)1) return 0;
  if ((g->varstack)[1]>2 && !(g->rw_base)) return 0;
  n=ti_preorder(g,u,x);
  if (n<0) { wr_dmu_drop(g,(void*)1); return 0; }
  dmu_query=x;
  k=ti_match_best(g,u,0,n,wr_dmu_accept,1,&payload,&aux,&nv,&vals);
  dmu_query=0;
  if (!k) return 0;
  vc=((gptr)(g->varstack))+1;
  for(k=0;k<nv;k++) SETVAR(encode_var(TREE_VARBANK*NROF_VARSINBANK+k),vals[k],g->varbanks,g->varstack,vc);
  *node=payload;
  *rhs=aux;
  (g->stat_dmu_rewrites)++;
  return 1;
}

int wr_dmi_best(glb* g, gint x, gint* node, gint* rhs) {
  ti_index* d=(ti_index*)(g->dmi_index);
  gint payload, aux, *vals;
  gptr vc;
  int32_t n, nv, k;
  if (d==(void*)1) return -1;
  if (!d) { *node=0; return wr_dmu_best(g,x,node,rhs); } // no oriented demodulator filed yet
  if ((g->varstack)[1]>2 && !(g->rw_base)) return -1; // the list tests the first candidate with these bindings
  n=ti_preorder(g,d,x);
  if (n<0) { (g->stat_dmi_tree_stopped)=2; wr_dmi_fail(g); return -1; }
  if (!ti_match_best(g,d,0,n,wr_dmi_accept,g->rewrite_guard,&payload,&aux,&nv,&vals)) {
    *node=0;
    // the oriented demodulators first, then the unorientable equations
    return wr_dmu_best(g,x,node,rhs);
  }
  vc=((gptr)(g->varstack))+1;
  for(k=0;k<nv;k++) SETVAR(encode_var(TREE_VARBANK*NROF_VARSINBANK+k),vals[k],g->varbanks,g->varstack,vc);
  *node=payload;
  *rhs=aux;
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
  gint svn=(g->varstack)[1], q, a, b, an, bn, rhs;
  if (svn>2) { dmi_check_dirty++; return; } // the tree is not used then
  if (!(g->dmi_index) || (g->dmi_index)==(void*)1) return;
  dmi_check_tests=0;
  a=dmi_check_first(g,x,hashvec,hash);
  dmi_check_ltests+=dmi_check_tests;
  an=(g->varstack)[1];
  if (an*2>svcap) { svcap=an*4; sv=sys_realloc(sv,sizeof(gint)*svcap); }
  for(q=2;q<an;q++) { sv[2*q]=(g->varstack)[q]; sv[2*q+1]=*((gptr)((g->varstack)[q])); }
  wr_clear_varstack(g,g->varstack);
  if (wr_dmi_best(g,x,&b,&rhs)<0) { wr_clear_varstack(g,g->varstack); return; }
  dmi_check_itests++;
  if (b) dmi_check_hits++;
  bn=(g->varstack)[1];
  dmi_check_calls++;
  if (a!=b || an!=bn) dmi_check_diff++;
  else {
    for(q=2;q<an;q++) {
      // the list binds the variables of the demodulator, the tree those of
      // the tree bank, both in the order of their first occurrence: the
      // values must be equal
      if (sv[2*q+1]!=*((gptr)((g->varstack)[q]))) { dmi_check_diff++; break; }
    }
  }
  wr_clear_varstack(g,g->varstack);
}
#endif

#ifdef USE_DEMOD_INDEX

/* ---- demod_derived: the rewriting of a derived clause ----

  wr_demod_derived rewrites the literals of the half-built clause rptr
  (rpos literals in derived_termbuf) with the demodulators, after the
  literals of both parents are built and before the cuts, the subsumption
  and the weight check, as E does. Only a literal with a candidate is built
  again: a walk of the tree over each subterm of its atom (one preorder array
  per atom; a subterm is a slice of the symbols) finds whether a demodulator
  matches somewhere. The build is the build of the pick (wr_build_calc_term
  with build_rewrite 1, no substitution, no renaming): innermost first, and
  the right side of a rewrite is built again with rewriting on, until no
  demodulator applies.

  The variable stack holds the unifier of the parents (banks 1 and 2)
  while the result is processed. The half-built clause has only variables
  of bank 3, which are unbound then (wr_process_resolve_result_cleanupsubst
  clears them), and the tree binds the cells of the tree bank: the
  rewriting keeps the unifier, pushes its bindings above it (rw_base), and
  clears only that top slice.

  The completeness condition (Vampire's encompassment, design section 2):
  in a positive unit equation the top of a side is not rewritten by a
  demodulator that matches it by a renaming; the subterms of a side are
  rewritten freely.

  The demodulators used are in rewrite_clvec; wr_demod_add_rewriters adds
  them to the cutters of the clause, so that the derivation record of the
  clause lists them as parents.

  Returns 0: no change; 1: a literal was rewritten; -1: drop the clause (a
  positive literal s=s after the rewriting, or the buffer was full).
*/

/* the limit of rewrites per literal of a derived clause; the ordering
   orients every demodulator, so a rewriting always ends; reaching the limit
   shows a defect */
#define WR_DEMOD_RW_LIMIT 10000

/* the earlier form: the full rebuild of a literal, for the check build and
   for demod_unoriented */

/* a side of a positive unit equation: the arguments rewritten freely, the
   top with the completeness condition; a constant side is not rewritten at
   its top (every match of a constant is a renaming) */

static gint wr_demod_build_side(glb* g, gint s) {
  gptr xptr, yptr;
  gint xlen, tmp, res;
  int i, ilimit;
  if (!isdatarec(s)) return s;
  xptr=decode_record(g->db,s);
  xlen=get_record_len(xptr);
  yptr=wr_alloc_from_cvec(g,g->build_buffer,(RECORD_HEADER_GINTS+xlen));
  if (yptr==NULL) return WG_ILLEGAL;
  ilimit=RECORD_HEADER_GINTS+(g->unify_firstuseterm);
  for(i=0;i<ilimit;i++) yptr[i]=xptr[i];
  for(;i<RECORD_HEADER_GINTS+xlen;i++) {
    tmp=wr_build_calc_term(g,xptr[i]);
    if (tmp==WG_ILLEGAL) return WG_ILLEGAL;
    yptr[i]=tmp;
  }
  res=encode_record(g->db,yptr);
  (g->rewrite_guard)=1;
  res=wr_rewrite_term(g,res);
  (g->rewrite_guard)=0;
  return res;
}

/* the atom of a positive unit equation: the symbol = as it is, both sides
   with wr_demod_build_side */

static gint wr_demod_build_eq(glb* g, gint atom) {
  gptr xptr, yptr;
  gint xlen, tmp;
  int i, ilimit, p1, p2;
  xptr=decode_record(g->db,atom);
  xlen=get_record_len(xptr);
  yptr=wr_alloc_from_cvec(g,g->build_buffer,(RECORD_HEADER_GINTS+xlen));
  if (yptr==NULL) return WG_ILLEGAL;
  ilimit=RECORD_HEADER_GINTS+(g->unify_firstuseterm);
  for(i=0;i<ilimit;i++) yptr[i]=xptr[i];
  p1=RECORD_HEADER_GINTS+(g->unify_funarg1pos);
  p2=RECORD_HEADER_GINTS+(g->unify_funarg2pos);
  for(;i<RECORD_HEADER_GINTS+xlen;i++) {
    if (i==p1 || i==p2) {
      tmp=wr_demod_build_side(g,xptr[i]);
      if (tmp==WG_ILLEGAL) return WG_ILLEGAL;
      yptr[i]=tmp;
    } else {
      yptr[i]=xptr[i];
    }
  }
  return encode_record(g->db,yptr);
}

/* ---- the one-pass normalisation of a term (demod_derived) ----

  wr_demod_norm gives the normal form of a term with the demodulators, in
  the order of the build of the pick (wr_build_calc_term with build_rewrite
  1): the elements of a record left to right and innermost first, then the
  top. The symbols of the term are copied in preorder into an array once,
  in a frame of copied arrays; the lookup at a subterm is a walk over its
  slice of the symbols, with no new copy. A subterm with no match
  whose elements did not change is
  kept as it is (no copy). A record whose elements changed is built new and
  rewritten at its top with wr_rewrite_term; at a subterm with a match,
  wr_rewrite_term does the rewrite. wr_rewrite_term builds the instance of
  the right side with build_subst 1 and, in this mode (rw_base), normalises
  it with wr_demod_norm again, where the parts that come from the bindings
  are walked but give no match (they are in normal form already).
  Constants go to wr_rewrite_constant (the tree, one symbol).

  The frames are a stack of preorder arrays addressed by offset, since a
  nested normalisation may grow them.
*/

static uint64_t* nt_syms=NULL;
static int32_t* nt_skip=NULL;
static gint* nt_terms=NULL;
static int32_t nt_cap=0, nt_top=0;

/* copy the symbols of t in preorder into a new frame; returns its base, or -1 */

static int32_t wr_nt_push(glb* g, ti_index* d, gint t) {
  uint64_t *tk; int32_t *sk, n, base, i; gint *tm;
  n=ti_preorder(g,d,t);
  if (n<0) return -1;
  if (nt_top+n>nt_cap) {
    int32_t nc=(nt_top+n)*2+256;
    uint64_t* a=sys_realloc(nt_syms,sizeof(uint64_t)*nc);
    int32_t* b; gint* c;
    if (!a) return -1;
    nt_syms=a;
    b=sys_realloc(nt_skip,sizeof(int32_t)*nc);
    if (!b) return -1;
    nt_skip=b;
    c=sys_realloc(nt_terms,sizeof(gint)*nc);
    if (!c) return -1;
    nt_terms=c;
    nt_cap=nc;
  }
  ti_preorder_arrays(d,&tk,&sk,&tm);
  base=nt_top;
  for(i=0;i<n;i++) { nt_syms[base+i]=tk[i]; nt_skip[base+i]=sk[i]; nt_terms[base+i]=tm[i]; }
  nt_top+=n;
  return base;
}

/* the normal form of the subterm at symbol i of the frame base; guardtop: i
   is the top of a side of a positive unit equation (the completeness
   condition: no rewrite by a renaming, and a constant side is not
   rewritten) */

static gint wr_demod_norm_pos(glb* g, ti_index* d, int32_t base, int32_t i, int guardtop) {
  gint t=nt_terms[base+i], res, payload, aux, *vals, e, ne;
  gptr tp, yp;
  int32_t j, nv;
  int k, start, end, changed=0, hit;
  gint newel[64];
  gint* nel=newel;
  if (isvar(t)) return t;
  if (!isdatarec(t)) return (guardtop ? t : wr_rewrite_constant(g,t));
  tp=decode_record(g->db,t);
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,tp);
  if (end-start>64) {
    nel=sys_malloc(sizeof(gint)*(end-start));
    if (!nel) return WG_ILLEGAL;
  }
  // the elements, left to right; j is the symbol of element k
  j=i+1;
  for(k=start;k<end;k++) {
    e=tp[k];
    ne=wr_demod_norm_pos(g,d,base,j,0);
    if (ne==WG_ILLEGAL) { if (nel!=newel) sys_free(nel); return WG_ILLEGAL; }
    if (ne!=e) changed=1;
    nel[k-start]=ne;
    j=nt_skip[base+j];
  }
  if (changed) {
    // a new record from the normal forms of the elements, then its top
    gint xlen=get_record_len(tp);
    yp=wr_alloc_from_cvec(g,g->build_buffer,(RECORD_HEADER_GINTS+xlen));
    if (yp==NULL) { if (nel!=newel) sys_free(nel); return WG_ILLEGAL; }
    for(k=0;k<RECORD_HEADER_GINTS+xlen;k++) yp[k]=tp[k];
    for(k=start;k<end;k++) yp[k]=nel[k-start];
    if (nel!=newel) sys_free(nel);
    res=encode_record(g->db,yp);
    if (guardtop) (g->rewrite_guard)=1;
    res=wr_rewrite_term(g,res);
    (g->rewrite_guard)=0;
    return res;
  }
  if (nel!=newel) sys_free(nel);
  // no element changed: a walk over the slice of the subterm
  hit=ti_match_best_ext(g,d,nt_syms+base,nt_skip+base,nt_terms+base,i,nt_skip[base+i],
                        wr_dmi_accept,guardtop,&payload,&aux,&nv,&vals);
  if (!hit && WR_DMU_ON(g)) {
    // demod_unoriented: the sides of unorientable equations, on the same arrays
    dmu_query=t;
    hit=ti_match_best_ext(g,(ti_index*)(g->dmu_index),nt_syms+base,nt_skip+base,nt_terms+base,i,
                          nt_skip[base+i],wr_dmu_accept,1,&payload,&aux,&nv,&vals);
    dmu_query=0;
  }
  if (!hit) return t;
  if (guardtop) (g->rewrite_guard)=1;
  res=wr_rewrite_term(g,t); // the rewrite at this position (its lookup finds the same demodulator)
  (g->rewrite_guard)=0;
  return res;
}

/* the normal form of the term t; eqatom: t is the atom of a positive unit
   equation, whose symbol = and top are not rewritten and whose sides have
   the completeness condition */

gint wr_demod_norm(glb* g, gint t, int eqatom) {
  ti_index* d=(ti_index*)(g->dmi_index);
  int32_t base, j;
  gint res, ne, e;
  gptr tp, yp;
  int k, start, end, changed=0, p1, p2;
  gint nel[8];
  if (!d || d==(void*)1 || isvar(t)) return t;
  if (!isdatarec(t)) return wr_rewrite_constant(g,t);
  base=wr_nt_push(g,d,t);
  if (base<0) return WG_ILLEGAL;
  if (!eqatom) {
    res=wr_demod_norm_pos(g,d,base,0,0);
    nt_top=base;
    return res;
  }
  tp=decode_record(g->db,t);
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,tp);
  p1=RECORD_HEADER_GINTS+(g->unify_funarg1pos);
  p2=RECORD_HEADER_GINTS+(g->unify_funarg2pos);
  if (end-start>8) { nt_top=base; return WG_ILLEGAL; }
  j=1;
  for(k=start;k<end;k++) {
    e=tp[k];
    if (k==p1 || k==p2) {
      ne=wr_demod_norm_pos(g,d,base,j,1);
      if (ne==WG_ILLEGAL) { nt_top=base; return WG_ILLEGAL; }
    } else {
      ne=e; // the symbol =
    }
    if (ne!=e) changed=1;
    nel[k-start]=ne;
    j=nt_skip[base+j];
  }
  nt_top=base;
  if (!changed) return t;
  yp=wr_alloc_from_cvec(g,g->build_buffer,(RECORD_HEADER_GINTS+get_record_len(tp)));
  if (yp==NULL) return WG_ILLEGAL;
  for(k=0;k<RECORD_HEADER_GINTS+get_record_len(tp);k++) yp[k]=tp[k];
  for(k=start;k<end;k++) yp[k]=nel[k-start];
  return encode_record(g->db,yp);
}

#ifdef GKC_DEMOD_NORM_CHECK
static long nc_lits=0, nc_diff=0, nc_rew=0;
static void nc_report(void) { fprintf(stderr,"DEMOD_NORM_CHECK literals %ld differences %ld rewritten %ld\n",nc_lits,nc_diff,nc_rew); }
__attribute__((constructor)) static void nc_init(void) { atexit(nc_report); }
#endif

int wr_demod_derived(glb* g, gptr rptr, int rpos) {
  ti_index* d=(ti_index*)(g->dmi_index);
  gptr vs=g->varstack;
  gint atom, newatom, meta, prev;
  int j, started=0, changed=0, res=0, eqflag;
  int s_subst=0, s_rename=0, s_rewrite=0, s_calc=0, s_comp=0;
  gptr s_buffer=NULL;
  (g->demod_this_clause)=0;
  if ((!d || d==(void*)1) && !WR_DMU_ON(g)) return 0;
  if (!(g->have_rewrite_terms)) return 0;
  (g->stat_demod_derived_checked)++;
  for(j=0;j<rpos;j++) {
    atom=rptr[(j*LIT_WIDTH)+LIT_ATOM_POS];
    if (!isdatarec(atom)) continue;
    if (!started) {
      // the bindings of the rewriting go above the unifier; set up the build of the pick
      (g->rw_base)=vs[1];
      s_subst=(g->build_subst); s_rename=(g->build_rename); s_rewrite=(g->build_rewrite);
      s_calc=(g->build_calc); s_comp=(g->use_comp_funs); s_buffer=(g->build_buffer);
      (g->build_subst)=0; (g->build_rename)=0; (g->build_calc)=0; (g->use_comp_funs)=0;
      (g->build_buffer)=(g->derived_termbuf);
      (g->rewrite_clvec)[1]=2;
      (g->derived_build_active)=1;
      (g->derived_build_overflow)=0;
      started=1;
    }
    meta=rptr[(j*LIT_WIDTH)+LIT_META_POS];
    eqflag=(rpos==1 && !wg_atom_meta_is_neg(g->db,meta) && wr_equality_atom(g,atom));
    prev=(g->tmp_rewrites);
    (g->demod_rw_stop)=prev+WR_DEMOD_RW_LIMIT;
    (g->demod_rw_hit)=0;
    (g->build_rewrite)=1;
#ifdef GKC_DEMOD_NORM_CHECK
    {
      // the check build: the one-pass form against the full rebuild of the
      // literal (the earlier form); the full rebuild gives the result
      gint nw, rv=(g->rewrite_clvec)[1], tr=(g->tmp_rewrites);
      (g->demod_oldform)=0;
      nw=wr_demod_norm(g,atom,eqflag);
      (g->rewrite_clvec)[1]=rv; (g->tmp_rewrites)=tr;
      (g->demod_oldform)=1;
      newatom=(eqflag ? wr_demod_build_eq(g,atom) : wr_build_calc_term(g,atom));
      (g->demod_oldform)=0;
      nc_lits++;
      if ((g->tmp_rewrites)!=tr) nc_rew++;
      if (nw!=WG_ILLEGAL && newatom!=WG_ILLEGAL && !wr_equal_term(g,nw,newatom,1)) nc_diff++;
    }
#else
    if (WR_DMU_ON(g) && (!d || d==(void*)1)) {
      // demod_unoriented with no tree of oriented demodulators yet: the full
      // rebuild, whose lookups at every subterm also try the unorientable
      // equations (the one-pass form needs the oriented tree for its arrays)
      (g->demod_oldform)=1;
      newatom=(eqflag ? wr_demod_build_eq(g,atom) : wr_build_calc_term(g,atom));
      (g->demod_oldform)=0;
    } else {
      newatom=wr_demod_norm(g,atom,eqflag);
    }
#endif
    (g->build_rewrite)=0;
    (g->rewrite_guard)=0;
    (g->demod_rw_stop)=0;
    if (g->demod_rw_hit) {
      // a defect: the rewriting of a literal did not end within the limit;
      // the clause keeps the literal as far as it was rewritten (sound)
      (g->demod_rw_hit)=0;
      (g->stat_demod_rw_limit)++;
      if ((g->print_level_flag)>=40) {
        wr_printf("\ndemod_derived: the limit of %d rewrites reached on the literal ",WR_DEMOD_RW_LIMIT);
        wr_print_term(g,atom);
        wr_printf("\n");
      }
    }
    if (newatom==WG_ILLEGAL || (g->derived_build_overflow)) { res=-1; break; }
    if ((g->tmp_rewrites)!=prev) {
      rptr[(j*LIT_WIDTH)+LIT_ATOM_POS]=newatom;
      changed=1;
      // a positive literal s=s makes the clause a tautology; a negative
      // literal s!=s is false (counted; the clause keeps it)
      if (wr_equality_atom(g,newatom)) {
        gptr tp=decode_record(g->db,newatom);
        if (wr_equal_term(g,tp[RECORD_HEADER_GINTS+(g->unify_funarg1pos)],
                            tp[RECORD_HEADER_GINTS+(g->unify_funarg2pos)],1)) {
          if (!wg_atom_meta_is_neg(g->db,meta)) { res=-1; break; }
          (g->stat_demod_neg_trivial)++;
          if (rpos==1) (g->stat_demod_neg_trivial_unit)++;
        }
      }
    }
  }
  if (started) {
    // restore the build settings and the variable stack
    (g->build_subst)=s_subst; (g->build_rename)=s_rename; (g->build_rewrite)=s_rewrite;
    (g->build_calc)=s_calc; (g->use_comp_funs)=s_comp; (g->build_buffer)=s_buffer;
    if (g->derived_build_overflow) (g->stat_derived_overflow_dropped)++;
    (g->derived_build_active)=0;
    (g->derived_build_overflow)=0;
    wr_clear_varstack_topslice(g,vs,(g->rw_base));
    (g->rw_base)=0;
  }
  if (res<0) { (g->stat_demod_derived_dropped)++; (g->demod_this_clause)=0; return -1; }
  if (changed) {
    (g->stat_demod_derived_rewritten)++;
    (g->demod_this_clause)=1;
    (g->early_weight_known)=0;
    (g->lithash_n)=0;
    return 1;
  }
  return 0;
}

/* add the demodulators of the rewriting of the derived clause after the
   cuts in cut_clvec, so that its derivation record lists them */

void wr_demod_add_rewriters(glb* g, int cuts) {
  gptr cv=g->cut_clvec, rv=g->rewrite_clvec;
  int i, k=cuts+1, n, need;
  if (!(g->demod_this_clause)) return;
  (g->demod_this_clause)=0;
  n=(int)(rv[1]);
  need=k+(n-2)+2;
  if (cv[0]<=need) {
    // the cutters and the demodulators do not fit: a larger cut_clvec, with
    // the cutters copied (the history builders read g->cut_clvec)
    int len=(int)(cv[0]), nlen=need*2;
    gptr nv=wr_vec_new(g,nlen);
    if (!nv) { (g->stat_demod_parents_lost)++; return; }
    for(i=1;i<k && i<len;i++) nv[i]=cv[i];
    wr_vec_free(g,cv);
    (g->cut_clvec)=nv;
    cv=nv;
  }
  for(i=2;i<n;i++) cv[k++]=rv[i];
  cv[k]=(gint)NULL;
}

#else

int wr_demod_derived(glb* g, gptr rptr, int rpos) { UNUSED(g); UNUSED(rptr); UNUSED(rpos); return 0; }
void wr_demod_add_rewriters(glb* g, int cuts) { UNUSED(g); UNUSED(cuts); }

#endif

#ifdef GKC_DEMOD_CHECK
/* the check build for a constant: the first node of the list that is not
   blocked, not rewrite_self and equal to x, against the tree */
static long dmi_cc_calls=0, dmi_cc_diff=0, dmi_cc_hits=0;
static void dmi_cc_report(void) {
  fprintf(stderr,"DMI_CHECK_CONST lookups %ld differences %ld rewritten %ld\n",dmi_cc_calls,dmi_cc_diff,dmi_cc_hits);
}
__attribute__((constructor)) static void dmi_cc_init(void) { atexit(dmi_cc_report); }
static void dmi_check_const(glb* g, gint x) {
  void* db=g->db;
  ti_index* d=(ti_index*)(g->dmi_index);
  gint hash=wr_term_basehash(g,x), node, a=0, payload=0, aux, *vals;
  gptr hashvec=rotp(g,g->hash_rewrite_terms), nodeptr;
  int32_t n, nv;
  if (wr_clterm_hashlist_len(g,hashvec,hash)) {
    for(node=wr_clterm_hashlist_start(g,hashvec,hash); node; node=wr_clterm_hashlist_next(g,hashvec,node)) {
      nodeptr=otp(db,node);
      if (!wr_dmi_accept(g,node)) continue;
      if (wr_equal_term(g,nodeptr[CLTERM_HASHNODE_TERM_POS],x,1)) { a=node; break; }
    }
  }
  n=ti_preorder(g,d,x);
  if (n<0 || !ti_match_best(g,d,0,n,wr_dmi_accept,0,&payload,&aux,&nv,&vals)) payload=0;
  dmi_cc_calls++;
  if (a) dmi_cc_hits++;
  if (a!=payload) dmi_cc_diff++;
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

/* the clear of the variable stack after a match: the whole stack, or with
   rw_base (the rewriting of a derived clause) only the slice above it, so
   that the unifier of the parents below it stays bound */
#define WR_RW_CLEAR(g) (((g)->rw_base) ? wr_clear_varstack_topslice((g),(g)->varstack,(g)->rw_base) : \
                                         wr_clear_varstack((g),(g)->varstack))

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
  gint dmi_rhs=0; // the right side of the demodulator that the tree found, in the tree bank
#endif
  int hash_known=0; // the hash of fun is computed only for a list

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
  if ((g->demod_rw_stop) && (g->tmp_rewrites)>=(g->demod_rw_stop)) {
    // the limit of rewrites of one derived clause: a defect detector
    (g->demod_rw_hit)=1;
    return x;
  }
  for(dbused=0; dbused<2; dbused++) {            
#if defined(USE_DEMOD_INDEX) && !defined(GKC_DEMOD_CHECK)
    // the local demodulators: the tree gives the demodulator of the list
    // that rewrites x, with its bindings on the variable stack; dmi_n is
    // then 1 (found) or 0 (none), and no hash and no list are used
    dmi_n=-1;
    if (dbused==0 && (g->dmi_index)!=(void*)1) {
      dmi_n=wr_dmi_best(g,x,&node,&dmi_rhs);
      if (dmi_n==0) continue;
      // demod_source 2: a demodulator is a kept clause, whose variables can
      // share cells with the term to rewrite; the list binds through the
      // cells, so it is not used for the local table; the rewriting of a
      // derived clause (rw_base): the unifier of the parents is bound in the
      // banks of the active demodulators, so the list is not used there
      if (dmi_n<0 && ((g->demod_source)==2 || (g->rw_base))) continue;
    }
    if (dmi_n<0) {
#endif
    if (!hash_known) { hash=wr_term_basehash(g,fun); hash_known=1; }
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
#if defined(USE_DEMOD_INDEX) && !defined(GKC_DEMOD_CHECK)
    }
#endif
#ifdef USE_DEMOD_INDEX
#ifdef GKC_DEMOD_CHECK
    // the check build: the list answers; the tree is compared with it
    dmi_n=-1;
    if (dbused==0) dmi_check(g,x,hashvec,hash);
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
        WR_RW_CLEAR(g); 
        // get next node;
        node=WR_DMI_NEXT(node);
      } else {
        // term rewriter found  
        (g->tmp_rewrites)++;
        (g->rewrite_guard)=0; // the condition holds only for the top of a side
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
        } else if (g->rw_base) {
          (g->stat_demod_parents_lost)++; // demod_derived: no room for the demodulator
        }
        // build the result
        // b is now the raw right side of rewrite rule before rebuilding
#ifdef DEBUG 
        wr_printf("\n right side of rewrite rule before rebuilding ");
        wr_print_term(g,b);
        wr_printf("\n");
#endif 
        // the tree: the copy of the right side in the tree bank, whose
        // variables are bound on the stack
#ifdef USE_DEMOD_INDEX
        if (WR_DMI_FOUND) b=dmi_rhs;
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
          WR_RW_CLEAR(g);
          return wr_rewrite_constant(g,b); // no need to further process constants            
        } /* else {
          // non-rewritable constant
          WR_RW_CLEAR(g);
          return b;
        }
        */

        // b is now the fully built (subst into) right side of rewrite rule
        // we have to rewrite again until chain terminates

        WR_RW_CLEAR(g);
        if (isvar(b)) {
          return b;
        } else if (isdatarec(b)) {
          // the rewriting of a derived clause (rw_base): the one-pass
          // normalisation of the instance
#ifdef USE_DEMOD_INDEX
          if ((g->rw_base) && !(g->demod_oldform)) res=wr_demod_norm(g,b,0);
          else res=wr_build_calc_term(g,b);
#else
          res=wr_build_calc_term(g,b);
#endif
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
#undef WR_RW_CLEAR
gint wr_rewrite_constant(glb* g, gint x) {
  void* db=g->db;
  gint hash,node,yterm,path,yatom,b;
  int dbused,hlen,tmp;
  gptr hashvec,nodeptr,ycl;
#ifdef USE_DEMOD_INDEX
  ti_index* d=(ti_index*)(g->dmi_index);
  int usetree=(d!=(void*)1);
  int hash_known=0;
#ifdef GKC_DEMOD_CHECK
  // the check build: the list answers; the tree is compared with it
  if (usetree && d) dmi_check_const(g,x);
  usetree=0;
#endif
#endif
  
#ifdef DEBUG
  wr_printf("\nwr_rewrite_constant called on ");
  wr_print_term(g,x);
  wr_printf("\n");
#endif  

  // loop over local termhash (dbused==0) and external termhash (dbused==1)   
#ifndef USE_DEMOD_INDEX
  hash=wr_term_basehash(g,x);
#endif
  for(dbused=0; dbused<2; dbused++) {            
#ifdef USE_DEMOD_INDEX
    if (dbused==0 && usetree) {
      // the local demodulators: the tree, where a constant is one symbol (its
      // gint); the newest demodulator that is not blocked and not
      // rewrite_self; no hash and no list
      gint payload, aux, *vals;
      int32_t n, nv;
      if (!d) continue; // no demodulator filed yet
      n=ti_preorder(g,d,x);
      if (n<0) { (g->stat_dmi_tree_stopped)=2; wr_dmi_fail(g); usetree=0; dbused=-1; continue; }
      if (!ti_match_best(g,d,0,n,wr_dmi_accept,0,&payload,&aux,&nv,&vals)) continue;
      nodeptr=otp(db,payload);
      ycl=otp(db,nodeptr[CLTERM_HASHNODE_CL_POS]);
      (g->tmp_rewrites)++;
      b=wr_dmi_rhs(g,payload); // ground: every variable of a right side is in its left side
      tmp=(int)((g->rewrite_clvec)[1]);
      if (tmp<NROF_REWRITE_CLVEC_ELS-4) {
        (g->rewrite_clvec)[tmp]=(gint)ycl;
        ((g->rewrite_clvec)[1])++;
      } else if (g->rw_base) {
        (g->stat_demod_parents_lost)++; // demod_derived: no room for the demodulator
      }
      // rewrite again until rewrite chain terminates
      return wr_rewrite_constant(g,b);
    }
    if (!hash_known) { hash=wr_term_basehash(g,x); hash_known=1; }
#endif
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
        } else if (g->rw_base) {
          (g->stat_demod_parents_lost)++; // demod_derived: no room for the demodulator
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

