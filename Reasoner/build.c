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

 /** @file build.h
 *  Term and clause building functions.
 *
 */

/* ====== Includes =============== */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "rincludes.h"  
#include "../Db/dballoc.h"  // for ACONST_TRUE/FALSE

/* ====== Private headers and defs ======== */

#define PRINT_LIMITS
//#define DEBUG
#undef DEBUG

#define USE_TERM_META

/* ======= Private protos ================ */


/* ====== Functions ============== */


/* ** subst and calc term
  
  uses global flags set before:
  g->build_subst    // subst var values into vars
  g->build_calc     // do fun and pred calculations
  g->build_dcopy    // copy nonimmediate data (vs return ptr)
  g->build_buffer;  // build everything into tmp local buffer area (vs main area)
  
*/

/*
gptr wr_rebuild_calc_given_cl(glb* g, gptr xptr) {
  void* db;
  int ruleflag;  
  gptr yptr;    
  gint xlen;
  gint xatomnr; 
  gint xmeta, xatom, yatom;  
  int i;
  gint tmp;
  int ilimit;
  
  //printf("wr_rebuild_calc_given_cl called\n"); 
  db=g->db;
  if (g->build_rename) (g->build_rename_vc)=0;
  ruleflag=wg_rec_is_rule_clause(db,xptr);  
  if (!ruleflag) {
    // in some cases, no change, no copy: normally copy
    //yptr=xptr;     
    //printf("\in wr_rebuild_calc_given_cl !ruleflag\n"); 
    xlen=get_record_len(xptr);
    //printf("\nxlen %d\n",xlen);
    // allocate space
    if ((g->build_buffer)!=NULL) {
      yptr=wr_alloc_from_cvec(g,g->build_buffer,(RECORD_HEADER_GINTS+xlen));       
    } else {
      yptr=wg_create_raw_record(db,xlen);                   
    }        
    if (yptr==NULL) return NULL;

    // copy rec header and clause header
    ilimit=RECORD_HEADER_GINTS+1; // this should change with probabs!!!!
    for(i=0;i<RECORD_HEADER_GINTS+CLAUSE_EXTRAHEADERLEN;i++) {
      //printf("\n i1: %d RECORD_HEADER_GINTS: %d, CLAUSE_EXTRAHEADERLEN: %d xptr[i]: %d \n",i,RECORD_HEADER_GINTS,CLAUSE_EXTRAHEADERLEN,xptr[i]);
      yptr[i]=xptr[i];     
    }  

    //wr_print_varbank(g,g->varbanks);
    // loop over rest of clause elems
    
    for(;i<xlen+RECORD_HEADER_GINTS;i++) {
      tmp=xptr[i];
      //printf("\n i: %d xlen: %d RECORD_HEADER_GINTS: %d\n",i,xlen,RECORD_HEADER_GINTS);
      //printf("\ntmp 1: %d\n",tmp);
      tmp=wr_build_calc_term(g,xptr[i]);    
      //printf("\ntmp 2: %d\n",tmp);
      if (tmp==WG_ILLEGAL) return NULL; // could be memory err
      yptr[i]=tmp; 
    } 
  
  } else {        

    //printf("\in wr_rebuild_calc_given_cl ruleflag\n"); 

    xlen=get_record_len(xptr);
    // allocate space
    if ((g->build_buffer)!=NULL) {
      yptr=wr_alloc_from_cvec(g,g->build_buffer,(RECORD_HEADER_GINTS+xlen));       
    } else {
      yptr=wg_create_raw_record(db,xlen);                   
    }        
    if (yptr==NULL) return NULL;
    // copy rec header and clause header
    ilimit=RECORD_HEADER_GINTS+CLAUSE_EXTRAHEADERLEN;
    for(i=0;i<ilimit;i++) {
      yptr[i]=xptr[i];     
    }  
    //wr_print_varbank(g,g->varbanks);
    // loop over clause elems
    xatomnr=wg_count_clause_atoms(db,xptr);
    for(i=0;i<xatomnr;i++) {
      xmeta=wg_get_rule_clause_atom_meta(db,xptr,i);
      xatom=wg_get_rule_clause_atom(db,xptr,i);
      wr_set_rule_clause_atom_meta(g,yptr,i,xmeta);
      yatom=wr_build_calc_term(g,xatom);
      if (yatom==WG_ILLEGAL) return NULL; // could be memory err
      if (yatom==ACONST_FALSE) return yatom; // for computable atoms
      if (yatom==ACONST_TRUE) return yatom; // for computable atoms, but should not do that here!!!
      wr_set_rule_clause_atom(g,yptr,i,yatom);
    }   
    //wr_print_varbank(g,g->varbanks);
  }
  ++(g->stat_built_cl);
  return yptr;  
} 

*/

