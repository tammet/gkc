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

 /** @file subsume.c
 *  Subsumption functions.
 *
 */

/* ====== Includes =============== */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>
#include "rincludes.h"  
 

//#define DEBUG  
//define DEBUG1 
//#undef DEBUG

//#define BACKDEBUG  
//#undef BACKDEBUG

//#define SCUTDEBUG
  
/* ====== Private headers and defs ======== */

#define ANS_IGNORE

/* ======= Private protos ================ */

static int wr_matchable_lit_meta(glb* g, gint mx, gint my);


/* ====== Functions ============== */

  /* ------------------------------------------------------ 

   clause-to-clause-list subsumption

   ------------------------------------------------------
*/



/* =============================================

  Index of clactivesubsume for the forward subsumption of the picked clause
  (64-bit builds). Each active clause in clactivesubsume is filed once,
  under one of its literals: the one whose predicate and sign are the
  rarest among the literals filed so far. The key is the predicate, the
  sign and the top symbols of the first two arguments; a variable or a
  number at an argument of the filed literal is a wildcard. The picked
  clause probes, for each of its literals, the keys with its own symbols
  and with wildcards (and, for an equality, both argument orders). A
  clause C subsumes D only if each literal of C, under the substitution,
  equals a literal of D, so every subsumer is under a probed key. The
  candidates get the same meta pre-test and full test as the walk, so the
  answer (subsumed or not) is the same; only the order of the tests
  differs.

================================================ */

#ifdef USE_LIT_HASH64

typedef struct { uint64_t key; gint* list; gint n, cap; } wr_fsi_bucket;
typedef struct { uint64_t key; gint cnt; } wr_fsi_freq;
typedef struct {
  wr_fsi_bucket* tab; gint cap, cnt;   // keys and their lists of clactivesubsume positions
  wr_fsi_freq* freq; gint fcap, fcnt;  // counts of filed literals per predicate and sign
  gint* stamp; gint scap;              // per position: the call that tested it last
  gint calls;
} wr_fsi_t;

#define FSI_WILD 0
#define FSI_SYMBOL 1
#define FSI_NOARG 2
#define FSI_QVAR 3

static uint64_t wr_fsi_mix(uint64_t a, uint64_t b) {
  uint64_t k=(a^(b+UINT64_C(0x9E3779B97F4A7C15)+(a<<6)+(a>>2)))*UINT64_C(0xBF58476D1CE4E5B9);
  return k^(k>>31);
}

/* the kind and the symbol of an argument of an atom: stored is 1 for a
   filed literal (a variable or a number is a wildcard), 0 for a literal
   of the picked clause (a variable or a number matches only a wildcard) */

static void wr_fsi_arg(glb* g, gptr ap, int pos, int stored, int* kind, gint* sym) {
  gint x;
  int len=get_record_len(ap);
  *sym=0;
  if (len<(g->unify_firstuseterm)+pos+1) { *kind=FSI_NOARG; return; }
  x=ap[RECORD_HEADER_GINTS+(pos==1 ? (g->unify_funarg1pos) : (g->unify_funarg2pos))];
  if (isdatarec(x)) { *kind=FSI_SYMBOL; *sym=(rotp(g,x))[RECORD_HEADER_GINTS+(g->unify_funpos)]; return; }
  if (!isvar(x) && islongstr(x)) { *kind=FSI_SYMBOL; *sym=x; return; }
  *kind=(stored ? FSI_WILD : FSI_QVAR);
}

static uint64_t wr_fsi_key(gint pred, int neg, int k1, gint s1, int k2, gint s2) {
  uint64_t h=wr_fsi_mix((uint64_t)pred,(uint64_t)(neg ? 1 : 0));
  h=wr_fsi_mix(h,((uint64_t)k1<<1)^((uint64_t)s1<<8));
  h=wr_fsi_mix(h,((uint64_t)k2<<1)^((uint64_t)s2<<8));
  return h|1; // 0 marks an empty slot
}

static void wr_fsi_lit(glb* g, gptr cl, int i, gint* atom, gint* pred, int* neg) {
  void* db=g->db;
  gint meta;
  if (wg_rec_is_rule_clause(db,cl)) {
    *atom=wg_get_rule_clause_atom(db,cl,i);
    meta=wg_get_rule_clause_atom_meta(db,cl,i);
  } else {
    *atom=encode_record(db,cl);
    meta=0;
  }
  *neg=wg_atom_meta_is_neg(db,meta) ? 1 : 0;
  *pred=(rotp(g,*atom))[RECORD_HEADER_GINTS+(g->unify_funpos)];
}

void wr_fsi_free(glb* g) {
  wr_fsi_t* x=(wr_fsi_t*)(g->fsi_index);
  gint i;
  if (!x) return;
  if (x==(void*)1) { (g->fsi_index)=NULL; return; }
  if (x->tab) {
    for(i=0;i<x->cap;i++) if (x->tab[i].list) sys_free(x->tab[i].list);
    sys_free(x->tab);
  }
  if (x->freq) sys_free(x->freq);
  if (x->stamp) sys_free(x->stamp);
  sys_free(x);
  (g->fsi_index)=NULL;
}

static gint* wr_fsi_freq_slot(wr_fsi_t* x, uint64_t key) {
  gint i=(gint)(key&(uint64_t)(x->fcap-1));
  while(x->freq[i].key && x->freq[i].key!=key) i=(i+1)&(x->fcap-1);
  if (!x->freq[i].key) { x->freq[i].key=key; x->freq[i].cnt=0; (x->fcnt)++; }
  return &(x->freq[i].cnt);
}

static wr_fsi_bucket* wr_fsi_find(wr_fsi_t* x, uint64_t key, int ins) {
  gint i=(gint)(key&(uint64_t)(x->cap-1));
  while(x->tab[i].key) {
    if (x->tab[i].key==key) return &(x->tab[i]);
    i=(i+1)&(x->cap-1);
  }
  if (!ins) return NULL;
  x->tab[i].key=key; x->tab[i].list=NULL; x->tab[i].n=0; x->tab[i].cap=0;
  (x->cnt)++;
  return &(x->tab[i]);
}

/* grow a table to twice its size; returns 0 on a failed allocation */

static int wr_fsi_grow(wr_fsi_t* x) {
  wr_fsi_bucket* ot=x->tab; gint oc=x->cap, i, j;
  wr_fsi_bucket* nt=sys_malloc(sizeof(wr_fsi_bucket)*oc*2);
  if (!nt) return 0;
  memset(nt,0,sizeof(wr_fsi_bucket)*oc*2);
  x->tab=nt; x->cap=oc*2;
  for(i=0;i<oc;i++) if (ot[i].key) {
    j=(gint)(ot[i].key&(uint64_t)(x->cap-1));
    while(nt[j].key) j=(j+1)&(x->cap-1);
    nt[j]=ot[i];
  }
  sys_free(ot);
  return 1;
}

static int wr_fsi_grow_freq(wr_fsi_t* x) {
  wr_fsi_freq* of=x->freq; gint oc=x->fcap, i, j;
  wr_fsi_freq* nf=sys_malloc(sizeof(wr_fsi_freq)*oc*2);
  if (!nf) return 0;
  memset(nf,0,sizeof(wr_fsi_freq)*oc*2);
  x->freq=nf; x->fcap=oc*2;
  for(i=0;i<oc;i++) if (of[i].key) {
    j=(gint)(of[i].key&(uint64_t)(x->fcap-1));
    while(nf[j].key) j=(j+1)&(x->fcap-1);
    nf[j]=of[i];
  }
  sys_free(of);
  return 1;
}

/* drop the index: the walk takes over */

static void wr_fsi_fail(glb* g) {
  wr_fsi_free(g);
  (g->fsi_index)=(void*)1; // marks a failed index for the rest of the run
}

/* file the active clause cl whose meta block starts at position pos of
   clactivesubsume */

void wr_fsi_insert(glb* g, gptr cl, gint pos) {
  wr_fsi_t* x=(wr_fsi_t*)(g->fsi_index);
  void* db=g->db;
  gint i, n, best=0, bc=-1, atom, pred, s1, s2, *c;
  int neg, k1, k2;
  wr_fsi_bucket* b;

  UNUSED(db);
  if ((g->inkb) || x==(void*)1) return;
  if (!x) {
    x=sys_malloc(sizeof(wr_fsi_t));
    if (!x) { wr_fsi_fail(g); return; }
    memset(x,0,sizeof(wr_fsi_t));
    x->cap=1<<12; x->fcap=1<<10; x->scap=1<<12;
    x->tab=sys_malloc(sizeof(wr_fsi_bucket)*x->cap);
    x->freq=sys_malloc(sizeof(wr_fsi_freq)*x->fcap);
    x->stamp=sys_malloc(sizeof(gint)*x->scap);
    (g->fsi_index)=x;
    if (!x->tab || !x->freq || !x->stamp) { wr_fsi_fail(g); return; }
    memset(x->tab,0,sizeof(wr_fsi_bucket)*x->cap);
    memset(x->freq,0,sizeof(wr_fsi_freq)*x->fcap);
    memset(x->stamp,0,sizeof(gint)*x->scap);
  }
  n=wg_rec_is_rule_clause(db,cl) ? wg_count_clause_atoms(db,cl) : 1;
  if (2*(x->fcnt+n+1)>x->fcap && !wr_fsi_grow_freq(x)) { wr_fsi_fail(g); return; }
  for(i=0;i<n;i++) {
    wr_fsi_lit(g,cl,i,&atom,&pred,&neg);
    c=wr_fsi_freq_slot(x,wr_fsi_mix((uint64_t)pred,(uint64_t)neg)|1);
    if (bc<0 || *c<bc) { bc=*c; best=i; }
  }
  for(i=0;i<n;i++) {
    wr_fsi_lit(g,cl,i,&atom,&pred,&neg);
    (*wr_fsi_freq_slot(x,wr_fsi_mix((uint64_t)pred,(uint64_t)neg)|1))++;
  }
  wr_fsi_lit(g,cl,best,&atom,&pred,&neg);
  wr_fsi_arg(g,rotp(g,atom),1,1,&k1,&s1);
  wr_fsi_arg(g,rotp(g,atom),2,1,&k2,&s2);
  if (2*(x->cnt+1)>x->cap && !wr_fsi_grow(x)) { wr_fsi_fail(g); return; }
  b=wr_fsi_find(x,wr_fsi_key(pred,neg,k1,s1,k2,s2),1);
  if (b->n>=b->cap) {
    gint ncap=(b->cap ? 2*(b->cap) : 4);
    gint* nl=sys_malloc(sizeof(gint)*ncap);
    if (!nl) { wr_fsi_fail(g); return; }
    if (b->list) { memcpy(nl,b->list,sizeof(gint)*b->n); sys_free(b->list); }
    b->list=nl; b->cap=ncap;
  }
  b->list[(b->n)++]=pos;
  while (pos/CLMETABLOCK_ELS>=x->scap) {
    gint* ns=sys_malloc(sizeof(gint)*x->scap*2);
    if (!ns) { wr_fsi_fail(g); return; }
    memset(ns,0,sizeof(gint)*x->scap*2);
    memcpy(ns,x->stamp,sizeof(gint)*x->scap);
    sys_free(x->stamp);
    x->stamp=ns; x->scap*=2;
  }
}

/* the test of one entry of clactivesubsume, as in the walk */

static int wr_fsi_test_entry(glb* g, gptr actptr, gint iactive, gptr given_cl, gptr given_cl_metablock) {
  gptr cl;
  if (given_cl_metablock!=NULL) {
    (g->stat_clsubs_top_meta_attempted)++;
    if (!wr_clmetablock_can_subsume(g,actptr+iactive,given_cl_metablock)) {
      (g->stat_clsubs_top_meta_failed)++;
      return 0;
    }
  }
  cl=rotp(g,(actptr[iactive+CLMETABLOCK_CL_POS]));
  if (cl!=NULL && wr_subsume_cl(g,cl,given_cl,1)) return 1;
  return 0;
}

static int wr_fsi_probe(glb* g, wr_fsi_t* x, gptr actptr, uint64_t key,
                        gptr given_cl, gptr given_cl_metablock) {
  wr_fsi_bucket* b=wr_fsi_find(x,key,0);
  gint j, pos;
  if (!b) return 0;
  for(j=0;j<b->n;j++) {
    pos=b->list[j];
    if (x->stamp[pos/CLMETABLOCK_ELS]==x->calls) continue; // tested in this call
    x->stamp[pos/CLMETABLOCK_ELS]=x->calls;
    if (wr_fsi_test_entry(g,actptr,pos,given_cl,given_cl_metablock)) return 1;
  }
  return 0;
}

/* the forward subsumption of given_cl by the clauses of clactivesubsume
   through the index: 1 subsumed, 0 not, -1 no index (use the walk) */

static int wr_fsi_subsumed(glb* g, gptr given_cl, gptr given_cl_metablock) {
  wr_fsi_t* x=(wr_fsi_t*)(g->fsi_index);
  void* db=g->db;
  gptr actptr;
  gint i, n, atom, pred, s1, s2;
  int neg, k1, k2, eq, a, c;
  int ka[2], kb[2]; gint sa[2], sb[2];

  UNUSED(db);
  actptr=rotp(g,g->clactivesubsume);
  if (!x) return (CVEC_NEXT(actptr)>CVEC_START) ? -1 : 0; // no index: the walk
  if (x==(void*)1) return -1;
  (x->calls)++;
  n=wg_rec_is_rule_clause(db,given_cl) ? wg_count_clause_atoms(db,given_cl) : 1;
  for(i=0;i<n;i++) {
    wr_fsi_lit(g,given_cl,i,&atom,&pred,&neg);
    wr_fsi_arg(g,rotp(g,atom),1,0,&k1,&s1);
    wr_fsi_arg(g,rotp(g,atom),2,0,&k2,&s2);
    eq=wr_equality_atom(g,atom);
    // the argument kinds that a filed literal can have at each position:
    // the same symbol (or the same missing argument), or a wildcard
    for(c=0;c<(eq ? 2 : 1);c++) {
      int qk1=(c ? k2 : k1), qk2=(c ? k1 : k2); gint qs1=(c ? s2 : s1), qs2=(c ? s1 : s2);
      int na=0, nb=0;
      if (qk1==FSI_SYMBOL || qk1==FSI_NOARG) { ka[na]=qk1; sa[na]=qs1; na++; }
      if (qk1!=FSI_NOARG) { ka[na]=FSI_WILD; sa[na]=0; na++; }
      if (qk2==FSI_SYMBOL || qk2==FSI_NOARG) { kb[nb]=qk2; sb[nb]=qs2; nb++; }
      if (qk2!=FSI_NOARG) { kb[nb]=FSI_WILD; sb[nb]=0; nb++; }
      for(a=0;a<na;a++) {
        int bb;
        for(bb=0;bb<nb;bb++) {
          if (wr_fsi_probe(g,x,actptr,wr_fsi_key(pred,neg,ka[a],sa[a],kb[bb],sb[bb]),
                           given_cl,given_cl_metablock)) return 1;
        }
      }
    }
  }
  return 0;
}

#else

void wr_fsi_free(glb* g) { UNUSED(g); }

#endif

