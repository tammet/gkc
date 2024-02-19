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
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "rincludes.h"  
#include "../Db/dballoc.h"  // for ACONST_TRUE/FALSE

/* ====== Private headers and defs ======== */

#define LISTPROCESSING
#define PRINT_LIMITS
//#define DEBUG
//#undef DEBUG


/* ======= Private protos ================ */


/* ====== Functions ============== */


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

#ifdef DEBUG
  wr_printf("\nwr_build_calc_cl called\n");
  wr_print_record(g,xptr);
  wr_printf("\n");
#endif
  /*
  wr_printf("\nwr_build_calc_cl called\n");
  wr_print_record(g,xptr);
  wr_printf("\ng->build_buffer %ld\n",(gint)(g->build_buffer)); 
  */
#ifdef DEBUG
  wr_printf("\nwr_build_calc_cl called, g->build_buffer ptr is %lx \n", (unsigned long int)g->build_buffer); 
#endif
  db=g->db;
  if (g->build_rename) (g->build_rename_vc)=0;
  ruleflag=wg_rec_is_rule_clause(db,xptr);  

  if (!ruleflag) { 
    // atom case starts

    // in some cases, no change, no copy: normally copy
    //yptr=xptr;     
#ifdef DEBUG    
    wr_printf("\nin wr_build_calc_cl !ruleflag case\n"); 
#endif    
    xlen=get_record_len(xptr);
#ifdef DEBUG
    wr_printf("\nin wr_build_calc_cl got record with len %ld\n",xlen);
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
    wr_printf("\n in wr_build_calc_cl cp0 original xptr:\n");
    wr_print_record(g,xptr);
    wr_printf("\n");

    wr_printf("\n in wr_build_calc_cl cp0 created yptr:\n");
    wr_print_record(g,yptr);
    wr_printf("\n");
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
    wr_printf("\n in wr_build_calc_cl cp1 yptr:\n");
    wr_print_record(g,yptr);
    wr_printf("\n");
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
    wr_printf("\n in wr_build_calc_cl final result yptr:\n");
    wr_print_record(g,yptr);
    wr_printf("\n");
#endif    

  } else {        
    // rule case starts

#ifdef DEBUG
    wr_printf("\nin wr_build_calc_cl ruleflag case\n"); 
#endif    
    xlen=get_record_len(xptr);
#ifdef DEBUG
    wr_printf("\nnin wr_build_calc_cl got record with len %ld\n",xlen);
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
    /*
    CP12
    wg_check_record(g->db,yptr);
    check_doubles_globally(g->db,g);
    CP13
    */
#ifdef DEBUG
    wr_printf("\n in wr_build_calc_cl cp0 original xptr:\n");
    wr_print_record(g,xptr);
    wr_printf("\n");

    wr_printf("\n in wr_build_calc_cl cp0 created yptr:\n");
    wr_print_record(g,yptr);
    wr_printf("\n");
#endif

    // loop over clause elems
    xatomnr=wg_count_clause_atoms(db,xptr);
    for(i=0;i<xatomnr;i++) {
      //printf("\ni %d\n",i);
      xmeta=wg_get_rule_clause_atom_meta(db,xptr,i);
      xatom=wg_get_rule_clause_atom(db,xptr,i);
      wr_set_rule_clause_atom_meta(g,yptr,i,xmeta);
      /*
      CP14
      wg_check_record(g->db,yptr);
      check_doubles_globally(g->db,g);
      CP15
      */
      yatom=wr_build_calc_term(g,xatom);
      /*
      wr_printf("\nyatom ");
      wr_print_record(g,rotp(g,yatom));
      wr_printf("\n");


      CP16
      wg_check_record(g->db,rotp(g,yatom));
      check_doubles_globally(g->db,g);
      CP17
      */ 
      if (yatom==WG_ILLEGAL) return NULL; // could be memory err
      if (yatom==ACONST_FALSE) return (gptr)ACONST_FALSE; // return yatom; // for computable atoms !!!! ???
      if (yatom==ACONST_TRUE) return (gptr)ACONST_TRUE; // return yatom; // for computable atoms, but should not do that here!!! ???
      wr_set_rule_clause_atom(g,yptr,i,yatom);
    }   
#ifdef DEBUG
    wr_printf("\n in wr_build_calc_cl final result yptr:\n");
    wr_print_record(g,yptr);
    wr_printf("\n");
#endif    
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

#ifdef DEBUG
  wr_printf("\nwr_build_calc_term called, g->build_buffer ptr is %lx \n", (unsigned long int)g->build_buffer); 
  wr_print_term(g,x);
  wr_printf("\n(g->build_rewrite) %d\n",(g->build_rewrite));
#endif
  if (isvar(x) && (g->build_subst || g->build_rename))  x=VARVAL_F(x,(g->varbanks));
  if (!isdatarec(x)) {
    // now we have a simple value 
    if (!isvar(x)) {
      if (g->build_rewrite) {      
        return wr_rewrite_constant(g,x); 
      } else {
        return x;
      }       
    } else if (!(g->build_rename)) {
      return x;
    } 
    // rename var
    vnr=decode_var(x);      
    if (vnr<FIRST_UNREAL_VAR_NR) {
      //normal variable, has to be renamed 
      newvar=(g->build_rename_vc)+FIRST_UNREAL_VAR_NR;
      if ((g->build_rename_vc)>=NROF_VARSINBANK) {
        ++(g->stat_internlimit_discarded_cl); 
        (g->alloc_err)=3;      
#ifdef PRINT_LIMITS         
        wr_printf("limiterr in wr_build_calc_term for renamed var nrs\n");                    
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
  /*
  CP16 
  check_doubles_globally(g->db,g);
  */
  if (0) {
  } else {  
    db=g->db;
    xptr=decode_record(db,x);
#ifdef USE_TERM_META    
    //printf("\n!!!! check if ground term:\n");
    //wr_print_term(g,rpto(g,xptr));
    //printf("\n");
    //printf("\nissmallint %d\n",issmallint(xptr[RECORD_HEADER_GINTS+TERM_META_POS]));
    //printf("\n groundflag %d\n",decode_smallint(xptr[RECORD_HEADER_GINTS+TERM_META_POS] & TERMMETA_GROUND_MASK));
    // return ground terms without change or copy
    //if (issmallint(xptr[RECORD_HEADER_GINTS+TERM_META_POS]) && 
    //    (decode_smallint(xptr[RECORD_HEADER_GINTS+TERM_META_POS]) & TERMMETA_GROUND_MASK) ) {
      //printf("\n!!!! ground term copied:\n");
      //wr_print_term(g,rpto(g,xptr));
      //printf("\n");
      //return x;
    //}
#endif       
    xlen=get_record_len(xptr);
    // allocate space
    if ((g->build_buffer)!=NULL) {       
      yptr=wr_alloc_from_cvec(g,g->build_buffer,(RECORD_HEADER_GINTS+xlen)); 
      //printf("\nv1 xlen %d yptr %ld (g->build_buffer) %ld\n",xlen,(gint)yptr,(gint)(g->build_buffer));    
      //yptr=malloc(64);
    } else {
      yptr=wg_create_raw_record(db,xlen);  
      //printf("\nv2 xlen %d yptr %ld (g->build_buffer) %ld\n",xlen,(gint)yptr,(gint)(g->build_buffer));   
    }    
    /*
    CP17 
    //check_doubles_globally(g->db,g);
    check_global_stores(g->db,g);
    wr_print_buffers(g);
    */
    if (yptr==NULL) return WG_ILLEGAL;
    // copy rec header and term header
    ilimit=RECORD_HEADER_GINTS+(g->unify_firstuseterm);
    for(i=0;i<ilimit;i++) yptr[i]=xptr[i];    
    /*
    CP18
    //check_doubles_globally(g->db,g);
    check_global_stores(g->db,g);
    */
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
      if (!substflag && !isdatarec(xptr[i]) && !(g->build_rewrite)) yptr[i]=xptr[i];       
      else {     
        /*
        CP7 
        check_doubles_globally(g->db,g);
        CP15
        */
        //tmp=wr_build_calc_term_copyground(g,xptr[i]);
        tmp=wr_build_calc_term(g,xptr[i]);
        /*
        CP8        
        check_doubles_globally(g->db,g);
        CP9
        */
        //wg_check_record(g->db,yptr);
        //CP10
        
        if (tmp==WG_ILLEGAL) return WG_ILLEGAL;
#ifdef USE_TERM_META_EXPERIMENTAL
        smeta=decode_smallint(*(wg_decode_record(db,tmp)+(RECORD_HEADER_GINTS+TERM_META_POS)));
        tmeta=tmeta+(smeta & TERMMETA_SIZE_MASK);
        if (tmeta>TERMMETA_SIZE_MASK) tmeta=TERMMETA_SIZE_MASK;
        if (!(smeta & TERMMETA_GROUND_MASK)) hasvars=1;
#endif        
        //printf("wr_build_calc_term loop tmp %d \n",(gint)tmp);
        yptr[i]=tmp;
        /*
        CP11
        printf("\ni %d ",i);        
        check_doubles_globally(g->db,g);
        CP12
        */
        
      }  
    }   
    /*
    CP0
    wg_check_record(g->db,yptr);
    check_doubles_globally(g->db,g);
    CP1
    */
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
      //printf("\ncomp %d\n",comp);
      //wr_print_term(g,rpto(g,yptr));
      if (comp) {        
        res=wr_compute_from_termptr(g,yptr,comp); 
        if (res==WG_ILLEGAL) return WG_ILLEGAL;  
        else if (res==ACONST_TRUE) return ACONST_TRUE;
        else if (res==ACONST_FALSE) return ACONST_FALSE;        
        else yptr=rotp(g,res);                
      }       
      //wr_print_term(g,rpto(g,yptr));      
    }
    res=encode_record(db,yptr);
    if (g->build_rewrite) {  
      if ((g->print_level_flag)==50) {
        printf("\nto  rewrite ");
        wr_print_term(g,res);         
      } 
      res=wr_rewrite_term(g,res); 
      if ((g->print_level_flag)==50) {       
        printf("\ngot rewrite ");
        wr_print_term(g,res); 
        printf("\n");      
      }         
    }
    return res;
  }   
}  

gint wr_build_calc_term_copyground(glb* g, gint x) {
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

#ifdef DEBUG
  wr_printf("\nwr_build_calc_term_copyground called, g->build_buffer ptr is %lx \n", (unsigned long int)g->build_buffer); 
  wr_print_term(g,x);
  wr_printf("\n");
#endif
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
        wr_printf("limiterr in wr_build_calc_term for renamed var nrs\n");                    
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
    //printf("\n!!!! check if ground term:\n");
    //wr_print_term(g,rpto(g,xptr));
    //printf("\n");
    //printf("\nissmallint %d\n",issmallint(xptr[RECORD_HEADER_GINTS+TERM_META_POS]));
    //printf("\n groundflag %d\n",decode_smallint(xptr[RECORD_HEADER_GINTS+TERM_META_POS] & TERMMETA_GROUND_MASK));
    // return ground terms without change or copy
    if (issmallint(xptr[RECORD_HEADER_GINTS+TERM_META_POS]) && 
        (decode_smallint(xptr[RECORD_HEADER_GINTS+TERM_META_POS]) & TERMMETA_GROUND_MASK) ) {
      //printf("\n!!!! ground term copied:\n");
      //wr_print_term(g,rpto(g,xptr));
      //printf("\n");
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
        tmp=wr_build_calc_term_copyground(g,xptr[i]);
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

#ifdef DEBUG
  wr_printf("\nwr_build_calc_term_replace called, g->build_buffer ptr is %lx \n", (unsigned long int)g->build_buffer); 
  wr_print_term(g,x);
  wr_printf("\n");
#endif 
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
        wr_printf("limiterr in wr_build_calc_term_replace for renamed var nrs\n");                    
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
      if (!substflag && !isdatarec(xptr[i]) && (*path)!=replpos) yptr[i]=xptr[i];
      else if ((*path)<=replpos) {
        //printf("\n(*path)<=replpos case"); 
        if (incpath) tmp=wr_build_calc_term_replace(g,xptr[i],replpos,replterm,path);

        else tmp=wr_build_calc_term(g,xptr[i]);
        //else tmp=wr_build_calc_term_copyground(g,xptr[i]);

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
#ifdef DEBUG
    wr_printf("\nwr_build_calc_term_replace returning newatom\n ");  
    if (res==ACONST_FALSE) wr_printf("ACONST_FALSE");
    else if (res==ACONST_TRUE) wr_printf("ACONST_TRUE");
    else wr_print_term(g,res);   
    wr_printf("\n");
#endif
    return res;
  }   
}  


/* ----------------

answer preds and specialcomp preds

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
    if (str[0]=='$' && str[1]=='a' && str[2]=='n' && str[3]=='s' && str[4]=='\0' &&
        wg_decode_uri_prefix(g->db,fun)==NULL) return 1;
    else return 0;
  }  
  return 0;  
}  


int wr_specialcomp_lit(glb* g, gint atom) {
  gptr tptr;
  gint fun;
  //gint nr;
  char *str;
  if (!(g->use_comp_arithmetic)) return 0;
  tptr=rotp(g,atom);
  fun=tptr[RECORD_HEADER_GINTS+(g->unify_funpos)];
  if (wg_get_encoded_type(g->db,fun)==WG_URITYPE) {    
    str = wg_decode_uri(g->db,fun);  

    //if ((str[0]=='<') && (str[1]=='\0')) return 1;

    if (str[0]!='$') return 0;    

    // less
    if (str[1]=='l' && str[2]=='e' && str[3]=='s' && str[4]=='s' && str[5]=='\0')
       return 1; 
    // lesseq   
    if (str[1]=='l' && str[2]=='e' && str[3]=='s' && str[4]=='s'  
        && str[5]=='e' && str[6]=='q' && str[7]=='\0')
       return 1; 
    // greater
    if (str[1]=='g' && str[2]=='r' && str[3]=='e' && str[4]=='a' && str[5]=='t'
        && str[6]=='e' && str[7]=='r' && str[8]=='\0')
       return 1;  
    // greatereq
    if (str[1]=='g' && str[2]=='r' && str[3]=='e' && str[4]=='a' && str[5]=='t'
        && str[6]=='e' && str[7]=='r' && str[8]=='e' && str[9]=='q' && str[10]=='\0')
       return 1; 
    // substr      
    if (str[1]=='s' && str[2]=='u' && str[3]=='b' && str[4]=='s' && str[5]=='t'
        && str[6]=='r' && str[7]=='\0')
       return 1; 
    // substrat       
    if (str[1]=='s' && str[2]=='u' && str[3]=='b' && str[4]=='s' && str[5]=='t'
        && str[6]=='r' && str[7]=='a'&& str[8]=='t' && str[9]=='\0')
       return 1;    

    if (str[1]!='i' && str[2]!='s' && str[3]!='_') return 0;

    // atom
    if (str[4]=='a' && str[5]=='t' 
        && str[6]=='o' && str[7]=='m' && str[8]=='\0' &&        
        wg_decode_uri_prefix(g->db,fun)==NULL) return 1;
    // distinct
    if (str[4]=='d' && str[5]=='i' 
        && str[6]=='s' && str[7]=='t' && str[8]=='i' && str[9]=='n' && str[10]=='c' 
        && str[11]=='t' && str[11]=='\0' &&        
        wg_decode_uri_prefix(g->db,fun)==NULL) return 1;    
    // list    
    if (str[4]=='l' && str[5]=='i' 
        && str[6]=='s' && str[7]=='t' && str[8]=='\0' &&
        wg_decode_uri_prefix(g->db,fun)==NULL) return 1;   
    // int         
    if (str[4]=='i' && str[5]=='n' 
        && str[6]=='t' && str[7]=='\0' &&
        wg_decode_uri_prefix(g->db,fun)==NULL) return 1;   
    // real    
    if (str[4]=='r' && str[5]=='e' 
        && str[6]=='a' && str[7]=='l' && str[8]=='\0' &&
        wg_decode_uri_prefix(g->db,fun)==NULL) return 1;       
    // number    
    if (str[4]=='n' && str[5]=='u' 
        && str[6]=='m' && str[7]=='b' && str[8]=='e' && str[9]=='r' && str[10]=='\0' &&
        wg_decode_uri_prefix(g->db,fun)==NULL) return 1;     

    /*
           //if (!strcmp(str,"$less")) return COMP_FUN_LESS;
        if (!strcmp(str,"$lesseq")) return COMP_FUN_LESSEQ;
        if (!strcmp(str,"$greatereq"))  return COMP_FUN_GREATEREQ;
        if (!strcmp(str,"$greater"))  return COMP_FUN_GREATER;          
    */
    /*
          if (!strcmp(str,"$is_int"))  return COMP_FUN_IS_INT;
          if (!strcmp(str,"$is_real"))  return COMP_FUN_IS_REAL;
          if (!strcmp(str,"$is_number"))  return COMP_FUN_IS_NUMBER;
          if (!strcmp(str,"$is_list"))  return COMP_FUN_IS_LIST;
          if (!strcmp(str,"$is_map"))  return COMP_FUN_IS_MAP;
          if (!strcmp(str,"$is_atom"))  return COMP_FUN_IS_ATOM;
    */          
    /*
    if (str[1]=='i' && str[2]=='s' && str[3]=='_' && str[4]=='a' && str[5]=='t' 
        && str[6]=='o' && str[7]=='m' && str[8]=='\0' &&
        wg_decode_uri_prefix(g->db,fun)==NULL) return 1;
    */    
    else return 0;
  }  
  return 0;  
}  

