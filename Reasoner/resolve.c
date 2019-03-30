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


/* ======= Private protos ================ */



/* ====== Functions ============== */



void wr_resolve_binary_all_active(glb* g, gptr cl, gptr cl_as_active) { 
  void* db=g->db;
  int i;
  int len;      
  int ruleflag; // 0 if not rule
  int posok=1;  // default allow
  int negok=1;  // default allow
  gint meta;
  int negflag; // 1 if negative
  int nonanslen; // length not counting ans literals
  gint hash;
  int addflag=0; // ok to resolve literal or not
  vec hashvec;
  int hlen;
  gint node;
  gint xatom=0;
  gint yatom=0;
  gptr xcl;
  gptr ycl;
  int ures;
  int dbused;
  int negadded=0;
  int posadded=0;
  
 
#ifdef DEBUG
  printf("wr_resolve_binary_all_active called for clause ");
  wr_print_clause(g,cl);
  printf("\n");     
  wr_print_vardata(g);
#endif  
  // get clause data for input clause
       
  ruleflag=wg_rec_is_rule_clause(db,cl);
  if (ruleflag) len = wg_count_clause_atoms(db, cl);
  else len=1;  
  // for negpref check out if negative literals present
  wr_set_stratlimits_cl(g,cl,ruleflag,len,&posok,&negok,&nonanslen);

  xcl=cl; 
#ifdef DEBUG
  printf("ruleflag %d len %d  posok %d negok %d\n",
          ruleflag,len,posok,negok);
#endif  
  // loop over literals
  for(i=0; i<len; i++) {  
    if ((g->queryfocus_strat) && i>0) break; // only the first lit of given allowed for query strat
    negflag=0;
    addflag=0;
    if (!ruleflag) {
      xatom=encode_record(db,xcl);
      //printf("!ruleflag atom with i %d: \n",i);
#ifdef DEBUG      
      wr_print_record(g,wg_decode_record(db,xatom));
#endif      
      hash=wr_atom_funhash(g,xatom);
      //printf("hash %d: \n",hash);
      addflag=1;
    } else {       
      meta=wg_get_rule_clause_atom_meta(db,xcl,i);
      if (wg_atom_meta_is_neg(db,meta)) negflag=1;
      // if ok to resolve upon, set addflag=1
      xatom=wg_get_rule_clause_atom(db,xcl,i);               
      if (!wr_answer_lit(g,xatom) && 
           wr_order_resolvable_atom(g,negflag,negok,posok,negadded,posadded)) {
        if (negflag) negadded++; 
        else posadded++;                     
#ifdef DEBUG            
        printf("atom nr %d from record \n",i);
        wr_print_record(g,xcl);
        printf("\natom\n");              
        wr_print_record(g,wg_decode_record(db,xatom));
        printf("negflag %d\n",negflag);             
#endif                 
        hash=wr_atom_funhash(g,xatom);
        //printf("hash %d\n",hash);
        addflag=1;
      }     
    }
    // xcl: active clause
    // xatom: active atom
    if (!xatom) continue;
    if (addflag) {      
      // now loop over hash vectors for all active unification candidates
      // ycl: cand clause
      // yatom: cand atom

      // loop over local atom hash (dbused==0) and external atom hash (dbused==1)

      for(dbused=0; dbused<2; dbused++) {
        if (dbused==0) {
          if (negflag) hashvec=rotp(g,g->hash_pos_atoms);
          else hashvec=rotp(g,g->hash_neg_atoms);   
#ifdef DEBUG
          printf("\n**** in wr_resolve_binary_all_active dbused %d hashvec is *****\n",dbused);
          wr_clterm_hashlist_print(g,hashvec);
#endif
        } else {
#ifdef DEBUG
          printf("\n**** in wr_resolve_binary_all_active dbused %d hashvec is *****\n",dbused);
          //wr_clterm_hashlist_print(g,hashvec);
#endif
          if (r_kb_g(g)) {
            //printf("\nr_kb_g(g) used\n");
            if (negflag) hashvec=rotp(g,r_kb_g(g)->hash_pos_atoms);
            else hashvec=rotp(g,r_kb_g(g)->hash_neg_atoms); 
#ifdef DEBUG            
            wr_clterm_hashlist_print(g,hashvec);            
#endif            
          }        
        }

  #ifndef QUIET      
        printf("\n----- inner wr_genloop cycle (active hash list) starts ----------\n"); 

        printf("\nhash %ld\n",hash);
  #endif             
        
        //wr_clterm_hashlist_print(g,hashvec);       
        hlen=wr_clterm_hashlist_len(g,hashvec,hash);
        if (hlen==0) {
  #ifdef DEBUG        
          printf("no matching atoms in hash\n");
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
          printf("after while(node!=0): \n");
          printf("ycl: \n");
          wr_print_clause(g,ycl);                   
          printf("xcl: \n");
          wr_print_clause(g,xcl);
          printf("xatom: \n");
          wr_print_clause(g,xatom);
          */
          if ((g->back_subsume) && wr_blocked_clause(g,ycl)) {
            /*
            printf("\nCP1 blocked clause: \n");          
            wr_print_clause(g,ycl);
            printf("\n");
            */
            node=wr_clterm_hashlist_next(g,hashvec,node);
            continue;
          }
          if (g->print_active_cl) {
            printf("\n* active: ");
            wr_print_clause(g,ycl); 
            //printf("\n");
          }  
          /*
          printf("\n* active: ");
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
          printf("\nxatom ");
          wr_print_term(g,xatom);
          printf(" in xcl ");
          wr_print_clause(g,xcl);
          printf("yatom ");
          wr_print_term(g,yatom);
          printf(" in ycl ");
          wr_print_clause(g,ycl);
          //wg_print_record(db,ycl);
          //printf("calling equality check\n");
          wr_print_vardata(g);
  #endif          
          //printf("!!!!!!!!!!!!!!!!!!!!! before unification\n");
          //wr_print_vardata(g); 
          //printf("CLEAR\n");
          //wr_clear_varstack(g,g->varstack);           
          //wr_print_vardata(g); 
          //printf("START UNIFICATION\n");
          
          ures=wr_unify_term(g,xatom,yatom,1); // uniquestrflag=1
  #ifdef DEBUG        
          printf("unification check res: %d\n",ures);
  #endif        
          //wr_print_vardata(g);
          //wr_print_vardata(g);
          //wr_clear_varstack(g,g->varstack);
          //wr_print_vardata(g);
          if (ures) {
            // build and process the new clause
  #ifdef DEBUG         
            printf("\nin wr_resolve_binary_all_active to call wr_process_resolve_result\n");
  #endif           
            wr_process_resolve_result(g,xatom,xcl,yatom,ycl,cl_as_active); 
  #ifdef DEBUG           
            printf("\nin wr_resolve_binary_all_active after wr_process_resolve_result\n");
            printf("\nxatom\n");
            wr_print_term(g,xatom);
            printf("\nyatom\n");
            wr_print_term(g,yatom);
            printf("!!!!! wr_resolve_binary_all_active after  wr_process_resolve_result queue is\n");
            wr_show_clqueue(g);
            printf("\nqueue ended\n");
  #endif             
            if (g->proof_found || g->alloc_err) {
              wr_clear_varstack(g,g->varstack);          
              return;          
            }  
          }
  #ifdef DEBUG
          printf("wr_resolve_binary_all_active before wr_clear_varstack queue is\n");
          wr_show_clqueue(g);
          printf("\nqueue ended\n");
  #endif         
          wr_clear_varstack(g,g->varstack);              
          //wr_print_vardata(g);
          // get next node;
          node=wr_clterm_hashlist_next(g,hashvec,node);       
        } /* over one single hash vector */
  #ifdef DEBUG      
        printf("\nexiting node loop\n");      
  #endif      
      } /* over local and external db hash vectors */
    }  
  }         
  return;
}




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
  printf("\n!!! wr_factor called for clause ");
  wr_print_clause(g,cl);
  printf("\n");   
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
      printf("--- xatom i %d \n",i);        
      wr_print_record(g,wg_decode_record(db,xatom));
      printf("xnegflag %d\n",xnegflag);
      printf("yatom nr %d  \n",j);        
      wr_print_record(g,wg_decode_record(db,yatom));
      printf("ynegflag %d\n",ynegflag);
#endif            
      if (wg_get_encoded_type(db,xatom)==WG_RECORDTYPE &&
          wg_get_encoded_type(db,yatom)==WG_RECORDTYPE) {
        //termflag=1;
        ures=wr_unify_term(g,xatom,yatom,1); // uniquestrflag=1
#ifdef DEBUG        
        printf("unification check res: %d\n",ures);
#endif        
        //wr_clear_varstack(g,g->varstack);
        //wr_print_vardata(g);
        if (ures) {
          // build and process the new clause
#ifdef DEBUG         
          printf("\nin wr_factor to call wr_process_factor_result\n");
#endif           
          wr_process_factor_result(g,xcl,len,i,xatom,j,yatom,cl_as_active);                      
          if (g->proof_found || g->alloc_err) {
            wr_clear_varstack(g,g->varstack);          
            return;          
          }  
        }      
        wr_clear_varstack(g,g->varstack);             
      }     
    }  
  }      
