/*
*
* Copyright (c) Tanel Tammet 2004-2026
*
* Contact: tanel.tammet at gmail.com
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
* GNU Affero General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with GKC.  If not, see <http://www.gnu.org/licenses/>.
*
*/

/** @file termindex.c
 *  A perfect discrimination tree of terms: the stored terms that match a
 *  query term (the stored term is more general), with the substitution.
 */

/* ====================================================

  The path of a stored term is its preorder over the positions of the
  unification:

  * a record gives the token "record of n elements", then its elements
    (the function symbol first);
  * a constant is a gint (Doc/DATA_REPRESENTATION.md, sections 1 and 8)
    and is its own token: the encoded gint, compared as an integer
    (new index code compares the gint only). For the symbols
    (interned URIs and long strings; find_create_longstr looks first into
    an attached KB), the small integers and the immediate kinds this is
    the rule of wr_equal_term (unify.c:777) and WR_EQUAL_TERM (unify.h:46)
    with uniquestrflag 1, which wr_match_term_aux uses. For a full
    integer, a full double and a short string, wr_equal_term compares by
    value (wr_equal_ptr_primitives, unify.c:983), and their encoders
    allocate a new object per value, a known defect of the encoders
    (Doc/DATA_REPRESENTATION.md, section 1, "Uniqueness of Encoded
    Values"). New index code assumes one encoding per constant, so the
    tree compares their gints too, and two encodings of one value do not
    match: a missed rewrite, never a wrong one. No hash is computed;
  * a variable gives the token "slot k": the variables of a stored term
    are numbered 0,1,2.. in the order of their first occurrence (as E's
    variables after ClauseNormalizeVars), so variants of a term share one
    path. The entry keeps the variables of the stored term in slot order,
    so the value of slot k is the value of the k-th variable of the
    stored term, from which the caller builds the right side.

  The tree is a full tree (every token is a node, no cut at a depth). As in
  E's perfect discrimination tree (CLAUSES/ccl_pdtrees.c), the walk over a
  query term binds and clears the variables of the tree: at the first
  occurrence of slot k on the path the walk binds slot k to the query
  subterm; at a later occurrence it compares the bound term with the query
  subterm (wr_equal_term) and goes on only if they are equal; on
  backtracking the binding is cleared. A leaf reached by the walk is a
  match, and the slot values are its substitution. The slots are an array
  of the walk: the walk writes no variable cell of gkc and does not use
  the variable stack, so a variable of the query term (a constant for the
  walk) and a variable of a stored term never mix, also while a unifier is
  on the variable stack.

  The children of a node: up to TI_INLINE children with a record token or
  a constant token in an array in the node, then a skip list sorted by
  token; the slot children in a list of their own (few: one per slot that
  the path can bind or repeat).

  An entry holds a payload (gint) and its insert number; a node keeps the
  greatest insert number below it, so a walk for the newest match cuts the
  subtrees that hold only older entries.

  The query term is tokenised once; its token array, the subterm of each
  token and the end of each subterm (the skip array) let a walk start at
  any subterm without a new tokenisation.

  The module allocates with sys_malloc and holds no pointers into gkc
  records except the payloads it is given and the constants and variables
  of the stored terms; it never writes a record.

================================================ */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "rincludes.h"

#ifdef USE_TERM_INDEX

/* A token: a record gives its arity n as n<<3 (low bits 000, which no
   constant has: a gint with the low bits 000 is a record); a constant is
   its encoded gint; a variable of a stored term gives its slot number k,
   and these tokens are kept only in the slot list of a node; a variable of
   a query term is its own gint and is never looked up (the walk takes only
   the slot children for it). Which kind a token is follows from its term
   (isvar), so the tokens of the three kinds need no tag bits. */
#define TI_REC(n) (((uint64_t)(n))<<3)
#define TI_SL_LEVELS 12
#define TI_MAXSLOTS 4096

typedef struct {
  uint64_t tok;
  int32_t maxe;             // the greatest entry number below this node, or -1
  int32_t vchild;           // the first slot child, or -1
  int32_t vsib;             // the next slot child of the parent, or -1
  int32_t nk;               // children with a record token or a constant with one encoding
  int32_t k[TI_INLINE];     // the first TI_INLINE of them
  int32_t sl;               // a skip list head (index into sln) when nk>TI_INLINE, else -1
  int32_t entries;          // the first entry (the newest), or -1
} ti_node;