/* ----------------

 true and false clauses:

 return 0 if unknown, 1 if true, -1 if false

  ------------------- */



int wr_prop_truefalsity_clause(glb* g, gptr cl) { 
  int i,len,falsecount=0;
  gint atom;
  if (wg_rec_is_rule_clause(g->db,cl)) {
    len=wg_count_clause_atoms(g->db,cl);
    for(i=0;i<len;i++) {
      atom=wg_get_rule_clause_atom(g->db,cl,i);
      if (wr_answer_lit(g,atom)) {
        falsecount++;
        continue;
      }        
      if (wr_special_prop_lit(g,wg_get_rule_clause_atom(g->db,cl,i),"$true")) {
        if (wg_atom_meta_is_neg(g->db,wg_get_rule_clause_atom_meta(g->db,cl,i))) {
          falsecount++;
        } else {
          return 1;          
        }
      } else if (wr_special_prop_lit(g,wg_get_rule_clause_atom(g->db,cl,i),"$false")) {
        if (wg_atom_meta_is_neg(g->db,wg_get_rule_clause_atom_meta(g->db,cl,i))) {
          return 1;
        } else {
          falsecount++;
        }
      }
    }
    if (len==falsecount) return -1;
    else return 0;
  } else {
    if (wr_special_prop_lit(g,rpto(g,cl),"$true")) return 1;
    if (wr_special_prop_lit(g,rpto(g,cl),"$false")) return -1;
    return 0;    
  }
}  