/* ** subst and calc term
  
  uses global flags set before:
  g->build_subst    // subst var values into vars
  g->build_calc     // do fun and pred calculations
  g->build_dcopy    // copy nonimmediate data (vs return ptr)
  g->build_buffer;  // build everything into tmp local buffer area (vs main area)
  
*/


gptr wr_build_calc_cl(glb* g, gptr xptr) {
  void* db;
  int ruleflag;  
  gptr yptr;    
  gint xlen;
  gint xatomnr; 
  gint xmeta, xatom, yatom;  
  int i;
  gint tmp;
  int ilimit;

/*
printf("\nwr_build_calc_cl called\n");
wr_print_record(g,xptr);
printf("\n");
*/

#ifdef DEBUG
  printf("\nwr_build_calc_cl called, g->build_buffer ptr is %lx \n", (unsigned long int)g->build_buffer); 
#endif
  db=g->db;
  if (g->build_rename) (g->build_rename_vc)=0;
  ruleflag=wg_rec_is_rule_clause(db,xptr);  

  if (!ruleflag) { ///// atom case starts

    // in some cases, no change, no copy: normally copy
    //yptr=xptr;     
#ifdef DEBUG    
    printf("\nin wr_build_calc_cl !ruleflag case\n"); 
#endif    
    xlen=get_record_len(xptr);
#ifdef DEBUG
    printf("\nin wr_build_calc_cl got record with len %ld\n",xlen);
#endif   

    // allocate space
    if ((g->build_buffer)!=NULL) {
      yptr=wr_alloc_from_cvec(g,g->build_buffer,(RECORD_HEADER_GINTS+xlen));
      if (yptr==NULL) return NULL;       
       // copy rec header and clause header
      ilimit=RECORD_HEADER_GINTS+1; // this should change with probabs!!!!
      for(i=0;i<RECORD_HEADER_GINTS+CLAUSE_EXTRAHEADERLEN;i++) {
        //printf("\n i1: %d RECORD_HEADER_GINTS: %d, CLAUSE_EXTRAHEADERLEN: %d xptr[i]: %d \n",i,RECORD_HEADER_GINTS,CLAUSE_EXTRAHEADERLEN,xptr[i]);
        yptr[i]=xptr[i];     
      }  
    } else {
      yptr=wg_create_raw_record(db,xlen); 
      if (yptr==NULL) return NULL;
       // copy rec header and clause header
      ilimit=RECORD_HEADER_GINTS+1; // this should change with probabs!!!!
      for(i=0;i<RECORD_HEADER_GINTS+CLAUSE_EXTRAHEADERLEN;i++) {
        //printf("\n i1: %d RECORD_HEADER_GINTS: %d, CLAUSE_EXTRAHEADERLEN: %d xptr[i]: %d \n",i,RECORD_HEADER_GINTS,CLAUSE_EXTRAHEADERLEN,xptr[i]);
        yptr[i]=xptr[i];     
      }  
      /*
      // copy clause header
      ilimit=RECORD_HEADER_GINTS+1; // this should change with probabs!!!!
      for(i=0;i<CLAUSE_EXTRAHEADERLEN;i++) {
        //printf("\n i1: %d RECORD_HEADER_GINTS: %d, CLAUSE_EXTRAHEADERLEN: %d xptr[i]: %d \n",i,RECORD_HEADER_GINTS,CLAUSE_EXTRAHEADERLEN,xptr[i]);
        yptr[i]=xptr[i];     
      } 
      */                   
    }        
    if (yptr==NULL) return NULL;
#ifdef DEBUG    
    printf("\n in wr_build_calc_cl cp0 original xptr:\n");
    wr_print_record(g,xptr);
    printf("\n");

    printf("\n in wr_build_calc_cl cp0 created yptr:\n");
    wr_print_record(g,yptr);
    printf("\n");
#endif    
    /*
    // copy rec header and clause header
    ilimit=RECORD_HEADER_GINTS+1; // this should change with probabs!!!!
    for(i=0;i<RECORD_HEADER_GINTS+CLAUSE_EXTRAHEADERLEN;i++) {
      //printf("\n i1: %d RECORD_HEADER_GINTS: %d, CLAUSE_EXTRAHEADERLEN: %d xptr[i]: %d \n",i,RECORD_HEADER_GINTS,CLAUSE_EXTRAHEADERLEN,xptr[i]);
      yptr[i]=xptr[i];     
    }  
    */
    /*
    printf("\n in wr_build_calc_cl cp1 yptr:\n");
    wr_print_record(g,yptr);
    printf("\n");
    */
    //wr_print_varbank(g,g->varbanks);
    // loop over rest of clause elems
    
    for(;i<xlen+RECORD_HEADER_GINTS;i++) {
      tmp=xptr[i];
      //printf("\n i: %d xlen: %d RECORD_HEADER_GINTS: %d\n",i,xlen,RECORD_HEADER_GINTS);
      //printf("\ntmp 1: %d\n",tmp);
      tmp=wr_build_calc_term(g,xptr[i]);    
      //printf("\ntmp 2: %d\n",tmp);
      if (tmp==WG_ILLEGAL) return NULL; // could be memory err
      yptr[i]=tmp; 
    } 
#ifdef DEBUG    
    printf("\n in wr_build_calc_cl final result yptr:\n");
    wr_print_record(g,yptr);
    printf("\n");
#endif    
    /* old version 

    tmp=wr_build_calc_term(g,encode_datarec_offset(pto(db,xptr)));
    if (tmp==WG_ILLEGAL)  return NULL; // could be memory err
    if (tmp==ACONST_FALSE || tmp==ACONST_TRUE) return tmp; // for computable atoms
    yptr=rotp(g,tmp);
    */

  } else {        ///// rule case starts

#ifdef DEBUG
    printf("\nin wr_build_calc_cl ruleflag case\n"); 
#endif    
    xlen=get_record_len(xptr);
#ifdef DEBUG
    printf("\nnin wr_build_calc_cl got record with len %ld\n",xlen);
#endif
    // allocate space
    if ((g->build_buffer)!=NULL) {
      yptr=wr_alloc_from_cvec(g,g->build_buffer,(RECORD_HEADER_GINTS+xlen));       
    } else {
      yptr=wg_create_raw_record(db,xlen);                   
    }        
    if (yptr==NULL) return NULL;
    
     
    // copy rec header and clause header
    ilimit=RECORD_HEADER_GINTS+CLAUSE_EXTRAHEADERLEN;
    for(i=0;i<ilimit;i++) {
      yptr[i]=xptr[i];     
    }  
#ifdef DEBUG
    printf("\n in wr_build_calc_cl cp0 original xptr:\n");
    wr_print_record(g,xptr);
    printf("\n");

    printf("\n in wr_build_calc_cl cp0 created yptr:\n");
    wr_print_record(g,yptr);
    printf("\n");
#endif

    //wr_print_varbank(g,g->varbanks);
    // loop over clause elems
    xatomnr=wg_count_clause_atoms(db,xptr);
    for(i=0;i<xatomnr;i++) {
      xmeta=wg_get_rule_clause_atom_meta(db,xptr,i);
      xatom=wg_get_rule_clause_atom(db,xptr,i);
      wr_set_rule_clause_atom_meta(g,yptr,i,xmeta);
      yatom=wr_build_calc_term(g,xatom);
      if (yatom==WG_ILLEGAL) return NULL; // could be memory err
      if (yatom==ACONST_FALSE) return (gptr)ACONST_FALSE; // return yatom; // for computable atoms !!!! ???
      if (yatom==ACONST_TRUE) return (gptr)ACONST_TRUE; // return yatom; // for computable atoms, but should not do that here!!! ???
      wr_set_rule_clause_atom(g,yptr,i,yatom);
    }   
#ifdef DEBUG
    printf("\n in wr_build_calc_cl final result yptr:\n");
    wr_print_record(g,yptr);
    printf("\n");
#endif
    //wr_print_varbank(g,g->varbanks);
  }
  ++(g->stat_built_cl);
  return yptr;  
} 


