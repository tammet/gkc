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

#ifdef __cplusplus
extern "C" {
#endif

#include "rincludes.h"
 

/* ====== Private headers and defs ======== */

#define DEBUG
//#undef DEBUG


/* ======= Private protos ================ */



/* ====== Functions ============== */

gint wr_rewrite_term(glb* g, gint x) {
  void* db=g->db;
  gint hash,node,res,yterm,path,yatom,b,fun;
  int dbused, ures, hlen;
  gptr hashvec,nodeptr,ycl,tptr;
  int tmp_subst,tmp_rename;
  //gint tmp;

  //UNUSED(tmp);
#ifdef DEBUG
  printf("\nwr_rewrite_term called on ");
  wr_print_term(g,x);
  printf("\n");
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
        printf("no matching atoms in local hash\n");
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
        printf("no matching atoms in external hash\n");
  #endif        
        continue;
      }               
    }   

    node=wr_clterm_hashlist_start(g,hashvec,hash);
    if (!node)  {      
      printf("\nnode was null \n");
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
      printf("\n before matching yterm (left side of rewrite rule):\n");
      wr_print_term(g,yterm);
      printf("\n and input term x:\n");
      wr_print_term(g,x);
      printf("\n and vardata:\n");
      wr_print_vardata(g); ;
#endif      
      ures=wr_match_term(g,yterm,x,1); // uniquestrflag=1
#ifdef DEBUG        
      printf("match check res: %d\n",ures);
#endif        
      if (!ures) {
        // matching failed     
        wr_clear_varstack(g,g->varstack); 
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
        // build the result
        // b is now the raw right side of rewrite rule before rebuilding
#ifdef DEBUG 
        printf("\n right side of rewrite rule before rebuilding ");
        wr_print_term(g,b);
        printf("\n");
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
  printf("\nwr_rewrite_term returns unchanged term ");
  wr_print_term(g,x);
  printf("\n");
#endif    
  return x;
}


gint wr_rewrite_constant(glb* g, gint x) {
  void* db=g->db;
  gint hash,node,yterm,path,yatom,b;
  int dbused,hlen;
  gptr hashvec,nodeptr,ycl;
  
#ifdef DEBUG
  printf("\nwr_rewrite_constant called on ");
  wr_print_term(g,x);
  printf("\n");
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
        printf("no matching atoms in local hash\n");
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
        printf("no matching atoms in external hash\n");
  #endif        
        continue;
      }               
    }   

    node=wr_clterm_hashlist_start(g,hashvec,hash);
    if (!node)  {      
      printf("\nnode was null \n");
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
      printf("\n before matching yterm (left side of rewrite rule):\n");
      wr_print_term(g,yterm);
      printf("\n and input term x:\n");
      wr_print_term(g,x);
      printf("\n and vardata:\n");
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
        printf("\n right side of rewrite rule before rebuilding ");
        wr_print_term(g,b);
        printf("\n");
#endif         
        // rewrite again until rewrite chain terminates
        return wr_rewrite_constant(g,b); 
      } // end of match cases: unsuccesful/successful       
    } // end loop over nodes in the hash bucket
  }  // end loop over local and external termhashes
#ifdef DEBUG
  printf("\nwr_rewrite_constant returns unchanged term ");
  wr_print_term(g,x);
  printf("\n");
#endif    
  return x;
}


#ifdef __cplusplus
}
#endif