int wr_special_prop_lit(glb* g, gint atom, char* argstr) {
  gptr tptr;
  gint fun;
  char *str;
  tptr=rotp(g,atom);
  fun=tptr[RECORD_HEADER_GINTS+(g->unify_funpos)];
  if (wg_get_encoded_type(g->db,fun)==WG_URITYPE) {    
    str = wg_decode_uri(g->db,fun);    
    if (str[0]=='$' && str[1]=='p' && str[2]=='r' && str[3]=='\0' &&
        wg_decode_uri_prefix(g->db,fun)==NULL) {
      fun=tptr[RECORD_HEADER_GINTS+(g->unify_funpos)+1];
      if (wg_get_encoded_type(g->db,fun)!=WG_URITYPE &&
          wg_get_encoded_type(g->db,fun)!=WG_ANONCONSTTYPE) return 0;
      str = wg_decode_uri(g->db,fun);    
      if (!strcmp(str,argstr)) {       
        return 1;  
      } else {
        return 0;
      }  
    }
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
#ifdef DEBUG
  wr_printf("wr_wr_is_equal_atom called with rec\n");
  wg_print_record(g->db,tptr);
  wr_printf("\n");
#endif
  fun=tptr[RECORD_HEADER_GINTS+(g->unify_funpos)];
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
  char *str;

#ifdef DEBUG  
  wr_printf("wr_computable_termptr called with rec\n");
  wg_print_record(g->db,tptr);
  wr_printf("\n");
#endif
  fun=tptr[RECORD_HEADER_GINTS+(g->unify_funpos)];
#ifdef DEBUG
  wr_printf("cp1 fun %d type %d :\n",fun,wg_get_encoded_type(g->db,fun));
  wg_debug_print_value(g->db,fun);
  wr_printf("\n");
#endif
  if (wg_get_encoded_type(g->db,fun)==WG_URITYPE) {    
    str = wg_decode_uri(g->db,fun);
    if (*str=='$') {
      if (str[1]=='a') return 0; // quick elimination of $ans
      if (g->use_comp_arithmetic) {        
        if (str[1]=='i') {          
          if (!strcmp(str,"$is_int"))  return COMP_FUN_IS_INT;
          if (!strcmp(str,"$is_real"))  return COMP_FUN_IS_REAL;
          if (!strcmp(str,"$is_number"))  return COMP_FUN_IS_NUMBER;
          if (!strcmp(str,"$is_list"))  return COMP_FUN_IS_LIST;
          if (!strcmp(str,"$is_map"))  return COMP_FUN_IS_MAP;
          if (!strcmp(str,"$is_atom"))  return COMP_FUN_IS_ATOM;
          if (!strcmp(str,"$is_distinct"))  return COMP_FUN_IS_UNIQUE;
          if (!strcmp(str,"$is_substr"))  return COMP_FUN_IS_SUBSTR;
          return 0;
        }
        if (!strcmp(str,"$sum")) return COMP_FUN_PLUS;
        if (!strcmp(str,"$product")) return COMP_FUN_MULT;
        if (!strcmp(str,"$difference"))  return COMP_FUN_MINUS;
        if (!strcmp(str,"$quotient"))  return COMP_FUN_DIV;

        if (!strcmp(str,"$less")) return COMP_FUN_LESS;
        if (!strcmp(str,"$lesseq")) return COMP_FUN_LESSEQ;
        if (!strcmp(str,"$greatereq"))  return COMP_FUN_GREATEREQ;
        if (!strcmp(str,"$greater"))  return COMP_FUN_GREATER;

        if (!strcmp(str,"$to_int"))  return COMP_FUN_TO_INT;
        if (!strcmp(str,"$to_real"))  return COMP_FUN_TO_REAL;
        if (!strcmp(str,"$floor"))  return COMP_FUN_FLOOR;
        if (!strcmp(str,"$ceiling"))  return COMP_FUN_CEILING;
        if (!strcmp(str,"$truncate"))  return COMP_FUN_TRUNCATE;
        if (!strcmp(str,"$round"))  return COMP_FUN_ROUND;
        if (!strcmp(str,"$uminus"))  return COMP_FUN_UMINUS;

        if (!strcmp(str,"$first")) return COMP_FUN_FIRST;
        if (!strcmp(str,"$rest")) return COMP_FUN_REST;
        
        if (!strcmp(str,"$strlen")) return COMP_FUN_STRLEN;
        if (!strcmp(str,"$substr")) return COMP_FUN_IS_SUBSTR;
        if (!strcmp(str,"$substrat")) return COMP_FUN_IS_SUBSTRAT;

        if (str[1]=='q' && str[2]=='u') {
          if (!strcmp(str,"$quotient_e"))  return COMP_FUN_QUOTIENT_E;
          if (!strcmp(str,"$quotient_t"))  return COMP_FUN_QUOTIENT_T;
          if (!strcmp(str,"$quotient_f"))  return COMP_FUN_QUOTIENT_F;
          return 0;
        } else if (str[1]=='r' && str[2]=='e') {
          if (!strcmp(str,"$remainder"))  return COMP_FUN_REMAINDER_E;
          if (!strcmp(str,"$remainder_e"))  return COMP_FUN_REMAINDER_E;
          if (!strcmp(str,"$remainder_t"))  return COMP_FUN_REMAINDER_T;
          if (!strcmp(str,"$remainder_f"))  return COMP_FUN_REMAINDER_F;
          return 0;
        }       
      }  
    } else {
      if (str[0]=='\0' || str[1]!='\0' || str[0]<42 || str[0]>62) return 0;
      if (wg_decode_uri_prefix(g->db,fun)!=NULL) return 0;
      if (str[0]=='=' && str[1]=='\0') return COMP_FUN_EQUAL;
      if (g->use_comp_arithmetic) {
        //if (str[0]=='<') return COMP_FUN_LESS;
        if (str[0]=='+') return COMP_FUN_PLUS;
        else if (str[0]=='-') return COMP_FUN_MINUS;
        else if (str[0]=='*') return COMP_FUN_MULT;
        else if (str[0]=='/') return COMP_FUN_DIV;
      }      
    }  
  }  
  return 0;  
}  

gint wr_compute_from_termptr(glb* g, gptr tptr, int comp) {  
  gint res;
  
  switch (comp) {
    case COMP_FUN_EQUAL:
      res=wr_compute_fun_equal(g,tptr);
      break;
    case COMP_FUN_LESS:
      res=wr_compute_fun_lesseq_core(g,tptr,ACONST_FALSE,1);
      break;
    case COMP_FUN_LESSEQ:
      res=wr_compute_fun_lesseq_core(g,tptr,ACONST_TRUE,1);
      break;  
    case COMP_FUN_GREATER:
      res=wr_compute_fun_lesseq_core(g,tptr,ACONST_FALSE,0);
      break;
    case COMP_FUN_GREATEREQ:
      res=wr_compute_fun_lesseq_core(g,tptr,ACONST_TRUE,0);
      break;  
    case COMP_FUN_PLUS:
    case COMP_FUN_MINUS:
    case COMP_FUN_MULT:
    case COMP_FUN_DIV:
    case COMP_FUN_QUOTIENT_E:
    case COMP_FUN_QUOTIENT_T:
    case COMP_FUN_QUOTIENT_F:
    case COMP_FUN_REMAINDER_E:
    case COMP_FUN_REMAINDER_T:
    case COMP_FUN_REMAINDER_F:
      res=wr_compute_fun_arith2(g,tptr,comp);
      break;
    case COMP_FUN_IS_INT:
    case COMP_FUN_IS_REAL:
    case COMP_FUN_IS_NUMBER:
    case COMP_FUN_IS_LIST:
    case COMP_FUN_IS_MAP:
    case COMP_FUN_IS_ATOM:
    case COMP_FUN_IS_UNIQUE:

    case COMP_FUN_TO_INT:
    case COMP_FUN_TO_REAL:
    case COMP_FUN_FLOOR:
    case COMP_FUN_CEILING:
    case COMP_FUN_TRUNCATE:
    case COMP_FUN_ROUND:
    case COMP_FUN_UMINUS:    
    case COMP_FUN_FIRST:  
      res=wr_compute_fun_list(g,tptr,1);
      break;
    case COMP_FUN_REST:  
      res=wr_compute_fun_list(g,tptr,0);  
      break;
    case COMP_FUN_STRLEN:
      res=wr_compute_fun_arith1(g,tptr,comp);
      break;  
    case COMP_FUN_IS_SUBSTR:  
    case COMP_FUN_IS_SUBSTRAT:
      res=wr_compute_fun_str(g,tptr,comp);  
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
  gint res;
    
  len=get_record_len(tptr);
  if (len<(g->unify_firstuseterm)+3) return encode_record(db,tptr);  
  a=tptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
  b=tptr[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];  
  res=wr_compute_fun_equal_terms(g,a,b,tptr);
  return res;
}


gint wr_compute_fun_equal_terms(glb* g, gint a, gint b, gptr tptr) {
  void* db=g->db;
  gint atype, btype;  

/*    
  printf("\nwr_compute_fun_equal_terms called on\n");
  wr_print_term(g,a);
  printf("\n");  
  wr_print_term(g,b);
  printf("\n");
*/  
  if (wr_equal_term(g,a,b,1)) return ACONST_TRUE;
  // here we have not immediately equal a and b
  atype=wg_get_encoded_type(db,a);
  btype=wg_get_encoded_type(db,b);
  if (atype==WG_VARTYPE) return encode_record(db,tptr);
  if (btype==WG_VARTYPE) return encode_record(db,tptr);
  // here we have not equal a and b with non-var types
#ifdef LISTPROCESSING
  gint tmp, ael, bel;
  gptr aptr, bptr;  
  aptr=rotp(g,a);
  bptr=rotp(g,b);
  if        (((btype==WG_INTTYPE) || (btype==WG_DOUBLETYPE)) &&
             ((atype==WG_RECORDTYPE && wr_record_is_list(g,aptr)) ||
              (atype==WG_URITYPE && 
                (wr_uri_is_list(g,a) || wr_uri_is_unique(g,a)) )))  {
    // number is not equal to a list or nil or unique uri
    return ACONST_FALSE;
  } else if (((atype==WG_INTTYPE) || (atype==WG_DOUBLETYPE)) &&            
             ((btype==WG_RECORDTYPE && wr_record_is_list(g,bptr)) ||
              (btype==WG_URITYPE && 
                (wr_uri_is_list(g,b) || wr_uri_is_unique(g,b)) ))) {
    // number is not equal to a list or nil or unique uri
    return ACONST_FALSE;  
  } else if (atype==WG_RECORDTYPE && btype==WG_URITYPE &&
             ((wr_uri_is_list(g,b) || wr_uri_is_unique(g,b))
               && wr_record_is_list(g,aptr) )) {              
    // list is not equal to nil or unique uri           
    return  ACONST_FALSE;    
  } else if (btype==WG_RECORDTYPE && atype==WG_URITYPE &&
             ((wr_uri_is_list(g,a) || wr_uri_is_unique(g,a)) 
               && wr_record_is_list(g,bptr) )) {
    // list is not equal to nil or unique uri     
    return  ACONST_FALSE;     
  } else if (atype==WG_RECORDTYPE && wr_record_is_list(g,aptr) &&
             btype==WG_RECORDTYPE && wr_record_is_list(g,bptr) ) {
    // list equality check       
    ael=wg_get_field(db,aptr,2);
    bel=wg_get_field(db,bptr,2);    
    tmp=wr_compute_fun_equal_terms(g,ael,bel,tptr);
    if (tmp==ACONST_FALSE) return ACONST_FALSE;
    if (tmp!=ACONST_TRUE) return tmp;
    ael=wg_get_field(db,aptr,3);
    bel=wg_get_field(db,bptr,3);     
    tmp=wr_compute_fun_equal_terms(g,ael,bel,tptr);
    if (tmp==ACONST_FALSE) return ACONST_FALSE;
    if (tmp!=ACONST_TRUE) return tmp;
    return ACONST_TRUE;
  } else if (atype==WG_URITYPE && btype==WG_URITYPE &&
            (wr_uri_is_unique(g,a) || wr_uri_is_list(g,a)) && 
            (wr_uri_is_unique(g,b) || wr_uri_is_list(g,b)) ) { 
    // unique uris are not the same at that point in code          
    return ACONST_FALSE;        
  } else if (atype==WG_URITYPE || atype==WG_ANONCONSTTYPE || 
             btype==WG_URITYPE || btype==WG_ANONCONSTTYPE) {         
    return encode_record(db,tptr);    
  } else if (atype==WG_RECORDTYPE || atype==WG_URITYPE || atype==WG_ANONCONSTTYPE || 
             btype==WG_RECORDTYPE || btype==WG_URITYPE || btype==WG_ANONCONSTTYPE)  {         
    return encode_record(db,tptr);
  } else {
    return ACONST_FALSE;
  }  
#else  
  if (atype==WG_RECORDTYPE || atype==WG_URITYPE || atype==WG_ANONCONSTTYPE || 
      btype==WG_RECORDTYPE || btype==WG_URITYPE || btype==WG_ANONCONSTTYPE) {
    return encode_record(db,tptr);    
  } else {
    // direct immediate values were not equal, hence false
    return ACONST_FALSE;
  }    
#endif  
}

gint wr_compute_fun_lesseq_core(glb* g, gptr tptr, gint ifequal, int isless) { 
  void* db=g->db;
  int len;
  gint a,b;
  gint atype, btype;
  double ad,bd;

  len=get_record_len(tptr);
  if (len<(g->unify_firstuseterm)+3) return encode_record(db,tptr);  
  a=tptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
  b=tptr[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];
  if (wr_equal_term(g,a,b,1)) return ifequal;

  // here we have not equal a and b 
  atype=wg_get_encoded_type(db,a);
  btype=wg_get_encoded_type(db,b);
  if (atype!=WG_INTTYPE && atype!=WG_DOUBLETYPE) return encode_record(db,tptr);
  if (btype!=WG_INTTYPE && btype!=WG_DOUBLETYPE) return encode_record(db,tptr);
  if (atype==WG_INTTYPE && btype==WG_INTTYPE) {
    // integer case
    if (isless) {
      if (wg_decode_int(db,a) < wg_decode_int(db,b)) return ACONST_TRUE;
      else return ACONST_FALSE;
    } else {
      if (wg_decode_int(db,a) > wg_decode_int(db,b)) return ACONST_TRUE;
      else return ACONST_FALSE;
    }         
  } else { 
    // double case
    if (atype==WG_INTTYPE) ad=(double)(wg_decode_int(db,a));
    else ad=wg_decode_double(db,a);
    if (btype==WG_INTTYPE) bd=(double)(wg_decode_int(db,b));
    else bd=wg_decode_double(db,b);
    if (isless) {
      if (ad < bd) return ACONST_TRUE;
      else return ACONST_FALSE; 
    } else {
      if (ad > bd) return ACONST_TRUE;
      else return ACONST_FALSE; 
    }                  
  }  
}

gint wr_compute_fun_arith2(glb* g, gptr tptr, int comp) {
  void* db=g->db;
  gint len;
  gint a,b;
  gint atype, btype;
  gint ai,bi,ri;
  double ad,bd,rd;
   
  //printf("wr_compute_fun_arith2 called\n");
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
        bi=wg_decode_int(db,b);
        if (!bi) return encode_record(db,tptr);
        ri=wg_decode_int(db,a) / bi;
        break;  
      case COMP_FUN_QUOTIENT_E:
        bi=wg_decode_int(db,b);
        if (!bi) return encode_record(db,tptr);
        ai=wg_decode_int(db,a);
        if (bi<0) ri=(gint)floor((double)ai / (double)bi);
        else ri=(gint)ceil((double)ai / (double)bi);        
        break;  
      case COMP_FUN_QUOTIENT_T:
        bi=wg_decode_int(db,b);
        if (!bi) return encode_record(db,tptr);
        ai=wg_decode_int(db,a);
        ri=(gint)trunc((double)ai / (double)bi);            
        break;   
      case COMP_FUN_QUOTIENT_F:
        bi=wg_decode_int(db,b);
        if (!bi) return encode_record(db,tptr);
        ai=wg_decode_int(db,a);
        ri=(gint)floor((double)ai / (double)bi);            
        break;   
      case COMP_FUN_REMAINDER_E:
        bi=wg_decode_int(db,b);
        if (!bi) return encode_record(db,tptr);
        ai=wg_decode_int(db,a);
        ri=(gint)(ai % bi);                
        break;  
      case COMP_FUN_REMAINDER_T:
        bi=wg_decode_int(db,b);
        if (!bi) return encode_record(db,tptr);
        ai=wg_decode_int(db,a);
        ri=(gint)(ai % bi);                
        break;   
      case COMP_FUN_REMAINDER_F:
        bi=wg_decode_int(db,b);
        if (!bi) return encode_record(db,tptr);
        ai=wg_decode_int(db,a);
        ri=(gint)(ai % bi);                
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
        if (bd==0.0) return encode_record(db,tptr);
        rd=ad/bd;
        break; 
      case COMP_FUN_QUOTIENT_E:       
        if (bd==0.0) return encode_record(db,tptr);        
        if (bd<0) ri=(gint)floor((double)ad / (double)bd);
        else ri=(gint)ceil((double)ad / (double)bd);
        return wg_encode_int(db,ri);        
        break;  
      case COMP_FUN_QUOTIENT_T:        
        if (bd==0.0) return encode_record(db,tptr);
        ri=(gint)trunc((double)ad / (double)bd);            
        return wg_encode_int(db,ri);
        break;   
      case COMP_FUN_QUOTIENT_F:
        if (bd==0.0) return encode_record(db,tptr);       
        ri=(gint)floor((double)ad / (double)bd);            
        return wg_encode_int(db,ri);
        break;  
      case COMP_FUN_REMAINDER_E:       
        if (bd==0.0) return encode_record(db,tptr);        
        ri=(gint)((gint)ad % (gint)bd);        
        return wg_encode_int(db,ri);        
        break;  
      case COMP_FUN_REMAINDER_T:        
        if (bd==0.0) return encode_record(db,tptr);        
        ri=(gint)((gint)ad % (gint)bd);             
        return wg_encode_int(db,ri);
        break;   
      case COMP_FUN_REMAINDER_F:
        if (bd==0.0) return encode_record(db,tptr);        
        ri=(gint)((gint)ad % (gint)bd);             
        return wg_encode_int(db,ri);
        break;      
      default: 
        return encode_record(db,tptr);
    }
    return wg_encode_double(db,rd);        
  }  
} 