/* ** subst and calc term
  
  uses global flags set before:
  g->build_subst    // subst var values into vars
  g->build_calc     // do fun and pred calculations
  g->build_dcopy    // copy nonimmediate data (vs return ptr)
  g->build_buffer;  // build everything into tmp local buffer area (vs main area)
  
*/


gint wr_build_calc_term(glb* g, gint x) {
  void* db;
  gptr xptr,yptr;
  gint xlen,uselen;
  gint tmp; // used by VARVAL_F
  gint vnr;
  gint newvar;
  int i;
  gint res;
  int ilimit;
  int substflag;
  int comp;
#ifdef USE_TERM_META_EXPERIMENTAL  
  int hasvars=0;
  gint smeta,tmeta=0;
#endif  

  /*  
  printf("wr_build_calc_term called with x %d type %d\n",x,wg_get_encoded_type(g->db,x));
  wr_print_vardata(g);
  wr_print_term(g,x);
  printf("\n");
  if (isvar(x)) printf("\nisvar\n");
  else printf("\nnot isvar\n");  
  printf("g->build_subst %d\n",g->build_subst);
  printf("g->build_rename %d\n",g->build_rename);
  */
  //printf("VARVAL_F(x,(g->varbanks)): %d\n",VARVAL_F(x,(g->varbanks)));
  //wr_print_term(g,VARVAL_F(x,(g->varbanks)));
  //wg_print_record(g->db,VARVAL_F(x,(g->varbanks)));
  //printf("\n");

  if (isvar(x) && (g->build_subst || g->build_rename))  x=VARVAL_F(x,(g->varbanks));
  if (!isdatarec(x)) {
    // now we have a simple value  
    if (!isvar(x) || !(g->build_rename)) return x;
    vnr=decode_var(x);      
    if (vnr<FIRST_UNREAL_VAR_NR) {
      //normal variable, has to be renamed 
      newvar=(g->build_rename_vc)+FIRST_UNREAL_VAR_NR;
      if ((g->build_rename_vc)>=NROF_VARSINBANK) {
        ++(g->stat_internlimit_discarded_cl); 
        (g->alloc_err)=3;      
#ifdef PRINT_LIMITS         
        printf("limiterr in wr_build_calc_term for renamed var nrs\n");                    
#endif        
        return WG_ILLEGAL;          
      }  
      ++(g->build_rename_vc);
      SETVAR(x,encode_var(newvar),(g->varbanks),(g->varstack),(g->tmp_unify_vc));
      return encode_var(((g->build_rename_banknr)*NROF_VARSINBANK)+(newvar-FIRST_UNREAL_VAR_NR));
    } else {
      return encode_var(((g->build_rename_banknr)*NROF_VARSINBANK)+(vnr-FIRST_UNREAL_VAR_NR));
    }    
  }   
  // now we have a datarec
  if (0) {
  } else {  
    db=g->db;
    xptr=decode_record(db,x);
#ifdef USE_TERM_META    
    // return ground terms without change or copy
    if (issmallint(xptr[RECORD_HEADER_GINTS+TERM_META_POS]) && 
        (xptr[RECORD_HEADER_GINTS+TERM_META_POS] & TERMMETA_GROUND_MASK)) {
      return x;
    }
#endif       
    xlen=get_record_len(xptr);
    // allocate space
    if ((g->build_buffer)!=NULL) {       
      yptr=wr_alloc_from_cvec(g,g->build_buffer,(RECORD_HEADER_GINTS+xlen));     
      //yptr=malloc(64);
    } else {
      yptr=wg_create_raw_record(db,xlen);     
    }    
    if (yptr==NULL) return WG_ILLEGAL;
    // copy rec header and term header
    ilimit=RECORD_HEADER_GINTS+(g->unify_firstuseterm);
    for(i=0;i<ilimit;i++) yptr[i]=xptr[i];    
#ifdef USE_TERM_META    
    // set termmeta to 0 if smallint (and not ground: but this we already know)    
    if (issmallint(yptr[RECORD_HEADER_GINTS+TERM_META_POS]) && (g->build_subst)) {
      yptr[RECORD_HEADER_GINTS+TERM_META_POS]=0;
    }
#endif    
    // loop over term elems, i already correct
    if (g->unify_maxuseterms) {
      if (((g->unify_maxuseterms)+(g->unify_firstuseterm))<xlen) 
        uselen=((g->unify_maxuseterms)+(g->unify_firstuseterm)+RECORD_HEADER_GINTS);
      else
        uselen=xlen+RECORD_HEADER_GINTS; 
    } else {    
      uselen=xlen+RECORD_HEADER_GINTS;
    }  
    substflag=(g->build_subst || g->build_rename);
    for(;i<uselen;i++) {
      //printf("wr_build_calc_term loop i %d xptr[i] %d\n",i,xptr[i]);
      if (!substflag && !isdatarec(xptr[i])) yptr[i]=xptr[i];       
      else {
        tmp=wr_build_calc_term(g,xptr[i]);
        if (tmp==WG_ILLEGAL) return WG_ILLEGAL;
#ifdef USE_TERM_META_EXPERIMENTAL
        smeta=decode_smallint(*(wg_decode_record(db,tmp)+(RECORD_HEADER_GINTS+TERM_META_POS)));
        tmeta=tmeta+(smeta & TERMMETA_SIZE_MASK);
        if (tmeta>TERMMETA_SIZE_MASK) tmeta=TERMMETA_SIZE_MASK;
        if (!(smeta & TERMMETA_GROUND_MASK)) hasvars=1;
#endif        
        //printf("wr_build_calc_term loop tmp %d \n",(gint)tmp);
        yptr[i]=tmp;
      }  
    }
    // copy term footer (in addition to rec/term header), i already correct
    if (g->unify_maxuseterms) {
      ilimit=RECORD_HEADER_GINTS+xlen;
      for(;i<ilimit;i++) {
        yptr[i]=xptr[i];     
      }  
    }
#ifdef USE_TERM_META_EXPERIMENTAL
    if (!hasvars) tmeta=tmeta | TERMMETA_GROUND_MASK;
    tmeta=encode_smallint(tmeta);
    *(yptr+(RECORD_HEADER_GINTS+TERM_META_POS))=tmeta;
#endif              
    if (g->use_comp_funs) {
      comp=wr_computable_termptr(g,yptr);
      if (comp) {
        res=wr_compute_from_termptr(g,yptr,comp); 
        if (res==WG_ILLEGAL) return WG_ILLEGAL;
      } else {
        res=encode_record(db,yptr);
      }
    } else {
      res=encode_record(db,yptr);
    }       
    return res;
  }   
}  

