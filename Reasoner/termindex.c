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
    path. Slot k is variable k of the variable bank of the tree (cells
    given to ti_new), a bank that no term given to the tree uses; the
    caller renames the other terms of an entry (a right side) to that bank
    by slot number (ti_last_vars, ti_rename_copy).

  The tree is a full tree (every token is a node, no cut at a depth). As in
  E's perfect discrimination tree (CLAUSES/ccl_pdtrees.c), the walk over a
  query term binds and clears the variables of the tree: at the first
  occurrence of slot k on the path the walk binds slot k to the query
  subterm; at a later occurrence it compares the bound term with the query
  subterm (wr_equal_term) and goes on only if they are equal; on
  backtracking the binding is cleared. A leaf reached by the walk is a
  match, and the slot values are its substitution. The walk binds the cell
  of a slot with SETVAR, so the variable stack registers it, and on
  backtracking clears the top slice of the stack back to the position noted
  at that node (wr_clear_varstack_topslice), as the subsumption matching of
  wr_subsume_cl_aux (subsume.c) does; the bindings below, such as the
  unifier of the parent clauses, stay untouched. The walk for the newest
  match goes on after a match: it keeps a copy of the slot values of the
  best match, and after the walk, when all its own bindings are cleared,
  the caller sets them again with SETVAR and clears them the same way after
  the build. A variable of the query term (a constant for the walk) and a
  variable of the tree are always two different cells.

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
#define TI_MAXSLOTS NROF_VARSINBANK // a slot is a variable of the tree bank

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
  gint aux;                 // a second payload of the caller (the demodulators: the right side)
} ti_entry;

struct ti_index_s {
  ti_node* nodes; int32_t ncnt, ncap;
  ti_slnode* sln; int32_t slcnt, slcap;
  ti_entry* ents; int32_t ecnt, ecap;
  // the tokens of the last tokenised term
  uint64_t* toks; int32_t* skip; gint* terms; int32_t tcap, tn;
  gint* tvars; int32_t tnv;  // the variables of the last stored term, in slot order
  long qtreevars;            // variables of the tree bank found in a query term (a defect: expected 0)
  // the walk
  gptr vals; int32_t nb;     // the cells of the tree bank: slot k is variable k of the bank
  gint* bestvals; int32_t bestnb, best;
  int (*accept)(glb*, gint);
  int norenaming;
  int first, stop;           // first: the walk stops at the first accepted entry (stop set)
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

/* cells: the first cell of the variable bank of the tree (its cells are
   UNASSIGNED between walks) */

ti_index* ti_new(gptr cells) {
  ti_index* x=sys_malloc(sizeof(ti_index));
  if (!x) return NULL;
  memset(x,0,sizeof(ti_index));
  x->ncap=1<<10; x->slcap=1<<6; x->ecap=1<<9; x->tcap=1<<8;
  x->nodes=sys_malloc(sizeof(ti_node)*x->ncap);
  x->sln=sys_malloc(sizeof(ti_slnode)*x->slcap);
  x->ents=sys_malloc(sizeof(ti_entry)*x->ecap);
  x->toks=sys_malloc(sizeof(uint64_t)*x->tcap);
  x->skip=sys_malloc(sizeof(int32_t)*x->tcap);
  x->terms=sys_malloc(sizeof(gint)*x->tcap);
  x->tvars=sys_malloc(sizeof(gint)*TI_MAXSLOTS);
  x->vals=cells;
  x->bestvals=sys_malloc(sizeof(gint)*TI_MAXSLOTS);
  x->rnd=UINT64_C(0x2545F4914F6CDD1D);
  if (!x->nodes || !x->sln || !x->ents || !x->toks || !x->skip || !x->terms ||
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
  if (x->toks) sys_free(x->toks);
  if (x->skip) sys_free(x->skip);
  if (x->terms) sys_free(x->terms);
  if (x->tvars) sys_free(x->tvars);
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
    if (!stored) {
      if ((g->varbanks)+decode_var(t)>=x->vals && (g->varbanks)+decode_var(t)<x->vals+TI_MAXSLOTS) (x->qtreevars)++;
      x->toks[n]=(uint64_t)t; x->skip[n]=n+1; return n+1;
    }
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

int ti_insert(glb* g, ti_index* x, gint term, gint payload, gint aux) {
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
  e=x->ecnt++;
  x->ents[e].payload=payload;
  x->ents[e].aux=aux;
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
      (x->visit)(g,x->visitctx,x->ents[e].payload,x->nb,x->vals);
      continue;
    }
    if (!(x->first) && e<=x->best) return; // the entries of a leaf are newest first
    if (x->norenaming && ti_is_renaming(x)) continue;
    if (x->accept && !(x->accept)(g,x->ents[e].payload)) continue;
    x->best=e;
    memcpy(x->bestvals,x->vals,sizeof(gint)*(x->nb));
    x->bestnb=x->nb;
    if (x->first) x->stop=1;
    return;
  }
}

static void ti_walk(glb* g, ti_index* x, int32_t node, int32_t i, int32_t end) {
  int32_t c, k;
  uint64_t t;
  gint q;
  (x->steps)++;
  if (x->stop) return;
  if (!(x->visit) && !(x->first) && x->nodes[node].maxe<=x->best) return;
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
      // the first occurrence of slot k on the path: bind its cell with
      // SETVAR, so the variable stack registers it, and on backtracking
      // clear the top slice back to the position noted here (the pattern
      // of wr_subsume_cl_aux in subsume.c)
      gint vc_tmp=(g->varstack)[1];
      gptr vc=((gptr)(g->varstack))+1;
      SETVAR(encode_var((x->vals)-(g->varbanks)+k),q,g->varbanks,g->varstack,vc);
      (x->nb)++;
      ti_walk(g,x,c,x->skip[i],end);
      (x->nb)--;
      if (vc_tmp!=(g->varstack)[1]) wr_clear_varstack_topslice(g,g->varstack,vc_tmp);
      if (x->stop) return;
    } else if (k<x->nb && ti_eq(g,x->vals[k],q)) {
      // a later occurrence: the bound term must be equal
      ti_walk(g,x,c,x->skip[i],end);
    }
  }
}