gint wr_compute_fun_str(glb* g, gptr tptr, int comp) {
  void* db=g->db;
  gint len, len2;
  gint ci,itmp;
  gint a,b,c;
  gint atype, btype, ctype;
  char *as, *bs, *tmp;

   
  //printf("wr_compute_fun_str called\n");
  len=get_record_len(tptr); 
  if (len<(g->unify_firstuseterm)+3) return encode_record(db,tptr);     
  a=tptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
  atype=wg_get_encoded_type(db,a);
  if (atype!=WG_URITYPE) return encode_record(db,tptr);
  b=tptr[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];
  btype=wg_get_encoded_type(db,b);
  if (btype!=WG_URITYPE) return encode_record(db,tptr);
  as=wg_decode_uri(db,a);
  bs=wg_decode_uri(db,b);
  if(!as || !bs) return encode_record(db,tptr);
  switch(comp) {
    case COMP_FUN_IS_SUBSTR: 
      tmp=strstr(bs,as);
      if (tmp==NULL) return ACONST_FALSE;
      else return ACONST_TRUE;
      break;
    case COMP_FUN_IS_SUBSTRAT: 
      c=tptr[RECORD_HEADER_GINTS+(g->unify_funarg2pos)+1];
      ctype=wg_get_encoded_type(db,c);
      if (ctype!=WG_INTTYPE) return encode_record(db,tptr);
      ci=wg_decode_int(db,c);
      if (ci<0) return encode_record(db,tptr);
      len=strlen(bs);
      if (ci>len) return encode_record(db,tptr);      
      len2=strlen(as);
      if (len2+ci>len) return encode_record(db,tptr);
      itmp=memcmp(bs+ci,as,1);
      if (!itmp) return ACONST_TRUE;  
      else return ACONST_FALSE;     
      break;
  }
  return encode_record(db,tptr);
}  