/*
  used only in case a subterm at replpos has to be replaced with replterm:
  otherwise use wr_build_calc_term

  Replacement is done only once, when *path==replpos

  *path is for storing the current path, not a real argument
  NB! *path must not be increased while processing any susbtituted-into values:
  it should follow the order of the original term only.
  
*/

gint wr_build_calc_term_replace(glb* g, gint x, int replpos, gint replterm, int* path) {
  void* db;
  gptr xptr,yptr;
  gint xlen,uselen;
  gint tmp; // used by VARVAL_F
  gint vnr;
  gint newvar;
  int i;
  gint res;
  int ilimit;
  int substflag;
  int comp;
  int incpath=1;

  /*   
  printf("\nwr_build_calc_term_replace called with x %d type %d replpos %d path %d term x:\n",
          x,wg_get_encoded_type(g->db,x),replpos,*path);
  wr_print_term(g,x);
  printf("\nreplterm:\n");
  wr_print_term(g,replterm);
  printf("\n");
  */
  if (*path==replpos) {         
    //printf("\n*path==replpos case\n"); 
    res=wr_build_calc_term(g,replterm);    
    //printf("\nwr_build_calc_term on replacement returns\n");
    //wr_print_term(g,res); 
    return res;
  }
  if (isvar(x) && (g->build_subst || g->build_rename)) {
    x=VARVAL_F(x,(g->varbanks));
    // block increasing path counter when inside a substituted-into value:
    if (isdatarec(x)) { 
      incpath=0;
    }  
  } 
  if (!isdatarec(x)) {
    // now we have a simple value  
    // *path will not be increased
    if (!isvar(x) || !(g->build_rename)) return x;
    vnr=decode_var(x);      
    if (vnr<FIRST_UNREAL_VAR_NR) {
      //normal variable, has to be renamed 
      newvar=(g->build_rename_vc)+FIRST_UNREAL_VAR_NR;
      if ((g->build_rename_vc)>=NROF_VARSINBANK) {
        ++(g->stat_internlimit_discarded_cl); 
        (g->alloc_err)=3;      
#ifdef PRINT_LIMITS         
        printf("limiterr in wr_build_calc_term_replace for renamed var nrs\n");                    
#endif        
        return WG_ILLEGAL;          
      }  
      ++(g->build_rename_vc);
      SETVAR(x,encode_var(newvar),(g->varbanks),(g->varstack),(g->tmp_unify_vc));
      return encode_var(((g->build_rename_banknr)*NROF_VARSINBANK)+(newvar-FIRST_UNREAL_VAR_NR));
    } else {
      return encode_var(((g->build_rename_banknr)*NROF_VARSINBANK)+(vnr-FIRST_UNREAL_VAR_NR));
    }    
  }   
  // now we have a datarec
  // iff it is obtained from variable value (substitution) then here we have
  // incpath==0
  if (0) {
  } else {  
    db=g->db;
    xptr=decode_record(db,x);
    xlen=get_record_len(xptr);
    //printf("wr_build_calc_term xptr %d xlen %d\n",(gint)xptr,xlen);
    // allocate space
    if ((g->build_buffer)!=NULL) {       
      yptr=wr_alloc_from_cvec(g,g->build_buffer,(RECORD_HEADER_GINTS+xlen));     
      //yptr=malloc(64);
    } else {
      yptr=wg_create_raw_record(db,xlen);     
    }    
    if (yptr==NULL) return WG_ILLEGAL;
    // copy rec header and term header
    ilimit=RECORD_HEADER_GINTS+(g->unify_firstuseterm);
    for(i=0;i<ilimit;i++) yptr[i]=xptr[i];      
    // set termmeta to 0 if smallint
#ifdef USE_TERM_META     
    if (issmallint(yptr[RECORD_HEADER_GINTS+TERM_META_POS])) {
      yptr[RECORD_HEADER_GINTS+TERM_META_POS]=0;
    }  
#endif    
    //printf("wr_build_calc_term i %d \n",i);
    // loop over term elems, i already correct
    if (g->unify_maxuseterms) {
      if (((g->unify_maxuseterms)+(g->unify_firstuseterm))<xlen) 
        uselen=((g->unify_maxuseterms)+(g->unify_firstuseterm)+RECORD_HEADER_GINTS);
      else
        uselen=xlen+RECORD_HEADER_GINTS; 
    } else {    
      uselen=xlen+RECORD_HEADER_GINTS;
    }  
    substflag=(g->build_subst || g->build_rename);
    for(;i<uselen;i++) {   
#ifdef PARA_INTO_FUNSYMB          
      if (incpath) (*path)++;
#else      
      if (i==ilimit) {
        // funsymb     
        //printf("\ni %d ilimit %d",i,ilimit); 
        yptr[i]=xptr[i];
        continue;
      }
      if (incpath) (*path)++;       
#endif      
      /*
      printf("wr_build_calc_term loop i %d xptr[i] %d path %d replpos %d uselen %d\n",i,xptr[i],*path,replpos,uselen);
      wr_print_term(g,xptr[i]);
      printf("\n");
      */     
      if (!substflag && !isdatarec(xptr[i]) && (*path)!=replpos) yptr[i]=xptr[i];
      else if ((*path)<=replpos) {
        //printf("\n(*path)<=replpos case"); 
        if (incpath) tmp=wr_build_calc_term_replace(g,xptr[i],replpos,replterm,path);
        else tmp=wr_build_calc_term(g,xptr[i]);
        //printf("\nreturns\n");
        //wr_print_term(g,tmp); 
        if (tmp==WG_ILLEGAL) return WG_ILLEGAL;
        //printf("wr_build_calc_term loop tmp %d \n",(gint)tmp);
        yptr[i]=tmp;   
      } else {
        //printf("\nnot (*path)<=replpos case"); 
        tmp=wr_build_calc_term(g,xptr[i]);
        if (tmp==WG_ILLEGAL) return WG_ILLEGAL;
        //printf("wr_build_calc_term loop tmp %d \n",(gint)tmp);
        yptr[i]=tmp;
      }        
    }
    // copy term footer (in addition to rec/term header), i already correct
    if (g->unify_maxuseterms) {
      ilimit=RECORD_HEADER_GINTS+xlen;
      for(;i<ilimit;i++) {
        yptr[i]=xptr[i];     
      }  
    }          
    if (g->use_comp_funs) {
      comp=wr_computable_termptr(g,yptr);
      if (comp) {
        res=wr_compute_from_termptr(g,yptr,comp); 
        if (res==WG_ILLEGAL) return WG_ILLEGAL;
      } else {
        res=encode_record(db,yptr);
      }
    } else {
      res=encode_record(db,yptr);
    }    
    /*
    printf("\nwr_build_calc_term_replace returning newatom\n ");  
    if (res==ACONST_FALSE) printf("ACONST_FALSE");
    else if (res==ACONST_TRUE) printf("ACONST_TRUE");
    else wr_print_term(g,res);   
    printf("\n");
    */
    return res;
  }   
}  