typedef struct {
  uint64_t tok;
  int32_t child;
  int32_t next[TI_SL_LEVELS];
} ti_slnode;

typedef struct {
  gint payload;
  int32_t next;             // the next (older) entry of the same leaf, or -1
  int32_t vars;             // the variables of the stored term in slot order: index into vmap
  int32_t nv;               // their number
} ti_entry;

struct ti_index_s {
  ti_node* nodes; int32_t ncnt, ncap;
  ti_slnode* sln; int32_t slcnt, slcap;
  ti_entry* ents; int32_t ecnt, ecap;
  gint* vmap; int32_t vmcnt, vmcap;
  // the tokens of the last tokenised term
  uint64_t* toks; int32_t* skip; gint* terms; int32_t tcap, tn;
  gint* tvars; int32_t tnv;  // the variables of the last stored term, in slot order
  // the walk
  gint* vals; int32_t nb;    // the slot values on the path
  gint* bestvals; int32_t bestnb, best;
  int (*accept)(glb*, gint);
  int norenaming;
  ti_visit_fn visit; void* visitctx;
  long steps;                // nodes visited by the walks
  uint64_t rnd;
  int failed;
};

static int32_t ti_new_node(ti_index* x, uint64_t tok) {
  int32_t n, i;
  if (x->ncnt>=x->ncap) {
    ti_node* nn=sys_realloc(x->nodes,sizeof(ti_node)*(x->ncap)*2);
    if (!nn) return -1;
    x->nodes=nn; x->ncap*=2;
  }
  n=x->ncnt++;
  x->nodes[n].tok=tok; x->nodes[n].maxe=-1;
  x->nodes[n].vchild=-1; x->nodes[n].vsib=-1; x->nodes[n].nk=0;
  for(i=0;i<TI_INLINE;i++) x->nodes[n].k[i]=-1;
  x->nodes[n].sl=-1; x->nodes[n].entries=-1;
  return n;
}

ti_index* ti_new(void) {
  ti_index* x=sys_malloc(sizeof(ti_index));
  if (!x) return NULL;
  memset(x,0,sizeof(ti_index));
  x->ncap=1<<10; x->slcap=1<<6; x->ecap=1<<9; x->vmcap=1<<9; x->tcap=1<<8;
  x->nodes=sys_malloc(sizeof(ti_node)*x->ncap);
  x->sln=sys_malloc(sizeof(ti_slnode)*x->slcap);
  x->ents=sys_malloc(sizeof(ti_entry)*x->ecap);
  x->vmap=sys_malloc(sizeof(gint)*x->vmcap);
  x->toks=sys_malloc(sizeof(uint64_t)*x->tcap);
  x->skip=sys_malloc(sizeof(int32_t)*x->tcap);
  x->terms=sys_malloc(sizeof(gint)*x->tcap);
  x->tvars=sys_malloc(sizeof(gint)*TI_MAXSLOTS);
  x->vals=sys_malloc(sizeof(gint)*TI_MAXSLOTS);
  x->bestvals=sys_malloc(sizeof(gint)*TI_MAXSLOTS);
  x->rnd=UINT64_C(0x2545F4914F6CDD1D);
  if (!x->nodes || !x->sln || !x->ents || !x->vmap || !x->toks || !x->skip || !x->terms ||
      !x->tvars || !x->vals || !x->bestvals || ti_new_node(x,0)!=0) {
    ti_free(x);
    return NULL;
  }
  return x;
}

void ti_free(ti_index* x) {
  if (!x) return;
  if (x->nodes) sys_free(x->nodes);
  if (x->sln) sys_free(x->sln);
  if (x->ents) sys_free(x->ents);
  if (x->vmap) sys_free(x->vmap);
  if (x->toks) sys_free(x->toks);
  if (x->skip) sys_free(x->skip);
  if (x->terms) sys_free(x->terms);
  if (x->tvars) sys_free(x->tvars);
  if (x->vals) sys_free(x->vals);
  if (x->bestvals) sys_free(x->bestvals);
  sys_free(x);
}

/* ---- the skip list of a node with many children ---- */