int wr_given_cl_subsumed(glb* g, gptr given_cl, gptr given_cl_metablock) {
  void* db=g->db;
  gptr cl;
  int iactive;
  gptr actptr;
  gint iactivelimit;
  int sres;
  int i,cllen;
  gint meta, lit;
  int dbused;
 
#ifdef DEBUG
  wr_printf("wr_given_cl_is_subsumed is called with \n");
  wr_print_clause(g,given_cl); 
  wr_printf("\n"); 
#endif  
  UNUSED(db);
  // first try to subsume with active unit clauses  
  if (!wg_rec_is_rule_clause(db,given_cl)) {
    if (wr_active_subsumed_lit(g,rpto(g,given_cl),0)) {
#ifdef DEBUG1
      wr_printf("\n fact cl is subsumed by active unit by wr_active_subsumed_lit\n");
#endif      
      (g->stat_clsubs_fact_groundunit_found)++;
      return 1;
    } 
  } else {
    // here a rule clause
    cllen=wg_count_clause_atoms(db,given_cl);     
    for(i=0;i<cllen;i++) {
      meta=wg_get_rule_clause_atom_meta(db,given_cl,i);
      if (!wg_atom_meta_is_ground(db,meta)) break; // only nonground will come later
      lit=wg_get_rule_clause_atom(db,given_cl,i);
      if (wr_active_subsumed_lit(g,lit,wg_atom_meta_is_neg(db,meta))) {
#ifdef DEBUG1
        wr_printf("\n rule cl literal is subsumed by active unit by wr_active_subsumed_lit\n");
#endif           
        (g->stat_clsubs_rule_groundunit_found)++;
        return 1;
      }  
      //if (!wg_atom_meta_is_neg(db,meta) && wr_equal_term(g,lit1,lit2,uniquestrflag)) return 1;        
    }
  }  

#ifdef DEBUG
  wr_printf("wr_given_cl_is_subsumed ground unit subsumption loop did not find a subsumer \n");
#endif     

  // next try to subsume with active clauses in lists

  //printf("\n** clactivesubsume data from external:\n"); 
  //wr_show_clactivesubsume(r_kb_g(g));

  // loop over local active subslist (dbused==0) and external subslist (dbused==1)

  for(dbused=0; dbused<2; dbused++) {
    if (dbused==0) {
#ifdef USE_LIT_HASH64
      // the clauses of this run through the index, if it exists
      sres=wr_fsi_subsumed(g,given_cl,given_cl_metablock);
      if (sres==1) return 1;
      if (sres==0) continue;
#endif
      actptr=rotp(g,g->clactivesubsume);   
#ifdef DEBUG
      wr_printf("\n ** dbused %d trying local clactivesubsume\n",dbused);
#endif          
    } else {
#ifdef DEBUG
        wr_printf("\n ** dbused %d \n",dbused);        
#endif        
      if (r_kb_g(g)) {        
        actptr=rotp(g,(r_kb_g(g))->clactivesubsume); 
#ifdef DEBUG
        wr_printf("\n ** dbused %d trying external clactivesubsume\n",dbused);        
#endif          
      }  
      else break;      
    }
    if(1) {    
      //actptr=rotp(g,g->clactivesubsume);
      iactivelimit=CVEC_NEXT(actptr);
      for(iactive=CVEC_START; iactive<iactivelimit; iactive+=CLMETABLOCK_ELS) {
        if (given_cl_metablock!=NULL) {
  #ifdef DEBUG          
            
            wr_printf("\ngeneral iactive %d :",iactive);    
            wr_print_clause(g,rotp(g,(actptr[iactive+CLMETABLOCK_CL_POS])));
            wr_printf("\n");
            wr_print_record(g,rotp(g,(actptr[iactive+CLMETABLOCK_CL_POS])));    
            wr_printf("\nmeta addr %d\n",(int)actptr+iactive);
            wr_printf("\nmetablock1 %d %d %d %d \n",*(actptr+iactive),*(actptr+iactive+1),*(actptr+iactive+2),*(actptr+iactive+3));
            wr_printf("\n lengths   ");
            wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_LENGTHS_POS]);
            wr_printf("\n real length %d\n",
              (*(actptr+iactive) & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
            wr_printf("\n sizes     ");
            wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_SIZES_POS]);
            wr_printf("\n pref1bits ");
            wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_PREF1BITS_POS]);
            wr_printf("\n pref2bits ");
            wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_PREF2BITS_POS]);
            wr_printf("\n pref3bits ");
            wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_PREF3BITS_POS]);
            wr_printf("\n");

            wr_printf("\n specific given_cl:");    
            wr_print_clause(g,given_cl);
            wr_printf("\n");
            wr_print_record(g,given_cl);   
            
            wr_printf("\nmeta addr %d\n",(int)given_cl_metablock); 
            wr_printf("\nmetablock2 %d %d %d %d \n",*(given_cl_metablock),*(given_cl_metablock+1),*(given_cl_metablock+2),*(given_cl_metablock+3));
            wr_printf("\n lengths   ");
            wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_LENGTHS_POS]);
            wr_printf("\n real length %d\n",
              (*(given_cl_metablock) & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
            wr_printf("\n sizes     ");
            wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_SIZES_POS]);
            wr_printf("\n pref1bits ");
            wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_PREF1BITS_POS]);
            wr_printf("\n pref2bits ");
            wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_PREF2BITS_POS]);
            wr_printf("\n pref3bits ");
            wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_PREF3BITS_POS]);
            wr_printf("\n");
            
  #endif       
            //wr_printf("\ngeneral iactive %d :",iactive);    
            //wr_print_clause(g,rotp(g,(actptr[iactive+CLMETABLOCK_CL_POS])));
            //wr_printf("\n");
            //wr_printf("\n specific given_cl:");    
            //wr_print_clause(g,given_cl);
            //wr_printf("\n");
            //wr_print_record(g,given_cl);   

          (g->stat_clsubs_top_meta_attempted)++;
          if (!wr_clmetablock_can_subsume(g,actptr+iactive,given_cl_metablock)) {
            (g->stat_clsubs_top_meta_failed)++;
            //wr_printf("\n meta-detected cannot subsume \n");  
  #ifdef DEBUG          
            wr_printf("\n meta-detected cannot subsume \n");            
            wr_print_clause(g,rotp(g,(actptr[iactive+CLMETABLOCK_CL_POS])));
            wr_printf("\n");
            wr_print_clause(g,given_cl);
            wr_printf("\n");            
  #endif    
                  
            continue;
          }
          //wr_printf("\n meta-detected CAN subsume \n");
  #ifdef DEBUG        
          wr_printf("\n meta-detected CAN subsume \n");          
          wr_print_clause(g,rotp(g,(actptr[iactive+CLMETABLOCK_CL_POS])));
          wr_printf("\n");
          wr_print_clause(g,given_cl);
          wr_printf("\n");          
  #endif        
        }        
        cl=rotp(g,(actptr[iactive+CLMETABLOCK_CL_POS]));
        if (cl!=NULL) {      
  #ifdef DEBUG
          wr_printf(" *** about to try subsuming with active clause at pos %d nr %d:\n",
            iactive+CLMETABLOCK_CL_POS,iactive/CLMETABLOCK_ELS); 
          wr_print_clause(g,cl);
          wr_printf("\n"); 
  #endif            
          // try to subsume
          sres=wr_subsume_cl(g,cl,given_cl,1);
          if (sres) {
  #ifdef DEBUG1
            wr_printf(" * subsumer was found! \n"); 
            wr_print_clause(g,cl);
            wr_printf("\n");        
  #endif           
            return 1;
          }  
        }	      
      }
  #ifdef DEBUG
      wr_printf(" *** no subsumer was found from this list of active clauses\n");         
  #endif         
    }  
  }
#ifdef DEBUG
  wr_printf(" *** no subsumer was found from all active clauses\n");         
#endif     
  return 0;    
}  

/*

back subsumption

*/

int wr_given_cl_backsubsume(glb* g, gptr given_cl, gptr given_cl_metablock) {
  void* db=g->db;
  gptr cl;
  int iactive;
  gptr actptr;
  gint iactivelimit;
  int sres;
  int subsumedcount;

#ifdef BACKDEBUG
  wr_printf("wr_given_cl_backsubsume is called with \n");
  wr_print_clause(g,given_cl); 
  wr_printf("\n"); 
#endif  
 
  UNUSED(db);
  // next try to subsume with active clauses in a list
  subsumedcount=0;
  if(1) {    
    actptr=rotp(g,g->clactivesubsume);
    iactivelimit=CVEC_NEXT(actptr);
    for(iactive=CVEC_START; iactive<iactivelimit; iactive+=CLMETABLOCK_ELS) {
      if (given_cl_metablock!=NULL) {
        
#ifdef BACKDEBUG   
/*       
          wr_printf("\nactptr %ld specific iactive %d :",actptr,iactive);   
          wr_print_clause(g,rotp(g,actptr[iactive+CLMETABLOCK_CL_POS]));
          wr_printf("\n");
          wr_print_record(g,rotp(g,actptr[iactive+CLMETABLOCK_CL_POS]));    
          wr_printf("\nmeta addr %d\n",(int)actptr+iactive);
          wr_printf("\nmetablock1 %d %d %d %d \n",*(actptr+iactive),*(actptr+iactive+1),*(actptr+iactive+2),*(actptr+iactive+3));
          wr_printf("\n lengths   ");
          wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_LENGTHS_POS]);
          wr_printf("\n real length %d\n",
            (*(actptr+iactive) & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
          wr_printf("\n sizes     ");
          wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_SIZES_POS]);
          wr_printf("\n pref1bits ");
          wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_PREF1BITS_POS]);
          wr_printf("\n pref2bits ");
          wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_PREF2BITS_POS]);
          wr_printf("\n pref3bits ");
          wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_PREF3BITS_POS]);
          wr_printf("\n");

          wr_printf("\n general given_cl:");    
          wr_print_clause(g,given_cl);
          wr_printf("\n");
          wr_print_record(g,given_cl);   
          wr_printf("\nmeta addr %d\n",(int)given_cl_metablock); 
          wr_printf("\nmetablock2 %d %d %d %d \n",*(given_cl_metablock),*(given_cl_metablock+1),*(given_cl_metablock+2),*(given_cl_metablock+3));
          wr_printf("\n lengths   ");
          wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_LENGTHS_POS]);
          wr_printf("\n real length %d\n",
            (*(given_cl_metablock) & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
          wr_printf("\n sizes     ");
          wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_SIZES_POS]);
          wr_printf("\n pref1bits ");
          wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_PREF1BITS_POS]);
          wr_printf("\n pref2bits ");
          wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_PREF2BITS_POS]);
          wr_printf("\n pref3bits ");
          wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_PREF3BITS_POS]);
          wr_printf("\n");
*/          
#endif         

        (g->stat_clsubs_top_meta_attempted)++;
        if (!wr_clmetablock_can_subsume(g,given_cl_metablock,actptr+iactive)) {
          (g->stat_clsubs_top_meta_failed)++;
#ifdef BACKDEBUG      
/*    
          wr_printf("\n meta-detected cannot subsume \n");
          wr_print_clause(g,rotp(g,actptr[iactive+CLMETABLOCK_CL_POS]));
          wr_printf("\n");
          wr_print_clause(g,given_cl);
          wr_printf("\n");
*/          
#endif    
                 
          continue;
        }
#ifdef BACKDEBUG     
/*   
        wr_printf("\n meta-detected CAN subsume \n");
        wr_print_clause(g,rotp(g,actptr[iactive+CLMETABLOCK_CL_POS]));
        wr_printf("\n");
        wr_print_clause(g,given_cl);
        wr_printf("\n");
*/        
#endif        
      }        
 
      cl=rotp(g,(actptr[iactive+CLMETABLOCK_CL_POS]));
      if (cl!=NULL) {  
   
#ifdef BACKDEBUG
/*
        wr_printf(" *** about to try back-subsuming with active clause at pos %d nr %d:\n",
          iactive+CLMETABLOCK_CL_POS,iactive/CLMETABLOCK_ELS); 
        wr_print_clause(g,cl);
        wr_printf("\n"); 
*/        
#endif           
        // try to subsume
        sres=wr_subsume_cl(g,given_cl,cl,1);
        if (sres) {
          (g->stat_backward_subsumed)++;
          subsumedcount++;
          wr_set_cl_backsubsumed(g,cl);
          /*
          wr_printf("\n * back-subsumed: \n");
          wr_print_clause(g,cl);
          wr_printf("\n"); 
          */
#ifdef BACKDEBUG
          wr_printf(" * back-subsumer was found! \n");         
#endif                     
        }  
      }	      
    }
#ifdef BACKDEBUG
    wr_printf(" *** back-subsumed: %d\n",subsumedcount);         
#endif     
    return 0;
  }    
}  


void wr_set_cl_backsubsumed(glb* g,gptr cl) {
  gint clid;

  clid=wr_get_clid(g,cl);
  if (clid>=(g->backsubsume_bytes)) return;
  (g->backsubsume_values)[clid]=(char)1;
}

int wr_get_cl_backsubsumed(glb* g,gptr cl) {
  gint clid;

  clid=wr_get_clid(g,cl);
  if (clid>=(g->backsubsume_bytes)) return 0;
  return (int)((g->backsubsume_values)[clid]);
}