/* ----------------

 answer preds

  ------------------- */


int wr_answer_lit(glb* g, gint atom) {
  gptr tptr;
  gint fun;
  //gint nr;
  char *str;
  tptr=rotp(g,atom);
  fun=tptr[RECORD_HEADER_GINTS+(g->unify_funpos)];
  if (wg_get_encoded_type(g->db,fun)==WG_URITYPE) {    
    str = wg_decode_uri(g->db,fun);    
    if (str[0]=='a' && str[1]=='n' && str[2]=='s' && str[3]=='\0' &&
        wg_decode_uri_prefix(g->db,fun)==NULL) return 1;
    else return 0;
  }  
  return 0;  
}  

/* ----------------

 equality atom

  ------------------- */

int wr_equality_atom(glb* g, gint atom) {
  gptr tptr;
  gint fun;
  char *str;
  tptr=rotp(g,atom);
  /*
  printf("wr_wr_is_equal_atom called with rec\n");
  wg_print_record(g->db,tptr);
  printf("\n");
  */
  fun=tptr[RECORD_HEADER_GINTS+(g->unify_funpos)];
  /*
  printf("cp1 fun %d type %d :\n",fun,wg_get_encoded_type(g->db,fun));
  wg_debug_print_value(g->db,fun);
  printf("\n");
  */
  if (wg_get_encoded_type(g->db,fun)==WG_URITYPE) {    
    str = wg_decode_uri(g->db,fun);
    if (str[0]=='=' && str[1]=='\0') {
      if (wg_decode_uri_prefix(g->db,fun)!=NULL) return 0;
      else return 1;
    }
  }
  return 0;
}