static int32_t ti_sl_new(ti_index* x, uint64_t tok, int32_t child) {
  int32_t n, i;
  if (x->slcnt>=x->slcap) {
    ti_slnode* ns=sys_realloc(x->sln,sizeof(ti_slnode)*(x->slcap)*2);
    if (!ns) return -1;
    x->sln=ns; x->slcap*=2;
  }
  n=x->slcnt++;
  x->sln[n].tok=tok; x->sln[n].child=child;
  for(i=0;i<TI_SL_LEVELS;i++) x->sln[n].next[i]=-1;
  return n;
}

static int32_t ti_sl_find(ti_index* x, int32_t head, uint64_t tok) {
  int32_t cur=head, lv, nx;
  for(lv=TI_SL_LEVELS-1; lv>=0; lv--) {
    while((nx=x->sln[cur].next[lv])>=0 && x->sln[nx].tok<tok) cur=nx;
  }
  nx=x->sln[cur].next[0];
  if (nx>=0 && x->sln[nx].tok==tok) return x->sln[nx].child;
  return -1;
}

static int ti_sl_insert(ti_index* x, int32_t head, uint64_t tok, int32_t child) {
  int32_t upd[TI_SL_LEVELS], cur=head, lv, nx, n, h=1;
  for(lv=TI_SL_LEVELS-1; lv>=0; lv--) {
    while((nx=x->sln[cur].next[lv])>=0 && x->sln[nx].tok<tok) cur=nx;
    upd[lv]=cur;
  }
  x->rnd^=x->rnd<<13; x->rnd^=x->rnd>>7; x->rnd^=x->rnd<<17;
  { uint64_t r=x->rnd; while(h<TI_SL_LEVELS && (r&3)==0) { h++; r>>=2; } }
  n=ti_sl_new(x,tok,child);
  if (n<0) return 0;
  for(lv=0;lv<h;lv++) {
    x->sln[n].next[lv]=x->sln[upd[lv]].next[lv];
    x->sln[upd[lv]].next[lv]=n;
  }
  return 1;
}

/* the child of node for a record token or a constant with one encoding, or -1 */

static int32_t ti_child(ti_index* x, int32_t node, uint64_t tok) {
  ti_node* p=&(x->nodes[node]);
  int32_t i, m;
  m=(p->nk<TI_INLINE) ? p->nk : TI_INLINE;
  for(i=0;i<m;i++) if (x->nodes[p->k[i]].tok==tok) return p->k[i];
  if (p->sl>=0) return ti_sl_find(x,p->sl,tok);
  return -1;
}

/* equal terms: a slot binding and a query subterm (records are not shared) */

static int ti_eq(glb* g, gint a, gint b) {
  return (a==b) || wr_equal_term(g,a,b,1);
}

/* the slot child of node for slot k, or -1 */

static int32_t ti_vchild(ti_index* x, int32_t node, uint64_t k) {
  int32_t c;
  for(c=x->nodes[node].vchild; c>=0; c=x->nodes[c].vsib) if (x->nodes[c].tok==k) return c;
  return -1;
}

/* add the child c (with the token tok) to node; returns 0 on a failed allocation */

static int ti_add_child(ti_index* x, int32_t node, int32_t c, uint64_t tok, int slot) {
  int32_t head;
  if (slot) {
    x->nodes[c].vsib=x->nodes[node].vchild;
    x->nodes[node].vchild=c;
    return 1;
  }
  if (x->nodes[node].nk<TI_INLINE) {
    x->nodes[node].k[x->nodes[node].nk]=c;
    (x->nodes[node].nk)++;
    return 1;
  }
  if (x->nodes[node].sl<0) {
    head=ti_sl_new(x,0,-1); // the head holds no token
    if (head<0) return 0;
    x->nodes[node].sl=head;
  }
  if (!ti_sl_insert(x,x->nodes[node].sl,tok,c)) return 0;
  (x->nodes[node].nk)++;
  return 1;
}

/* ---- tokens ---- */

static int ti_grow_tokens(ti_index* x) {
  int32_t nc=x->tcap*2;
  uint64_t* nt; int32_t* ns; gint* nm;
  nt=sys_realloc(x->toks,sizeof(uint64_t)*nc);
  if (!nt) return 0;
  x->toks=nt;
  ns=sys_realloc(x->skip,sizeof(int32_t)*nc);
  if (!ns) return 0;
  x->skip=ns;
  nm=sys_realloc(x->terms,sizeof(gint)*nc);
  if (!nm) return 0;
  x->terms=nm;
  x->tcap=nc;
  return 1;
}