int wr_clmetablock_can_subsume(glb* g, gptr genblock, gptr specblock) {
  gint gel, sel;

  gel=genblock[CLMETABLOCK_LENGTHS_POS];
  sel=specblock[CLMETABLOCK_LENGTHS_POS];
  (g->stat_clsubs_top_meta_nonpref_attempted)++;
  /*
  wr_printf("\ngel %d sel %d",gel,sel);
  wr_printf("\n gel bits:\n");
  wr_print_cl_meta(g, gel);
  wr_printf(" sel bits:\n");
  wr_print_cl_meta(g, sel);
  wr_printf("\n");
  
  wr_printf("gel isground %d\n",(gel & (1<<CLMETABLOCK_ISGROUND_SHIFT))>>CLMETABLOCK_ISGROUND_SHIFT);
  wr_printf("sel isground %d\n",(sel & (1<<CLMETABLOCK_ISGROUND_SHIFT))>>CLMETABLOCK_ISGROUND_SHIFT);
  wr_printf("gel length %d\n",(gel & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
  wr_printf("sel length %d\n",(sel & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
  wr_printf("gel neglength %d\n",(gel & (255<<CLMETABLOCK_NEGLENGTH_SHIFT))>>CLMETABLOCK_NEGLENGTH_SHIFT);
  wr_printf("sel neglength %d\n",(sel & (255<<CLMETABLOCK_NEGLENGTH_SHIFT))>>CLMETABLOCK_NEGLENGTH_SHIFT);
  wr_printf("gel groundlength %d\n",(gel & (255<<CLMETABLOCK_GROUNDLENGTH_SHIFT))>>CLMETABLOCK_GROUNDLENGTH_SHIFT);
  wr_printf("sel groundlength %d\n",(sel & (255<<CLMETABLOCK_GROUNDLENGTH_SHIFT))>>CLMETABLOCK_GROUNDLENGTH_SHIFT);
  //printf("gel length %d\n",(gel & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
  //printf("sel length %d\n",(sel & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
  */
  if (gel>sel) return 0;
  if ((gel & (255<<CLMETABLOCK_LENGTH_SHIFT)) > 
      (sel & (255<<CLMETABLOCK_LENGTH_SHIFT))) return 0;
  if ((gel & (255<<CLMETABLOCK_NEGLENGTH_SHIFT)) > 
      (sel & (255<<CLMETABLOCK_NEGLENGTH_SHIFT))) return 0; 
  if ((gel & (255<<CLMETABLOCK_GROUNDLENGTH_SHIFT)) > 
      (sel & (255<<CLMETABLOCK_GROUNDLENGTH_SHIFT))) return 0;       
  if ((gel & (255<<CLMETABLOCK_LENGTH_SHIFT)) == 
      (sel & (255<<CLMETABLOCK_LENGTH_SHIFT)) &&  
      ((sel & (255<<CLMETABLOCK_LENGTH_SHIFT))<255) &&
      ((gel & (255<<CLMETABLOCK_NEGLENGTH_SHIFT)) != 
       (sel & (255<<CLMETABLOCK_NEGLENGTH_SHIFT)) )) return 0;     
  if ((g->instgen_strat) && 
      ((gel & (255<<CLMETABLOCK_LENGTH_SHIFT)) == 
       (sel & (255<<CLMETABLOCK_LENGTH_SHIFT)) ) &&
      ((sel & (255<<CLMETABLOCK_LENGTH_SHIFT)) < (255<<CLMETABLOCK_LENGTH_SHIFT)) &&
      ((gel!=sel) || (genblock[CLMETABLOCK_SIZES_POS]!=specblock[CLMETABLOCK_SIZES_POS]) ||
       (genblock[CLMETABLOCK_PREF1BITS_POS]!=specblock[CLMETABLOCK_PREF1BITS_POS]) ||
       (genblock[CLMETABLOCK_PREF2BITS_POS]!=specblock[CLMETABLOCK_PREF2BITS_POS]) ))
    return 0; // only shorter or exactly equal may subsume for instgen
  gel=genblock[CLMETABLOCK_SIZES_POS];
  sel=specblock[CLMETABLOCK_SIZES_POS];
  if (gel>sel) return 0;
  if ((gel & (255<<CLMETABLOCK_SIZE_SHIFT)) > 
      (sel & (255<<CLMETABLOCK_SIZE_SHIFT))) return 0;
  if ((gel & (255<<CLMETABLOCK_PREFLEN_SHIFT)) > 
      (sel & (255<<CLMETABLOCK_PREFLEN_SHIFT))) return 0;   
  (g->stat_clsubs_top_meta_nonpref_succeeded)++;  
  (g->stat_clsubs_top_meta_pref_attempted)++;   
  gel=genblock[CLMETABLOCK_PREF1BITS_POS];
  sel=specblock[CLMETABLOCK_PREF1BITS_POS];
  if ((gel & sel) != gel) return 0;
  (g->stat_clsubs_top_meta_pref1_succeeded)++;
  gel=genblock[CLMETABLOCK_PREF2BITS_POS];
  sel=specblock[CLMETABLOCK_PREF2BITS_POS];
  if ((gel & sel) != gel) return 0;
  (g->stat_clsubs_top_meta_pref2_succeeded)++;
  gel=genblock[CLMETABLOCK_PREF3BITS_POS]; 
  sel=specblock[CLMETABLOCK_PREF3BITS_POS];
  if ((gel & sel) != gel) return 0;
  (g->stat_clsubs_top_meta_pref3_succeeded)++;
  (g->stat_clsubs_top_meta_pref_succeeded)++;
  return 1;
}


/*

  return 0 if no subsumption
  return 1 if subsumed

*/

int wr_active_subsumed_lit(glb* g, gint atom, int negflag) {
  gint kbhash, hash, uhash;
  cvec bucket;
  gptr atomptr;

#ifdef DEBUG
  wr_printf("\nwr_active_subsumed_lit is called \n");
  wr_print_term(g,atom);  
  wr_printf("\n and negflag %d\n",negflag);
#endif

  hash=wr_lit_hash(g,atom); uhash=wr_lit_hash_unit(g,atom,hash); // the unit tables 
  kbhash=(r_kb_g(g) ? wr_lit_hash_kb(g,atom) : hash); // a shared KB keeps the old hash
  atomptr=rotp(g,atom);
  if (negflag) {
    bucket=wr_find_termhash_unit(g,rotp(g,g->hash_neg_active_groundunits),atomptr,uhash);      
    if (bucket!=NULL) {
      return 1;
    } else if (r_kb_g(g)) {
      bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_neg_active_groundunits),atomptr,kbhash);
      if (bucket!=NULL) return 1;
      else return 0;        
    } else {
      return 0;
    }   
  } else {
    bucket=wr_find_termhash_unit(g,rotp(g,g->hash_pos_active_groundunits),atomptr,uhash);
    if (bucket!=NULL) {
      return 1;
    } else if (r_kb_g(g)) {
      bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_pos_active_groundunits),atomptr,kbhash);
      if (bucket!=NULL) return 1;
      else return 0;        
    } else {
      return 0;
    }   
  }
}



/*

  return -1 if subsumed
  return 0 if no subsumption, no cuts
  return a nr of cuts (more than 0) if any cuts

*/


/* ---- the full unit cut and the full unit subsumption (keys unit_cut_match,
   unit_subsume_match) ----

  Two trees of the module termindex.c, one per sign, over the atoms of the
  stored units; the payload of an entry is the unit clause. Every unit that
  enters the unit tables enters the tree of its sign at the same moment
  (wr_add_cl_to_unithash_h). A lookup finds a stored unit whose atom
  matches the atom of a literal (the unit is equal or more general); the
  walk stops at the first accepted unit in the walk order. An equality
  literal is looked up a second time with its two sides swapped (a scratch
  record), since the unit s=t also matches the literal t=s. The variables
  of the trees are the cells of the tree bank, bound with SETVAR and
  cleared as a top slice of the variable stack, so the unifier of the
  parents on the stack is not touched, and each literal is a lookup of its
  own. The tables test nothing on a stored unit but equality (or a variant
  with unit_variant_hash): the lookup has no accept test either.

  The key value 1 looks into the tree after the local tables; the value 2
  looks into the tree in place of them (the tables of an attached KB stay,
  and the tables answer when the trees are stopped). With unit_match_equal
  1 the tree accepts only what the tables accept (an equal atom, a variant
  with unit_variant_hash) and does not swap an equation, so the search is
  the search of the tables. The tables are still filled: the simplification
  of the picked clause (wr_atom_cut_and_subsume) and the later runs of a
  process (wr_activate_from_termhash) read them.
*/

#ifdef USE_TERM_INDEX

#ifdef GKC_UCM_CHECK
/* the check build: on every literal of a derived clause, a unit that the
   local unit tables find must be found by the tree of its sign too */
long ucm_check_lookups=0, ucm_check_miss=0; // printed with the statistics (rmain.c)
static void wr_ucm_check_lit(glb* g, gint xatom, gint hash);
static gptr wr_ucm_find(glb* g, gint xatom, int neg);
#endif

void wr_ucm_free(glb* g) {
  if ((g->ucm_pos) && (g->ucm_pos)!=(void*)1) ti_free((ti_index*)(g->ucm_pos));
  if ((g->ucm_neg) && (g->ucm_neg)!=(void*)1) ti_free((ti_index*)(g->ucm_neg));
  (g->ucm_pos)=NULL; (g->ucm_neg)=NULL;
  if (g->ucm_scratch) sys_free(g->ucm_scratch);
  (g->ucm_scratch)=NULL; (g->ucm_scratch_cap)=0;
}

static void wr_ucm_fail(glb* g, int why) {
  if ((g->ucm_pos) && (g->ucm_pos)!=(void*)1) ti_free((ti_index*)(g->ucm_pos));
  if ((g->ucm_neg) && (g->ucm_neg)!=(void*)1) ti_free((ti_index*)(g->ucm_neg));
  (g->ucm_pos)=(void*)1; (g->ucm_neg)=(void*)1;
  (g->stat_ucm_stopped)=why;
}

/* file the unit clause cl with the atom xatom in the tree of its sign */

void wr_ucm_insert(glb* g, gptr cl, gptr xatom, int neg) {
  void** slot=(neg ? &(g->ucm_neg) : &(g->ucm_pos));
  ti_index* x=(ti_index*)(*slot);
  if (x==(void*)1 || (g->inkb)) return;
  if (!(g->unify_samelen) || (g->unify_maxuseterms)) { wr_ucm_fail(g,3); return; }
  if (!x) {
    x=ti_new((g->varbanks)+TREE_VARBANK*NROF_VARSINBANK,g->tree_compress);
    if (!x) { wr_ucm_fail(g,1); return; }
    *slot=x;
  }
  if (!ti_insert(g,x,rpto(g,xatom),(gint)cl,0) || ti_failed(x)) { wr_ucm_fail(g,1); return; }
  (g->stat_ucm_inserted)++;
}

/* unit_match_equal: the test of the unit tables on a unit that the walk
   found (wr_find_termhash_unit): its atom equal to the literal, or a variant
   with unit_variant_hash. Both tests only compare. */

static gint ucm_eq_query=0;

static int wr_ucm_accept_equal(glb* g, gint payload) {
  void* db=g->db;
  gptr cl=(gptr)payload;
  gint atom;
  UNUSED(db);
  if (wg_rec_is_rule_clause(db,cl)) atom=wg_get_rule_clause_atom(db,cl,0);
  else atom=rpto(g,cl);
  if (g->unit_variant_hash) return wr_variant_term(g,ucm_eq_query,atom);
  return wr_equal_term(g,ucm_eq_query,atom,1);
}

#ifdef GKC_UCM_CHECK
static void wr_ucm_check_lit(glb* g, gint xatom, gint hash) {
  gint uh=wr_lit_hash_unit(g,xatom,hash);
  int neg;
  gptr hb, tb;
  if ((g->ucm_pos)==(void*)1 || (g->inkb)) return;
  for(neg=0;neg<2;neg++) {
    hb=wr_find_termhash_unit(g,rotp(g,neg ? g->hash_neg_groundunits : g->hash_pos_groundunits),rotp(g,xatom),uh);
    tb=wr_ucm_find(g,xatom,neg);
    ucm_check_lookups++;
    if (hb && !tb) ucm_check_miss++;
  }
}
#endif

/* at the pick: the picked clause is itself in its tree when it is a unit;
   it must not cut or subsume itself (as rewrite_self for the demodulators) */

static gptr ucm_self=NULL;

static int wr_ucm_accept(glb* g, gint payload) {
  if (ucm_self && (gptr)payload==ucm_self) return 0;
  if (g->unit_match_equal) return wr_ucm_accept_equal(g,payload);
  return 1;
}

/* the key value 2 (unit_cut_match, unit_subsume_match): the tree in place
   of the local unit tables, when the trees answer (not stopped, not filled
   while a KB is built) */

static int wr_ucm_replaces(glb* g, int key) {
  return key==2 && !(g->inkb) && (g->ucm_pos)!=(void*)1 && (g->ucm_neg)!=(void*)1;
}

/* the key value 1 at the pick: the tree after the local tables */

static int wr_ucm_after(glb* g, int key) {
  return key==1 && !(g->inkb) && (g->ucm_pos)!=(void*)1 && (g->ucm_neg)!=(void*)1;
}

/* a unit of the tree of the sign neg whose atom matches xatom, or NULL */

#ifdef GKC_UCM_WALK_CHECK
/* the check build: the lookup by the walk over the preorder array (ti_preorder, ti_match_first),
   compared with the walk that reads the term on every lookup */
long ucm_walk_check_lookups=0, ucm_walk_check_diff=0;
static gptr wr_ucm_find_tok(glb* g, gint xatom, int neg) {
  ti_index* x=(ti_index*)(neg ? (g->ucm_neg) : (g->ucm_pos));
  gint payload;
  int32_t n;
  gptr ap, sp;
  gint len, t;
  int i, p1, p2;
  if (!x || x==(void*)1) return NULL;
  n=ti_preorder(g,x,xatom);
  if (n<0) { wr_ucm_fail(g,1); return NULL; }
  (g->stat_ucm_lookups)++;
  ucm_eq_query=xatom;
  if (ti_match_first(g,x,0,n,(ucm_self || (g->unit_match_equal)) ? wr_ucm_accept : NULL,&payload)) return (gptr)payload;
  if ((g->unit_match_equal) || !wr_equality_atom(g,xatom)) return NULL; // the tables do not swap
  // the equality literal with its two sides swapped
  ap=rotp(g,xatom);
  len=get_record_len(ap);
  if (RECORD_HEADER_GINTS+len>(g->ucm_scratch_cap)) {
    gptr nb=sys_realloc(g->ucm_scratch,sizeof(gint)*(RECORD_HEADER_GINTS+len+8));
    if (!nb) return NULL;
    (g->ucm_scratch)=nb; (g->ucm_scratch_cap)=RECORD_HEADER_GINTS+len+8;
  }
  sp=(g->ucm_scratch);
  for(i=0;i<RECORD_HEADER_GINTS+len;i++) sp[i]=ap[i];
  p1=RECORD_HEADER_GINTS+(g->unify_funarg1pos);
  p2=RECORD_HEADER_GINTS+(g->unify_funarg2pos);
  t=sp[p1]; sp[p1]=sp[p2]; sp[p2]=t;
  n=ti_preorder(g,x,rpto(g,sp));
  if (n<0) { wr_ucm_fail(g,1); return NULL; }
  (g->stat_ucm_lookups)++;
  if (ti_match_first(g,x,0,n,ucm_self ? wr_ucm_accept : NULL,&payload)) return (gptr)payload;
  return NULL;
}
#endif

/* a unit of the tree of the sign neg whose atom matches xatom, or NULL: the
   walk reads the literal as it goes (ti_match_first_term), and an equation
   is looked up a second time with its two sides read the other way round */

static gptr wr_ucm_find(glb* g, gint xatom, int neg) {
  ti_index* x=(ti_index*)(neg ? (g->ucm_neg) : (g->ucm_pos));
  gint payload;
  int r;
  if (!x || x==(void*)1) return NULL;
  (g->stat_ucm_lookups)++;
  ucm_eq_query=xatom;
  r=ti_match_first_term(g,x,xatom,0,(ucm_self || (g->unit_match_equal)) ? wr_ucm_accept : NULL,&payload);
  if (ti_failed(x)) { wr_ucm_fail(g,1); return NULL; }
  if (!r && !(g->unit_match_equal) && wr_equality_atom(g,xatom)) {
    // the equality literal with its two sides swapped (the tables do not swap)
    (g->stat_ucm_lookups)++;
    r=ti_match_first_term(g,x,xatom,1,ucm_self ? wr_ucm_accept : NULL,&payload);
    if (ti_failed(x)) { wr_ucm_fail(g,1); return NULL; }
  }
#ifdef GKC_UCM_WALK_CHECK
  {
    gptr o=wr_ucm_find_tok(g,xatom,neg);
    ucm_walk_check_lookups++;
    if ((o==NULL)!=(r==0) || (r && o!=(gptr)payload)) ucm_walk_check_diff++;
  }
#endif
  return r ? (gptr)payload : NULL;
}

/* the lookup at the pick (wr_atom_cut_and_subsume): the tree of the sign
   neg, leaving out the picked clause self; sub: a subsumption (else a cut),
   for the counters */

static gptr wr_ucm_pick(glb* g, gint xatom, int neg, int sub) {
  gptr r;
  ucm_self=(g->ucm_pick_self);
  r=wr_ucm_find(g,xatom,neg);
  ucm_self=NULL;
  if (r) {
    if (sub) (g->stat_usm_pick_subsumed)++;
    else (g->stat_ucm_pick_cut)++;
  }
  return r;
}

#else

void wr_ucm_free(glb* g) { UNUSED(g); }
void wr_ucm_insert(glb* g, gptr cl, gptr xatom, int neg) { UNUSED(g); UNUSED(cl); UNUSED(xatom); UNUSED(neg); }
static gptr wr_ucm_find(glb* g, gint xatom, int neg) { UNUSED(g); UNUSED(xatom); UNUSED(neg); return NULL; }
static int wr_ucm_replaces(glb* g, int key) { UNUSED(g); UNUSED(key); return 0; }
static int wr_ucm_after(glb* g, int key) { UNUSED(g); UNUSED(key); return 0; }
static gptr wr_ucm_pick(glb* g, gint xatom, int neg, int sub) { UNUSED(g); UNUSED(xatom); UNUSED(neg); UNUSED(sub); return NULL; }