/* ----------------

 computable funs and preds

  ------------------- */


int wr_computable_termptr(glb* g, gptr tptr) {
  gint fun;
  //gint nr;
  char *str;
  /*
  printf("wr_computable_termptr called with rec\n");
  wg_print_record(g->db,tptr);
  printf("\n");
  */
  fun=tptr[RECORD_HEADER_GINTS+(g->unify_funpos)];
  /*
  printf("cp1 fun %d type %d :\n",fun,wg_get_encoded_type(g->db,fun));
  wg_debug_print_value(g->db,fun);
  printf("\n");
  */
  if (wg_get_encoded_type(g->db,fun)==WG_URITYPE) {    
    str = wg_decode_uri(g->db,fun);
    if (str[0]=='\0' || str[1]!='\0' || str[0]<42 || str[0]>62) return 0;
    if (wg_decode_uri_prefix(g->db,fun)!=NULL) return 0;
    if (str[0]=='=' && str[1]=='\0') return COMP_FUN_EQUAL;
    else if (str[0]=='<') return COMP_FUN_LESS;
    else if (str[0]=='+') return COMP_FUN_PLUS;
    else if (str[0]=='-') return COMP_FUN_MINUS;
    else if (str[0]=='*') return COMP_FUN_MULT;
    else if (str[0]=='/') return COMP_FUN_DIV;
  }  
  return 0;  
}  