/* the newest stored term that matches the query subterm at tokens
   [start,end) of the last ti_tokens and passes the tests: accept (when not
   NULL) and, with norenaming, a substitution that is not a renaming.
   Returns 1 and sets *payload, *aux, *nv (the number of slots) and *vals
   (the values of the slots 0..nv-1, valid until the next call); 0 when
   there is none. The cells of the tree bank are UNASSIGNED after the walk;
   the caller binds them from *vals. */

int ti_match_best(glb* g, ti_index* x, int32_t start, int32_t end,
                  int (*accept)(glb*, gint), int norenaming,
                  gint* payload, gint* aux, int32_t* nv, gint** vals) {
  if (x->ecnt==0) return 0;
  x->nb=0; x->best=-1; x->bestnb=0;
  x->accept=accept; x->norenaming=norenaming; x->visit=NULL;
  x->first=0; x->stop=0;
  ti_walk(g,x,0,start,end);
  if (x->best<0) return 0;
  *payload=x->ents[x->best].payload;
  *aux=x->ents[x->best].aux;
  *nv=x->bestnb;
  *vals=x->bestvals;
  return 1;
}

/* any stored term that matches the query subterm at tokens [start,end)
   and passes accept (when not NULL): the walk stops at the first leaf with
   an accepted entry, in the walk order (at a node the child of the query
   token first, then the slot children from the newest), so the answer is
   fixed for a given tree. Returns 1 and sets *payload, or 0. The cells of
   the tree bank are UNASSIGNED after the walk. */

int ti_match_first(glb* g, ti_index* x, int32_t start, int32_t end,
                   int (*accept)(glb*, gint), gint* payload) {
  if (x->ecnt==0) return 0;
  x->nb=0; x->best=-1; x->bestnb=0;
  x->accept=accept; x->norenaming=0; x->visit=NULL;
  x->first=1; x->stop=0;
  ti_walk(g,x,0,start,end);
  x->first=0; x->stop=0;
  if (x->best<0) return 0;
  *payload=x->ents[x->best].payload;
  return 1;
}