#endif

int wr_derived_cl_cut_and_subsume(glb* g, gptr rptr, int rpos, gptr clhashptr) {

  int i,tmp;
  gint kbhash, hash, uhash, xatom,xatommeta;
  gptr xatomptr;
  cvec bucket;
  int cuts=0, oldcuts;
  gint clhash;
  // the key value 2: the unit trees in place of the local unit tables
  int cutrep=wr_ucm_replaces(g,g->unit_cut_match), subrep=wr_ucm_replaces(g,g->unit_subsume_match);

#ifdef DEBUG
  wr_printf("\nwr_derived_cl_cut_and_subsume is called \n");
#endif  
  (g->stat_forwardsubs_attempted)++;
  (g->cut_clvec)[1]=(gint)NULL;
  clhash=0;  
  // keep the literal hashes for the final steps of the derived clause
  if (!wr_lithash_reserve(g,rpos)) (g->lithash_n)=0;
  for(i=0;i<rpos;i++) { 
    tmp=i*LIT_WIDTH;
    xatom=rptr[tmp+LIT_ATOM_POS];
    xatommeta=rptr[tmp+LIT_META_POS];   
    oldcuts=cuts;
#ifdef DEBUG     
    wr_printf("\n i %d xatom %d xatommeta %d atom:\n",i,xatom,xatommeta);
    wr_print_term(g,xatom);    
#endif    
    xatomptr=rotp(g,xatom);
    hash=wr_lit_hash(g,xatom); uhash=((cutrep && subrep) ? 0 : wr_lit_hash_unit(g,xatom,hash)); // the unit tables
#ifdef GKC_UCM_CHECK
    if (cutrep || subrep) wr_ucm_check_lit(g,xatom,hash);
#endif
    kbhash=(r_kb_g(g) ? wr_lit_hash_kb(g,xatom) : hash); // a shared KB keeps the old hash
    if ((g->lithash_size)>i) {
      (g->lithash_atoms)[i]=xatom;
      (g->lithash_vals)[i]=hash;
      (g->lithash_n)=i+1;
    }
#ifdef DEBUG    
    wr_printf("\ncalculated hash %d \n",hash);
#endif    
    if (wg_atom_meta_is_neg(db,xatommeta)) {
#ifdef DEBUG      
      wr_printf("\nneg\n");
#endif 
      if ((g->forward_subsume_derived) && !subrep)
        bucket=wr_find_termhash_unit(g,rotp(g,g->hash_neg_groundunits),xatomptr,uhash);
      else
        bucket=NULL;       
      //printf("\ncpneg\n");
      if (bucket!=NULL) {
#ifdef DEBUG        
        wr_printf("\nsubsumed by ground hash as neg!\n"); 
        wr_print_term(g,xatomptr);
        wr_printf("\n bucket:\n");
        wr_print_clause(g,bucket); 
#endif        
        (g->stat_lit_hash_subsume_ok)++;
        (g->stat_forward_subsumed)++;
        return -1;
      }
      if (r_kb_g(g)) {
#ifdef DEBUG
        wr_printf("\nexternal kb r_kb_g(g) present");
#endif  
        if (g->forward_subsume_derived)      
          bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_neg_groundunits),xatomptr,kbhash);     
        else
          bucket=NULL;  
        if (bucket!=NULL) {
  #ifdef DEBUG        
          wr_printf("\nsubsumed by kb ground hash as neg!\n"); 
          wr_print_term(g,xatomptr);
          wr_printf("\n bucket:\n");
          wr_print_clause(g,bucket); 
  #endif        
          (g->stat_lit_hash_subsume_ok)++;
          (g->stat_forward_subsumed)++;
          return -1;
        }
      }  

      // unit_subsume_match: a negative unit that matches the literal
      if ((g->unit_subsume_match) && (g->forward_subsume_derived) && wr_ucm_find(g,xatom,1)) {
        (g->stat_usm_subsumed)++;
        (g->stat_forward_subsumed)++;
        return -1;
      }
      bucket=(cutrep ? NULL : wr_find_termhash_unit(g,rotp(g,g->hash_pos_groundunits),xatomptr,uhash));
      if (bucket!=NULL) {         
#ifdef DEBUG        
        wr_printf("\ncutoff by ground hash as neg!\n"); 
        wr_print_term(g,xatom);
        wr_printf("\n bucket:\n");
        wr_print_clause(g,bucket);        
#endif
        cuts++;
        rptr[tmp+LIT_META_POS]=0; // mark cutoff lit metas as 0
        if ((g->cut_clvec)[0]>cuts+1) {
          (g->cut_clvec)[cuts]=(gint)bucket;
          (g->cut_clvec)[cuts+1]=(gint)NULL;
        }  
        (g->stat_lit_hash_cut_ok)++;   
        //return 0;
      }
      if (r_kb_g(g) && rptr[tmp+LIT_META_POS]) {
#ifdef DEBUG
        wr_printf("\nexternal kb r_kb_g(g) present");
#endif          
        bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_pos_groundunits),xatomptr,kbhash);      
        if (bucket!=NULL) {         
  #ifdef DEBUG        
          wr_printf("\ncutoff by kb ground hash as neg!\n"); 
          wr_print_term(g,xatom);
          wr_printf("\n bucket:\n");
          wr_print_clause(g,bucket);        
  #endif        
          cuts++;
          rptr[tmp+LIT_META_POS]=0; // mark cutoff lit metas as 0
          if ((g->cut_clvec)[0]>cuts+1) {
            (g->cut_clvec)[cuts]=(gint)bucket;
            (g->cut_clvec)[cuts+1]=(gint)NULL;
          }  
          (g->stat_lit_hash_cut_ok)++;   
          //return 0;
        }
      }

      // unit_cut_match: a positive unit that matches the literal
      if ((g->unit_cut_match) && rptr[tmp+LIT_META_POS] && (bucket=wr_ucm_find(g,xatom,0))!=NULL) {
        cuts++;
        rptr[tmp+LIT_META_POS]=0; // mark cutoff lit metas as 0
        if ((g->cut_clvec)[0]>cuts+1) {
          (g->cut_clvec)[cuts]=(gint)bucket;
          (g->cut_clvec)[cuts+1]=(gint)NULL;
        }
        (g->stat_ucm_cut)++;
      }
    } else {
#ifdef DEBUG      
      wr_printf("\npos\n");
#endif
      if ((g->forward_subsume_derived) && !subrep)
        bucket=wr_find_termhash_unit(g,rotp(g,g->hash_pos_groundunits),xatomptr,uhash);
      else
        bucket=NULL;  
      //printf("\ncppos\n");
      if (bucket!=NULL) {
#ifdef DEBUG        
        wr_printf("\nsubsumed by ground hash as pos!\n");
        wr_print_term(g,xatom);;
        wr_printf("\n bucket:\n");
        wr_print_clause(g,bucket);
#endif        
        (g->stat_lit_hash_subsume_ok)++;
        (g->stat_forward_subsumed)++;
        return -1;
      }
      if (r_kb_g(g)) {
#ifdef DEBUG
        wr_printf("\nexternal kb r_kb_g(g) present");
#endif          
        if (g->forward_subsume_derived)   
          bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_pos_groundunits),xatomptr,kbhash);      
        else
          bucket=NULL;  
        if (bucket!=NULL) {
  #ifdef DEBUG        
          wr_printf("\nsubsumed by kb ground hash as pos!\n"); 
          wr_print_term(g,xatomptr);
          wr_printf("\n bucket:\n");
          wr_print_clause(g,bucket); 
  #endif        
          (g->stat_lit_hash_subsume_ok)++;
          (g->stat_forward_subsumed)++;
          return -1;
        }
      }  
      // unit_subsume_match: a positive unit that matches the literal
      if ((g->unit_subsume_match) && (g->forward_subsume_derived) && wr_ucm_find(g,xatom,0)) {
        (g->stat_usm_subsumed)++;
        (g->stat_forward_subsumed)++;
        return -1;
      }
      bucket=(cutrep ? NULL : wr_find_termhash_unit(g,rotp(g,g->hash_neg_groundunits),xatomptr,uhash));
      if (bucket!=NULL) {            
#ifdef DEBUG        
        wr_printf("\ncutoff by ground hash as pos!\n"); 
        wr_print_term(g,xatom);
        wr_printf("\n bucket:\n");
        wr_print_clause(g,bucket);    
#endif        
        cuts++;
        rptr[tmp+LIT_META_POS]=0; // mark cutoff lit metas as 0
        if ((g->cut_clvec)[0]>cuts+1) {
          (g->cut_clvec)[cuts]=(gint)bucket;
          (g->cut_clvec)[cuts+1]=(gint)NULL;
        }  
        (g->stat_lit_hash_cut_ok)++;       
        //return 0;
      }
      if (r_kb_g(g) && rptr[tmp+LIT_META_POS]) {
#ifdef DEBUG
        wr_printf("\nexternal kb r_kb_g(g) present");
#endif          
        bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_neg_groundunits),xatomptr,kbhash);      
        if (bucket!=NULL) {         
  #ifdef DEBUG        
          wr_printf("\ncutoff by kb ground hash as pos!\n"); 
          wr_print_term(g,xatom);
          wr_printf("\n bucket:\n");
          wr_print_clause(g,bucket);        
  #endif        
          cuts++;
          rptr[tmp+LIT_META_POS]=0; // mark cutoff lit metas as 0
          if ((g->cut_clvec)[0]>cuts+1) {
            (g->cut_clvec)[cuts]=(gint)bucket;
            (g->cut_clvec)[cuts+1]=(gint)NULL;
          }  
          (g->stat_lit_hash_cut_ok)++;   
          //return 0;
        }
      }  
    } 
    if (!wg_atom_meta_is_neg(db,xatommeta) && (g->unit_cut_match) && rptr[tmp+LIT_META_POS] &&
        (bucket=wr_ucm_find(g,xatom,1))!=NULL) {
      // unit_cut_match: a negative unit that matches the positive literal
      cuts++;
      rptr[tmp+LIT_META_POS]=0; // mark cutoff lit metas as 0
      if ((g->cut_clvec)[0]>cuts+1) {
        (g->cut_clvec)[cuts]=(gint)bucket;
        (g->cut_clvec)[cuts+1]=(gint)NULL;
      }
      (g->stat_ucm_cut)++;
    }
    if (clhashptr!=NULL && (oldcuts==cuts)) {
      // no cut during this iteration
#ifdef USE_LIT_HASH64
      // the clause hash keeps the old literal hash: the duplicate check of
      // instgen (wr_find_prop_clausehash) compares clauses modulo variables,
      // so its result depends on which clause hashes collide
      clhash+=(r_kb_g(g) ? kbhash : wr_lit_hash_kb(g,xatom));
#else
      clhash+=hash;
#endif
      if (wg_atom_meta_is_neg(db,xatommeta)) clhash+=10;      
      //if (wg_atom_meta_is_neg(db,xatommeta)) hash+=10;
      //clhash = hash + (clhash << 6) + (clhash << 16) - clhash;
      if (clhash<0) clhash=0-clhash;
      clhash=(gint)(0+(clhash%(NROF_CLTERM_HASHVEC_ELS-2)));
      if (clhash<0) clhash=0-clhash;
    }     
    //blt=wr_build_calc_term(g,rptr[tmp+LIT_ATOM_POS]);       
  }
  if (clhashptr!=NULL) {
    *clhashptr=clhash;
    //printf("\nabout to call wr_find_prop_clausehash with clhash %d and rpos %d\n",clhash,rpos);
    if (wr_find_prop_clausehash(g,rotp(g,g->prop_hash_clauses),rptr,rpos,clhash)) {
      //printf("\nwr_find_prop_clausehash returned 1\n");
      return -1;
    }
  }
#ifdef DEBUG  
  wr_printf("\n! wr_derived_cl_cut_and_subsume is returning %d \n",cuts);
#endif  
  return cuts;  
}

/*


  if (tmp==0 && !doublecut_found && len>1) {
    // attempt double cut
    tmp3=wr_atom_doublecut(g,yatom,xmeta,xcl,i,&foundbucket2); 
  } else {
    tmp3=2;
  }


*/


/*

  return -1 if subsumed
  return 0 if no subsumption, no cuts
  return 1 if any cuts

  //skipcl is a clause which should be always skipped when comparing (set to NULL if not used)
  if subsflag==0 then no subsumption done

*/

int wr_atom_cut_and_subsume(glb* g, gint xatom, gint xatommeta, cvec* foundbucket, int subsflag) {
  gptr xatomptr;
  cvec bucket;
  gint kbhash, hash, uhash;
  // the key value 2: the unit trees in place of the local unit tables, also at the pick
  int cutrep=wr_ucm_replaces(g,g->unit_cut_match), subrep=wr_ucm_replaces(g,g->unit_subsume_match);
  // the key value 1: the tree after the local tables, also at the pick
  int cutaft=wr_ucm_after(g,g->unit_cut_match), subaft=wr_ucm_after(g,g->unit_subsume_match);

#ifdef DEBUG
  wr_printf("\n!!!!! wr_atom_cut_and_subsume is called \n");    
  wr_printf("\n xatom %d xatommeta %d atom:\n",xatom,xatommeta);
  wr_print_term(g,xatom);  
#endif     
  xatomptr=rotp(g,xatom);
  hash=wr_lit_hash(g,xatom); uhash=((cutrep && subrep) ? 0 : wr_lit_hash_unit(g,xatom,hash)); // the unit tables 
  kbhash=(r_kb_g(g) ? wr_lit_hash_kb(g,xatom) : hash); // a shared KB keeps the old hash
  if (wg_atom_meta_is_neg(db,xatommeta)) {
    //printf("\nneg\n");
    if (subsflag) {
      bucket=(subrep ? wr_ucm_pick(g,xatom,1,1) : wr_find_termhash_unit(g,rotp(g,g->hash_neg_groundunits),xatomptr,uhash));
      if (bucket==NULL && subaft) bucket=wr_ucm_pick(g,xatom,1,1);      
      //printf("\ncpneg\n");
      if (bucket!=NULL) {
  #ifdef DEBUG
        wr_printf("\nsubsumed by ground hash as neg!\n"); 
        wr_print_term(g,xatom);
        wr_printf("\n bucket:\n");
        wr_print_clause(g,bucket); 
  #endif    
        return -1;
      }      
      if (r_kb_g(g)) {
        bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_neg_active_groundunits),xatomptr,kbhash);
        if (bucket!=NULL) {
    #ifdef DEBUG
          wr_printf("\nsubsumed by ground hash as neg!\n"); 
          wr_print_term(g,xatom);
          wr_printf("\n bucket:\n");
          wr_print_clause(g,bucket); 
    #endif    
          return -1;
        }
      }                   
    }  
    bucket=(cutrep ? wr_ucm_pick(g,xatom,0,0) : wr_find_termhash_unit(g,rotp(g,g->hash_pos_groundunits),xatomptr,uhash));
      if (bucket==NULL && cutaft) bucket=wr_ucm_pick(g,xatom,0,0);      
    if (bucket!=NULL) {         
#ifdef DEBUG            
      wr_printf("\ncutoff by ground hash as neg!\n"); 
      wr_print_term(g,xatom);
      wr_printf("\n bucket:\n");
      wr_print_clause(g,bucket);        
#endif
      *foundbucket=bucket;      
      return 1;
    }
    if (r_kb_g(g)) {
      bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_pos_active_groundunits),xatomptr,kbhash);
      if (bucket!=NULL) {
  #ifdef DEBUG
        wr_printf("\ncutoff by ground hash as neg!\n"); 
        wr_print_term(g,xatom);
        wr_printf("\n bucket:\n");
        wr_print_clause(g,bucket);
  #endif    
        *foundbucket=bucket;
        return 1;
      }
    }     

  } else {
    //printf("\npos\n");
    if (subsflag) {
      bucket=(subrep ? wr_ucm_pick(g,xatom,0,1) : wr_find_termhash_unit(g,rotp(g,g->hash_pos_groundunits),xatomptr,uhash));
      if (bucket==NULL && subaft) bucket=wr_ucm_pick(g,xatom,0,1);
      //printf("\ncppos\n");
      if (bucket!=NULL) {
  #ifdef DEBUG
        wr_printf("\nsubsumed by ground hash as pos!\n");
        wr_print_term(g,xatom);
        wr_printf("\n bucket:\n");
        wr_print_clause(g,bucket);
  #endif
        return -1;
      }
      if (r_kb_g(g)) {
        bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_pos_active_groundunits),xatomptr,kbhash);
        if (bucket!=NULL) {
    #ifdef DEBUG
          wr_printf("\nsubsumed by ground hash as pos!\n"); 
          wr_print_term(g,xatom);
          wr_printf("\n bucket:\n");
          wr_print_clause(g,bucket); 
    #endif    
          return -1;
        }
      }       
    }  
    bucket=(cutrep ? wr_ucm_pick(g,xatom,1,0) : wr_find_termhash_unit(g,rotp(g,g->hash_neg_groundunits),xatomptr,uhash));
      if (bucket==NULL && cutaft) bucket=wr_ucm_pick(g,xatom,1,0);      
    if (bucket!=NULL) {            
#ifdef DEBUG       
      wr_printf("\ncutoff by ground hash as pos!\n"); 
      wr_print_term(g,xatom);
      wr_printf("\n bucket:\n");
      wr_print_clause(g,bucket);    
#endif
      *foundbucket=bucket;      
      return 1;
    }
    if (r_kb_g(g)) {
      bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_neg_active_groundunits),xatomptr,kbhash);
      if (bucket!=NULL) {
  #ifdef DEBUG
        wr_printf("\ncutoff by ground hash as pos!\n"); 
        wr_print_term(g,xatom);
        wr_printf("\n bucket:\n");
        wr_print_clause(g,bucket);
  #endif    
        *foundbucket=bucket;
        return 1;
      }
    }     
  }   