gint wr_compute_from_termptr(glb* g, gptr tptr, int comp) {  
  //gint fun;
  gint res;
  
  switch (comp) {
    case COMP_FUN_EQUAL:
      res=wr_compute_fun_equal(g,tptr);
      break;
    case COMP_FUN_LESS:
      res=wr_compute_fun_less(g,tptr);
      break;
    case COMP_FUN_PLUS:
    case COMP_FUN_MINUS:
    case COMP_FUN_MULT:
    case COMP_FUN_DIV:
      res=wr_compute_fun_arith2(g,tptr,comp);
      break;
    default:
      res=encode_record(g->db,tptr);
  }
  return res;
}




gint wr_compute_fun_equal(glb* g, gptr tptr) {
  void* db=g->db;
  int len;
  gint a,b;
  gint atype, btype;
    
  len=get_record_len(tptr);
  if (len<(g->unify_firstuseterm)+3) return encode_record(db,tptr);  
  a=tptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
  b=tptr[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];
  if (wr_equal_term(g,a,b,1)) return ACONST_TRUE;
  // here we have not immediately equal a and b

  atype=wg_get_encoded_type(db,a);
  btype=wg_get_encoded_type(db,b);
  if (atype==WG_VARTYPE) return encode_record(db,tptr);
  if (btype==WG_VARTYPE) return encode_record(db,tptr);
  // here we have not equal a and b with non-var types
  if (atype==WG_RECORDTYPE || atype==WG_URITYPE || atype==WG_ANONCONSTTYPE || 
      btype==WG_RECORDTYPE || btype==WG_URITYPE || btype==WG_ANONCONSTTYPE) {
    return encode_record(db,tptr);
  } else {
    // direct immediate values were not equal, hence false
    return ACONST_FALSE;
  }    
}