/* stored: 1 for a term to store (variables to slots), 0 for a query term */

static int32_t ti_tokens_aux(glb* g, ti_index* x, gint t, int32_t n, int stored) {
  gptr tp;
  int i, start, end;
  int32_t me, k;
  if (n>=x->tcap && !ti_grow_tokens(x)) return -1;
  me=n;
  x->terms[n]=t;
  if (isvar(t)) {
    if (!stored) { x->toks[n]=(uint64_t)t; x->skip[n]=n+1; return n+1; }
    for(k=0;k<x->tnv;k++) if (x->tvars[k]==t) break;
    if (k==x->tnv) {
      if (k>=TI_MAXSLOTS) return -1;
      x->tvars[(x->tnv)++]=t;
    }
    x->toks[n]=(uint64_t)k;
    x->skip[n]=n+1;
    return n+1;
  }
  if (!isdatarec(t)) {
    // a constant: its encoded gint is the token (the encodings of doubles,
    // full integers and short strings are taken as unique; see the comment
    // at the top of this file)
    x->toks[n]=(uint64_t)t;
    x->skip[n]=n+1;
    return n+1;
  }
  tp=decode_record(g->db,t);
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,tp);
  x->toks[n]=TI_REC(end-start);
  n++;
  for(i=start;i<end;i++) {
    n=ti_tokens_aux(g,x,tp[i],n,stored);
    if (n<0) return -1;
  }
  x->skip[me]=n;
  return n;
}

int32_t ti_tokens(glb* g, ti_index* x, gint term) {
  x->tnv=0;
  x->tn=ti_tokens_aux(g,x,term,0,0);
  return x->tn;
}

int32_t ti_token_end(ti_index* x, int32_t i) { return x->skip[i]; }

int ti_token_is_record(ti_index* x, int32_t i) { return isdatarec(x->terms[i]) ? 1 : 0; }

int ti_token_is_var(ti_index* x, int32_t i) { return isvar(x->terms[i]) ? 1 : 0; }

gint ti_token_term(ti_index* x, int32_t i) { return x->terms[i]; }

/* ---- insert ---- */

int ti_insert(glb* g, ti_index* x, gint term, gint payload) {
  int32_t n, i, cur, nx, e;
  uint64_t t;
  int slot;
  if (x->failed) return 0;
  x->tnv=0;
  n=ti_tokens_aux(g,x,term,0,1);
  if (n<0) { x->failed=1; return 0; }
  cur=0;
  for(i=0;i<n;i++) {
    t=x->toks[i];
    slot=isvar(x->terms[i]);
    nx=slot ? ti_vchild(x,cur,t) : ti_child(x,cur,t);
    if (nx<0) {
      nx=ti_new_node(x,t);
      if (nx<0 || !ti_add_child(x,cur,nx,t,slot)) { x->failed=1; return 0; }
    }
    cur=nx;
  }
  if (x->ecnt>=x->ecap) {
    ti_entry* ne=sys_realloc(x->ents,sizeof(ti_entry)*(x->ecap)*2);
    if (!ne) { x->failed=1; return 0; }
    x->ents=ne; x->ecap*=2;
  }
  while (x->vmcnt+x->tnv>x->vmcap) {
    gint* nv=sys_realloc(x->vmap,sizeof(gint)*(x->vmcap)*2);
    if (!nv) { x->failed=1; return 0; }
    x->vmap=nv; x->vmcap*=2;
  }
  e=x->ecnt++;
  x->ents[e].payload=payload;
  x->ents[e].vars=x->vmcnt;
  x->ents[e].nv=x->tnv;
  for(i=0;i<x->tnv;i++) x->vmap[(x->vmcnt)++]=x->tvars[i];
  x->ents[e].next=x->nodes[cur].entries;
  x->nodes[cur].entries=e;
  // the greatest entry number below each node of the path
  cur=0;
  x->nodes[0].maxe=e;
  for(i=0;i<n;i++) {
    t=x->toks[i];
    cur=isvar(x->terms[i]) ? ti_vchild(x,cur,t) : ti_child(x,cur,t);
    x->nodes[cur].maxe=e;
  }
  return 1;
}

/* ---- the walk ---- */

/* are the slot values a renaming: all variables, no two equal */