#ifdef DEBUG
  wr_printf("\n!!!!! wr_atom_cut_and_subsume found no subs or cuts \n");
#endif     
  return 0;  
}


/*

  return -1 if subsumed
  return 0 if no subsumption, no cuts
  return 1 if any cuts

  //skipcl is a clause which should be always skipped when comparing (set to NULL if not used)
  if subsflag==0 then no subsumption done

*/

#ifdef USE_LIT_HASH64

/* the bucket walk of wr_atom_doublecut_h for a bucket that is at hand */

static int wr_atom_doublecut_bucket(glb* g, gint xatom, gint xatommeta, gptr xcl, int pos,
                                    cvec* foundbucket, gptr bucket_asp) {
  int j;

  for(j=2;j<bucket_asp[0] && j<bucket_asp[1]; j=j+2) {
    if (wr_atom_doublecut_aux(g,xatom,xatommeta,xcl,pos,rotp(g,bucket_asp[j+1]))) {
      *foundbucket=rotp(g,bucket_asp[j+1]);
      return 1;
    }
  }
  return 0;
}

/*
  The pair table of the double cut (hash_doublepairs). A two-literal clause
  Y = {P, Q} has two keys: (P, Q) and (Q, P), where the first literal is the
  one complementary to the literal that a double cut removes and the second
  one must occur in the clause that is cut. An entry holds the key, Y and
  the insertion number of Y. The full check wr_atom_doublecut_aux confirms
  each candidate, and the candidates are checked in the order of their
  insertion, which is the order of the bucket walk of wr_atom_doublecut_h:
  so the same partner is found.
*/

#define DOUBLEPAIR_BUCKETSIZE 6
#define DOUBLEPAIR_MAXCAND 64

static uint64_t wr_doublepair_key(gint hc, int negc, gint hm, int negm) {
  uint64_t a=(((uint64_t)hc)<<1)|(negc ? 1 : 0);
  uint64_t b=(((uint64_t)hm)<<1)|(negm ? 1 : 0);
  uint64_t k=(a*UINT64_C(0x9E3779B97F4A7C15))^(b*UINT64_C(0xC2B2AE3D27D4EB4F)+(a<<6)+(a>>2));
  k^=k>>31;
  k*=UINT64_C(0xBF58476D1CE4E5B9);
  k^=k>>29;
  return k;
}

static void wr_push_doublepair_key(glb* g, gint* hasharr, uint64_t key, gptr cl, gint seq) {
  gint idx, bucket, nbucket;
  cvec vec;

  idx=1+(gint)(key%(uint64_t)(hasharr[0]-1));
  bucket=hasharr[idx];
  if (!bucket) {
    vec=wr_cvec_new(g,DOUBLEPAIR_BUCKETSIZE);
    if (vec==NULL) { (g->alloc_err)=1; return; }
    bucket=rpto(g,vec);
    hasharr[idx]=bucket;
  }
  vec=wr_cvec_push(g,rotp(g,bucket),(gint)key);
  if (vec!=NULL) vec=wr_cvec_push(g,vec,rpto(g,cl));
  if (vec!=NULL) vec=wr_cvec_push(g,vec,seq);
  if (vec==NULL) { (g->alloc_err)=1; return; }
  nbucket=rpto(g,vec);
  if (nbucket!=bucket) hasharr[idx]=nbucket;
}

/* insert the two-literal clause cl with the literal hashes h0, h1 */

void wr_push_doublepair(glb* g, gptr cl, gint h0, int neg0, gint h1, int neg1) {
  gint* hasharr;
  gint seq;

  if (!(g->hash_doublepairs) || (g->inkb)) return;
  hasharr=rotp(g,g->hash_doublepairs);
  seq=(g->doublepair_seq)++;
  wr_push_doublepair_key(g,hasharr,wr_doublepair_key(h0,neg0,h1,neg1),cl,seq);
  wr_push_doublepair_key(g,hasharr,wr_doublepair_key(h1,neg1,h0,neg0),cl,seq);
}

/*
  The double cut of the literal at position pos of xcl (len literals,
  hashes: the literal hashes of xcl by position) through the pair table.
  Returns 1 with *foundbucket set, 0 when no stored clause cuts it, and -1
  when the table is missing or there are too many candidates: then the
  caller walks the bucket of the atom.
*/

static int wr_atom_doublecut_pairs(glb* g, gint xatom, gint xatommeta, gptr xcl, int len, int pos,
                                   gint* hashes, cvec* foundbucket) {
  void* db=g->db;
  gint* hasharr;
  UNUSED(db);
  gint candseq[DOUBLEPAIR_MAXCAND], candcl[DOUBLEPAIR_MAXCAND];
  gint atom, bucket, ts, tc, lastseq;
  gptr vec;
  uint64_t key;
  int j, k, n=0, negc;

  if (!(g->hash_doublepairs)) return -1;
  hasharr=rotp(g,g->hash_doublepairs);
  negc=!wg_atom_meta_is_neg(db,xatommeta); // the stored literal has the other sign
  for(j=0;j<len;j++) {
    atom=wg_get_rule_clause_atom(db,xcl,j);
    if (atom==xatom) continue; // as in the full check: not the cut atom itself
    key=wr_doublepair_key(hashes[pos],negc,hashes[j],
                          wg_atom_meta_is_neg(db,wg_get_rule_clause_atom_meta(db,xcl,j)));
    bucket=hasharr[1+(gint)(key%(uint64_t)(hasharr[0]-1))];
    if (!bucket) continue;
    vec=rotp(g,bucket);
    for(k=2;k<vec[1];k+=3) {
      if (vec[k]!=(gint)key) continue;
      if (n>=DOUBLEPAIR_MAXCAND) return -1;
      candcl[n]=vec[k+1];
      candseq[n]=vec[k+2];
      n++;
    }
  }
  // insertion sort by insertion number: the order of the bucket walk
  for(j=1;j<n;j++) {
    ts=candseq[j]; tc=candcl[j];
    for(k=j-1; k>=0 && candseq[k]>ts; k--) { candseq[k+1]=candseq[k]; candcl[k+1]=candcl[k]; }
    candseq[k+1]=ts; candcl[k+1]=tc;
  }
  lastseq=-1;
  for(j=0;j<n;j++) {
    if (candseq[j]==lastseq) continue; // the same clause found by two probes
    lastseq=candseq[j];
    if (wr_atom_doublecut_aux(g,xatom,xatommeta,xcl,pos,rotp(g,candcl[j]))) {
      *foundbucket=rotp(g,candcl[j]);
      return 1;
    }
  }
  return 0;
}

#endif

/*
  The double cut of the literal at position pos of xcl (len literals) with
  the literal hash hash of xatom. hashes: the literal hashes of xcl by
  position, or NULL. Every stored clause that the full check accepts is in
  the bucket of xatom, so an empty bucket gives 0. A bucket with more
  entries than DOUBLEPAIR_MINENTRIES times the number of pair probes is
  looked up in the pair table; the result is the same as the bucket walk of
  wr_atom_doublecut_h.
*/

int wr_atom_doublecut_hp(glb* g, gint xatom, gint xatommeta, gptr xcl, int len, int pos,
                         cvec* foundbucket, gint hash, gint* hashes) {
#ifdef USE_LIT_HASH64
  gint bucket;
  gptr bucket_asp;
  int res;

  if (hashes && len<=DOUBLEPAIR_MAXLEN && (g->hash_doublepairs)) {
    if (wg_atom_meta_is_neg(db,xatommeta))
      bucket=wr_find_offset_termbucket(g,rotp(g,g->hash_pos_grounddoubles),rotp(g,xatom),hash);
    else
      bucket=wr_find_offset_termbucket(g,rotp(g,g->hash_neg_grounddoubles),rotp(g,xatom),hash);
    if (!bucket) return 0;
    bucket_asp=rotp(g,bucket);
    if ((bucket_asp[1]-2)/2 > DOUBLEPAIR_MINENTRIES*(len-1)) {
      res=wr_atom_doublecut_pairs(g,xatom,xatommeta,xcl,len,pos,hashes,foundbucket);
      if (res>=0) return res;
    }
    return wr_atom_doublecut_bucket(g,xatom,xatommeta,xcl,pos,foundbucket,bucket_asp);
  }
#endif
  return wr_atom_doublecut_h(g,xatom,xatommeta,xcl,pos,foundbucket,hash);
}

int wr_atom_doublecut(glb* g, gint xatom, gint xatommeta, gptr xcl, int pos, cvec* foundbucket) {
  return wr_atom_doublecut_h(g,xatom,xatommeta,xcl,pos,foundbucket,wr_lit_hash(g,xatom));
}

/* wr_atom_doublecut with the literal hash of xatom given by the caller */

int wr_atom_doublecut_h(glb* g, gint xatom, gint xatommeta, gptr xcl, int pos,
                        cvec* foundbucket, gint hash) {
  gptr xatomptr;
  //cvec bucket;
  gint bucket;
  gptr bucket_asp;
  int j;

  /*
  wr_printf("\n!!!!! wr_atom_doublecut is called \n");    
  wr_printf("\n xatom %d xatommeta %d atom:\n",xatom,xatommeta);
  wr_print_term(g,xatom);  
  printf("\nclause\n");
  wr_print_clause(g,xcl);  
  printf("\n");
  */

#ifdef DEBUG
  wr_printf("\nwr_atom_doublecut is called \n");    
  wr_printf("\n xatom %d xatommeta %d atom:\n",xatom,xatommeta);
  wr_print_term(g,xatom);  
#endif     
  
  xatomptr=rotp(g,xatom);
  if (wg_atom_meta_is_neg(db,xatommeta)) {
    //printf("\nneg atom\n");   
    //bucket=wr_find_termhash(g,rotp(g,g->hash_pos_grounddoubles),xatomptr,hash);  
    
    //--- new part starts
    bucket=wr_find_offset_termbucket(g,rotp(g,g->hash_pos_grounddoubles),xatomptr,hash);
    if (bucket) {
      // printf("\nbucket found\n");
      //printf("bucket for hash %d size %d next free %d\n",hash,bucket[0],bucket[1]);
      bucket_asp=rotp(g,bucket);
      for(j=2;j<bucket_asp[0] && j<bucket_asp[1]; j=j+2) {
        /*
        printf("j %d meaning %d term ",j,(j-2)/2);
        //wr_print_term(g,rpto(g,bucket_asp[j]));
        wr_print_term(g,bucket_asp[j]);
        printf(" in cl \n");
        wr_print_clause(g,rotp(g,bucket_asp[j+1]));
        printf("\n");
        */

        if (wr_atom_doublecut_aux(g,xatom,xatommeta,xcl,pos,rotp(g,bucket_asp[j+1]))) {
          //CP10
          /*
          printf("\nmatch found for\n");
          wr_print_clause(g,xcl);
          printf("\nwith double\n");
          wr_print_clause(g,rotp(g,bucket_asp[j+1]));
          printf("\n");
          */
          *foundbucket=rotp(g,bucket_asp[j+1]);      
          return 1;
        }         
      }
    }
  } else {
    //printf("\npos atom\n");    
    //bucket=wr_find_termhash(g,rotp(g,g->hash_pos_grounddoubles),xatomptr,hash);  
    
    //--- new part starts
    bucket=wr_find_offset_termbucket(g,rotp(g,g->hash_neg_grounddoubles),xatomptr,hash);
    if (bucket) {
      //printf("\nbucket found\n");
      //printf("bucket for hash %d size %d next free %d\n",hash,bucket[0],bucket[1]);
      bucket_asp=rotp(g,bucket);
      for(j=2;j<bucket_asp[0] && j<bucket_asp[1]; j=j+2) {
        
        /*
        printf("j %d meaning %d term ",j,(j-2)/2);
        //wr_print_term(g,rpto(g,bucket_asp[j]));
        wr_print_term(g,bucket_asp[j]);
        printf(" in cl \n");
        wr_print_clause(g,rotp(g,bucket_asp[j+1]));
        printf("\n");
        */
        if (wr_atom_doublecut_aux(g,xatom,xatommeta,xcl,pos,rotp(g,bucket_asp[j+1]))) {
          //CP10
          /*
          printf("\nmatch found for\n");
          wr_print_clause(g,xcl);
          printf("\nwith double\n");
          wr_print_clause(g,rotp(g,bucket_asp[j+1]));
          printf("\n");
          */
          *foundbucket=rotp(g,bucket_asp[j+1]);      
          return 1;
        }         
      }
    }
  }   
#ifdef DEBUG
  wr_printf("\n!!!!! wr_atom_cut_and_subsume found no subs or cuts \n");
#endif     
  return 0;  
}

