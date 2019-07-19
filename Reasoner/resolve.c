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

//#define DEBUG
#undef DEBUG
#define QUIET
//#undef QUIET

//#define USE_RES_TERMS // loop over active clauses in wr_resolve_binary_all_active


//#define SIMPLE_ACTIVE_SEARCH_HASH

/* ======= Private protos ================ */



/* ====== Functions ============== */



void wr_resolve_binary_all_active(glb* g, gptr cl, gptr cl_as_active, cvec resolvability) { 
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
  float run_seconds;
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
            if ((g->back_subsume) && wr_blocked_clause(g,ycl)) {
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
            if ((g->stat_derived_cl%256)==0) {
              curclock=clock();
              run_seconds = (float)(curclock - (g->run_start_clock)) / CLOCKS_PER_SEC;            
              if ((g->max_run_seconds) && (run_seconds>(g->max_run_seconds))) return;
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


void wr_paramodulate_from_all_active(glb* g, gptr cl, gptr cl_as_active, cvec resolvability) { 
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
  int eqtermorder,eqtermorder_after;
  clock_t curclock;
  float run_seconds;
   
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
  nonanslen=wr_count_cl_nonans_atoms(g,cl);
  // check if strategy allows to para from this clause
  if ((g->posunitpara_strat) && len!=1) return;
  xcl=cl; 
#ifdef DEBUG
  wr_printf("ruleflag %d len %d posok %d negok %d\n",
          ruleflag,len,posok,negok);
#endif  
  // loop over literals
  for(i=0; i<len; i++) {
    if (!(resolvability[i+1])) continue; // limit para to resolvable only (!!???)
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
      if (g->queryfocus_strat) eqtermorder=3;
      else eqtermorder=wr_order_eqterms(g,a,b,NULL);

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
        eqtermorder=wr_order_eqterms(g,a,b,NULL);
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
            while(node!=0) {  
              nodeptr=otp(db,node);  
              yterm=nodeptr[CLTERM_HASHNODE_TERM_POS];
              ycl=otp(db,nodeptr[CLTERM_HASHNODE_CL_POS]);        
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
              if ((g->back_subsume) && wr_blocked_clause(g,ycl)) {
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
                  eqtermorder_after=wr_order_eqterms(g,a,b,(g->varbanks));       
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
              if (ures && ((eqtermorder_after==3) || eqtermorder_after==1)) {  
                // build and process the new clause
      #ifdef DEBUG         
                wr_printf("\nin wr_paramodulate_from_all_active to call wr_process_paramodulate_result\n");
      #endif                                       
                path=wg_decode_int(db,nodeptr[CLTERM_HASHNODE_PATH_POS]);              
                //printf("\n path before call %d\n",(int)path);
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
              if ((g->stat_derived_cl%256)==0) {
                curclock=clock();
                run_seconds = (float)(curclock - (g->run_start_clock)) / CLOCKS_PER_SEC;            
                if ((g->max_run_seconds) && (run_seconds>(g->max_run_seconds))) return;
              }
              // get next node;
              node=wr_clterm_hashlist_next(g,hashvec,node);       
            } // end loop over nodes in the hash bucket
          }  // end loop over local and external termhashes
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
  nonanslen=wr_count_cl_nonans_atoms(g,cl);
  xcl=cl; 
#ifdef DEBUG
  wr_printf("ruleflag %d len %d posok %d negok %d\n",
          ruleflag,len,posok,negok);
#endif  
  // loop over literals
  for(i=0; i<len; i++) {  
     if (!(resolvability[i+1])) continue; // limit para to resolvable only (!!???)   
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
  float run_seconds;


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
      }
#endif
      
    }
  }  
#ifdef PARA_INTO_ATOM  
#else
  // do not put into hash on the atom level  
  if (depth<1) return 1;  
#endif  

  // find matching terms from the hash table
  hash=wr_term_basehash(g,fun);
  xatom=atom; // atom of the subterm in the given cl, for which we search for eq unifier in hash
  xterm=term; // subterm in the given cl, for which we search for eq unifier in hash 
  xcl=cl; // given cl the subterms of which are matched with eq args in active
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
    while(node!=0) {    
      //nodeptr=otp(db,node);
      yterm=(otp(db,node))[CLTERM_HASHNODE_TERM_POS];     //nodeptr[CLTERM_HASHNODE_TERM_POS];
      ycl=otp(db,(otp(db,node))[CLTERM_HASHNODE_CL_POS]); //(otp(db,nodeptr)[CLTERM_HASHNODE_CL_POS]);
      if ((g->back_subsume) && wr_blocked_clause(g,ycl)) {
        node=wr_clterm_hashlist_next(g,hashvec,node);
        continue;
      }
      if (g->print_active_cl) {
        wr_printf("\n* active i: ");
        wr_print_clause(g,ycl); 
        //printf("\n");
      }  
      if ((g->res_shortarglen_limit) && (nonanslen>(g->res_shortarglen_limit))) {
        // must check that ycl is unit
        if (wr_count_cl_nonans_atoms(g,ycl) > (g->res_shortarglen_limit)) {
          // cannot use: continue loop
          node=wr_clterm_hashlist_next(g,hashvec,node);
          continue;     
        }  
      } 
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
        
        
        //wr_process_paramodulate_result(g,yatom,ycl,xatom,xcl,cl_as_active,yterm,b,eqpath,eqleftflag); 
    
        wr_process_paramodulate_result(g,yatom,ycl,xatom,xcl,cl_as_active,yterm,b,replpath,eqleftflag,0);

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
          wr_clear_varstack(g,g->varstack);
          if (wr_enough_answers(g) || g->alloc_err) return 1;   // return 1 ????
        }        
      }

      wr_clear_varstack(g,g->varstack);              
      //wr_print_vardata(g);
      if ((g->stat_derived_cl%256)==0) {
        curclock=clock();
        run_seconds = (float)(curclock - (g->run_start_clock)) / CLOCKS_PER_SEC;       
        if ((g->max_run_seconds) && (run_seconds>(g->max_run_seconds))) return 1;
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


#ifdef __cplusplus
}
#endif