/* the token arrays of the last ti_tokens */

void ti_token_arrays(ti_index* x, uint64_t** toks, int32_t** skip, gint** terms) {
  *toks=x->toks; *skip=x->skip; *terms=x->terms;
}

/* ti_match_best on token arrays of the caller (a copy of the arrays of an
   earlier ti_tokens), so the caller keeps the tokens of a term while other
   lookups tokenise other terms */

int ti_match_best_ext(glb* g, ti_index* x, uint64_t* toks, int32_t* skip, gint* terms,
                      int32_t start, int32_t end, int (*accept)(glb*, gint), int norenaming,
                      gint* payload, gint* aux, int32_t* nv, gint** vals) {
  uint64_t* st=x->toks; int32_t* ss=x->skip; gint* sm=x->terms;
  int r;
  x->toks=toks; x->skip=skip; x->terms=terms;
  r=ti_match_best(g,x,start,end,accept,norenaming,payload,aux,nv,vals);
  x->toks=st; x->skip=ss; x->terms=sm;
  return r;
}

/* the variables of term in slot order, as ti_insert numbers them; valid
   until the next call of the module; -1 on a failed allocation */

int32_t ti_term_vars(glb* g, ti_index* x, gint term, gint** vars) {
  x->tnv=0;
  if (ti_tokens_aux(g,x,term,0,1)<0) return -1;
  *vars=x->tvars;
  return x->tnv;
}

/* the variables of the last stored term (ti_insert) in slot order */

int32_t ti_last_vars(ti_index* x, gint** vars) {
  *vars=x->tvars;
  return x->tnv;
}

long ti_query_tree_vars(ti_index* x) { return x->qtreevars; }

/* a copy of the term t into buf (a cvec of the reasoner) with the variable
   vars[k] renamed to the variable first+k (first: the number of the first
   variable of the tree bank); returns WG_ILLEGAL when t has a variable
   that is not in vars (or when buf is full) */

gint ti_rename_copy(glb* g, gint t, gint* vars, int32_t nv, gint first, gptr buf) {
  gptr xptr, yptr;
  gint xlen, tmp;
  int i;
  int32_t k;
  if (isvar(t)) {
    for(k=0;k<nv;k++) if (vars[k]==t) return encode_var(first+k);
    return WG_ILLEGAL;
  }
  if (!isdatarec(t)) return t;
  xptr=decode_record(g->db,t);
  xlen=get_record_len(xptr);
  yptr=wr_alloc_from_cvec(g,buf,(RECORD_HEADER_GINTS+xlen));
  if (yptr==NULL) return WG_ILLEGAL;
  for(i=0;i<RECORD_HEADER_GINTS+(g->unify_firstuseterm);i++) yptr[i]=xptr[i];
  for(;i<RECORD_HEADER_GINTS+xlen;i++) {
    tmp=ti_rename_copy(g,xptr[i],vars,nv,first,buf);
    if (tmp==WG_ILLEGAL) return WG_ILLEGAL;
    yptr[i]=tmp;
  }
  return encode_record(g->db,yptr);
}

/* every stored term that matches the query subterm at tokens [start,end):
   visit is called with each payload, the number of slots and their values
   (the cells of the tree bank, bound at that moment) */

void ti_match_all(glb* g, ti_index* x, int32_t start, int32_t end, ti_visit_fn visit, void* ctx) {
  x->nb=0; x->best=-1; x->first=0; x->stop=0;
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
  gint* lhs; gint* rhs; gint* rhsc; int n, cap; // rhsc: the right side renamed to the tree bank
  int* found; gint** fvals; int nfound, fcap; // the matches of ti_match_all: index, a copy of the values per variable
  long brute, diffs;
} ti_test;

#define TI_TEST_DIFF(t) { ((t)->diffs)++; wr_printf("TI_TEST difference: "); }

static int ti_test_accept(glb* g, gint payload) {
  UNUSED(g);
  return (payload%3)!=2;
}