gint wr_compute_fun_less(glb* g, gptr tptr) {
  void* db=g->db;
  int len;
  gint a,b;
  gint atype, btype;
  double ad,bd;

  len=get_record_len(tptr);
  if (len<(g->unify_firstuseterm)+3) return encode_record(db,tptr);  
  a=tptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
  b=tptr[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];
  if (wr_equal_term(g,a,b,1)) return ACONST_FALSE;

  // here we have not equal a and b 
  atype=wg_get_encoded_type(db,a);
  btype=wg_get_encoded_type(db,b);
  if (atype!=WG_INTTYPE && atype!=WG_DOUBLETYPE) return encode_record(db,tptr);
  if (btype!=WG_INTTYPE && btype!=WG_DOUBLETYPE) return encode_record(db,tptr);
  if (atype==WG_INTTYPE && btype==WG_INTTYPE) {
    // integer case
    if (wg_decode_int(db,a) < wg_decode_int(db,b)) return ACONST_TRUE;
    else return ACONST_FALSE;     
  } else { 
    // double case
    if (atype==WG_INTTYPE) ad=(double)(wg_decode_int(db,a));
    else ad=wg_decode_double(db,a);
    if (btype==WG_INTTYPE) bd=(double)(wg_decode_int(db,b));
    else bd=wg_decode_double(db,b);
    if (ad < bd) return ACONST_TRUE;
    else return ACONST_FALSE;             
  }  
}

gint wr_compute_fun_arith2(glb* g, gptr tptr, int comp) {
  void* db=g->db;
  gint len;
  gint a,b;
  gint atype, btype;
  gint ri;
  double ad,bd,rd;
   
  //printf("wr_compute_fun_plus called\n");
  len=get_record_len(tptr); 
  if (len<(g->unify_firstuseterm)+3) return encode_record(db,tptr);     
  a=tptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
  atype=wg_get_encoded_type(db,a);
  if (atype!=WG_INTTYPE && atype!=WG_DOUBLETYPE) return encode_record(db,tptr);
  b=tptr[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];
  btype=wg_get_encoded_type(db,b);
  if (btype!=WG_INTTYPE && btype!=WG_DOUBLETYPE) return encode_record(db,tptr);
  if (atype==WG_INTTYPE && btype==WG_INTTYPE) {
    // integer res case
    switch (comp) {
      case COMP_FUN_PLUS:
        ri=wg_decode_int(db,a) + wg_decode_int(db,b);
        break;
      case COMP_FUN_MINUS:
        ri=wg_decode_int(db,a) - wg_decode_int(db,b);
        break;  
      case COMP_FUN_MULT:
        ri=wg_decode_int(db,a) * wg_decode_int(db,b);
        break;  
      case COMP_FUN_DIV:
        ri=wg_decode_int(db,a) % wg_decode_int(db,b);
        break;  
      default:
        return encode_record(db,tptr);
    }
    return wg_encode_int(db,ri);    
  } else { 
    // double res case
    if (atype==WG_INTTYPE) ad=(double)(wg_decode_int(db,a));
    else ad=wg_decode_double(db,a);
    if (btype==WG_INTTYPE) bd=(double)(wg_decode_int(db,b));
    else bd=wg_decode_double(db,b);
    switch (comp) {
      case COMP_FUN_PLUS:
        rd=ad+bd;
        break;
      case COMP_FUN_MINUS:
        rd=ad-bd;
        break;  
      case COMP_FUN_MULT:
        rd=ad*bd;
        break;  
      case COMP_FUN_DIV:
        rd=ad/bd;
        break;  
      default: 
        return encode_record(db,tptr);
    }
    return wg_encode_double(db,rd);        
  }  
} 


gint wr_compute_fun_plus(glb* g, gptr tptr) {
  void* db=g->db;
  gint len;
  gint a,b;
  gint atype, btype;
  gint ri;
  double ad,bd,rd;
   
  //printf("wr_compute_fun_plus called\n");
  len=get_record_len(tptr); 
  if (len<(g->unify_firstuseterm)+3) return encode_record(db,tptr);     
  a=tptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
  atype=wg_get_encoded_type(db,a);
  if (atype!=WG_INTTYPE && atype!=WG_DOUBLETYPE) return encode_record(db,tptr);
  b=tptr[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];
  btype=wg_get_encoded_type(db,b);
  if (btype!=WG_INTTYPE && btype!=WG_DOUBLETYPE) return encode_record(db,tptr);
  if (atype==WG_INTTYPE && btype==WG_INTTYPE) {
    // integer res case
    ri=wg_decode_int(db,a)+wg_decode_int(db,b);
    return wg_encode_int(db,ri);    
  } else { 
    // double res case
    if (atype==WG_INTTYPE) ad=(double)(wg_decode_int(db,a));
    else ad=wg_decode_double(db,a);
    if (btype==WG_INTTYPE) bd=(double)(wg_decode_int(db,b));
    else bd=wg_decode_double(db,b);
    rd=ad+bd;
    return wg_encode_double(db,rd);        
  }  
}  



#ifdef __slusplus
}
#endif