int wr_atom_doublecut_aux(glb* g, gint xatom, gint xatommeta, gptr xcl, int pos, cvec bucket) {
  int i, j, xcllen, posmatch, negmatch;
  gptr ycl;
  gint yclatom, xclatom, xclmeta,yclmeta;
  int iused,jused;

  /*
  printf("\nwr_atom_doublecut_aux called, pos %d\n",pos);
  printf("\nxatom ");
  wr_print_term(g,xatom);
  printf("\nxcl ");
  wr_print_clause(g,xcl); 
  printf("\nbucket ");
  wr_print_clause(g,bucket); 
  */

  posmatch=0;
  negmatch=0;
  iused=-1;
  jused=-1;
  ycl=bucket; 
  xcllen=wg_count_clause_atoms(db,xcl);
  for(i=0;i<2;i++) {
    yclmeta=wg_get_rule_clause_atom_meta(db,ycl,i);
    yclatom=wg_get_rule_clause_atom(db,ycl,i); 
    //printf("\ni %d ",i); 
    //wr_print_term(g,yclatom);
    //printf("\n");   
    for(j=0;j<xcllen;j++) {
      //if (i==iused || j==jused) continue;
      //printf("\nj %d ",j);   
      //if (j==posused || j==negused) continue;
      xclmeta=wg_get_rule_clause_atom_meta(db,xcl,j);      
      xclatom=wg_get_rule_clause_atom(db,xcl,j); 
      //wr_print_term(g,xclatom);
      //printf("\n"); 
      if (xatom!=xclatom && 
          ( (wg_atom_meta_is_neg(db,yclmeta) && wg_atom_meta_is_neg(db,xclmeta)) ||
            (!wg_atom_meta_is_neg(db,yclmeta) && !wg_atom_meta_is_neg(db,xclmeta)) )) {
        // same polarities and atom is not the same as cutoff atom
        if (jused!=j && iused!=i &&
            ((yclmeta&(ATOM_META_CANSUBS_MASK<<SMALLINTSHFT))==(xclmeta&(ATOM_META_CANSUBS_MASK<<SMALLINTSHFT))) &&
            wr_equal_term(g,yclatom,xclatom,1)) {
          //printf("\nposmatch found\n");    
          //wr_print_term(g,yclatom);
          //wr_print_term(g,xclatom);
          posmatch=1;
          //posused=yclatom;
          //xclatomused=xclatom;
          //yclatomused=yclatom;
          iused=i;
          jused=j;
          if (negmatch) {
            //printf("\n!!! same found\n"); 
            return 1;
          }
        }
      } else {
        // different polarities and atom is the same as cutoff atom
        if (xatom==xclatom && xatommeta==xclmeta &&
            jused!=j && iused!=i &&
            ( (wg_atom_meta_is_neg(db,yclmeta) && !wg_atom_meta_is_neg(db,xclmeta)) ||
              (!wg_atom_meta_is_neg(db,yclmeta) && wg_atom_meta_is_neg(db,xclmeta))) && 
            ((yclmeta&(ATOM_META_CANSUBS_MASK<<SMALLINTSHFT))==(xclmeta&(ATOM_META_CANSUBS_MASK<<SMALLINTSHFT))) && 
            wr_equal_term(g,yclatom,xclatom,1)) {
          //printf("\nnegmatch found\n");     
          //wr_print_term(g,yclatom);
          //wr_print_term(g,xclatom);
          negmatch=1;
          //negused=yclatom;
          //xclatomused=xclatom;
          iused=i;
          jused=j;
          if (posmatch) {
            //printf("\n!!! diff found\n");
            //CP12; 
            return 1;
          }
        }
      }
    }    
  }

  return 0;
}

/*

  return -1 if subsumed
  return 0 if no subsumption, no cuts
  return a nr of cuts (more than 0) if any cuts

*/

int wr_derived_cl_strong_cut(glb* g, gptr rptr, int rpos, int prevcuts) {

  int i,tmp;
  gint xatom,xatommeta;
  int cuts=0;
  //gint clhash;
  gptr cutter;
  int storecount,nongroundcount;


#ifdef DEBUG
  wr_printf("\nwr_derived_cl_strong_cut is called \n");
#endif          
  storecount=0;        
  nongroundcount=0;
  for(i=0; i < rpos; i++) { 
    tmp=i*LIT_WIDTH;    
    xatommeta=rptr[tmp+LIT_META_POS];    
    if (xatommeta!=0) storecount++;    
  }
  // now storecount is the length uncut
  if (storecount>1) {
    // here check whether there are more than one non-ground
    for(i=0; i < rpos; i++) { 
      tmp=i*LIT_WIDTH;    
      xatommeta=rptr[tmp+LIT_META_POS];    
      if (!xatommeta) continue;    
      xatom=rptr[tmp+LIT_ATOM_POS];
      if (wr_term_has_vars(g,xatom)) {
        nongroundcount++;
      }   
      //printf("\n pos %d nongroundcount %d ",i,nongroundcount);
      //wr_print_term(g,xatom);
      //printf("\n");
      if (nongroundcount>1) break;
    }
  }

  /* 
    clause length 1: full cutoff, no var checking
    clause contains a single non-ground: full cutoff, no var checking
    clause contains several non-ground: use atom-to-atom subsumption, not unification

   */

#ifdef DEBUG     
    wr_printf("\n storecount %d \n",storecount);    
#endif   

  for(i=0;i<rpos;i++) { 
    tmp=i*LIT_WIDTH;    
    xatommeta=rptr[tmp+LIT_META_POS];   
    if (xatommeta==0) continue;
    xatom=rptr[tmp+LIT_ATOM_POS];
    //oldcuts=cuts;
#ifdef SCUTDEBUG     
    printf("\n i %d xatom %d xatommeta %d atom:\n",i,xatom,xatommeta);
    wr_print_term(g,xatom);    
#endif       
    if (wg_atom_meta_is_neg(db,xatommeta)) {
#ifdef SCUTDEBUG      
      printf("\nneg\n");
#endif                
      cutter=strongly_cut_atom(g,xatom,rotp(g,g->hash_pos_units),(nongroundcount>1));
      if (cutter!=NULL) {         
#ifdef SCUTDEBUG        
        printf("\ncutoff as neg!\n"); 
        wr_print_term(g,xatom);
        printf("\n cutter:\n");
        wr_print_clause(g,cutter);        
#endif
        cuts++;
        rptr[tmp+LIT_META_POS]=0; // mark cutoff lit metas as 0
        if ((g->cut_clvec)[0]>prevcuts+cuts+1) {
          (g->cut_clvec)[prevcuts+cuts]=(gint)cutter;
          (g->cut_clvec)[prevcuts+cuts+1]=(gint)NULL;
        }  
        (g->stat_lit_strong_cut_ok)++; 
      }      
    } else {
#ifdef SCUTDEBUG      
      printf("\npos\n");
#endif 
      cutter=strongly_cut_atom(g,xatom,rotp(g,g->hash_neg_units),(nongroundcount>1));
      if (cutter!=NULL) {              
#ifdef SCUTDEBUG        
        printf("\ncutoff  as pos!\n"); 
        wr_print_term(g,xatom);
        wr_printf("\n bucket:\n");
        wr_print_clause(g,cutter);    
#endif        
        cuts++;
        rptr[tmp+LIT_META_POS]=0; // mark cutoff lit metas as 0
        if ((g->cut_clvec)[0]>prevcuts+cuts+1) {
          (g->cut_clvec)[prevcuts+cuts]=(gint)cutter;
          (g->cut_clvec)[prevcuts+cuts+1]=(gint)NULL;
        }  
        (g->stat_lit_strong_cut_ok)++;       
      }
    }     
  }
 
#ifdef SCUTDEBUG  
  wr_printf("\n! wr_derived_cl_strong_cut is returning %d \n",cuts);
#endif  
  return cuts;  
}


/* ====================================================

  index of the unit clauses for the strong cut

  With the key strong_cut at 1, the units of the strong cut are also filed
  in two trees of the module termindex.c, one per sign, beside the lists
  of hash_pos_units and hash_neg_units (see the comment at wr_sci_insert
  below). The lookup by the tree gives the cutter that the lists give.

  The lists stay filled: they answer without the key, in a 32-bit build,
  after a failed allocation, and with unify_samelen 0 or unify_maxuseterms
  set.

================================================ */

#ifdef USE_SCUT_INDEX

#ifdef GKC_SCUT_CHECK
/* the check build: both lookups, the answer of the lists, the differences on stderr */
static long sci_check_calls=0, sci_check_answer_diff=0, sci_check_cutter_diff=0, sci_check_dirty=0;
static void sci_check_report(void) {
  fprintf(stderr,"SCI_CHECK lookups %ld answer_differences %ld cutter_differences %ld dirty_varstack %ld\n",
          sci_check_calls,sci_check_answer_diff,sci_check_cutter_diff,sci_check_dirty);
}
__attribute__((constructor)) static void sci_check_init(void) { atexit(sci_check_report); }
static void sci_check_run_end(void) {
  sci_check_report();
  sci_check_calls=0; sci_check_answer_diff=0; sci_check_cutter_diff=0; sci_check_dirty=0;
}
#endif

/* The units of the strong cut in two trees of the module termindex.c (one
   per sign, the uncompressed form), filled where the lists are filled,
   only with the key strong_cut at 1 (the rule of the default runs keeps
   the lists: the cheaper inserts of the lists gave the same time there).
   Each entry is a unit (its atom and clause) with its place in the order of
   the lists: the units with a shorter prefix first, the newest first.

   Matching (usematch: the clause has more than one literal with
   variables): the perfect walk of the module decides. The slots of a
   stored unit are bound and compared in the walk, and the rank of an entry
   is its place in the order of the lists, so the walk gives the first unit
   of the lists that matches (ti_match_best).

   Unification (at most one literal with variables): a filter. The module
   gives the candidates (a walk that binds nothing: ti_candidates); they are
   sorted in the order of the lists and get the test of the lists
   (wr_unify_term with its occurs check), and the first that passes is the
   cutter. */

typedef struct { gint atom, cl; uint64_t order; } wr_sci_entry;
typedef struct {
  ti_index* t[2];                          // the trees of the positive and the negative units
  wr_sci_entry* ents; int32_t encnt, encap;
  uint64_t* cand; int32_t ccnt, ccap;      // candidates: the order key of each
  int32_t* candent;                        // candidates: the entry of each
  uint64_t seq;
  gint skipatom, skipcl;                   // a unit that the lookup leaves out (tested before with the bindings)
} wr_sci_t;

#define SCI_SEQMAX ((UINT64_C(1)<<48)-1)
#define SCI_RANKMAX (1<<28)

void wr_sci_free(glb* g) {
  wr_sci_t* x=(wr_sci_t*)(g->sci_index);
#ifdef GKC_SCUT_CHECK
  sci_check_run_end();
#endif
  if (!x) return;
  if (x==(void*)1) { (g->sci_index)=NULL; return; }
  if (x->t[0]) ti_free(x->t[0]);
  if (x->t[1]) ti_free(x->t[1]);
  if (x->ents) sys_free(x->ents);
  if (x->cand) sys_free(x->cand);
  if (x->candent) sys_free(x->candent);
  sys_free(x);
  (g->sci_index)=NULL;
}

static void wr_sci_fail(glb* g) {
  wr_sci_free(g);
  (g->sci_index)=(void*)1; // the lists answer for the rest of the run
}

static wr_sci_t* wr_sci_make(glb* g) {
  wr_sci_t* x=sys_malloc(sizeof(wr_sci_t));
  if (!x) return NULL;
  memset(x,0,sizeof(wr_sci_t));
  x->encap=1<<10; x->ccap=1<<8;
  x->ents=sys_malloc(sizeof(wr_sci_entry)*x->encap);
  x->cand=sys_malloc(sizeof(uint64_t)*x->ccap);
  x->candent=sys_malloc(sizeof(int32_t)*x->ccap);
  x->t[0]=ti_new((g->varbanks)+TREE_VARBANK*NROF_VARSINBANK,0);
  x->t[1]=ti_new((g->varbanks)+TREE_VARBANK*NROF_VARSINBANK,0);
  (g->sci_index)=x;
  if (!x->ents || !x->cand || !x->candent || !x->t[0] || !x->t[1]) return NULL;
  return x;
}

/* file a unit of the lists: atom of the clause cl, its sign and its prefix length */

void wr_sci_insert(glb* g, gint atom, gptr cl, int neg, int preflen) {
  wr_sci_t* x=(wr_sci_t*)(g->sci_index);
  int32_t e, rank;
  if ((g->strong_cut_strat)!=1) return; // the index only with the key strong_cut at 1
  if (x==(void*)1 || (g->inkb)) return;
  if (preflen<1) return; // the lists do not file it either
  if (!(g->unify_samelen) || (g->unify_maxuseterms)) { wr_sci_fail(g); return; }
  if (!x) {
    x=wr_sci_make(g);
    if (!x) { wr_sci_fail(g); return; }
  }
  if (x->encnt>=x->encap) {
    wr_sci_entry* ne=sys_realloc(x->ents,sizeof(wr_sci_entry)*(x->encap)*2);
    if (!ne) { wr_sci_fail(g); return; }
    x->ents=ne; x->encap*=2;
  }
  e=x->encnt++;
  if (e>=SCI_RANKMAX) { wr_sci_fail(g); return; }
  x->ents[e].atom=atom;
  x->ents[e].cl=pto(g->db,cl);
  (x->seq)++;
  // the lists test the units with a shorter prefix first, the newest first
  x->ents[e].order=(((uint64_t)(preflen-1))<<48)|(SCI_SEQMAX-((x->seq)&SCI_SEQMAX));
  // the same order as a rank of the tree (the higher rank first)
  rank=((3-preflen)<<28)+e; // preflen 1 to 3, e below 2^28
  if (!ti_insert_ranked(g,x->t[neg ? 1 : 0],atom,(gint)e,0,rank) || ti_failed(x->t[neg ? 1 : 0])) wr_sci_fail(g);
}

static void wr_sci_collect(glb* g, void* ctx, gint payload, int32_t nv, gint* vals) {
  wr_sci_t* x=(wr_sci_t*)ctx;
  int32_t e=(int32_t)payload;
  UNUSED(g); UNUSED(nv); UNUSED(vals);
  if (x->ccnt>=x->ccap) {
    int32_t nc=x->ccap*2;
    uint64_t* c=sys_realloc(x->cand,sizeof(uint64_t)*nc);
    int32_t* ce;
    if (!c) return;
    x->cand=c;
    ce=sys_realloc(x->candent,sizeof(int32_t)*nc);
    if (!ce) return;
    x->candent=ce; x->ccap=nc;
  }
  x->cand[x->ccnt]=x->ents[e].order;
  x->candent[x->ccnt]=e;
  (x->ccnt)++;
}

static int wr_sci_cmp(const void* a, const void* b) {
  uint64_t x=*(const uint64_t*)a, y=*(const uint64_t*)b;
  return (x<y) ? -1 : ((x>y) ? 1 : 0);
}

/* sort the candidates by their order key: an insertion sort for a few,
   qsort for more (a unification walk can give thousands). The keys are
   unique, and the entry of a key is its insert number minus 1. */

static void wr_sci_sort(wr_sci_t* x) {
  int32_t i, j, e;
  uint64_t o;
  if (x->ccnt>16) {
    qsort(x->cand,(size_t)(x->ccnt),sizeof(uint64_t),wr_sci_cmp);
    for(i=0;i<x->ccnt;i++)
      x->candent[i]=(int32_t)((SCI_SEQMAX-((x->cand[i])&SCI_SEQMAX))-1);
    return;
  }
  for(i=1;i<x->ccnt;i++) {
    o=x->cand[i]; e=x->candent[i];
    for(j=i-1; j>=0 && x->cand[j]>o; j--) { x->cand[j+1]=x->cand[j]; x->candent[j+1]=x->candent[j]; }
    x->cand[j+1]=o; x->candent[j+1]=e;
  }
}

/* the first node of the first non-empty list that the lists test for
   xatom, or 0 */