static void ti_test_visit(glb* g, void* ctx, gint payload, int32_t nv, gint* vals) {
  ti_test* t=(ti_test*)ctx;
  int32_t k;
  UNUSED(g);
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

/* the right side rc (renamed to the tree bank) built with the slot values
   as the rewriting builds it: the bindings set on the cells of the tree
   bank with SETVAR, wr_build_calc_term with build_subst 1, then the stack
   cleared */

static gint ti_test_build_rhs(glb* g, gint rc, int32_t nv, gint* vals) {
  gptr vc=((gptr)(g->varstack))+1;
  gint res;
  int32_t k;
  int s_subst=(g->build_subst), s_rename=(g->build_rename), s_rewrite=(g->build_rewrite);
  gptr s_buffer=(g->build_buffer);
  wr_clear_varstack(g,g->varstack);
  for(k=0;k<nv;k++) SETVAR(encode_var(TREE_VARBANK*NROF_VARSINBANK+k),vals[k],g->varbanks,g->varstack,vc);
  (g->build_subst)=1; (g->build_rename)=0; (g->build_rewrite)=0; (g->build_buffer)=(g->queue_termbuf);
  res=wr_build_calc_term(g,rc);
  (g->build_subst)=s_subst; (g->build_rename)=s_rename; (g->build_rewrite)=s_rewrite; (g->build_buffer)=s_buffer;
  wr_clear_varstack(g,g->varstack);
  return res;
}

/* compare the tree and the plain match on the query subterm q at the
   tokens [i,end) of the last ti_tokens */

static void ti_test_compare(glb* g, ti_index* x, ti_test* t, gint q, int32_t i, int32_t end) {
  gint vars[TI_MAXSLOTS], bvals[TI_MAXSLOTS];
  int j, f, k, nv, ok, mode, bf;
  gint payload, aux, *tvals;
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
    if (!ti_match_best(g,x,i,end,ti_test_accept,mode,&payload,&aux,&tnv,&tvals)) payload=-1;
    if ((int)payload!=bf) {
      TI_TEST_DIFF(t);
      wr_printf("newest match (renaming condition %d) tree %d plain %d on ",mode,(int)payload,bf);
      wr_print_term(g,q); wr_printf("\n");
    } else if (bf>=0) {
      for(k=0;k<tnv;k++) {
        if (tnv!=nv || !ti_eq(g,tvals[k],bvals[k])) {
          TI_TEST_DIFF(t);
          wr_printf("newest match substitution, variable %d, on ",k);
          wr_print_term(g,q); wr_printf("\n");
          break;
        }
      }
    }
  }
}

/* mode 2: store the atom a (no right side) */