static int ti_is_renaming(ti_index* x) {
  int32_t i, j;
  for(i=0;i<x->nb;i++) {
    if (!isvar(x->vals[i])) return 0;
    for(j=0;j<i;j++) if (x->vals[j]==x->vals[i]) return 0;
  }
  return 1;
}

static void ti_leaf(glb* g, ti_index* x, int32_t node) {
  int32_t e;
  for(e=x->nodes[node].entries; e>=0; e=x->ents[e].next) {
    if (x->visit) {
      (x->visit)(g,x->visitctx,x->ents[e].payload,x->nb,x->vals,x->vmap+x->ents[e].vars);
      continue;
    }
    if (e<=x->best) return; // the entries of a leaf are newest first
    if (x->norenaming && ti_is_renaming(x)) continue;
    if (x->accept && !(x->accept)(g,x->ents[e].payload)) continue;
    x->best=e;
    memcpy(x->bestvals,x->vals,sizeof(gint)*(x->nb));
    x->bestnb=x->nb;
    return;
  }
}

static void ti_walk(glb* g, ti_index* x, int32_t node, int32_t i, int32_t end) {
  int32_t c, k;
  uint64_t t;
  gint q;
  (x->steps)++;
  if (!(x->visit) && x->nodes[node].maxe<=x->best) return;
  if (i>=end) { ti_leaf(g,x,node); return; }
  t=x->toks[i];
  q=x->terms[i];
  if (!isvar(q)) {
    // a record or a constant of the query: the child with the same token
    c=ti_child(x,node,t);
    if (c>=0) ti_walk(g,x,c,i+1,end);
  }
  for(c=x->nodes[node].vchild; c>=0; c=x->nodes[c].vsib) {
    k=(int32_t)(x->nodes[c].tok);
    if (k==x->nb) {
      // the first occurrence of slot k on the path: bind it
      x->vals[k]=q;
      (x->nb)++;
      ti_walk(g,x,c,x->skip[i],end);
      (x->nb)--; // clear the binding
    } else if (k<x->nb && ti_eq(g,x->vals[k],q)) {
      // a later occurrence: the bound term must be equal
      ti_walk(g,x,c,x->skip[i],end);
    }
  }
}

/* the newest stored term that matches the query subterm at tokens
   [start,end) of the last ti_tokens and passes the tests: accept (when not
   NULL) and, with norenaming, a substitution that is not a renaming.
   Returns 1 and sets *payload, *nv, *vars (the variables of the stored
   term in slot order) and *vals (their values), valid until the next
   call; 0 when there is none. */

int ti_match_best(glb* g, ti_index* x, int32_t start, int32_t end,
                  int (*accept)(glb*, gint), int norenaming,
                  gint* payload, int32_t* nv, gint** vars, gint** vals) {
  if (x->ecnt==0) return 0;
  x->nb=0; x->best=-1; x->bestnb=0;
  x->accept=accept; x->norenaming=norenaming; x->visit=NULL;
  ti_walk(g,x,0,start,end);
  if (x->best<0) return 0;
  *payload=x->ents[x->best].payload;
  *nv=x->ents[x->best].nv;
  *vars=x->vmap+x->ents[x->best].vars;
  *vals=x->bestvals;
  return 1;
}

/* every stored term that matches the query subterm at tokens [start,end):
   visit is called with each payload, the number of slots, their values
   and the variables of the stored term in slot order */

void ti_match_all(glb* g, ti_index* x, int32_t start, int32_t end, ti_visit_fn visit, void* ctx) {
  x->nb=0; x->best=-1;
  x->visit=visit; x->visitctx=ctx;
  ti_walk(g,x,0,start,end);
  x->visit=NULL;
}

/* 0: the tree works; 1: a failed allocation or too many variables */

int ti_failed(ti_index* x) { return x->failed; }

/* ---- the self test (strategy key ti_selftest) ----

  The input clauses give the test: every positive unit equation l=r is a
  stored term l with the right side r, in input order (the newest is the
  last one); every unit clause ti_query(T,E) asks for the rewrite of T by
  the newest stored term that matches it, with the expected result E (the
  constant none: no match). The test also compares, for every subterm of
  every atom of the input, the matches of the tree with a plain match
  (wr_match_term) against every stored term: the set of matches, the
  substitutions, and the newest match under a filter that rejects every
  third stored term, with and without the renaming condition. It prints
  one line per query and a final line "TI_TEST result: ...". */