#ifdef DEBUG
  printf("!!! wr_factor finished \n");
#endif    
  return;
}


/*

  wr_paramodulate_from_all_active takes a given cl
  and tries to paramodulate from positive equalities in this given cl
  into terms of the clauses stored earlier as active,
  i.e. earlier clauses do not need to contain equalities.

*/


void wr_paramodulate_from_all_active(glb* g, gptr cl, gptr cl_as_active) { 
  void* db=g->db;
  int i;
  int len;      
  int ruleflag; // 0 if not rule
  int posok=1;  // default allow
  int negok=1;  // default allow
  //gint parent;
  gint meta;
  int negflag; // 1 if negative
  int nonanslen; // length without ans preds
  //int termflag; // 1 if complex atom  
  gint hash;
  int dbused;
  int useflag=0;  
  vec hashvec;
  int hlen;
  gint node;
  gint xatom=0;
  //gint yatom=0;
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
   
#ifdef DEBUG
  printf("\n!!! wr_paramodulate_from_all_active called for clause ");
  wr_print_clause(g,cl);
  printf("\n");     
  wr_print_vardata(g);
#endif  
  // get clause data for input clause
       
  ruleflag=wg_rec_is_rule_clause(db,cl);
  if (ruleflag) len = wg_count_clause_atoms(db, cl);
  else len=1;  
  // for negpref check out if negative literals present
  wr_set_stratlimits_cl(g,cl,ruleflag,len,&posok,&negok,&nonanslen);

  xcl=cl; 
#ifdef DEBUG
  printf("ruleflag %d len %d posok %d negok %d\n",
          ruleflag,len,posok,negok);
#endif  
  // loop over literals
  for(i=0; i<len; i++) {  
    if ((g->queryfocus_strat) && i>0) break; // only the first lit of given allowed for query strat
    negflag=0;
    useflag=0;
    if (!ruleflag) {
      xatom=encode_record(db,xcl);
      //printf("!ruleflag atom with i %d: \n",i);
#ifdef DEBUG 
      printf("\n xatom looked at:\n");       
      wr_print_record(g,wg_decode_record(db,xatom));
#endif
      if (wr_equality_atom(g,xatom)) {       
        useflag=1;
      }
    } else {       
      meta=wg_get_rule_clause_atom_meta(db,xcl,i);
      if (wg_atom_meta_is_neg(db,meta)) negflag=1;
      if ((!negflag && posok) || (negflag && negok)) {    
        xatom=wg_get_rule_clause_atom(db,xcl,i);             
#ifdef DEBUG            
        printf("\natom nr %d from record \n",i);
        wr_print_record(g,xcl);
        printf("\natom\n");              
        wr_print_record(g,wg_decode_record(db,xatom));
        printf("\nnegflag %d\n",negflag);             
#endif            
        if (wg_get_encoded_type(db,xatom)==WG_RECORDTYPE &&
            !negflag && 
            wr_equality_atom(g,xatom)) {                  
          useflag=1;
        }      
      }      
    }
    if (!xatom) continue;
#ifdef DEBUG            
    printf("\nuseflag %d negflag %d \n",useflag,negflag);   
    printf("\nxatom\n"); 
    if (xatom) wr_print_record(g,wg_decode_record(db,xatom));        
#endif 
    tptr=rotp(g,xatom);
    atomlen=get_record_len(tptr);
    if (atomlen<(g->unify_firstuseterm)+3) useflag=0; 
    // xcl: active clause
    // xatom: active atom    
    if (useflag) {            
      // check ordering: which terms are ok for para  

      a=tptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
      b=tptr[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];
      eqtermorder=wr_order_eqterms(g,a,b,NULL);

      atype=wg_get_encoded_type(db,a);
      btype=wg_get_encoded_type(db,b);
      /*
      printf("\n a and b terms: ");               
      wr_print_term(g,a);        
      printf(" and ");
      wr_print_term(g,b);  
      printf(" : ");      
      printf(" eqtermorder: %d\n",eqtermorder);
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
#ifdef DEBUG 
        printf("\n !!!! termpos %d, a and b terms: \n",termpos);               
        wr_print_term(g,a);        
        printf("\n");
        wr_print_term(g,b);  
        printf("\n");
        eqtermorder=wr_order_eqterms(g,a,b,NULL);
        printf("\n eqtermorder: %d\n",eqtermorder);
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
          printf("\n----- inner wr_genloop cycle (active hash list) starts ----------\n"); 
    #endif          

          // loop over local termhash (dbused==0) and external termhash (dbused==1)
          for(dbused=0; dbused<2; dbused++) {            
            if (dbused==0) {
              // local termhash
        #ifdef DEBUG
              printf("\n**** in wr_paramodulate_from_all_active dbused %d hashvec is *****\n",dbused);
              //wr_clterm_hashlist_print(g,hashvec);
        #endif              
              hashvec=rotp(g,g->hash_para_terms);       
              //wr_clterm_hashlist_print(g,hashvec);       
              hlen=wr_clterm_hashlist_len(g,hashvec,hash);
              if (hlen==0) {
        #ifdef DEBUG        
                printf("no matching atoms in hash\n");
        #endif        
                continue;
              }   
            } else {   
              // external kb termhash           
    #ifdef DEBUG
              printf("\n**** in wr_paramodulate_from_all_active dbused %d hashvec is *****\n",dbused);
              //wr_clterm_hashlist_print(g,hashvec);
    #endif
              if (!(r_kb_g(g))) continue;             
              //printf("\nr_kb_g(g) used\n");
              hashvec=rotp(g,r_kb_g(g)->hash_para_terms);       
              //wr_clterm_hashlist_print(g,hashvec);       
              hlen=wr_clterm_hashlist_len(g,hashvec,hash);
              if (hlen==0) {
        #ifdef DEBUG        
                printf("no matching atoms in hash\n");
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
              printf("after while(node!=0): \n");
              printf("ycl: \n");
              wr_print_clause(g,ycl);    
              printf("yterm: \n");
              wr_print_term(g,yterm);      
              printf("nodeptr[CLTERM_HASHNODE_PATH_POS]: %d\n",nodeptr[CLTERM_HASHNODE_PATH_POS]);
              */
              //printf("xcl: \n");
              //wr_print_clause(g,xcl);
              //printf("xatom: \n");
              //wr_print_term(g,xatom);
              if ((g->back_subsume) && wr_blocked_clause(g,ycl)) {
                /*
                printf("\nCP2 blocked clause: \n");          
                wr_print_clause(g,ycl);
                printf("\n");
                */
                node=wr_clterm_hashlist_next(g,hashvec,node);
                continue;
              }
              if (g->print_active_cl) {
                printf("\n* active: ");
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
              printf("\neq term a ");
              wr_print_term(g,a);
              printf(" in xcl ");
              wr_print_clause(g,xcl);
              printf("\nyterm ");
              wr_print_term(g,yterm);
              printf(" in ycl ");
              wr_print_clause(g,ycl);
              //wg_print_record(db,ycl);
              //printf("calling equality check\n");
              wr_print_vardata(g);
      #endif          
              //printf("!!!!!!!!!!!!!!!!!!!!! before unification\n");
              //wr_print_vardata(g); 
              //printf("CLEAR\n");
              //wr_clear_varstack(g,g->varstack);           
              //wr_print_vardata(g); 
              //printf("START UNIFICATION\n");
              ures=wr_unify_term(g,a,yterm,1); // uniquestrflag=1
      #ifdef DEBUG        
              printf("unification check res: %d\n",ures);
      #endif        
              //wr_print_vardata(g);
              //wr_print_vardata(g);
              //wr_clear_varstack(g,g->varstack);
              //wr_print_vardata(g);
              
              //eqtermorder_after=0; // =eqtermorder; // !!! was not initialized
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
                printf("\nin wr_paramodulate_from_all_active to call wr_process_paramodulate_result\n");
      #endif                                       
                path=wg_decode_int(db,nodeptr[CLTERM_HASHNODE_PATH_POS]);              
                //printf("\n path before call %d\n",(int)path);
                wr_process_paramodulate_result(g,xatom,xcl,0,ycl,cl_as_active,a,b,path,!termpos,1);                 
      #ifdef DEBUG                        
                printf("\nin wr_paramodulate_from_all_active after wr_process_paramodulate_result\n");
                printf("\na\n");
                wr_print_term(g,a);
                //printf("\nyatom\n");
                //wr_print_term(g,yatom);
                printf("!!!!! wr_paramodulate_from_all_active after  wr_process_paramodulate_result queue is\n");
                wr_show_clqueue(g);
                printf("\nqueue ended\n");
      #endif             
                if (g->proof_found || g->alloc_err) {
                  wr_clear_varstack(g,g->varstack);          
                  return;          
                }  
              }
      #ifdef DEBUG
              printf("wr_paramodulate_from_all_active before wr_clear_varstack queue is\n");
              wr_show_clqueue(g);
              printf("\nqueue ended\n");
      #endif         
              wr_clear_varstack(g,g->varstack);              
              //wr_print_vardata(g);
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


void wr_paramodulate_into_all_active(glb* g, gptr cl, gptr cl_as_active) { 
  void* db=g->db;
  int i;
  int len;      
  int ruleflag; // 0 if not rule
  int posok=1;  // default allow
  int negok=1;  // default allow
  gint meta;
  int negflag; // 1 if negative
  int nonanslen; // length not counting ans literals
  int addflag=0;
  gint xatom=0;
  gptr xcl;
  int termpath;
  gint subterm;
  int tmp;
  
  int negadded=0;
  int posadded=0;

#ifdef DEBUG
  printf("\n!!! wr_paramodulate_into_all_active called for clause ");
  wr_print_clause(g,cl);
  printf("\n");
#endif  
  // get clause data for input clause
       
  ruleflag=wg_rec_is_rule_clause(db,cl);
  if (ruleflag) len = wg_count_clause_atoms(db, cl);
  else len=1;  
  // for negpref check out if negative literals present
  wr_set_stratlimits_cl(g,cl,ruleflag,len,&posok,&negok,&nonanslen);  

  xcl=cl; 
#ifdef DEBUG
  printf("ruleflag %d len %d posok %d negok %d\n",
          ruleflag,len,posok,negok);
#endif  
  // loop over literals
  for(i=0; i<len; i++) {  
    if ((g->queryfocus_strat) && i>0) break; // only the first lit of given allowed for query strat
    negflag=0;
    //termflag=0;
    addflag=0;
    //printf("\nruleflag %d, addflag %d, len %d, i: %d\n",ruleflag,addflag,len,i);
    if (!ruleflag) {
      xatom=encode_record(db,xcl);
      //printf("!ruleflag atom with i %d: \n",i);
#ifdef DEBUG      
      wr_print_record(g,wg_decode_record(db,xatom));
#endif      
      //hash=wr_atom_funhash(g,xatom);
      //printf("hash %d: \n",hash);
      addflag=1;
    } else {       
      meta=wg_get_rule_clause_atom_meta(db,xcl,i);
      if (wg_atom_meta_is_neg(db,meta)) negflag=1;
      // if ok to resolve upon, set addflag=1
      xatom=wg_get_rule_clause_atom(db,xcl,i); 
      if (!wr_answer_lit(g,xatom) && 
           wr_order_resolvable_atom(g,negflag,negok,posok,negadded,posadded)) {
        if (negflag) negadded++; 
        else posadded++;           
#ifdef DEBUG            
        printf("atom nr %d from record \n",i);
        wr_print_record(g,xcl);
        printf("\natom\n");              
        wr_print_record(g,wg_decode_record(db,xatom));
        printf("negflag %d\n",negflag);             
#endif                             
        //hash=wr_atom_funhash(g,xatom);
        //printf("hash %d\n",hash);
        addflag=1;
      }      
    }
    // xcl: active clause
    // xatom: active atom
    if (!xatom) continue;
    if (addflag) {          
      termpath=0;
      subterm=xatom;              
      tmp=wr_paramodulate_into_subterms_all_active(g,cl,cl_as_active,xatom,subterm,0,i,&termpath,nonanslen);
      if (tmp<0) {
        // hard error
        //wr_sys_exiterr(g,"apparently wrong hash given to wr_clterm_hashlist_start");
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


  int replpath;

#ifdef DEBUG
  printf("\n !+!+!+! wr_paramodulate_into_subterms_all_active called at depth %d and litnr %d on term and cl: ",depth,litnr); 
  wr_print_term(g,term);
  printf("\n");
  wr_print_clause(g,cl);
  printf("\n");
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
      printf("\ndepth %d i %d unifiable subterm \n",depth,i);
      wr_print_term(g,yi);  
      printf("\n");
      //wr_cl_store_para_subterms(g,cl,yi,depth+1,litnr,termpath);
#else
      // do not recurse into atomic funsymb
      if (!(i==istart && !isdatarec(yi))) {
        (*termpath)++;
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
  printf("\nin wr_paramodulate_into_subterms_all_active fun hash is: %d litnr % origtermpath %d for fun and term:\n",
    (int)hash,litnr,origtermpath);
  wr_print_term(g,fun);
  printf("\n");  
  wr_print_term(g,term);
  printf("\n");
#endif      


  // loop over local termhash (dbused==0) and external termhash (dbused==1)
  for(dbused=0; dbused<2; dbused++) {            
    if (dbused==0) {
      // local termhash
#ifdef DEBUG
      printf("\n**** in wr_paramodulate_into_subterms_all_active dbused %d hashvec is *****\n",dbused);
      //wr_clterm_hashlist_print(g,hashvec);
#endif              
      hashvec=rotp(g,g->hash_eq_terms);       
      //wr_clterm_hashlist_print(g,hashvec);       
      hlen=wr_clterm_hashlist_len(g,hashvec,hash);
      if (hlen==0) {
#ifdef DEBUG        
        printf("no matching atoms in hash\n");
#endif        
        continue;
      }   
    } else {   
      // external kb termhash           
#ifdef DEBUG
      printf("\n**** in wr_paramodulate_into_subterms_all_active dbused %d hashvec is *****\n",dbused);
      //wr_clterm_hashlist_print(g,hashvec);
#endif
      if (!(r_kb_g(g))) return 1;             
      //printf("\nr_kb_g(g) used\n");
      hashvec=rotp(g,r_kb_g(g)->hash_eq_terms);       
      //wr_clterm_hashlist_print(g,hashvec);       
      hlen=wr_clterm_hashlist_len(g,hashvec,hash);
      if (hlen==0) {
#ifdef DEBUG        
        printf("no matching atoms in hash\n");
#endif        
        return 1;
      }               
    }


    /*
    hashvec=rotp(g,g->hash_eq_terms);  
    //path=wr_encode_para_termpath(g,litnr,origtermpath);
    hlen=wr_clterm_hashlist_len(g,hashvec,hash);
    if (hlen==0) {       
      return 1;
    } 
    */    
    //printf("\nhash table:");      
    //wr_clterm_hashlist_print(g,hashvec);
    /*
      node=wr_clterm_hashlist_start(g,hashvec,hash);
        if (!node)  {
          wr_sys_exiterr(g,"apparently wrong hash given to wr_clterm_hashlist_start");
          return;
        }      
        while(node!=0) {    
          yatom=(otp(db,node))[CLTERM_HASHNODE_TERM_POS];
          ycl=otp(db,(otp(db,node))[CLTERM_HASHNODE_CL_POS]);

    */
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
        /*
        printf("\nCP3 blocked clause: \n");          
        wr_print_clause(g,ycl);
        printf("\n");
        */
        node=wr_clterm_hashlist_next(g,hashvec,node);
        continue;
      }
      if (g->print_active_cl) {
        printf("\n* active1: ");
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
      ures=wr_unify_term(g,xterm,yterm,1); // uniquestrflag=1
  #ifdef DEBUG        
              printf("unification check res: %d\n",ures);
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
        printf("\nin wr_paramodulate_into_subterms_all_active to call wr_process_paramodulate_result\n");
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
            printf(" left ");
          }  
          b=(gint)(((gint *)(otp(db,yatom)))[RECORD_HEADER_GINTS+(g->unify_funarg2pos)]);
        } else {
          if (g->print_active_cl) {
            printf(" right ");
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
        printf("\nin wr_paramodulate_into_subterms_all_active after wr_process_paramodulate_result\n");
        printf("\nyterm\n");
        wr_print_term(g,yterm);
        //printf("\nyatom\n");
        //wr_print_term(g,yatom);
        printf("!!!!! wr_paramodulate_into_subterms_all_active after  wr_process_paramodulate_result queue is\n");
        wr_show_clqueue(g);
        printf("\nqueue ended\n");
  #endif        
        */     
        if (g->proof_found || g->alloc_err) {
          wr_clear_varstack(g,g->varstack);          
          return 1; // return 1 ????
        }        
      }

      wr_clear_varstack(g,g->varstack);              
      //wr_print_vardata(g);
      // get next node;
      node=wr_clterm_hashlist_next(g,hashvec,node);       
    }
  } // end loop over local and global termhash  

  //tmp=wr_clterm_add_hashlist_withpath(g,hashvec,hash,term,cl,path);
#ifdef XDEBUG 
  //printf("\nadding to hash ended with %d\n",tmp);
#endif       
  /*
  if (tmp) {
    wr_sys_exiterr2int(g,"adding term to hashlist in cl_store_para_terms, code ",tmp);
    return 1;        
  } 
  */ 
#ifdef DEBUGHASH      
  /*
  printf("\nhash table after adding:");      
  wr_clterm_hashlist_print(g,hashvec);
  */
#endif 
  return 1;     
}



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
  printf("\n!!! wr_resolve_equality_reflexive called for clause ");
  wr_print_clause(g,cl);
  printf("\n");   
  wr_print_vardata(g);
#endif  
  // cannot equality_reflexive fully positive clauses
  if (! wg_rec_is_rule_clause(db,cl)) return;
  len = wg_count_clause_atoms(db,cl);  
  xcl=cl;  
  // loop over literals
  for(i=0; i<len; i++) {
    if ((g->queryfocus_strat) && i>0) break; // only the first lit of given allowed for query strat      
    //termflag=0;
    //addflag=0;   
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
      printf("unification check res: %d\n",ures);
#endif        
      //wr_clear_varstack(g,g->varstack);
      //wr_print_vardata(g);
      if (ures) {
        // build and process the new clause
#ifdef DEBUG         
        printf("\nin wr_factor to call wr_process_factor_result\n");
#endif           
        wr_process_factor_result(g,xcl,len,i,xatom,-1,0,cl_as_active);                      
        if (g->proof_found || g->alloc_err) {
          wr_clear_varstack(g,g->varstack);          
          return;          
        }  
      }      
      wr_clear_varstack(g,g->varstack);             
    }      
  }      
#ifdef DEBUG
  printf("!!! wr_resolve_equality_reflexive finished \n");
#endif    
  return;
}


#ifdef __cplusplus
}
#endif