gint wr_compute_fun_arith1(glb* g, gptr tptr, int comp) {
  void* db=g->db;
  gint len;
  gint a;
  gint atype;
  gint ri;
  char* str;
  double rd;
/*   
  printf("\nwr_compute_fun_arith1 called\n");
  wg_print_record(db,tptr);
  printf("\n");
*/
  len=get_record_len(tptr); 
  if (len<(g->unify_firstuseterm)+2) return encode_record(db,tptr);     
  a=tptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
  atype=wg_get_encoded_type(db,a);
  //if (atype!=WG_INTTYPE && atype!=WG_DOUBLETYPE) return encode_record(db,tptr);  
  if (atype==WG_INTTYPE) {
    // integer case
    switch (comp) {
      case COMP_FUN_IS_INT:
        return ACONST_TRUE;
        break;
      case COMP_FUN_IS_REAL:
        return ACONST_FALSE;
        break;  
      case COMP_FUN_IS_NUMBER:
        return ACONST_TRUE;
        break;  
      case COMP_FUN_IS_LIST:
        return ACONST_FALSE;
        break;  
      case COMP_FUN_IS_MAP:
        return ACONST_FALSE;
        break;
      case COMP_FUN_IS_ATOM:
        return ACONST_TRUE;
        break;   
      case COMP_FUN_IS_UNIQUE:
        return ACONST_FALSE;
        break;  

      case COMP_FUN_TO_INT:
        return a; 
        //ri=wg_decode_int(db,a); 
        //return wg_encode_int(db,ri);
        break;
      case COMP_FUN_TO_REAL: 
        ri=wg_decode_int(db,a); 
        return wg_encode_double(db,(double)ri);
        break;
      case COMP_FUN_FLOOR: 
        return a;
        //ri=wg_decode_int(db,a); 
        //return wg_encode_int(db,ri);
        break;
      case COMP_FUN_CEILING: 
        return a;
        //ri=wg_decode_int(db,a); 
        //return wg_encode_int(db,ri);
        break;
      case COMP_FUN_TRUNCATE: 
        return a;
        //ri=wg_decode_int(db,a); 
        //return wg_encode_int(db,ri);
        break;
      case COMP_FUN_ROUND: 
        return a;
        //ri=wg_decode_int(db,a); 
        //return wg_encode_int(db,ri);
        break;          
      case COMP_FUN_UMINUS: 
        ri=wg_decode_int(db,a); 
        return wg_encode_int(db,0-ri);
        break;   
      default:
        return encode_record(db,tptr);
    }        
  } else if (atype==WG_DOUBLETYPE) { 
    // double res case    
    switch (comp) {
      case COMP_FUN_IS_INT:
        return ACONST_FALSE;
        break;
      case COMP_FUN_IS_REAL:
        return ACONST_TRUE;
        break;  
      case COMP_FUN_IS_NUMBER:
        return ACONST_TRUE;
        break;  
      case COMP_FUN_IS_LIST:
        return ACONST_FALSE;
        break;  
      case COMP_FUN_IS_MAP:
        return ACONST_FALSE;
        break;
      case COMP_FUN_IS_ATOM:
        return ACONST_TRUE;
        break;   
      case COMP_FUN_IS_UNIQUE:
        return ACONST_FALSE;
        break;  

      case COMP_FUN_TO_INT:         
        rd=wg_decode_double(db,a); 
        return wg_encode_int(db,(gint)(floor(rd)));
        break;
      case COMP_FUN_TO_REAL:
        return a; 
        //ri=wg_decode_int(db,a); 
        //return wg_encode_double(db,(double)ri);
        break;
      case COMP_FUN_FLOOR: 
        rd=wg_decode_double(db,a); 
        return wg_encode_int(db,(gint)(floor(rd)));
        break;
      case COMP_FUN_CEILING: 
        rd=wg_decode_double(db,a); 
        return wg_encode_int(db,(gint)(ceil(rd)));
        //ri=wg_decode_int(db,a); 
        //return wg_encode_int(db,ri);
        break;
      case COMP_FUN_TRUNCATE: 
        rd=wg_decode_double(db,a); 
        return wg_encode_int(db,(gint)(trunc(rd)));
        //ri=wg_decode_int(db,a); 
        //return wg_encode_int(db,ri);
        break;
      case COMP_FUN_ROUND: 
        rd=wg_decode_double(db,a); 
        return wg_encode_int(db,(gint)(round(rd)));
        //ri=wg_decode_int(db,a); 
        //return wg_encode_int(db,ri);
        break;          
      case COMP_FUN_UMINUS: 
        rd=wg_decode_double(db,a);  
        return wg_encode_double(db,0-rd);
        break;   
      default:
        return encode_record(db,tptr);
    }               
  } else if (atype==WG_RECORDTYPE) {
    switch (comp) {            
      case COMP_FUN_IS_ATOM:     
        if (wr_compute_fun_list(g,tptr,2)==ACONST_TRUE)
        return ACONST_FALSE;
        break;            
      case COMP_FUN_IS_UNIQUE:     
        if (wr_compute_fun_list(g,tptr,2)==ACONST_TRUE)
        return ACONST_FALSE;
        break;  
      case COMP_FUN_IS_LIST:
        return wr_compute_fun_list(g,tptr,2);        
        break;
      case COMP_FUN_FIRST:         
        return wr_compute_fun_list(g,tptr,1);        
        break;
      case COMP_FUN_REST:         
        return wr_compute_fun_list(g,tptr,0);        
        break;    
    }
  } else if  (atype==WG_URITYPE) {    
    str=wg_decode_uri_prefix(db,a); 
    if (str && str[0]=='#' && str[1]=='\0') {
      // unique uri
      switch (comp) {
        case COMP_FUN_IS_INT:
          return ACONST_FALSE;
          break;
        case COMP_FUN_IS_REAL:
          return ACONST_FALSE;
          break;  
        case COMP_FUN_IS_NUMBER:
          return ACONST_FALSE;
          break;  
        case COMP_FUN_IS_LIST:
          return ACONST_FALSE;
          break;  
        case COMP_FUN_IS_MAP:
          return ACONST_FALSE;
          break;
        case COMP_FUN_IS_ATOM:
          return ACONST_TRUE;
          break;   
        case COMP_FUN_IS_UNIQUE:
          return ACONST_TRUE;
          break;  
        case COMP_FUN_STRLEN:
          str=wg_decode_uri(db,a);
          if (str) {
            return wg_encode_int(db,(gint)(strlen(str)));
            break; 
          } else {
            break;  
          }  
      }    
    } else {
      // not a unique uri
      switch (comp) {
        case COMP_FUN_IS_ATOM:          
          return ACONST_TRUE; 
          break;
        case COMP_FUN_IS_LIST:
          str=wg_decode_uri(db,a);  
          if (str && 
              str[0]=='$' && str[1]=='n' && str[2]=='i' && str[3]=='l' &&
              str[4]=='\0')         
            return ACONST_TRUE; 
          break;                
      }  
    }  
  }
  /*    
   else if (atype==WG_VARTYPE) {     
    gint tmp; 
    switch (comp) {
      case COMP_FUN_IS_ATOM:
        printf("\n");        
        wr_print_vardata(g);
        //wr_print_term(g,wr_varval(rotp(g,tptr),(g->varbanks))); 
        printf("\n");     
        return ACONST_FALSE;       
    }    
  } else if (comp==COMP_FUN_IS_ATOM) {
    return ACONST_TRUE;
  } 
  */
  return encode_record(db,tptr);  
} 

