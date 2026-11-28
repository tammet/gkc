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

 /** @file resolve.c
 *  Procedures for the first part of new clause derivation (binary, factor, para, ...),
 *  with the result clause created by procedures in derive.c
 *
 */

/* ====== Includes =============== */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#include "../config-w32.h"
#else
#include "../config.h"
#endif
#include "../Db/dballoc.h"
#include "../Db/dbdata.h"
#include "../Db/dbhash.h"
#include "../Db/dblog.h"
#include "../Db/dbindex.h"
#include "../Db/dbcompare.h"
  
#include "rincludes.h"
 
  
/* ====== Private headers and defs ======== */

#define EQTERM_ORDER

//#define DEBUG
#undef DEBUG
#define QUIET
//#undef QUIET

//#define USE_RES_TERMS // loop over active clauses in wr_resolve_binary_all_active

//#define EQ_PREFERENCE  // testing, experimental

//#define SIMPLE_ACTIVE_SEARCH_HASH

/* ======= Private protos ================ */



/* ====== Functions ============== */



void wr_resolve_binary_all_active(glb* g, gptr cl, gptr cl_as_active, cvec resolvability) {
  int upon; // unit propagation is on and its table exists 
  void* db=g->db;
  int i;
  int len;      
  int ruleflag; // 0 if not rule
  gint meta;
  int negflag; // 1 if negative
  int nonanslen=0; // length not counting ans literals
  gint hash;
  vec hashvec;
  int hlen;
  gint node;
  gint xatom=0;
  gint yatom=0;
  gptr xcl;
  gptr ycl;
  int ures;
  int dbused;
  int preflen, plen, resolvedliterals;
  float run_seconds,run_dseconds;
  gint prefhashes[ATOM_PREFHASH_MAXLEN+1];
  clock_t curclock;
  
 
#ifdef DEBUG
  wr_printf("\n!!!!!!! wr_resolve_binary_all_active called for clause ");
  wr_print_clause(g,cl);
  wr_printf("\n");     
  wr_print_vardata(g);
#endif  
  // get clause data for input clause
  
  ruleflag=wg_rec_is_rule_clause(db,cl);
  if (ruleflag) len = wg_count_clause_atoms(db, cl);
  else len=1;  
  nonanslen=wr_count_cl_nonans_atoms(g,cl);
  xcl=cl; 
#ifdef DEBUG
  wr_printf("ruleflag %d len %d  posok %d negok %d\n",
          ruleflag,len,posok,negok);
#endif  
  // loop over literals
  resolvedliterals=0;
  for(i=0; i<len; i++) {     
    if ((g->queryfocus_strat) && resolvedliterals) break;    
    negflag=0;
    if (!ruleflag) {
      xatom=encode_record(db,xcl);
      //printf("!ruleflag atom with i %d: \n",i);
#ifdef DEBUG      
      wr_print_record(g,wg_decode_record(db,xatom));
#endif      
      hash=wr_atom_funhash(g,xatom);
      preflen=wr_atom_calc_prefhashes(g,xatom,prefhashes);
    } else {       
      if (!resolvability[i+1]) continue;
      meta=wg_get_rule_clause_atom_meta(db,xcl,i);
      if (wg_atom_meta_is_neg(db,meta)) negflag=1;
      xatom=wg_get_rule_clause_atom(db,xcl,i);       
#ifdef DEBUG            
      wr_printf("atom nr %d from record \n",i);
      wr_print_record(g,xcl);
      wr_printf("\natom\n");              
      wr_print_record(g,wg_decode_record(db,xatom));
      wr_printf("negflag %d\n",negflag);             
#endif                 
#ifdef SIMPLE_ACTIVE_SEARCH_HASH
      hash=wr_atom_funhash(g,xatom);
#else        
      preflen=wr_atom_calc_prefhashes(g,xatom,prefhashes);
#endif
    }
    // xcl: active clause
    // xatom: active atom
    if (!xatom) continue;
    if (1) {     
      resolvedliterals++; 
      if ((g->print_litterm_selection) && len>1) {
        wr_printf("\nselected nr %d for res: ",i);
        wr_print_term(g,xatom);
      }
      // now loop over hash vectors for all active unification candidates
      // ycl: cand clause
      // yatom: cand atom

      // loop over local atom hash (dbused==0) and external atom hash (dbused==1)

      for(dbused=0; dbused<2; dbused++) {
        if (dbused==0) {
          if (negflag) hashvec=rotp(g,g->hash_pos_atoms);
          else hashvec=rotp(g,g->hash_neg_atoms);   
          //printf("\n**** in wr_resolve_binary_all_active dbused %d hashvec is *****\n",dbused);
#ifdef DEBUG
          wr_printf("\n**** in wr_resolve_binary_all_active dbused %d hashvec is *****\n",dbused);
          wr_clterm_hashlist_print(g,hashvec);
#endif
        } else {
          //printf("\n**** in wr_resolve_binary_all_active dbused %d hashvec is *****\n",dbused);
#ifdef DEBUG
          wr_printf("\n**** in wr_resolve_binary_all_active dbused %d hashvec is *****\n",dbused);
          //wr_clterm_hashlist_print(g,hashvec);
#endif
          if (r_kb_g(g)) {
            //printf("\nr_kb_g(g) used\n");
            printf("\ng is %lx db is %lx, r_kb_g(g) used: %lx with r_kb_g(g)->db %lx\n",
              (unsigned long int)g,(unsigned long int)db,(unsigned long int)r_kb_g(g),(unsigned long int)(r_kb_g(g)->db));
            if (negflag) hashvec=rotp(g,r_kb_g(g)->hash_pos_atoms);
            else hashvec=rotp(g,r_kb_g(g)->hash_neg_atoms); 
#ifdef DEBUG            
            wr_clterm_hashlist_print(g,hashvec);            
#endif            
          } else {
            // no external database
            break;
          }       
        }         

  #ifndef QUIET      
        wr_printf("\n----- inner wr_genloop cycle (active hash list) starts ----------\n"); 

        wr_printf("\nhash %ld\n",hash);
  #endif             
        
        // now loop over different hash preflens of the given atom

#ifdef SIMPLE_ACTIVE_SEARCH_HASH
        for(plen=0;plen<1;plen++) {          
#else        
        for(plen=0;plen<ATOM_PREFHASH_MAXLEN;plen++) { 
          if (preflen==1) {
            //p(X,Z):
            //p with fullpreflen 1
            //p with fullpreflen 2
            //p with fullpreflen 3
            //hash=prefhashes[0]+plen;
            hash=WR_HASH_NORM(WR_HASH_ADD(plen,prefhashes[0]),NROF_CLTERM_HASHVEC_ELS);
          } else if (preflen==2) {
            //p(a,Z):
            //p with fullpreflen 1
            //p,a with fullpreflen 2
            //p,a with fullpreflen 3  
            if (plen==0) // hash=prefhashes[0]+plen;
              hash=WR_HASH_NORM(WR_HASH_ADD(plen,prefhashes[0]),NROF_CLTERM_HASHVEC_ELS);
            else // hash=prefhashes[1]+plen;
              hash=WR_HASH_NORM(WR_HASH_ADD(plen,prefhashes[1]),NROF_CLTERM_HASHVEC_ELS);
          } else {
            // preflen 3
            //p(a,b):  
            //p with fullpreflen 1
            //p,a with fullpreflen 2
            //p,a,b with fullpreflen 3
            //hash=prefhashes[plen]+plen;
            hash=WR_HASH_NORM(WR_HASH_ADD(plen,prefhashes[plen]),NROF_CLTERM_HASHVEC_ELS);
          }
#endif                      
#ifdef DEBUG
          wr_printf("\n looking for hash %ld for plen %d preflen %d\n",hash,plen,preflen);
#endif

          //wr_clterm_hashlist_print(g,hashvec);     
          hlen=wr_clterm_hashlist_len(g,hashvec,hash);
          if (hlen==0) {
    #ifdef DEBUG        
            wr_printf("no matching atoms in hash\n");
    #endif        
            continue;
          }     
          node=wr_clterm_hashlist_start(g,hashvec,hash);
          if (!node)  {
            wr_sys_exiterr(g,"apparently wrong hash given to wr_clterm_hashlist_start");
            return;
          }      
          upon=((g->up_index)!=NULL); // unit propagation: a clause becomes unusable only before a pick
          while(node!=0) {    
            yatom=(otp(db,node))[CLTERM_HASHNODE_TERM_POS];
            ycl=otp(db,(otp(db,node))[CLTERM_HASHNODE_CL_POS]);
            /*
            wr_printf("after while(node!=0): \n");
            wr_printf("ycl: \n");
            wr_print_clause(g,ycl);                   
            wr_printf("xcl: \n");
            wr_print_clause(g,xcl);
            wr_printf("xatom: \n");
            wr_print_clause(g,xatom);
            */
            if (upon && wr_up_unusable(g,ycl)) {
              // made unusable by unit propagation
              node=wr_clterm_hashlist_next(g,hashvec,node);
              continue;
            }
            if ((g->back_subsume) && wr_get_cl_backsubsumed(g,ycl)) {
              /*
              wr_printf("\nCP1 blocked clause: \n");          
              wr_print_clause(g,ycl);
              wr_printf("\n");
              */
              node=wr_clterm_hashlist_next(g,hashvec,node);
              continue;
            }
            if (g->print_active_cl) {
              wr_printf("\n* active: ");
              wr_print_clause(g,ycl); 
              //printf("\n");
            }  
            /*
            wr_printf("\n* active: ");
            wr_print_clause(g,ycl);    
            */
            if ((g->res_arglen_limit) && (wg_rec_is_rule_clause(db,ycl))) {
              if (wr_count_cl_nonans_atoms(g,ycl) > (g->res_arglen_limit)) {
                // cannot use: continue loop
                node=wr_clterm_hashlist_next(g,hashvec,node);
                continue;
              }
            }
            if ((g->res_shortarglen_limit) && (nonanslen>(g->res_shortarglen_limit))) {
              // must check that ycl is unit
              if (wr_count_cl_nonans_atoms(g,ycl) > (g->res_shortarglen_limit)) {
                // cannot use: continue loop
                node=wr_clterm_hashlist_next(g,hashvec,node);
                continue;
              }  
            }            
            
    #ifdef DEBUG        
            wr_printf("\nxatom ");
            wr_print_term(g,xatom);
            wr_printf(" in xcl ");
            wr_print_clause(g,xcl);
            wr_printf("yatom ");
            wr_print_term(g,yatom);
            wr_printf(" in ycl ");
            wr_print_clause(g,ycl);
            wr_print_vardata(g);
    #endif                          
            ures=wr_unify_term(g,xatom,yatom,1); // uniquestrflag=1            
    #ifdef DEBUG        
            wr_printf("unification check res: %d\n",ures);
    #endif        
            if (ures) {
              // build and process the new clause
    #ifdef DEBUG         
              wr_printf("\nin wr_resolve_binary_all_active to call wr_process_resolve_result\n");
    #endif           
              if (1) { //(!(g->instgen_strat)) {
                  wr_process_resolve_result(g,xatom,xcl,yatom,ycl,cl_as_active); 
              
      #ifdef DEBUG           
                wr_printf("\nin wr_resolve_binary_all_active after wr_process_resolve_result\n");
                wr_printf("\nxatom\n");
                wr_print_term(g,xatom);
                wr_printf("\nyatom\n");
                wr_print_term(g,yatom);
                wr_printf("!!!!! wr_resolve_binary_all_active after  wr_process_resolve_result queue is\n");
                wr_show_clqueue(g);
                wr_printf("\nqueue ended\n");
      #endif             
                if (g->proof_found || g->alloc_err) {
                  wr_clear_varstack(g,g->varstack);  
                  if (wr_enough_answers(g) || g->alloc_err) return;                
                } 
              }

              if ((g->instgen_strat) && wr_proper_substitution(g,g->varstack)) {
                wr_process_instgen_result(g,xatom,xcl,yatom,ycl,cl_as_active,1);
                if (g->proof_found || g->alloc_err) {
                  wr_clear_varstack(g,g->varstack);  
                  if (wr_enough_answers(g) || g->alloc_err) return;                
                } 
                wr_process_instgen_result(g,xatom,xcl,yatom,ycl,cl_as_active,0);
                if (g->proof_found || g->alloc_err) {
                  wr_clear_varstack(g,g->varstack);  
                  if (wr_enough_answers(g) || g->alloc_err) return;                
                } 
              }  
            }
    #ifdef DEBUG
            wr_printf("wr_resolve_binary_all_active before wr_clear_varstack queue is\n");
            wr_show_clqueue(g);
            wr_printf("\nqueue ended\n");
    #endif         
            wr_clear_varstack(g,g->varstack);              
            //wr_print_vardata(g);
            // extra time check
            if ((g->stat_derived_cl%256)==0 || (g->derived_overflow_timeout)) {
              if (g->derived_overflow_timeout) return; // time limit passed at a drop
              curclock=clock();
              run_seconds = (float)(curclock - (g->run_start_clock)) / CLOCKS_PER_SEC;            
              if ((g->max_run_seconds) && (run_seconds>(g->max_run_seconds))) return;
              run_dseconds = (float)(curclock - (g->run_start_clock)) / ((float)CLOCKS_PER_SEC / 10);            
              if ((g->max_run_dseconds) && (run_dseconds>(g->max_run_dseconds))) return;
            }
            // get next node;
            node=wr_clterm_hashlist_next(g,hashvec,node);               
          } /* over one single hash vector */
    #ifdef DEBUG      
          wr_printf("\nexiting node loop\n");      
    #endif      
        } /* over different preflens of the given atom */
      } /* over local and external db hash vectors */
    }  
  }         
  return;
}

/*
   factor performs factorization nonrecursively:
   factorize upon all literal pairs regardless of ordering
   
   EXCEPT for query strat, where only the first lit
   is factorized upon (NB! this should be improved)

   it but does not factorize the results further
*/


void wr_factor(glb* g, gptr cl, gptr cl_as_active) { 
  void* db=g->db;
  int i,j;
  int len;      
  //gint parent;
  gint xmeta,ymeta;
  int xnegflag,ynegflag; // 1 if negative
  //int termflag; // 1 if complex atom  
  gint xatom;
  gint yatom;
  gptr xcl;
  int ures;  

#ifdef DEBUG
  wr_printf("\n!!! wr_factor called for clause ");
  wr_print_clause(g,cl);
  wr_printf("\n");   
  wr_print_vardata(g);
#endif  
  // cannot factor unit clauses
  if (! wg_rec_is_rule_clause(db,cl)) return;
  len = wg_count_clause_atoms(db, cl);  
  if (len<2) return;
  xcl=cl;  
  // loop over literals
  for(i=0; i<len; i++) { 
    if ((g->queryfocus_strat) && i>0) break; // only the first lit of given allowed for query strat     
    //termflag=0;
    //addflag=0;   
    xmeta=wg_get_rule_clause_atom_meta(db,xcl,i);
    xatom=wg_get_rule_clause_atom(db,xcl,i); 
    if (wg_atom_meta_is_neg(db,xmeta)) xnegflag=1;   
    else xnegflag=0;  
    for(j=i+1; j<len; j++) {  
      ymeta=wg_get_rule_clause_atom_meta(db,xcl,j);      
      if (wg_atom_meta_is_neg(db,ymeta)) ynegflag=1;   
      else ynegflag=0;
      if (ynegflag!=xnegflag) continue;
      yatom=wg_get_rule_clause_atom(db,xcl,j);
#ifdef DEBUG                  
      wr_printf("--- xatom i %d \n",i);        
      wr_print_record(g,wg_decode_record(db,xatom));
      wr_printf("xnegflag %d\n",xnegflag);
      wr_printf("yatom nr %d  \n",j);        
      wr_print_record(g,wg_decode_record(db,yatom));
      wr_printf("ynegflag %d\n",ynegflag);
#endif            
      if (wg_get_encoded_type(db,xatom)==WG_RECORDTYPE &&
          wg_get_encoded_type(db,yatom)==WG_RECORDTYPE) {
        //termflag=1;
        ures=wr_unify_term(g,xatom,yatom,1); // uniquestrflag=1
#ifdef DEBUG        
        wr_printf("unification check res: %d\n",ures);
#endif        
        if (ures) {
          // build and process the new clause
#ifdef DEBUG         
          wr_printf("\nin wr_factor to call wr_process_factor_result\n");
#endif           
          wr_process_factor_result(g,xcl,len,i,xatom,j,yatom,cl_as_active);                      
          if (g->proof_found || g->alloc_err) {
            wr_clear_varstack(g,g->varstack); 
            if (wr_enough_answers(g) || g->alloc_err) return;                      
          }  
        }      
        wr_clear_varstack(g,g->varstack);             
      }     
    }  
  }      
#ifdef DEBUG
  wr_printf("!!! wr_factor finished \n");
#endif    
  return;
}


/*

  wr_paramodulate_from_all_active takes a given cl
  and tries to paramodulate from positive equalities in this given cl
  into terms of the clauses stored earlier as active,
  i.e. earlier clauses do not need to contain equalities.

*/


/* paramodulation with an equation that has a variable side (X = t, X not in
   t; or X = Y). A variable unifies with every term, so the side has no bucket
   of its own top symbol: the active variable sides are in hash_eq_varsides,
   and a variable side of the given clause walks every stored "into" term of
   hash_para_terms. The step is made only when the instance of the variable
   is not the smaller side after the unification, and not at all when the
   variable occurs in another literal of its clause (wr_varside_in_other_lits). */

/* para_into_ordered 2: the test of the "into" literal. When the literal atom
   is an equation, the side that holds the position pos (the number of the
   position in preorder, as the walks of the stored and the given terms
   count it: every field of a term below its top except the symbol) must not
   be smaller than the other side under the bindings of the unification. 1:
   the step may be made. */

static int wr_para_pos_count(glb* g, gint t) {
  void* db=g->db; gptr ptr; gint yi; int len, istart, ilimit, i, n=0;
  if (!isdatarec(t)) return 0;
  ptr=decode_record(db,t);
  len=get_record_len(ptr);
  if ((g->unify_maxuseterms) && ((g->unify_maxuseterms)+(g->unify_firstuseterm))<len)
    len=(g->unify_firstuseterm)+(g->unify_maxuseterms);
  istart=RECORD_HEADER_GINTS+(g->unify_firstuseterm); ilimit=RECORD_HEADER_GINTS+len;
  for(i=istart;i<ilimit;i++) {
    yi=ptr[i];
    if (i==istart && !isdatarec(yi)) continue;
    n+=1+wr_para_pos_count(g,yi);
  }
  return n;
}

static int wr_para_into_lit_ok(glb* g, gint atom, int pos) {
  void* db=g->db; gptr ptr; gint yi, s, t; int len, istart, ilimit, i, k=0, n;
  if (!isdatarec(atom) || !wr_equality_atom(g,atom)) return 1;
  ptr=decode_record(db,atom);
  len=get_record_len(ptr);
  if ((g->unify_maxuseterms) && ((g->unify_maxuseterms)+(g->unify_firstuseterm))<len)
    len=(g->unify_firstuseterm)+(g->unify_maxuseterms);
  istart=RECORD_HEADER_GINTS+(g->unify_firstuseterm); ilimit=RECORD_HEADER_GINTS+len;
  for(i=istart;i<ilimit;i++) {
    yi=ptr[i];
    if (i==istart && !isdatarec(yi)) continue;
    k++;
    n=wr_para_pos_count(g,yi);
    if (pos>=k && pos<=k+n) break;
    k+=n;
  }
  if (i==RECORD_HEADER_GINTS+(g->unify_funarg1pos)) { s=ptr[i]; t=ptr[RECORD_HEADER_GINTS+(g->unify_funarg2pos)]; }
  else if (i==RECORD_HEADER_GINTS+(g->unify_funarg2pos)) { s=ptr[i]; t=ptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)]; }
  else return 1;
  if (wr_order_eqterms(g,s,t,(g->varbanks))!=2) return 1;
  (g->stat_para_lit_order_skip)++;
  return 0;
}