typedef struct {
  gint* lhs; gint* rhs; int n, cap;
  int* found; gint** fvals; int nfound, fcap; // the matches of ti_match_all: index, a copy of the values per variable
  long brute, diffs;
} ti_test;

#define TI_TEST_DIFF(t) { ((t)->diffs)++; wr_printf("TI_TEST difference: "); }

static int ti_test_accept(glb* g, gint payload) {
  UNUSED(g);
  return (payload%3)!=2;
}

static void ti_test_visit(glb* g, void* ctx, gint payload, int32_t nv, gint* vals, gint* vars) {
  ti_test* t=(ti_test*)ctx;
  int32_t k;
  UNUSED(g); UNUSED(vars);
  if (t->nfound>=t->fcap) {
    t->fcap=(t->fcap)*2+16;
    t->found=sys_realloc(t->found,sizeof(int)*t->fcap);
    t->fvals=sys_realloc(t->fvals,sizeof(gint*)*t->fcap);
  }
  t->found[t->nfound]=(int)payload;
  t->fvals[t->nfound]=sys_malloc(sizeof(gint)*(nv+1));
  for(k=0;k<nv;k++) t->fvals[t->nfound][k]=vals[k];
  (t->nfound)++;
}

/* the variables of a term in the order of their first occurrence */

static int ti_test_vars(glb* g, gint t, gint* vars, int n) {
  gptr tp; int i, k, start, end;
  if (isvar(t)) {
    for(k=0;k<n;k++) if (vars[k]==t) return n;
    vars[n]=t;
    return n+1;
  }
  if (!isdatarec(t)) return n;
  tp=decode_record(g->db,t);
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,tp);
  for(i=start;i<end;i++) n=ti_test_vars(g,tp[i],vars,n);
  return n;
}

/* is the right side r with the slot values equal to e */

static int ti_test_inst_eq(glb* g, gint r, gint e, int32_t nv, gint* vars, gint* vals) {
  gptr rp, ep; int i, start, rend, eend; int32_t k;
  if (isvar(r)) {
    for(k=0;k<nv;k++) if (vars[k]==r) return ti_eq(g,vals[k],e);
    return r==e;
  }
  if (!isdatarec(r)) return ti_eq(g,r,e);
  if (!isdatarec(e)) return 0;
  rp=decode_record(g->db,r); ep=decode_record(g->db,e);
  start=wr_term_unify_startpos(g);
  rend=wr_term_unify_endpos(g,rp); eend=wr_term_unify_endpos(g,ep);
  if (rend!=eend) return 0;
  for(i=start;i<rend;i++) if (!ti_test_inst_eq(g,rp[i],ep[i],nv,vars,vals)) return 0;
  return 1;
}

/* compare the tree and the plain match on the query subterm q at the
   tokens [i,end) of the last ti_tokens */