static gint wr_sci_lists_first(glb* g, gint xatom, vec hashvec) {
  gint prefhashes[ATOM_PREFHASH_MAXLEN+1];
  gint hash;
  int preflen, plen;
  preflen=wr_atom_calc_prefhashes(g,xatom,prefhashes);
  for(plen=0;plen<ATOM_PREFHASH_MAXLEN;plen++) {
    if (preflen==1) hash=WR_HASH_NORM(WR_HASH_ADD(plen,prefhashes[0]),NROF_CLTERM_HASHVEC_ELS);
    else if (preflen==2) hash=WR_HASH_NORM(WR_HASH_ADD(plen,prefhashes[plen==0 ? 0 : 1]),NROF_CLTERM_HASHVEC_ELS);
    else hash=WR_HASH_NORM(WR_HASH_ADD(plen,prefhashes[plen]),NROF_CLTERM_HASHVEC_ELS);
    if (wr_clterm_hashlist_len(g,hashvec,hash)==0) continue;
    return wr_clterm_hashlist_start(g,hashvec,hash);
  }
  return 0;
}

/* the perfect walk leaves out the unit that was tested before with the bindings */

static wr_sci_t* sci_current=NULL;

static int wr_sci_accept(glb* g, gint payload) {
  wr_sci_t* x=sci_current;
  UNUSED(g);
  if (x->skipcl && x->ents[payload].atom==x->skipatom && x->ents[payload].cl==x->skipcl) return 0;
  return 1;
}

/* the cutter of xatom among the units of one sign (neg: negative units,
   hashvec: their lists), or NULL; *used is 0 when the lists have to answer.

   The caller can leave bindings on the variable stack (in resolution the
   unifier of the parents). The lists test their first unit under these
   bindings and clear the stack after every test, so the later tests are
   without them. The tree does the same: with bindings on the stack it
   tests the first unit of the lists first and leaves it out of its walk,
   and whenever the lists would test a unit it clears the stack at the
   end. */

gptr wr_sci_cut_atom(glb* g, gint xatom, int neg, int usematch, int* used, vec hashvec) {
  wr_sci_t* x=(wr_sci_t*)(g->sci_index);
  void* db=g->db;
  ti_index* t;
  int32_t n, i;
  gint yatom, fnode, payload, aux, *vals;
  int32_t nv;
  int ures;
  gptr res=NULL;
  *used=0;
  if (x==(void*)1) return NULL;
  *used=1;
  if (!x) return NULL; // no unit filed yet
  t=x->t[neg ? 1 : 0];
  x->skipatom=0; x->skipcl=0;
  fnode=wr_sci_lists_first(g,xatom,hashvec);
  if (!fnode) return NULL; // the lists test nothing; no unit of the tree can pass
  if (CVEC_NEXT(g->varstack)>2) {
    yatom=(otp(db,fnode))[CLTERM_HASHNODE_TERM_POS];
    if (usematch) ures=wr_match_term(g,yatom,xatom,1);
    else ures=wr_unify_term(g,xatom,yatom,1); // uniquestrflag=1
    wr_clear_varstack(g,g->varstack);
    if (ures) return otp(db,(otp(db,fnode))[CLTERM_HASHNODE_CL_POS]);
    x->skipatom=yatom; x->skipcl=(otp(db,fnode))[CLTERM_HASHNODE_CL_POS];
  }
  n=ti_preorder(g,t,xatom);
  if (n<0) { wr_sci_fail(g); *used=0; return NULL; }
  if (usematch) {
    sci_current=x;
    (g->stat_sci_match_lookups)++;
    if (ti_match_best(g,t,0,n,wr_sci_accept,0,&payload,&aux,&nv,&vals)) {
      res=otp(db,x->ents[payload].cl);
      (g->stat_sci_match_found)++;
    }
    wr_clear_varstack(g,g->varstack); // as after the tests of the lists
    return res;
  }
  // unification: the candidates of the filter in the order of the lists
  x->ccnt=0;
  ti_candidates(g,t,0,n,1,wr_sci_collect,x);
  (g->stat_sci_unify_lookups)++;
  (g->stat_sci_unify_cands)+=x->ccnt;
  if (x->ccnt>1) wr_sci_sort(x);
  for(i=0;i<x->ccnt;i++) {
    if (x->skipcl && x->ents[x->candent[i]].atom==x->skipatom && x->ents[x->candent[i]].cl==x->skipcl) continue;
    yatom=x->ents[x->candent[i]].atom;
    (g->stat_sci_unify_tests)++;
    ures=wr_unify_term(g,xatom,yatom,1); // uniquestrflag=1
    wr_clear_varstack(g,g->varstack);
    if (ures) { (g->stat_sci_unify_ok)++; return otp(db,x->ents[x->candent[i]].cl); }
  }
  wr_clear_varstack(g,g->varstack); // as after the tests of the lists
  return NULL;
}

#else

void wr_sci_free(glb* g) { UNUSED(g); }

#endif


/* the cutter of xatom among the stored units of hashvec: the index when
   the key strong_cut is 1 and the index can answer, otherwise the lists */

gptr strongly_cut_atom(glb* g, gint xatom, vec hashvec, int usematch) {
#ifdef USE_SCUT_INDEX
  int used;
  gptr r;
  if ((g->strong_cut_strat)!=1) return strongly_cut_atom_lists(g,xatom,hashvec,usematch);
#ifdef GKC_SCUT_CHECK
  // both lookups start from the same variable stack: save it, run the
  // lists, restore it, run the index
  static gint* sv=NULL; static gint svcap=0;
  gint svn=(g->varstack)[1], q;
  gptr l;
  if (svn>2) sci_check_dirty++;
  if (svn>svcap) { svcap=svn*2; sv=sys_realloc(sv,sizeof(gint)*2*svcap); }
  for(q=2;q<svn;q++) { sv[2*q]=(g->varstack)[q]; sv[2*q+1]=*((gptr)((g->varstack)[q])); }
  l=strongly_cut_atom_lists(g,xatom,hashvec,usematch);
  wr_clear_varstack(g,g->varstack);
  for(q=2;q<svn;q++) { (g->varstack)[q]=sv[2*q]; *((gptr)(sv[2*q]))=sv[2*q+1]; }
  (g->varstack)[1]=svn;
#endif
  r=wr_sci_cut_atom(g,xatom,(hashvec==rotp(g,g->hash_neg_units)),usematch,&used,hashvec);
#ifdef GKC_SCUT_CHECK
  if (used) {
    sci_check_calls++;
    if ((r==NULL)!=(l==NULL)) sci_check_answer_diff++;
    else if (r!=l) sci_check_cutter_diff++;
    if (r!=l && getenv("SCI_PRINT")) {
      static int printed=0;
      if (printed<12) {
        printed++;
        printf("\nSCIDIFF usematch %d literal: ",usematch); wr_print_term(g,xatom);
        printf("\nSCIDIFF index: "); if (r) wr_print_clause(g,r); else printf("none");
        printf("\nSCIDIFF lists: "); if (l) wr_print_clause(g,l); else printf("none");
        printf("\n");
      }
    }
    return l;
  }
  return l;
#else
  if (used) return r;
#endif
#endif
  return strongly_cut_atom_lists(g,xatom,hashvec,usematch);
}

/* the present lookup: the lists of the prefix hashes */

gptr strongly_cut_atom_lists(glb* g, gint xatom, vec hashvec, int usematch) {
  void* db=g->db;
  gint hash;
  int hlen;
  gint node;
  gint yatom=0;
  gptr ycl;
  int ures;
  int preflen, plen, iseq=0, atomlen;
  gptr tptr;
  gint a=0,b=0,ay,by;
  gint prefhashes[ATOM_PREFHASH_MAXLEN+1];
  /*
  if (wr_equality_atom(g,xatom)) {    
    tptr=rotp(g,xatom);
    atomlen=get_record_len(tptr);
    if (atomlen>=(g->unify_firstuseterm)+3) {
      iseq=1;
      // take equality args (two):
      a=tptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
      b=tptr[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];
      //atype=wg_get_encoded_type(db,a);
      //btype=wg_get_encoded_type(db,b);
    }  
  }
  */
  preflen=wr_atom_calc_prefhashes(g,xatom,prefhashes);
  for(plen=0;plen<ATOM_PREFHASH_MAXLEN;plen++) {   
    if (preflen==1) {      
      hash=WR_HASH_NORM(WR_HASH_ADD(plen,prefhashes[0]),NROF_CLTERM_HASHVEC_ELS);
    } else if (preflen==2) {      
      if (plen==0) // hash=prefhashes[0]+plen;
        hash=WR_HASH_NORM(WR_HASH_ADD(plen,prefhashes[0]),NROF_CLTERM_HASHVEC_ELS);
      else // hash=prefhashes[1]+plen;
        hash=WR_HASH_NORM(WR_HASH_ADD(plen,prefhashes[1]),NROF_CLTERM_HASHVEC_ELS);
    } else {      
      hash=WR_HASH_NORM(WR_HASH_ADD(plen,prefhashes[plen]),NROF_CLTERM_HASHVEC_ELS);
    }                     
#ifdef SCUTDEBUG
    wr_printf("\n looking for hash %ld for plen %d preflen %d\n",hash,plen,preflen);
#endif

    //wr_clterm_hashlist_print(g,hashvec);       
    hlen=wr_clterm_hashlist_len(g,hashvec,hash);
    if (hlen==0) {
#ifdef SCUTDEBUG        
      wr_printf("no matching atoms in hash\n");
#endif        
      continue;
    }     
    node=wr_clterm_hashlist_start(g,hashvec,hash);
    if (!node)  {
      wr_sys_exiterr(g,"apparently wrong hash given to wr_clterm_hashlist_start");
      return NULL;
    }      
    while(node!=0) {    
      yatom=(otp(db,node))[CLTERM_HASHNODE_TERM_POS];
      
      /*
      wr_printf("after while(node!=0): \n");
      wr_printf("\nxatom ");
      wr_print_term(g,xatom);     
      wr_printf("\nyatom ");
      wr_print_term(g,yatom);  
      printf("\nyclause ") ;
      wr_print_clause(g,otp(db,(otp(db,node))[CLTERM_HASHNODE_CL_POS]));   
      printf("\nat pos %ld\n",(otp(db,node))[CLTERM_HASHNODE_PATH_POS]);
      */            
#ifdef SCUTDEBUG        
      wr_printf("\nxatom ");
      wr_print_term(g,xatom);     
      wr_printf("\nyatom ");
      wr_print_term(g,yatom);  
      printf("\nyclause ") ;
      wr_print_clause(g,otp(db,(otp(db,node))[CLTERM_HASHNODE_CL_POS]));   
      printf("\nat pos %ld\n",(otp(db,node))[CLTERM_HASHNODE_PATH_POS]);
      wr_print_vardata(g);
#endif    
      if (usematch) {
        ures=wr_match_term(g,yatom,xatom,1);
      } else {
        ures=wr_unify_term(g,xatom,yatom,1); // uniquestrflag=1            
      }  

      if (!ures && iseq && wr_equality_atom(g,yatom)) {       
        tptr=rotp(g,yatom);
        atomlen=get_record_len(tptr);
        if (atomlen>=(g->unify_firstuseterm)+3) {
          wr_clear_varstack(g,g->varstack);
          // take equality args (two):
          ay=tptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
          by=tptr[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];
          if (usematch) {
            ures=wr_match_term(g,a,by,1);
            if (ures) {
              ures=wr_match_term(g,b,ay,1);
            }
          } else {
            ures=wr_unify_term(g,a,by,1); // uniquestrflag=1                       
            if (ures) {
              ures=wr_unify_term(g,b,ay,1);
            }
          }
        }  
      }

#ifdef SCUTDEBUG        
      wr_printf("unification check res: %d\n",ures);
#endif        
      if (ures) {        
        ycl=otp(db,(otp(db,node))[CLTERM_HASHNODE_CL_POS]);

        /*
        wr_printf("\nin wr_resolve_binary_all_active after wr_process_resolve_result\n");
        wr_printf("\nxatom\n");
        wr_print_term(g,xatom);
        wr_printf("\nyatom\n");
        wr_print_term(g,yatom);
        wr_printf(" in ycl ");
        wr_print_clause(g,ycl);
        wr_printf("\n!!!!! wr_resolve_binary_all_active after wr_process_resolve_result queue is\n");
        wr_show_clqueue(g);
        wr_printf("\nqueue ended\n");
        */
        
#ifdef SCUTDEBUG                   
        wr_printf("\nin wr_resolve_binary_all_active after wr_process_resolve_result\n");
        wr_printf("\nxatom\n");
        wr_print_term(g,xatom);
        wr_printf("\nyatom\n");
        wr_print_term(g,yatom);
        wr_printf(" in ycl ");
        wr_print_clause(g,ycl);
        wr_printf("\n!!!!! wr_resolve_binary_all_active after wr_process_resolve_result queue is\n");
        wr_show_clqueue(g);
        wr_printf("\nqueue ended\n");
#endif             
        wr_clear_varstack(g,g->varstack); // !! added in june
        return ycl;                
      }
#ifdef SCUTDEBUG
      wr_printf("wr_resolve_binary_all_active before wr_clear_varstack queue is\n");
      wr_show_clqueue(g);
      wr_printf("\nqueue ended\n");
#endif         
      wr_clear_varstack(g,g->varstack);              
      //wr_print_vardata(g);      
      // get next node;
      node=wr_clterm_hashlist_next(g,hashvec,node);               
    } /* over one single hash vector */
#ifdef SCUTDEBUG      
    wr_printf("\nexiting node loop\n");      
#endif      
  } /* over different preflens of the given atom */
  
  return NULL;
}

/* ------------------------------------------------------ 

   clause-to-clause subsumption

   ------------------------------------------------------
*/   
  
/*
  cl1 is gcl, cl2 is scl: 
  test if cl1 subsumes cl2

  Each gcl literal must match a different scl literal.


  Take first gcl literal glit
    Loop over all scl literals     
      if match found glit<->slit and
         recursive subsumption without glit and slit present is OK
      then return OK
      else restore variable settings before last match attempt
    If match found during internal loop 

    otherwise 

  Loop over all gcl literals
    Loop over all scl literals     
      if match found glit<->slit then stop internal loop
      else restore variable settings before last match attempt


*/