int wr_uri_is_unique(glb* g, gint fun) {
  char* funstr;
  
  funstr=wg_decode_uri_prefix(g->db,fun);    
  if (funstr &&
      funstr[0]=='#' && funstr[1]=='\0') 
    return 1;
  else
    return 0;
} 
   
int wr_uri_is_list(glb* g, gint fun) {
  char* funstr;
  
  funstr=wg_decode_uri(g->db,fun);    
  if (funstr &&
      funstr[0]=='$' && funstr[1]=='n' && funstr[2]=='i' && funstr[3]=='l' &&
      funstr[4]=='\0') 
    return 1;
  else
    return 0;     
}


int wr_record_is_list(glb* g, gptr tptr) {
  void* db=g->db;
  int len;
  gint fun;
  gint funtype;
  char* funstr;
  
  len=get_record_len(tptr); 
  if (len!=(g->unify_firstuseterm)+3) return 0;  // wrong nr of args to $list
  //fun=aptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)+1]; 
  fun=wg_get_field(db, tptr, 1);      
  // fun should be $list
  funtype=wg_get_encoded_type(db,fun);     
  if (funtype!=WG_URITYPE)  return 0;
  funstr=wg_decode_uri(db,fun);   
  if (funstr &&
      funstr[0]=='$' && funstr[1]=='l' && funstr[2]=='i' && funstr[3]=='s' &&
      funstr[4]=='t' && funstr[5]=='\0') 
    return 1;
  else
    return 0;     
}