static void ti_test_compare(glb* g, ti_index* x, ti_test* t, gint q, int32_t i, int32_t end) {
  gint vars[TI_MAXSLOTS], bvals[TI_MAXSLOTS];
  int j, f, k, nv, ok, mode, bf;
  gint payload, *tvars, *tvals;
  int32_t tnv;
  // all matches
  t->nfound=0;
  ti_match_all(g,x,i,end,ti_test_visit,t);
  for(j=t->n-1;j>=0;j--) {
    wr_clear_varstack(g,g->varstack);
    ok=wr_match_term(g,t->lhs[j],q,1);
    (t->brute)++;
    for(f=0;f<t->nfound;f++) if (t->found[f]==j) break;
    if (!ok) {
      if (f<t->nfound) {
        TI_TEST_DIFF(t);
        wr_printf("the tree matches stored term %d, the plain match does not, on ",j);
        wr_print_term(g,q); wr_printf("\n");
      }
      continue;
    }
    if (f==t->nfound) {
      TI_TEST_DIFF(t);
      wr_printf("the plain match matches stored term %d, the tree does not, on ",j);
      wr_print_term(g,q); wr_printf("\n");
      continue;
    }
    nv=ti_test_vars(g,t->lhs[j],vars,0);
    for(k=0;k<nv;k++) {
      if (!ti_eq(g,VARVAL_DIRECT(vars[k],(g->varbanks)),t->fvals[f][k])) {
        TI_TEST_DIFF(t);
        wr_printf("substitution of stored term %d, variable %d, on ",j,k);
        wr_print_term(g,q); wr_printf("\n");
        break;
      }
    }
  }
  wr_clear_varstack(g,g->varstack);
  for(f=0;f<t->nfound;f++) sys_free(t->fvals[f]);
  // the newest match under the filter, without and with the renaming condition
  for(mode=0;mode<2;mode++) {
    bf=-1;
    for(j=t->n-1;j>=0;j--) {
      if (!ti_test_accept(g,j)) continue;
      wr_clear_varstack(g,g->varstack);
      ok=wr_match_term(g,t->lhs[j],q,1);
      if (ok && mode && wr_match_is_renaming(g)) ok=0;
      if (ok) {
        bf=j;
        nv=ti_test_vars(g,t->lhs[j],vars,0);
        for(k=0;k<nv;k++) bvals[k]=VARVAL_DIRECT(vars[k],(g->varbanks));
        break;
      }
    }
    wr_clear_varstack(g,g->varstack);
    (t->brute)++;
    if (!ti_match_best(g,x,i,end,ti_test_accept,mode,&payload,&tnv,&tvars,&tvals)) payload=-1;
    if ((int)payload!=bf) {
      TI_TEST_DIFF(t);
      wr_printf("newest match (renaming condition %d) tree %d plain %d on ",mode,(int)payload,bf);
      wr_print_term(g,q); wr_printf("\n");
    } else if (bf>=0) {
      for(k=0;k<tnv;k++) {
        if (tvars[k]!=vars[k] || !ti_eq(g,tvals[k],bvals[k])) {
          TI_TEST_DIFF(t);
          wr_printf("newest match substitution, variable %d, on ",k);
          wr_print_term(g,q); wr_printf("\n");
          break;
        }
      }
    }
  }
}

/* compare on every subterm of the term a */

static void ti_test_subterms(glb* g, ti_index* x, ti_test* t, gint a) {
  int32_t n, i;
  gint* subs; int32_t* ends;
  n=ti_tokens(g,x,a);
  if (n<=0) return;
  // ti_tokens is overwritten by nothing in the compare, but keep a copy of the slices
  subs=sys_malloc(sizeof(gint)*n); ends=sys_malloc(sizeof(int32_t)*n);
  for(i=0;i<n;i++) { subs[i]=ti_token_term(x,i); ends[i]=ti_token_end(x,i); }
  for(i=0;i<n;i++) ti_test_compare(g,x,t,subs[i],i,ends[i]);
  sys_free(subs); sys_free(ends);
}