/* the test of the "into" literal for a step from the given clause into the
   stored term with the path path of the active clause ycl */

static int wr_para_into_active_lit_ok(glb* g, gptr ycl, gint path) {
  void* db=g->db; gint yatom; int litnr;
  if ((g->para_into_ordered)!=2 || WR_QF_FORCE3(g)) return 1;
  litnr=wr_decode_para_termpath_litnr(g,path);
  if (wg_rec_is_rule_clause(db,ycl)) yatom=wg_get_rule_clause_atom(db,ycl,litnr);
  else yatom=encode_record(db,ycl);
  return wr_para_into_lit_ok(g,yatom,wr_decode_para_termpath_pos(g,path));
}

/* the tests of the active clause ycl, as in the loops of the other sides */

static int wr_para_partner_ok(glb* g, gptr ycl, int nonanslen, int upon, int unitonly) {
  void* db=g->db;
  (void)db; // the clause macros do not use it
  if ((g->prohibit_nested_para) && (wr_get_cl_history_tag(g,ycl)==WR_HISTORY_TAG_PARA)) return 0;
  if (upon && wr_up_unusable(g,ycl)) return 0;
  if ((g->back_subsume) && wr_get_cl_backsubsumed(g,ycl)) return 0;
  if ((g->res_arglen_limit) && (wg_rec_is_rule_clause(db,ycl))) {
    if (wr_count_cl_nonans_atoms(g,ycl) > (g->res_arglen_limit)) return 0;
  }
  if ((g->res_shortarglen_limit) && (nonanslen>(g->res_shortarglen_limit))) {
    if (wr_count_cl_nonans_atoms(g,ycl) > (g->res_shortarglen_limit)) return 0;
  }
  if (unitonly && (g->posunitpara_strat) && (wg_rec_is_rule_clause(db,ycl))) {
    if (wr_count_cl_nonans_atoms(g,ycl)>1) return 0;
  }
  return 1;
}