gint wr_subsume_cl(glb* g, gptr cl1, gptr cl2, int uniquestrflag) {
  void* db=g->db;
  int cllen1,cllen2;
  int i2;
  gint meta1,meta2;
  gint lit1,lit2;
  int vc_tmp;
  int mres;
  
#ifdef DEBUG  
  wr_printf("\nwr_subsume_cl called on clauses %d %d :\n",(int)cl1,(int)cl2);
  wr_print_clause(g,cl1);
  wr_printf("\n");
  wr_print_clause(g,cl2);
  wr_printf("\n");
#endif

  ++(g->stat_clsubs_attempted);  

  // check fact clause cases first
  if (!wg_rec_is_rule_clause(db,cl1)) {
    if (!wg_rec_is_rule_clause(db,cl2)) {
#ifdef DEBUG  
       wr_printf("both clauses are facts \n");
#endif                 
      ++(g->stat_clsubs_unit_attempted);      
      if (wr_equal_term(g,encode_record(db,cl1),encode_record(db,cl2),uniquestrflag))
        return 1;
      else
        return 0;
    } else {
      // fact clause subsuming a rule clause
      cllen2=wg_count_clause_atoms(db,cl2);
      if ((g->instgen_strat) && cllen2<2) return 0; // only shorter may subsume for instgen
      lit1=encode_record(db,cl1);      
      ++(g->stat_clsubs_unit_attempted);
      for(i2=0;i2<cllen2;i2++) {
        meta2=wg_get_rule_clause_atom_meta(db,cl2,i2);
        if (!wg_atom_meta_is_ground(db,meta2)) return 0; // only nonground will come later
        lit2=wg_get_rule_clause_atom(db,cl2,i2);
        if (!wg_atom_meta_is_neg(db,meta2) && wr_equal_term(g,lit1,lit2,uniquestrflag)) return 1;        
      }  
      return 0;
    }      
  } 
  // now cl1 is a rule clause
  cllen1=wg_count_clause_atoms(db,cl1);
  
  if (!wg_rec_is_rule_clause(db,cl2)) {
    // unit rule clause subsuming a unit fact clause case
    if (g->instgen_strat) return 0; // only shorter may subsume for instgen
    ++(g->stat_clsubs_unit_attempted);    
    cllen2=1;
    if (cllen1>1) return 0;    
    meta1=wg_get_rule_clause_atom_meta(db,cl1,0);
    if (wg_atom_meta_is_neg(d,meta1)) return 0;
    lit1=wg_get_rule_clause_atom(db,cl1,0);    
    lit2=rpto(g,cl2);
    vc_tmp=2;
    mres=wr_match_term(g,lit1,lit2,uniquestrflag);
    if (vc_tmp!=*((g->varstack)+1)) wr_clear_varstack(g,g->varstack);
    return mres;      
  } else {
    // cl2 is a rule clause
    cllen2=wg_count_clause_atoms(db,cl2); 
  }    
  // now both clauses are rule clauses   
  
#ifdef DEBUG  
  wr_printf("cllen1 %d cllen2 %d\n",cllen1,cllen2);
#endif   
  // check unit rule clause case
  if (cllen1==1) { 
#ifdef DEBUG  
    wr_printf("unit clause subsumption case \n");
#endif        
    //++(g->stat_clsubs_unit_attempted);    // ?? why double??   
    ++(g->stat_clsubs_unit_attempted);
    meta1=wg_get_rule_clause_atom_meta(db,cl1,0);
    lit1=wg_get_rule_clause_atom(db,cl1,0);
    vc_tmp=2;
    *((g->varstack)+1)=vc_tmp; // zero varstack pointer 
    for(i2=0;i2<cllen2;i2++) {
        meta2=wg_get_rule_clause_atom_meta(db,cl2,i2);        
        if (wr_sort_meta_bigger(meta1,meta2)) {
          // lit1 cannot subsume any literal in cl2vec here or coming after, like:
          // general  s(b)
          // general  l(X,Y)
          // specific p(a,b) |  s(b) |  m(X,Y) | r(X)
          (g->stat_clsubs_meta_attempted)++;
          (g->stat_clsubs_meta_failed)++;
          return 0;
        } else if (!litmeta_negpolarities(meta1,meta2) && wr_matchable_lit_meta(g,meta1,meta2)) {  
          lit2=wg_get_rule_clause_atom(db,cl2,i2);
          if ((g->instgen_strat) && cllen2==1) {
            mres=wr_equal_mod_vars_term(g,lit1,lit2,uniquestrflag);
          } else {  
            mres=wr_match_term(g,lit1,lit2,uniquestrflag);                     
          }  
          if (vc_tmp!=*((g->varstack)+1)) wr_clear_varstack(g,g->varstack);
          if (mres) return 1;
        }                    
    }  
    return 0;
  }    
  if (cllen1>cllen2) return 0;
  
  // now both clauses are nonunit rule clauses and we do full subsumption
  // prepare for subsumption: set globals etc 
#ifdef DEBUG  
  wr_printf("general subsumption case \n");
#endif  
  /* 
  if ((g->instgen_strat) && cllen1==cllen2) {
    CP0;
    mres=wr_equal_instgen_clauses(g,cl1,cl2);
    printf("\nmres %d\n",mres);
    return mres;
  }  
  */
  g->tmp_unify_vc=(g->varstack)+1;  // var counter for varstack   
  // clear lit information vector (0 pos holds vec len)
  for(i2=1;i2<=cllen2;i2++) (g->tmp_litinf_vec)=wr_vec_store(g,g->tmp_litinf_vec,i2,0);
  ++(g->stat_clsubs_full_attempted);
  mres=wr_subsume_cl_aux(g,cl1,cl2,
                   cl1+RECORD_HEADER_GINTS+CLAUSE_EXTRAHEADERLEN,
                   cl2+RECORD_HEADER_GINTS+CLAUSE_EXTRAHEADERLEN,
                   0,0,
                   cllen1,cllen2,
                   uniquestrflag);
  wr_clear_varstack(g,g->varstack);
  return mres;  
}   



/*
  Each gcl literal must match a different scl literal.


  Take first gcl literal glit
    Loop over all scl literals     
      if match found glit<->slit and
         recursive subsumption without glit and slit present is OK
      then return OK
      else restore variable settings before last match attempt
    If match found during internal loop 

    in other words

  Loop over all gcl literals
    Loop over all scl literals     
      if match found glit<->slit then stop internal loop
      else restore variable settings before last match attempt

*/

gint wr_subsume_cl_aux(glb* g,gptr cl1vec, gptr cl2vec, 
	                  gptr litpt1, gptr litpt2, 
	                  int litind1, int litind2, 
                    int cllen1, int cllen2,
                    int uniquestrflag) {
  int i1,i2; 
  gint lit1,lit2;
  gptr pt1,pt2;  
  gint meta1,meta2;	    
  //int foundflag;	    
  int vc_tmp;	    
  int nobackflag;	
#ifdef ANS_IGNORE
  int lit1ans=0;
  int lit2ans=0;
#endif      

#ifdef DEBUG 
  wr_printf("\nwr_subsume_cl_aux called with litind1 %d litind2 %d cllen1 %d cllen2 %d gen and specific literals:\n",
    litind1,litind2,cllen1,cllen2);
  wr_print_term(g,*(litpt1+LIT_ATOM_POS)); 
  wr_printf("\n");
  wr_print_term(g,*(litpt2+LIT_ATOM_POS)); 
  wr_printf("\n");
#endif            
  if(litind1<cllen1) {   
    i1=litind1;
    pt1=litpt1;
    meta1=*(pt1+LIT_META_POS); 
    lit1=*(pt1+LIT_ATOM_POS);  
#ifdef ANS_IGNORE
    if ((g->required_answer_nr)==1 && wr_answer_lit(g,lit1)) lit1ans=1;
#endif       
    nobackflag=0; // backtracing will be prohibited if match found without vars set
    for(i2=0,pt2=litpt2; !nobackflag && i2<cllen2; i2++,pt2+=LIT_WIDTH) {
#ifdef DEBUG      
      wr_printf("\n in wr_subsume_cl_aux i2 %d nobackflag %d cllen2 %d\n",i2,nobackflag,cllen2);
#endif      
      if ((g->tmp_litinf_vec)[i2+1]==0) {
	      // literal not bound by subsumption yet
        meta2=*(pt2+LIT_META_POS); 
        lit2=*(pt2+LIT_ATOM_POS);       
#ifdef ANS_IGNORE
        if ((g->required_answer_nr)==1 && wr_answer_lit(g,lit2)) lit2ans=1;
        else lit2ans=0;
#endif
        //foundflag=0;          
#ifdef DEBUG
        wr_printf("\n in wr_subsume_cl_aux gen lit1 and specific lit2:\n");
        wr_print_term(g,lit1);
        wr_printf("\n");
        wr_print_term(g,lit2);
        wr_printf("\n");
#endif        
#ifdef ANS_IGNORE
        if (wr_sort_meta_bigger(meta1,meta2) && !lit1ans && !lit2ans) {
#else
        if (wr_sort_meta_bigger(meta1,meta2)) {
#endif          
          // lit1 cannot subsume any literal in cl2vec here or coming after, like:
          // general  s(b)
          // general  l(X,Y)
          // specific p(a,b) |  s(b) |  m(X,Y) | r(X)
          (g->stat_clsubs_meta_attempted)++;
          (g->stat_clsubs_meta_failed)++;
          return 0;
        } else if (!litmeta_negpolarities(meta1,meta2) 
#ifdef ANS_IGNORE 
                   && ((lit1ans && lit2ans) || wr_matchable_lit_meta(g,meta1,meta2))) {
#else        
                   && wr_matchable_lit_meta(g,meta1,meta2)) {          
#endif                     
#ifdef DEBUG      
          wr_printf("\n ok polarities and matchable lit meta\n");
#endif             
          vc_tmp=*(g->tmp_unify_vc); // store current value of varstack pointer ????????    
          if (((g->instgen_strat) && cllen1==cllen2) ?
               wr_eqmodvars_term_aux(g,lit1,lit2,uniquestrflag) :
#ifdef ANS_IGNORE
               (lit1ans && lit2ans) || wr_match_term_aux(g,lit1,lit2,uniquestrflag) ) {
#else               
               wr_match_term_aux(g,lit1,lit2,uniquestrflag) ) {
#endif                 
          //if (wr_match_term_aux(g,lit1,lit2,uniquestrflag)) { // not instgen case
#ifdef DEBUG      
            wr_printf("\n wr_match_term_aux returned 1\n");
#endif                
        
            
#ifdef DEBUG             
	          wr_printf("lit match ok with *(g->tmp_unify_vc): %d\n",*(g->tmp_unify_vc));
            wr_print_vardata(g);
#endif            
            // literals were successfully matched
	          (g->tmp_litinf_vec)[i2+1]=1; // mark as a bound literal
	          if (vc_tmp==*(g->tmp_unify_vc)) nobackflag=1; // no need to backtrack
            if ((i1+1>=cllen1) ||
	               wr_subsume_cl_aux(g,cl1vec,cl2vec,pt1+(LIT_WIDTH),litpt2,
	                             i1+1,i2,cllen1,cllen2,uniquestrflag)) {
	            // found a right match for current gcl lit
#ifdef DEBUG                                  
              wr_printf("rest ok with *(g->tmp_unify_vc): %d\n",*(g->tmp_unify_vc));		
              wr_print_vardata(g);                                  
#endif                                 
	            return 1;	    
	          } 
#ifdef DEBUG             
	          wr_printf("rest failed with *(g->tmp_unify_vc): %d\n",*(g->tmp_unify_vc));
            wr_print_vardata(g);
#endif            
	          if (vc_tmp!=*(g->tmp_unify_vc)) wr_clear_varstack_topslice(g,g->varstack,vc_tmp);
            (g->tmp_litinf_vec)[i2+1]=0; // clear as a bound literal	      
          } else {
   	        //printf("lit match failed with *(g->tmp_unify_vc): %d\n",*(g->tmp_unify_vc));
  	        if (vc_tmp!=*(g->tmp_unify_vc)) wr_clear_varstack_topslice(g,g->varstack,vc_tmp);	 
          }		  
        } 
      }		
    }
    // all literals checked, no way to subsume using current lit1
    return 0;        
  }
  // clause 
  wr_printf("REASONER ERROR! something wrong in calling wr_subsume_cl_aux\n");
  //printf("litind1: %d cllen1: %d i1: %d \n", litind1,cllen1,i1);  
  return 0;  
}     

/** Match terms:  mx is the general term meta, my is a meta of an instance of x
    NB! we do not consider the polarities here, ie both pos/pos and neg/pos may be matched
*/

int wr_matchable_lit_meta(glb* g, gint mx, gint my) {

#ifdef DEBUG
  wr_printf("\nwr_matchable_lit_meta called on\n  general mx %d: ",mx);
  wr_print_cl_meta(g, mx);
  wr_printf("\n");
  wr_printf("  specific my %d: ",my);
  wr_print_cl_meta(g, my);
  wr_printf("\n");
#endif  
  (g->stat_clsubs_meta_attempted)++;
  if ((mx&(ATOM_META_CANSUBS_MASK<<SMALLINTSHFT)) > (my&(ATOM_META_CANSUBS_MASK<<SMALLINTSHFT))) {
    (g->stat_clsubs_meta_failed)++;    
    //dp("\n initial ATOM_META_CANSUBS_MASK failed, returning 0\n");
    return 0;
  }
  if ((mx&((ATOM_META_GROUND_MASK<<ATOM_META_GROUND_SHIFT)<<SMALLINTSHFT))) {      
    // x and y are ground (y due to previous comparison): have to be equal to subsume
    /*
     if (   ((mx&(ATOM_META_DEPTHCANSUBS_MASK<<SMALLINTSHFT)) > (my&(ATOM_META_DEPTHCANSUBS_MASK<<SMALLINTSHFT))) 
      || ((mx&(ATOM_META_PREFLENCANSUBS_MASK<<SMALLINTSHFT)) > (my&(ATOM_META_PREFLENCANSUBS_MASK<<SMALLINTSHFT))) ) {
      (g->stat_clsubs_meta_failed)++;
      //printf("\n ground depth or preflen failed\n");
      return 0;
    } 
    return 1;
    */
    if ((mx&(ATOM_META_GROUNDCMP_MASK<<SMALLINTSHFT)) != (my&(ATOM_META_GROUNDCMP_MASK<<SMALLINTSHFT))) {
      (g->stat_clsubs_meta_failed)++;      
      //dp("\n GROUNDCMP failed, returning 0\n");
      return 0;
    } else {
       //dp("\n returning 1\n");
      return 1;
    }
  }
  //return 1;
  // here x and y are both non-ground
  //dp("\n here x and y are both non-ground\n");

  if (   ((mx&(ATOM_META_DEPTHCANSUBS_MASK<<SMALLINTSHFT)) > (my&(ATOM_META_DEPTHCANSUBS_MASK<<SMALLINTSHFT))) 
      || ((mx&(ATOM_META_PREFLENCANSUBS_MASK<<SMALLINTSHFT)) > (my&(ATOM_META_PREFLENCANSUBS_MASK<<SMALLINTSHFT))) ) {
    (g->stat_clsubs_meta_failed)++;
    //dp("\n nonground depth or pref failed, returning 0\n");
    //printf("\n nonground depth or pref failed\n");
    return 0;
  } 

  // if preflens are equal (checking that they are not max!), the prefhashes must be equal

  if (   ((mx&((ATOM_META_PREFLEN_MASK<<ATOM_META_PREFLEN_SHIFT)<<SMALLINTSHFT)) == 
          (my&((ATOM_META_PREFLEN_MASK<<ATOM_META_PREFLEN_SHIFT)<<SMALLINTSHFT)) &&
          
         (((mx&((ATOM_META_PREFLEN_MASK<<ATOM_META_PREFLEN_SHIFT)<<SMALLINTSHFT))
          >>ATOM_META_PREFLEN_SHIFT)>>SMALLINTSHFT)!=ATOM_META_PREFLEN_MASK)

      && ((mx&(ATOM_META_PREFHASH_MASK<<SMALLINTSHFT)) != (my&(ATOM_META_PREFHASH_MASK<<SMALLINTSHFT))) ) {
    //dp("\n hashes for same-len prefixes failed, returning 0\n");
    //printf("\n preflen %d mask %d\n",
    //   (((mx&((ATOM_META_PREFLEN_MASK<<ATOM_META_PREFLEN_SHIFT)<<SMALLINTSHFT))
    //      >>ATOM_META_PREFLEN_SHIFT)>>SMALLINTSHFT), ATOM_META_PREFLEN_MASK);
    (g->stat_clsubs_meta_failed)++;
    return 0;
  } 
  //dp("\n final case returning 1\n");  
  return 1;
}


#ifdef __cplusplus
}
#endif