void wr_ti_selftest(glb* g) {
  void* db=g->db;
  ti_index* x;
  ti_test t;
  gint cell, atom, meta, q, e, payload, *tvars, *tvals;
  gcell* cellptr;
  gptr rec, ap;
  int pass, i, nat, queries=0, wrong=0, inserted=0, ncl=0, cli;
  gptr* cls;
  int32_t n, tnv;
  char* str;
  ti_stat st;
  memset(&t,0,sizeof(t));
  t.cap=1024;
  t.lhs=sys_malloc(sizeof(gint)*t.cap); t.rhs=sys_malloc(sizeof(gint)*t.cap);
  x=ti_new();
  if (!x || !t.lhs || !t.rhs) { wr_printf("TI_TEST result: no memory\n"); return; }
  // the clause list holds the newest clause first: take the clauses in input order
  for(cell=(dbmemsegh(db)->clauselist); cell; cell=cellptr->cdr) { cellptr=(gcell*)offsettoptr(db,cell); ncl++; }
  cls=sys_malloc(sizeof(gptr)*(ncl+1));
  if (!cls) { wr_printf("TI_TEST result: no memory\n"); return; }
  cli=ncl;
  for(cell=(dbmemsegh(db)->clauselist); cell; cell=cellptr->cdr) {
    cellptr=(gcell*)offsettoptr(db,cell);
    cls[--cli]=offsettoptr(db,cellptr->car);
  }
  // pass 0: the stored terms; pass 1: the queries; pass 2: every subterm
  for(pass=0;pass<3;pass++) {
    for(cli=0;cli<ncl;cli++) {
      rec=cls[cli];
      nat=wg_rec_is_rule_clause(db,rec) ? wg_count_clause_atoms(db,rec) : 1;
      for(i=0;i<nat;i++) {
        if (wg_rec_is_rule_clause(db,rec)) {
          atom=wg_get_rule_clause_atom(db,rec,i);
          meta=wg_get_rule_clause_atom_meta(db,rec,i);
          if (wg_atom_meta_is_neg(db,meta)) { if (pass==2) ti_test_subterms(g,x,&t,atom); continue; }
        } else {
          atom=encode_record(db,rec);
        }
        if (pass==2) { ti_test_subterms(g,x,&t,atom); continue; }
        if (nat!=1 || !isdatarec(atom)) continue;
        ap=decode_record(db,atom);
        if (pass==0 && wr_equality_atom(g,atom)) {
          if (t.n>=t.cap) {
            t.cap*=2;
            t.lhs=sys_realloc(t.lhs,sizeof(gint)*t.cap); t.rhs=sys_realloc(t.rhs,sizeof(gint)*t.cap);
          }
          t.lhs[t.n]=ap[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
          t.rhs[t.n]=ap[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];
          if (!ti_insert(g,x,t.lhs[t.n],t.n)) {
            wr_printf("TI_TEST insert failed for stored term %d\n",t.n);
          } else {
            inserted++;
          }
          (t.n)++;
          continue;
        }
        if (pass==1 && wg_get_encoded_type(db,ap[RECORD_HEADER_GINTS+(g->unify_funpos)])==WG_URITYPE) {
          str=wg_decode_uri(db,ap[RECORD_HEADER_GINTS+(g->unify_funpos)]);
          if (strcmp(str,"ti_query")) continue;
          q=ap[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
          e=ap[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];
          queries++;
          n=ti_tokens(g,x,q);
          wr_printf("TI_TEST query %d: ",queries);
          wr_print_term(g,q);
          if (n>0 && ti_match_best(g,x,0,n,NULL,0,&payload,&tnv,&tvars,&tvals)) {
            wr_printf(" -> stored term %d: ",(int)payload);
            wr_print_term(g,t.rhs[payload]);
            if (ti_test_inst_eq(g,t.rhs[payload],e,tnv,tvars,tvals)) {
              wr_printf(" ok\n");
            } else {
              wr_printf(" WRONG, expected ");
              wr_print_term(g,e);
              wr_printf("\n");
              wrong++;
            }
          } else {
            str=(wg_get_encoded_type(db,e)==WG_URITYPE) ? wg_decode_uri(db,e) : NULL;
            if (str && !strcmp(str,"none")) {
              wr_printf(" -> none ok\n");
            } else {
              wr_printf(" -> none WRONG, expected ");
              wr_print_term(g,e);
              wr_printf("\n");
              wrong++;
            }
          }
        }
      }
    }
  }
  ti_stats(x,&st);
  wr_printf("TI_TEST tree: stored %d nodes %ld skip_lists %ld max_children %ld max_slot_children %ld\n",
    inserted,st.nodes,st.skiplists,st.maxchildren,st.maxvchildren);
  wr_printf("TI_TEST result: queries %d wrong %d plain_matches %ld differences %ld\n",
    queries,wrong,t.brute,t.diffs);
  ti_free(x);
  sys_free(cls);
  sys_free(t.lhs); sys_free(t.rhs);
  if (t.found) sys_free(t.found);
  if (t.fvals) sys_free(t.fvals);
}

/* ---- the size, for the reports ---- */

void ti_stats(ti_index* x, ti_stat* s) {
  int32_t i, c, n;
  memset(s,0,sizeof(ti_stat));
  for(i=0;i<x->ncnt;i++) {
    n=0;
    for(c=x->nodes[i].vchild; c>=0; c=x->nodes[c].vsib) n++;
    if (n>s->maxvchildren) s->maxvchildren=n;
    if (n>1) (s->nodes_vchildren_gt1)++;
    if (x->nodes[i].nk>s->maxchildren) s->maxchildren=x->nodes[i].nk;
    if (x->nodes[i].sl>=0) (s->skiplists)++;
  }
  s->nodes=x->ncnt; s->entries=x->ecnt; s->steps=x->steps;
  s->bytes=(long)(sizeof(ti_node)*x->ncap+sizeof(ti_slnode)*x->slcap+sizeof(ti_entry)*x->ecap+
                  sizeof(gint)*x->vmcap);
}

#else

void wr_ti_selftest(glb* g) {
  UNUSED(g);
  wr_printf("TI_TEST result: no tree in this build\n");
}

#endif