/* 0 when the search must stop: a proof (enough answers), no memory or the time limit */

static int wr_para_varside_continue(glb* g) {
  clock_t curclock;
  float run_seconds, run_dseconds;
  if (g->proof_found || g->alloc_err) {
    if (wr_enough_answers(g) || g->alloc_err) return 0;
  }
  if ((g->stat_derived_cl%256)==0 || (g->derived_overflow_timeout)) {
    if (g->derived_overflow_timeout) return 0;
    curclock=clock();
    run_seconds = (float)(curclock - (g->run_start_clock)) / CLOCKS_PER_SEC;
    if ((g->max_run_seconds) && (run_seconds>(g->max_run_seconds))) return 0;
    run_dseconds = (float)(curclock - (g->run_start_clock)) / ((float)CLOCKS_PER_SEC / 10);
    if ((g->max_run_dseconds) && (run_dseconds>(g->max_run_dseconds))) return 0;
  }
  return 1;
}

/* from the variable side a of the literal xatom (number litnr) of the given
   clause xcl into the stored terms of one hash vector: the buckets of the
   list buckets, or all buckets when buckets is NULL (an attached KB) */

static int wr_para_varside_from_vec(glb* g, gptr xcl, gptr cl_as_active, gint xatom, gint a, gint b,
                                    int leftflag, int nonanslen, vec hashvec, cvec buckets) {
  void* db=g->db;
  gint k, kend, hash, node, yterm, path;
  gptr nodeptr, ycl;
  int upon=((g->up_index)!=NULL);
  kend=(buckets ? CVEC_NEXT(buckets) : VEC_LEN(hashvec));
  for(k=(buckets ? CVEC_START : 1); k<kend; k++) {
    hash=(buckets ? buckets[k] : k);
    if (hash<1 || hash>=VEC_LEN(hashvec) || !hashvec[hash]) continue;
    for(node=wr_clterm_hashlist_start(g,hashvec,hash); node; node=wr_clterm_hashlist_next(g,hashvec,node)) {
      nodeptr=otp(db,node);
      yterm=nodeptr[CLTERM_HASHNODE_TERM_POS];
      ycl=otp(db,nodeptr[CLTERM_HASHNODE_CL_POS]);
      if (!wr_para_partner_ok(g,ycl,nonanslen,upon,0)) continue;
      if (wr_unify_term(g,a,yterm,1)) { // uniquestrflag=1
        (g->stat_varside_tried)++;
        if (!WR_QF_NONE(g) && wr_order_eqterms(g,a,b,(g->varbanks))==2) {
          (g->stat_varside_skip_order)++;
        } else {
          path=wg_decode_int(db,nodeptr[CLTERM_HASHNODE_PATH_POS]);
          if (wr_para_into_active_lit_ok(g,ycl,path)) {
            (g->stat_varside_steps)++;
            wr_process_paramodulate_result(g,xatom,xcl,0,ycl,cl_as_active,a,b,path,leftflag,1);
          }
        }
      }
      wr_clear_varstack(g,g->varstack);
      if (!wr_para_varside_continue(g)) return 0;
    }
  }
  return 1;
}