gint wr_compute_fun_list(glb* g, gptr tptr, int opcode) {
  void* db=g->db;
  int len;
  gint a,fun;
  gint funtype,res;
  gptr aptr;
  char* funstr;
    
  // tptr should be $first($list(x,y))   
  len=get_record_len(tptr);
  if (len!=(g->unify_firstuseterm)+2) return encode_record(db,tptr);  // wrong nr of args to $first
  a=tptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)]; 
  //a=wg_get_field(db, tptr, 1);  
  if (!isdatarec(a)) return encode_record(db,tptr);  
  // a is indeed a function term 
  aptr=rotp(g,a);
  len=get_record_len(aptr); 
  if (len!=(g->unify_firstuseterm)+3) return encode_record(db,tptr);  // wrong nr of args to $list
  //fun=aptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)+1]; 
  fun=wg_get_field(db, aptr, 1);      
  // fun should be $list
  funtype=wg_get_encoded_type(db,fun);     
  if (funtype!=WG_URITYPE)  return encode_record(db,tptr);
  funstr=wg_decode_uri(db,fun);   
  if (funstr[0]=='$' && funstr[1]=='l' && funstr[2]=='i' && funstr[3]=='s' &&
      funstr[4]=='t' && funstr[5]=='\0') { 
    if (opcode==1) {
      //res=aptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)+1];     
      res=wg_get_field(db, aptr, 2);
      return res;
    } else if (opcode==0) {
      res=wg_get_field(db, aptr, 3);
      res=aptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)+1];   
    } else if (opcode==2) {
      return ACONST_TRUE;
    } else {
      return encode_record(db,tptr);
    }
    return res;  
  } else {
    return encode_record(db,tptr);
  }
}

void wr_print_buffers(glb* g) {
  printf("\ng->build_buffer    %ld",(gint)(g->build_buffer));
  printf("\ng->derived_termbuf %ld",(gint)(g->derived_termbuf));
  printf("\ng->queue_termbuf   %ld",(gint)(g->queue_termbuf));
  printf("\ng->std_termbuf   %ld",(gint)(g->std_termbuf));
  printf("\ng->given_termbuf   %ld",(gint)(g->given_termbuf));
  printf("\ng->hyper_termbuf   %ld",(gint)(g->hyper_termbuf));
}

#ifdef __slusplus
}
#endif