static void ti_test_store_atom(glb* g, ti_index* x, ti_test* t, gint a, int* inserted) {
  if (t->n>=t->cap) {
    t->cap*=2;
    t->lhs=sys_realloc(t->lhs,sizeof(gint)*t->cap); t->rhs=sys_realloc(t->rhs,sizeof(gint)*t->cap);
    t->rhsc=sys_realloc(t->rhsc,sizeof(gint)*t->cap);
  }
  t->lhs[t->n]=a; t->rhs[t->n]=0; t->rhsc[t->n]=0;
  if (!ti_insert(g,x,a,t->n,0)) {
    wr_printf("TI_TEST insert failed for stored term %d\n",t->n);
  } else {
    (*inserted)++;
  }
  (t->n)++;
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
  gint cell, atom, meta, q, e, payload, aux, *tvals, lvars[TI_MAXSLOTS];
  int lnv;
  long notfiled=0;
  gcell* cellptr;
  gptr rec, ap;
  int pass, i, nat, queries=0, wrong=0, inserted=0, ncl=0, cli;
  gptr* cls;
  int32_t n, tnv;
  char* str;
  ti_stat st;
  memset(&t,0,sizeof(t));
  t.cap=1024;
  t.lhs=sys_malloc(sizeof(gint)*t.cap); t.rhs=sys_malloc(sizeof(gint)*t.cap); t.rhsc=sys_malloc(sizeof(gint)*t.cap);
  x=ti_new((g->varbanks)+TREE_VARBANK*NROF_VARSINBANK);
  if (!x || !t.lhs || !t.rhs || !t.rhsc) { wr_printf("TI_TEST result: no memory\n"); return; }
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
          if (wg_atom_meta_is_neg(db,meta)) {
            if (pass==2) ti_test_subterms(g,x,&t,atom);
            else if (pass==0 && (g->ti_selftest)==2 && nat==1) ti_test_store_atom(g,x,&t,atom,&inserted);
            continue;
          }
        } else {
          atom=encode_record(db,rec);
        }
        if (pass==2) { ti_test_subterms(g,x,&t,atom); continue; }
        if (nat!=1 || !isdatarec(atom)) continue;
        ap=decode_record(db,atom);
        if (pass==0 && (g->ti_selftest)==2) {
          // mode 2: the atoms of the unit clauses (not the queries) are the stored terms
          if (wg_get_encoded_type(db,ap[RECORD_HEADER_GINTS+(g->unify_funpos)])!=WG_URITYPE ||
              strcmp(wg_decode_uri(db,ap[RECORD_HEADER_GINTS+(g->unify_funpos)]),"ti_query"))
            ti_test_store_atom(g,x,&t,atom,&inserted);
          continue;
        }
        if (pass==0 && wr_equality_atom(g,atom)) {
          if (t.n>=t.cap) {
            t.cap*=2;
            t.lhs=sys_realloc(t.lhs,sizeof(gint)*t.cap); t.rhs=sys_realloc(t.rhs,sizeof(gint)*t.cap);
            t.rhsc=sys_realloc(t.rhsc,sizeof(gint)*t.cap);
          }
          t.lhs[t.n]=ap[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
          t.rhs[t.n]=ap[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];
          // the right side renamed to the tree bank by the slot order of the left side
          lnv=ti_test_vars(g,t.lhs[t.n],lvars,0);
          t.rhsc[t.n]=ti_rename_copy(g,t.rhs[t.n],lvars,lnv,TREE_VARBANK*NROF_VARSINBANK,g->queue_termbuf);
          if (t.rhsc[t.n]==WG_ILLEGAL) {
            notfiled++; // a variable of the right side is not in the left side
            wr_printf("TI_TEST stored term %d not filed: a variable of the right side is not in the left side\n",t.n);
          } else if (!ti_insert(g,x,t.lhs[t.n],t.n,t.rhsc[t.n])) {
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
          if ((g->ti_selftest)==2) {
            // mode 2: any stored atom that matches (the first in the walk order); e is yes or none
            int found=(n>0 && ti_match_first(g,x,0,n,NULL,&payload));
            str=(wg_get_encoded_type(db,e)==WG_URITYPE) ? wg_decode_uri(db,e) : NULL;
            if (found) { wr_printf(" -> stored term %d: ",(int)payload); wr_print_term(g,t.lhs[payload]); }
            else wr_printf(" -> none");
            if (str && ((found && !strcmp(str,"yes")) || (!found && !strcmp(str,"none")))) {
              wr_printf(" ok\n");
            } else {
              wr_printf(" WRONG, expected ");
              wr_print_term(g,e);
              wr_printf("\n");
              wrong++;
            }
            continue;
          }
          if (n>0 && ti_match_best(g,x,0,n,NULL,0,&payload,&aux,&tnv,&tvals)) {
            wr_printf(" -> stored term %d: ",(int)payload);
            wr_print_term(g,t.rhs[payload]);
            if (wr_equal_term(g,ti_test_build_rhs(g,aux,tnv,tvals),e,1)) {
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
  wr_printf("TI_TEST result: queries %d wrong %d plain_matches %ld differences %ld not_filed %ld tree_bank_vars_in_queries %ld\n",
    queries,wrong,t.brute,t.diffs,notfiled,ti_query_tree_vars(x));
  ti_free(x);
  sys_free(cls);
  sys_free(t.lhs); sys_free(t.rhs); sys_free(t.rhsc);
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
  s->bytes=(long)(sizeof(ti_node)*x->ncap+sizeof(ti_slnode)*x->slcap+sizeof(ti_entry)*x->ecap);
}

#else

void wr_ti_selftest(glb* g) {
  UNUSED(g);
  wr_printf("TI_TEST result: no tree in this build\n");
}

#endif