static int wr_para_varside_from(glb* g, gptr xcl, gptr cl_as_active, gint xatom, int litnr,
                                gint a, gint b, int leftflag, int nonanslen) {
  // the condition of the filing: X does not occur in the other side (also when
  // sos_ordering "none" gives 3 to every equation)
  if (wr_order_eqterms(g,a,b,NULL)!=3) return 1;
  if (wr_varside_in_other_lits(g,xcl,litnr,a)) { (g->stat_varside_skip_lit)++; return 1; }
  (g->stat_varside_given)++;
  if (g->para_terms_buckets) {
    if (!wr_para_varside_from_vec(g,xcl,cl_as_active,xatom,a,b,leftflag,nonanslen,
                                  rotp(g,g->hash_para_terms),rotp(g,g->para_terms_buckets))) return 0;
  }
  if (r_kb_g(g)) {
    if (!wr_para_varside_from_vec(g,xcl,cl_as_active,xatom,a,b,leftflag,nonanslen,
                                  rotp(g,r_kb_g(g)->hash_para_terms),NULL)) return 0;
  }
  return 1;
}

/* into the subterm xterm (at the path litnr, origtermpath) of the given clause
   xcl from the active variable sides */

static int wr_para_varsides_into(glb* g, gptr xcl, gptr cl_as_active, gint xatom, gint xterm,
                                 int litnr, int origtermpath, int nonanslen) {
  void* db=g->db;
  vec hashvec;
  gint node, yterm, yatom, b, eqpath;
  gptr ycl;
  int upon=((g->up_index)!=NULL), eqlitnr, eqleftflag;
  if (!(g->hash_eq_varsides)) return 1;
  hashvec=rotp(g,g->hash_eq_varsides);
  for(node=wr_clterm_hashlist_start(g,hashvec,1); node; node=wr_clterm_hashlist_next(g,hashvec,node)) {
    yterm=(otp(db,node))[CLTERM_HASHNODE_TERM_POS];
    ycl=otp(db,(otp(db,node))[CLTERM_HASHNODE_CL_POS]);
    if (!wr_para_partner_ok(g,ycl,nonanslen,upon,1)) continue;
    if (wr_unify_term(g,xterm,yterm,1)) { // uniquestrflag=1
      (g->stat_varside_tried)++;
      eqpath=wg_decode_int(db,otp(db,node)[CLTERM_HASHNODE_PATH_POS]);
      eqlitnr=wr_decode_para_termpath_litnr(g,eqpath);
      eqleftflag=wr_decode_para_termpath_pos(g,eqpath);
      if (wg_rec_is_rule_clause(db,ycl)) yatom=wg_get_rule_clause_atom(db,ycl,eqlitnr);
      else yatom=encode_record(db,ycl);
      if (eqleftflag) b=((gint *)(otp(db,yatom)))[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];
      else b=((gint *)(otp(db,yatom)))[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
      if (!WR_QF_NONE(g) && wr_order_eqterms(g,yterm,b,(g->varbanks))==2) {
        (g->stat_varside_skip_order)++;
      } else if ((g->para_into_ordered)!=2 || WR_QF_FORCE3(g) || wr_para_into_lit_ok(g,xatom,origtermpath)) {
        (g->stat_varside_steps)++;
        wr_process_paramodulate_result(g,yatom,ycl,xatom,xcl,cl_as_active,yterm,b,
                                       wr_encode_para_termpath(g,litnr,origtermpath),eqleftflag,0);
      }
    }
    wr_clear_varstack(g,g->varstack);
    if (!wr_para_varside_continue(g)) return 0;
  }
  return 1;
}

void wr_paramodulate_from_all_active(glb* g, gptr cl, gptr cl_as_active, cvec resolvability) {
  int upon; // unit propagation is on and its table exists 
  void* db=g->db;
  int i;
  int len;      
  int ruleflag; // 0 if not rule 
  gint meta;
  int nonanslen; // length without ans preds 
  gint hash;
  int dbused;
  vec hashvec;
  int hlen;
  gint node;
  gint xatom=0;
  gint yterm;
  gptr xcl;
  gptr ycl;
  int termpos;
  int ures;

  int atomlen;
  gint a,b,c;
  gint atype, btype, ctype;
  gptr tptr, nodeptr, ptr;
  gint fun, path;
  int eqtermorder=3,eqtermorder_after=3;
  clock_t curclock;
  float run_seconds,run_dseconds;
   
#ifdef DEBUG
  wr_printf("\n!!! wr_paramodulate_from_all_active called for clause ");
  wr_print_clause(g,cl);
  wr_printf("\n");     
  wr_print_vardata(g);
#endif  
  // get clause data for input clause
       
  ruleflag=wg_rec_is_rule_clause(db,cl);
  if (ruleflag) len = wg_count_clause_atoms(db, cl);
  else len=1;  

#ifdef EQ_PREFERENCE
  if (len>1 && ruleflag) {
    for(i=0; i<len; i++) {
      xatom=wg_get_rule_clause_atom(db,cl,i);
      if (wg_get_encoded_type(db,xatom)!=WG_RECORDTYPE ||
          !wr_equality_atom(g,xatom)) {                  
        return;  // do not paramodulate from clauses containing some non-equality literals
      }  
      meta=wg_get_rule_clause_atom_meta(db,cl,i);
      if (wg_atom_meta_is_neg(db,meta)) return;     
    }  
  }
#endif 

  nonanslen=wr_count_cl_nonans_atoms(g,cl);
  // check if strategy allows to para from this clause
  if ((g->posunitpara_strat) && len!=1) return;
  if ((g->hyperres_strat) && !(g->relaxed_hyperres_strat) && !wr_hyperres_satellite_cl(g,cl)) return;
  xcl=cl; 
#ifdef DEBUG
  wr_printf("ruleflag %d len %d posok %d negok %d\n",
          ruleflag,len,posok,negok);
#endif  
  // loop over literals
  for(i=0; i<len; i++) {
    if (!(resolvability[i+1])) continue; // limit para to resolvable only (!!???) TESTING: normal: do limit
    if (!ruleflag) {
      xatom=encode_record(db,xcl);
#ifdef DEBUG 
      wr_printf("\n xatom looked at:\n");       
      wr_print_record(g,wg_decode_record(db,xatom));
#endif
      if (!wr_equality_atom(g,xatom)) continue;
    } else {       
      meta=wg_get_rule_clause_atom_meta(db,xcl,i);
      if (wg_atom_meta_is_neg(db,meta)) continue;     
      xatom=wg_get_rule_clause_atom(db,xcl,i);             
#ifdef DEBUG            
      wr_printf("\natom nr %d from record \n",i);
      wr_print_record(g,xcl);
      wr_printf("\natom\n");              
      wr_print_record(g,wg_decode_record(db,xatom));               
#endif            
      if (wg_get_encoded_type(db,xatom)!=WG_RECORDTYPE ||
          !wr_equality_atom(g,xatom)) {                  
        continue;
      }                 
    }    
    if (!xatom) continue;
#ifdef DEBUG             
    wr_printf("\nxatom\n"); 
    if (xatom) wr_print_record(g,wg_decode_record(db,xatom));        
#endif 
    tptr=rotp(g,xatom);
    atomlen=get_record_len(tptr);
    if (atomlen<(g->unify_firstuseterm)+3) continue;
    // xcl: active clause
    // xatom: active atom    
    if (1) {            
      // check ordering: which terms are ok for para  
   
      if ((g->print_litterm_selection) && len>1) {
        wr_printf("\nselected nr %d for para-from: ",i);
        wr_print_term(g,xatom);
      }

      a=tptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
      b=tptr[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];
      if (WR_QF_FORCE3(g)) eqtermorder=3; // query_focus (sos_ordering absent or "none")
      else 
#ifdef EQTERM_ORDER   
            eqtermorder=wr_order_eqterms(g,a,b,NULL);
#else
            eqtermorder=3;
#endif        
      atype=wg_get_encoded_type(db,a);
      btype=wg_get_encoded_type(db,b);
      /*
      wr_printf("\n a and b terms: ");               
      wr_print_term(g,a);        
      wr_printf(" and ");
      wr_print_term(g,b);  
      wr_printf(" : ");      
      wr_printf(" eqtermorder: %d\n",eqtermorder);
      */
      // loop over equality args (two):
      for(termpos=0; termpos<2; termpos++) {
        // eqtermorder values:
        // 0: none bigger, neither ok for para
        // 1: a bigger than b (prohibits b)
        // 2: b bigger than a (prohibits a)
        // 3: none bigger, both ok for para
        if (eqtermorder==0) break; // cannot para at all
        else if (eqtermorder==1 && termpos!=0) break; // only the first ok
        else if (eqtermorder==2 && termpos!=1) continue; // only second ok
        if ((g->prohibit_unordered_para) && (eqtermorder==3)) break;  
        // else both ok              
        if (termpos) {        
          // swap a and b for the second arg
          c=a;
          a=b;
          b=c;
          ctype=atype;
          atype=btype;
          btype=ctype;
        }      
        //printf("\n termpos used: %d\n",termpos);          

        if (g->print_litterm_selection) {
          if (!eqtermorder) { wr_printf("\nparamodulation blocked"); }
          else if (eqtermorder==1) { wr_printf("\npara from left"); }
          else if (eqtermorder==2) { wr_printf("\npara from right"); }
          else { wr_printf("\npara from left and right");  }       
        }
#ifdef DEBUG 
        wr_printf("\n !!!! termpos %d, a and b terms: \n",termpos);               
        wr_print_term(g,a);        
        wr_printf("\n");
        wr_print_term(g,b);  
        wr_printf("\n");
#endif      
#ifdef EQTERM_ORDER   
        // computed again for the side: also with query_focus, unless sos_ordering is "none"
        eqtermorder=(WR_QF_NONE(g) ? 3 : wr_order_eqterms(g,a,b,NULL));
#else
        eqtermorder=3;
#endif        
#ifdef DEBUG        
        wr_printf("\n eqtermorder: %d\n",eqtermorder);
#endif 
        // var types are not ok for from eq arg
        if (atype!=WG_VARTYPE) {
            // (atype==WG_RECORDTYPE || atype==WG_URITYPE || atype==WG_ANONCONSTTYPE) {
          if (atype==WG_RECORDTYPE) {
            ptr=decode_record(db,a);  
            fun=get_field(ptr,(g->unify_funpos)); 
          } else {
            fun=a;
          }  
          hash=wr_term_basehash(g,fun); 
          // now loop over hash vectors for all active unification candidates
          // ycl: cand clause
          // yatom: cand atom
    #ifndef QUIET      
          wr_printf("\n----- inner wr_genloop cycle (active hash list) starts ----------\n"); 
    #endif          

          // loop over local termhash (dbused==0) and external termhash (dbused==1)
          for(dbused=0; dbused<2; dbused++) {            
            if (dbused==0) {
              // local termhash
        #ifdef DEBUG
              wr_printf("\n**** in wr_paramodulate_from_all_active dbused %d hashvec is *****\n",dbused);
              //wr_clterm_hashlist_print(g,hashvec);
        #endif              
              hashvec=rotp(g,g->hash_para_terms);       
              //wr_clterm_hashlist_print(g,hashvec);       
              hlen=wr_clterm_hashlist_len(g,hashvec,hash);
              if (hlen==0) {
        #ifdef DEBUG        
                wr_printf("no matching atoms in hash\n");
        #endif        
                continue;
              }   
            } else {   
              // external kb termhash           
    #ifdef DEBUG
              wr_printf("\n**** in wr_paramodulate_from_all_active dbused %d hashvec is *****\n",dbused);
              //wr_clterm_hashlist_print(g,hashvec);
    #endif
              if (!(r_kb_g(g))) continue;             
              //printf("\nr_kb_g(g) used\n");
              hashvec=rotp(g,r_kb_g(g)->hash_para_terms);       
              //wr_clterm_hashlist_print(g,hashvec);       
              hlen=wr_clterm_hashlist_len(g,hashvec,hash);
              if (hlen==0) {
        #ifdef DEBUG        
                wr_printf("no matching atoms in hash\n");
        #endif        
                continue;
              }               
            }

            node=wr_clterm_hashlist_start(g,hashvec,hash);
            if (!node)  {
              wr_sys_exiterr(g,"apparently wrong hash given to wr_clterm_hashlist_start");
              return;
            }      
            upon=((g->up_index)!=NULL); // unit propagation: a clause becomes unusable only before a pick
            while(node!=0) {  
              nodeptr=otp(db,node);  
              yterm=nodeptr[CLTERM_HASHNODE_TERM_POS];
              ycl=otp(db,nodeptr[CLTERM_HASHNODE_CL_POS]); 
              if ((g->prohibit_nested_para) && (wr_get_cl_history_tag(g,ycl)==WR_HISTORY_TAG_PARA)) {
                node=wr_clterm_hashlist_next(g,hashvec,node);
                continue;
              }       
              if (upon && wr_up_unusable(g,ycl)) {
                // made unusable by unit propagation
                node=wr_clterm_hashlist_next(g,hashvec,node);
                continue;
              }
              /*
              wr_printf("after while(node!=0): \n");
              wr_printf("ycl: \n");
              wr_print_clause(g,ycl);    
              wr_printf("yterm: \n");
              wr_print_term(g,yterm);      
              wr_printf("nodeptr[CLTERM_HASHNODE_PATH_POS]: %d\n",nodeptr[CLTERM_HASHNODE_PATH_POS]);
              */
              //printf("xcl: \n");
              //wr_print_clause(g,xcl);
              //printf("xatom: \n");
              //wr_print_term(g,xatom);
              if ((g->back_subsume) && wr_get_cl_backsubsumed(g,ycl)) {
                /*
                wr_printf("\nCP2 blocked clause: \n");          
                wr_print_clause(g,ycl);
                wr_printf("\n");
                */
                node=wr_clterm_hashlist_next(g,hashvec,node);
                continue;
              }
              if (g->print_active_cl) {
                wr_printf("\n* active f: ");
                wr_print_clause(g,ycl); 
                //printf("\n");
              }  
              if ((g->res_arglen_limit) && (wg_rec_is_rule_clause(db,ycl))) {
                if (wr_count_cl_nonans_atoms(g,ycl) > (g->res_arglen_limit)) {
                  // cannot use: continue loop
                  node=wr_clterm_hashlist_next(g,hashvec,node);
                  continue;
                }
              }
              if ((g->res_shortarglen_limit) && (nonanslen>(g->res_shortarglen_limit))) {
                // must check that ycl is unit
                if (wr_count_cl_nonans_atoms(g,ycl) > (g->res_shortarglen_limit)) {
                  // cannot use: continue loop
                  node=wr_clterm_hashlist_next(g,hashvec,node);
                  continue;                   
                }  
              }
              
      #ifdef DEBUG        
              wr_printf("\neq term a ");
              wr_print_term(g,a);
              wr_printf(" in xcl ");
              wr_print_clause(g,xcl);
              wr_printf("\nyterm ");
              wr_print_term(g,yterm);
              wr_printf(" in ycl ");
              wr_print_clause(g,ycl);
              wr_print_vardata(g);
      #endif          
   

              if (g->print_litterm_selection) {                
                wr_printf("\nactive paramodulation term ");
                wr_print_term(g,yterm);
                wr_printf("\n");                       
              }
              ures=wr_unify_term(g,a,yterm,1); // uniquestrflag=1
      #ifdef DEBUG        
              wr_printf("unification check res: %d\n",ures);
      #endif        
              if (ures) {
                if (eqtermorder==3) {
                  // equality terms were initially unordered
                  // check order after unification
#ifdef EQTERM_ORDER   
                  eqtermorder_after=(WR_QF_NONE(g) ? 3 : wr_order_eqterms(g,a,b,(g->varbanks)));
#else
                  eqtermorder=3;
#endif                        
                  //printf("\n eqtermorder_after: %d\n",eqtermorder_after);
                } else {
                  eqtermorder_after=eqtermorder;
                }  
              }
              /*             
              if (ures && eqtermorder==3) {
                // equality terms were initially unordered
                // check order after unification
                eqtermorder_after=wr_order_eqterms(g,a,b,(g->varbanks));       
                //printf("\n eqtermorder_after: %d\n",eqtermorder_after);
              }
              */
              // eqtermorder values:
              // 0: none bigger, neither ok for para
              // 1: a bigger than b (prohibits b)
              // 2: b bigger than a (prohibits a)
              // 3: none bigger, both ok for para
              
              //if (ures && ((eqtermorder!=3) || (eqtermorder_after==3) || eqtermorder_after==1)) {     
              if (ures &&
                   (((g->prohibit_unordered_para) && eqtermorder_after==1) ||
                    (!(g->prohibit_unordered_para) && ((eqtermorder_after==3) || eqtermorder_after==1)) )) {
                // build and process the new clause
      #ifdef DEBUG         
                wr_printf("\nin wr_paramodulate_from_all_active to call wr_process_paramodulate_result\n");
      #endif                                       
                path=wg_decode_int(db,nodeptr[CLTERM_HASHNODE_PATH_POS]);              
                //printf("\n path before call %d\n",(int)path);
                if (wr_para_into_active_lit_ok(g,ycl,path))
                  wr_process_paramodulate_result(g,xatom,xcl,0,ycl,cl_as_active,a,b,path,!termpos,1);                 
      #ifdef DEBUG                        
                wr_printf("\nin wr_paramodulate_from_all_active after wr_process_paramodulate_result\n");
                wr_printf("\na\n");
                wr_print_term(g,a);
                //printf("\nyatom\n");
                //wr_print_term(g,yatom);
                wr_printf("!!!!! wr_paramodulate_from_all_active after  wr_process_paramodulate_result queue is\n");
                wr_show_clqueue(g);
                wr_printf("\nqueue ended\n");
      #endif             
                if (g->proof_found || g->alloc_err) {
                  wr_clear_varstack(g,g->varstack);    
                  if (wr_enough_answers(g) || g->alloc_err) return;          
                }  
              } else {
                if (g->print_litterm_selection) {                
                  wr_printf("\npara result blocked by ordering");                                     
                }
              }
      #ifdef DEBUG
              wr_printf("wr_paramodulate_from_all_active before wr_clear_varstack queue is\n");
              wr_show_clqueue(g);
              wr_printf("\nqueue ended\n");
      #endif         
              wr_clear_varstack(g,g->varstack);              
              //wr_print_vardata(g);
              // extra time check              
              if ((g->stat_derived_cl%256)==0 || (g->derived_overflow_timeout)) {
                if (g->derived_overflow_timeout) return; // time limit passed at a drop
                curclock=clock();
                run_seconds = (float)(curclock - (g->run_start_clock)) / CLOCKS_PER_SEC;            
                if ((g->max_run_seconds) && (run_seconds>(g->max_run_seconds))) return;
                run_dseconds = (float)(curclock - (g->run_start_clock)) / ((float)CLOCKS_PER_SEC / 10);            
                if ((g->max_run_dseconds) && (run_dseconds>(g->max_run_dseconds))) return;
              }
              // get next node;
              node=wr_clterm_hashlist_next(g,hashvec,node);       
            } // end loop over nodes in the hash bucket
          }  // end loop over local and external termhashes
        } else if (eqtermorder==3) {
          // a variable side: into every stored term
          if (!wr_para_varside_from(g,xcl,cl_as_active,xatom,i,a,b,!termpos,nonanslen)) return;
        } // if (atype==WG_RECORDTYPE ...
      }  // for(termpos=0; ...    
    }  // if (useflag) ...
  } // for(i=0; i<len;        
  return;
}

/*

  wr_paramodulate_into_all_active takes a given cl
  and tries to paramodulate from positive equalities in active clauses
  stored earlier as active.

  i.e. given_cl does not have to contain equalities, but
  each relevant clause found in active clauses must contain equalities

*/


void wr_paramodulate_into_all_active(glb* g, gptr cl, gptr cl_as_active, cvec resolvability) { 
  void* db=g->db;
  int i;
  int len;      
  int ruleflag; // 0 if not rule
  int nonanslen; // length not counting ans literals
  gint xatom=0;
  gptr xcl;
  int termpath;
  gint subterm;
  int tmp;

#ifdef DEBUG
  wr_printf("\n!!! wr_paramodulate_into_all_active called for clause ");
  wr_print_clause(g,cl);
  wr_printf("\n");
#endif  
  // get clause data for input clause
       
  ruleflag=wg_rec_is_rule_clause(db,cl);
  if (ruleflag) len = wg_count_clause_atoms(db, cl);
  else len=1;
  if ((g->hyperres_strat) && !(g->relaxed_hyperres_strat) && !wr_hyperres_satellite_cl(g,cl)) return;
  nonanslen=wr_count_cl_nonans_atoms(g,cl);
  xcl=cl; 
#ifdef DEBUG
  wr_printf("ruleflag %d len %d posok %d negok %d\n",
          ruleflag,len,posok,negok);
#endif  
  // loop over literals
  for(i=0; i<len; i++) {  
    if (!(resolvability[i+1])) continue; // limit para to resolvable only (!!???) TESTING: normal: do limit
    if (!ruleflag) {
      xatom=encode_record(db,xcl);
#ifdef DEBUG      
      wr_print_record(g,wg_decode_record(db,xatom));
#endif
    } else {       
      xatom=wg_get_rule_clause_atom(db,xcl,i);       
#ifdef DEBUG        
      wr_printf("atom nr %d from record \n",i);
      wr_print_record(g,xcl);
      wr_printf("\natom\n");              
      wr_print_record(g,wg_decode_record(db,xatom));             
#endif                                 
    }
    // xcl: active clause
    // xatom: active atom
    if (!xatom) continue;
    if (1) {      
      if ((g->print_litterm_selection) && len>1) {
        wr_printf("\nselected nr %d for para-into: ",i);
        wr_print_term(g,xatom);
      }
      termpath=0;
      subterm=xatom;              
      tmp=wr_paramodulate_into_subterms_all_active(g,cl,cl_as_active,xatom,subterm,0,i,&termpath,nonanslen);
      if (tmp<0) {
        // hard error
        wr_sys_exiterr(g,"apparently wrong hash given to wr_clterm_hashlist_start");
        return;         
      }
    }
  }
}    

/*
  return 1 is OK, return 0 indicates a discarded res, return -1 indicates error

*/

int wr_paramodulate_into_subterms_all_active(glb* g, gptr cl, gptr cl_as_active, gint atom, gint term, 
      int depth, int litnr, int* termpath, int nonanslen) {
  int upon; // unit propagation is on and its table exists          
  void* db=g->db;
  gptr ptr;
  gint fun, yi, hash;
  int len, istart, ilimit, i, origtermpath; // path
  vec hashvec;
  gint xterm,yterm,xatom;
  gptr xcl,ycl;
  gint eqpath, eqlitnr, yatom;
  gint b;
  int eqleftflag;
  int hlen;
  gint node;
  int ures;
  int dbused;
  clock_t curclock;
  float run_seconds,run_dseconds;


  int replpath;

#ifdef DEBUG
  wr_printf("\n !+!+!+! wr_paramodulate_into_subterms_all_active called at depth %d and litnr %d on term and cl: ",depth,litnr); 
  wr_print_term(g,term);
  wr_printf("\n");
  wr_print_clause(g,cl);
  wr_printf("\n");
#endif  
  if (litnr>PARA_TERMPATH_MAXLITNR || (*termpath)>PARA_TERMPATH_MAXPOS) {
    (g->stat_internlimit_discarded_para)++;
    return 0;
  }
  origtermpath=(*termpath);
  if (!isdatarec(term)) {
    // term is a primitive
    if (isvar(term)) return 1;
    // term is a constant
    fun=term;    
  } else {
    // complex term   
    ptr=decode_record(db,term);  
    fun=get_field(ptr,(g->unify_funpos)); 
    // recurse into arguments
    len=get_record_len(ptr);    
    if (g->unify_maxuseterms) {
      if (((g->unify_maxuseterms)+(g->unify_firstuseterm))<len) 
        len=(g->unify_firstuseterm)+(g->unify_maxuseterms);
    }    
    istart=RECORD_HEADER_GINTS+(g->unify_firstuseterm);
    ilimit=RECORD_HEADER_GINTS+len;
    for(i=istart; i<ilimit; i++) {   
      yi=*(ptr+i);

      //printf("\ndepth %d i %d istart %d ilimit %d \n",depth,i,istart,ilimit);
      //wr_print_term(g,yi);  
      //printf("\n");
#ifdef PARA_INTO_FUNSYMB
      (*termpath)++;
      wr_printf("\ndepth %d i %d unifiable subterm \n",depth,i);
      wr_print_term(g,yi);  
      wr_printf("\n");
      //wr_cl_store_para_subterms(g,cl,yi,depth+1,litnr,termpath);
#else
      // do not recurse into atomic funsymb
      if (!(i==istart && !isdatarec(yi))) {
        (*termpath)++;
        if (isvar(yi)) continue;
        if (g->print_litterm_selection) {                
          wr_printf("\nselected term at depth %d path %d for para-into: ",depth,(*termpath));
          wr_print_term(g,yi);
          wr_printf(" term ");
          wr_print_term(g,term);
        }    
        wr_paramodulate_into_subterms_all_active(g,cl,cl_as_active,atom,yi,depth+1,litnr,termpath,nonanslen);
        /*
        if ( !(g->prohibit_deep_para) || 
             ((g->prohibit_deep_para) && (depth<1))) {
          wr_paramodulate_into_subterms_all_active(g,cl,cl_as_active,atom,yi,depth+1,litnr,termpath,nonanslen);
        } 
        */ 
        if (g->proof_found || g->alloc_err) break;
      }
#endif
      
    }
  }  
  /*
  if (g->proof_found || g->alloc_err) {
    printf("\nwr_paramodulate_into_subterms_all_active proceeding with proof \n");
  }
  */
#ifdef PARA_INTO_ATOM  
#else
  // do not put into hash on the atom level  
  if (depth<1) return 1;  
#endif  
  if ((g->prohibit_deep_para) && (depth>2)) return 1;

  // find matching terms from the hash table
  hash=wr_term_basehash(g,fun);
  xatom=atom; // atom of the subterm in the given cl, for which we search for eq unifier in hash
  xterm=term; // subterm in the given cl, for which we search for eq unifier in hash 
  xcl=cl; // given cl the subterms of which are matched with eq args in active
  if (!wr_para_varsides_into(g,xcl,cl_as_active,xatom,xterm,litnr,origtermpath,nonanslen)) return 1;
#ifdef DEBUG 
  wr_printf("\nin wr_paramodulate_into_subterms_all_active fun hash is: %d litnr %d origtermpath %d for fun and term:\n",
    (int)hash,litnr,origtermpath);
  wr_print_term(g,fun);
  wr_printf("\n");  
  wr_print_term(g,term);
  wr_printf("\n");
#endif      


  // loop over local termhash (dbused==0) and external termhash (dbused==1)
  for(dbused=0; dbused<2; dbused++) {            
    if (dbused==0) {
      // local termhash
#ifdef DEBUG
      wr_printf("\n**** in wr_paramodulate_into_subterms_all_active dbused %d hashvec is *****\n",dbused);
      //wr_clterm_hashlist_print(g,hashvec);
#endif              
      hashvec=rotp(g,g->hash_eq_terms);       
      //wr_clterm_hashlist_print(g,hashvec);       
      hlen=wr_clterm_hashlist_len(g,hashvec,hash);
      if (hlen==0) {
#ifdef DEBUG        
        wr_printf("no matching atoms in hash\n");
#endif        
        continue;
      }   
    } else {   
      // external kb termhash           
#ifdef DEBUG
      wr_printf("\n**** in wr_paramodulate_into_subterms_all_active dbused %d hashvec is *****\n",dbused);
      //wr_clterm_hashlist_print(g,hashvec);
#endif
      if (!(r_kb_g(g))) return 1;             
      //printf("\nr_kb_g(g) used\n");
      hashvec=rotp(g,r_kb_g(g)->hash_eq_terms);       
      //wr_clterm_hashlist_print(g,hashvec);       
      hlen=wr_clterm_hashlist_len(g,hashvec,hash);
      if (hlen==0) {
#ifdef DEBUG        
        wr_printf("no matching atoms in hash\n");
#endif        
        return 1;
      }               
    }

    node=wr_clterm_hashlist_start(g,hashvec,hash);
    if (!node)  {
      wr_sys_exiterr(g,"apparently wrong hash given to wr_clterm_hashlist_start");
      return -1;
    }      
    upon=((g->up_index)!=NULL); // unit propagation: a clause becomes unusable only before a pick
    while(node!=0) {    
      //nodeptr=otp(db,node);
      yterm=(otp(db,node))[CLTERM_HASHNODE_TERM_POS];     //nodeptr[CLTERM_HASHNODE_TERM_POS];
      ycl=otp(db,(otp(db,node))[CLTERM_HASHNODE_CL_POS]); //(otp(db,nodeptr)[CLTERM_HASHNODE_CL_POS]);
      if ((g->prohibit_nested_para) && (wr_get_cl_history_tag(g,ycl)==WR_HISTORY_TAG_PARA)) {
        node=wr_clterm_hashlist_next(g,hashvec,node);
        continue;
      }
      if (upon && wr_up_unusable(g,ycl)) {
        // made unusable by unit propagation
        node=wr_clterm_hashlist_next(g,hashvec,node);
        continue;
      }
      if ((g->back_subsume) && wr_get_cl_backsubsumed(g,ycl)) {
        node=wr_clterm_hashlist_next(g,hashvec,node);
        continue;
      }
      if (g->print_active_cl) {
        wr_printf("\n* active i: ");
        wr_print_clause(g,ycl); 
        //printf("\n");
      }  
      if ((g->res_arglen_limit) && (wg_rec_is_rule_clause(db,ycl))) {
        if (wr_count_cl_nonans_atoms(g,ycl) > (g->res_arglen_limit)) {
          // cannot use: continue loop
          node=wr_clterm_hashlist_next(g,hashvec,node);
          continue;
        }
      }
      if ((g->res_shortarglen_limit) && (nonanslen>(g->res_shortarglen_limit))) {
        // must check that ycl is unit
        if (wr_count_cl_nonans_atoms(g,ycl) > (g->res_shortarglen_limit)) {
          // cannot use: continue loop
          node=wr_clterm_hashlist_next(g,hashvec,node);
          continue;     
        }  
      } 
      // NEW ADDITION in may
      if ((g->posunitpara_strat) && (wg_rec_is_rule_clause(db,ycl))) {
        if (wr_count_cl_nonans_atoms(g,ycl)>1) {
          // cannot use: continue loop
          node=wr_clterm_hashlist_next(g,hashvec,node);
          continue;
        }  
      }
#ifdef EQ_PREFERENCE
      if (wr_count_cl_nonans_atoms(g,ycl)>1 && wg_rec_is_rule_clause(db,ycl)) {
        int itmp;
        gint metatmp;
        int tmplen = wg_count_clause_atoms(db, ycl);
        for(itmp=0; itmp<tmplen; itmp++) {
          yatom=wg_get_rule_clause_atom(db,ycl,itmp);
          if (wg_get_encoded_type(db,yatom)!=WG_RECORDTYPE ||
              !wr_equality_atom(g,yatom)) {                  
            continue;  // do not paramodulate from clauses containing some non-equality literals
          }  
          metatmp=wg_get_rule_clause_atom_meta(db,ycl,itmp);
          if (wg_atom_meta_is_neg(db,metatmp)) continue;     
        }  
      }
#endif 

      if (g->print_litterm_selection) {                
          wr_printf("\nactive para-into eq term:");
          wr_print_term(g,yterm); 
          wr_printf(" in cl ");
          wr_print_clause(g,ycl);
      }
      ures=wr_unify_term(g,xterm,yterm,1); // uniquestrflag=1
  #ifdef DEBUG        
              wr_printf("unification check res: %d\n",ures);
  #endif 
      if (ures) { //  ideally should know if was orderered before a la if ... eqtermorder==3
        /* TODO
        // ideally should check if order ok
        eqtermorder_after=wr_order_eqterms(g,a,b,(g->varbanks));      
        // eqtermorder values:
        // 0: none bigger, neither ok for para
        // 1: a bigger than b (prohibits b)
        // 2: b bigger than a (prohibits a)
        // 3: none bigger, both ok for para
        if (eqtermorder_after==3 || eqtermorder_after==1) {
          // build and process the new clause
          //...
        }
        */
        // build and process the new clause
  #ifdef DEBUG         
        wr_printf("\nin wr_paramodulate_into_subterms_all_active to call wr_process_paramodulate_result\n");
  #endif                                       
        //path=wg_decode_int(db,nodeptr[CLTERM_HASHNODE_PATH_POS]);

        replpath=wr_encode_para_termpath(g,litnr,origtermpath);
        eqpath=wg_decode_int(db,otp(db,node)[CLTERM_HASHNODE_PATH_POS]);      
        eqlitnr=wr_decode_para_termpath_litnr(g,eqpath);
        eqleftflag=wr_decode_para_termpath_pos(g,eqpath);
        if (wg_rec_is_rule_clause(db,ycl)) {
          yatom=wg_get_rule_clause_atom(db,ycl,eqlitnr); // eq literal
        } else {
          yatom=encode_record(db,ycl);
        }  
        if (eqleftflag) {
          if (g->print_active_cl) {
            wr_printf(" left ");
          }  
          b=(gint)(((gint *)(otp(db,yatom)))[RECORD_HEADER_GINTS+(g->unify_funarg2pos)]);
        } else {
          if (g->print_active_cl) {
            wr_printf(" right ");
          }  
          b=(gint)(((gint *)(otp(db,yatom)))[RECORD_HEADER_GINTS+(g->unify_funarg1pos)]);
        }  
        //a=xterm; // eq first arg (unified upon) ????
        //b=yterm; // eq second arg, for which term is replaced 

        //--- new part starts ----
        /*
        int eqtermorder_after;
        eqtermorder_after=wr_order_eqterms(g,yterm,b,(g->varbanks));
        if (eqtermorder_after==3 || eqtermorder_after==1) {
           wr_process_paramodulate_result(g,yatom,ycl,xatom,xcl,cl_as_active,yterm,b,replpath,eqleftflag,0);
        }   
        */
        // --- new part ends ---        
        
        //wr_process_paramodulate_result(g,yatom,ycl,xatom,xcl,cl_as_active,yterm,b,eqpath,eqleftflag); 
    
        // with the key para_into_ordered, the test after the substitution: no
        // step when the replaced side is the smaller one (not with query_focus,
        // where the ordering is off)
        // the side of an oriented equation is left out of the test: it always passes.
        // With para_into_ordered 2 also the test of the "into" literal.
        { int ok=1;
          if ((g->para_into_ordered) && !WR_QF_FORCE3(g)) {
            if (!(eqpath & PARA_TERMPATH_ORIENTEDMASK) && wr_order_eqterms(g,yterm,b,(g->varbanks))==2) {
              ok=0; (g->stat_para_from_order_skip)++;
            }
            if (ok && (g->para_into_ordered)==2) ok=wr_para_into_lit_ok(g,xatom,origtermpath);
          }
          if (ok) wr_process_paramodulate_result(g,yatom,ycl,xatom,xcl,cl_as_active,yterm,b,replpath,eqleftflag,0); // new part: commented out
        }

        /*
        wr_process_paramodulate_result(g,xatom,xcl,0,ycl,cl_as_active,a,b,path,leftflag); 
  #ifdef DEBUG           
        wr_printf("\nin wr_paramodulate_into_subterms_all_active after wr_process_paramodulate_result\n");
        wr_printf("\nyterm\n");
        wr_print_term(g,yterm);
        //printf("\nyatom\n");
        //wr_print_term(g,yatom);
        wr_printf("!!!!! wr_paramodulate_into_subterms_all_active after  wr_process_paramodulate_result queue is\n");
        wr_show_clqueue(g);
        wr_printf("\nqueue ended\n");
  #endif        
        */     
        if (g->proof_found || g->alloc_err) {
          //printf("\nwr_paramodulate_into_subterms_all_active found proof\n");
          wr_clear_varstack(g,g->varstack);
          if (wr_enough_answers(g) || g->alloc_err) return 1;   // return 1 ????
        }        
      }

      wr_clear_varstack(g,g->varstack);              
      //wr_print_vardata(g);     
      if ((g->stat_derived_cl%256)==0 || (g->derived_overflow_timeout)) {
        if (g->derived_overflow_timeout) return 1; // time limit passed at a drop
        curclock=clock();
        run_seconds = (float)(curclock - (g->run_start_clock)) / CLOCKS_PER_SEC;            
        if ((g->max_run_seconds) && (run_seconds>(g->max_run_seconds))) return 1;
        run_dseconds = (float)(curclock - (g->run_start_clock)) / ((float)CLOCKS_PER_SEC / 10);            
        if ((g->max_run_dseconds) && (run_dseconds>(g->max_run_dseconds))) return 1;
      }
      // get next node;
      node=wr_clterm_hashlist_next(g,hashvec,node);       
    }
  } // end loop over local and global termhash  

#ifdef XDEBUG 
  //printf("\nadding to hash ended with %d\n",tmp);
#endif       
  return 1;     
}

/*
   performs eq reflexive factor-like derivations nonrecursively:
   reflex upon all literal pairs regardless of ordering
   
   EXCEPT for query strat, where only the first lit
   is reflexed upon (NB! this should be improved)

   it but does not reflex the results further
*/


void wr_resolve_equality_reflexive(glb* g, gptr cl, gptr cl_as_active) { 
  int i;
  int len;      
  gint xmeta; 
  gint xatom;
  gptr xcl;
  int ures;  
  gint a,b;
  gptr tptr;
  int atomlen;

#ifdef DEBUG
  wr_printf("\n!!! wr_resolve_equality_reflexive called for clause ");
  wr_print_clause(g,cl);
  wr_printf("\n");   
  wr_print_vardata(g);
#endif  
  // cannot equality_reflexive fully positive clauses
  if (! wg_rec_is_rule_clause(db,cl)) return;
  len = wg_count_clause_atoms(db,cl);  
  xcl=cl;  
  // loop over literals
  for(i=0; i<len; i++) {
    if ((g->queryfocus_strat) && i>0) break; // only the first lit of given allowed for query strat      
    // can only resolve on negative equalities 
    xmeta=wg_get_rule_clause_atom_meta(db,xcl,i);
    if (! wg_atom_meta_is_neg(db,xmeta)) continue;
    xatom=wg_get_rule_clause_atom(db,xcl,i);        
    if (! wr_equality_atom(g,xatom)) continue;

    tptr=rotp(g,xatom);
    atomlen=get_record_len(tptr);
    if (atomlen>=(g->unify_firstuseterm)+3) {
      // take equality args (two):
      a=tptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
      b=tptr[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];
      //atype=wg_get_encoded_type(db,a);
      //btype=wg_get_encoded_type(db,b);
      ures=wr_unify_term(g,a,b,1); // uniquestrflag=1

#ifdef DEBUG        
      wr_printf("unification check res: %d\n",ures);
#endif        
      if (ures) {
        // build and process the new clause
#ifdef DEBUG         
        wr_printf("\nin wr_factor to call wr_process_factor_result\n");
#endif           
        wr_process_factor_result(g,xcl,len,i,xatom,-1,0,cl_as_active);                      
        if (g->proof_found || g->alloc_err) {
          wr_clear_varstack(g,g->varstack);  
          if (wr_enough_answers(g) || g->alloc_err) return;          
        }  
      }      
      wr_clear_varstack(g,g->varstack);             
    }      
  }      
#ifdef DEBUG
  wr_printf("!!! wr_resolve_equality_reflexive finished \n");
#endif    
  return;
}

// values WR_HISTORY_TAG_FACTORIAL, WR_HISTORY_TAG_PARA etc

int wr_get_cl_history_tag(glb* g, gptr cl) {
  void *db=g->db;
  gint history,head,htype,dechead;
  int tag;
  
  history=wr_get_history(g,cl);
  if (!history) return 0;
  /* an input history has HISTORY_PREFIX_LEN fields and no derivation tag */
  if (wg_get_record_len(db,otp(db,history))<=HISTORY_DERIVATION_TAG_POS) return 0;
  head=wr_get_history_record_field(db,otp(db,history),HISTORY_DERIVATION_TAG_POS);
  htype=wg_get_encoded_type(db,head);
  if (htype!=WG_INTTYPE) return 0;
  dechead=wg_decode_int(db,head);
  tag=wr_get_history_tag(g,dechead);  
  return tag;
}  
  


#ifdef __cplusplus
}
#endif
