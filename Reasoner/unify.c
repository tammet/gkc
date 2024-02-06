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

 /** @file unify.c
 *  Unification functions.
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
 

//#define DEBUG  
#undef DEBUG
  
/* ====== Private headers and defs ======== */

static gint wr_occurs_in(glb* g, gint x, gint y, gptr vb);


/* ======= Private protos ================ */


/* ====== Functions ============== */

/** Plain term unification using g->unify_samelen and g->unify_maxuseterms
*
*  Metainfo is not filtered out. Must be exactly the same.  
*
*/  
  
gint wr_unify_term(glb* g, gint x, gint y, int uniquestrflag) { 
  g->tmp_unify_vc=((gptr)(g->varstack))+1; // pointer arithmetic: &(varstack[1])
  g->tmp_unify_do_occcheck=1;
  g->tmp_unify_occcheck=1;
  if (wr_unify_term_aux(g,x,y,uniquestrflag)) {
    return 1;
  } else {
    return 0;  
  }  
}  
  
  
/** Plain term unification using g->unify_samelen and g->unify_maxuseterms
*
*  Metainfo is not filtered out. Must be exactly the same.  
*
*/

gint wr_unify_term_aux(glb* g, gint x, gint y, int uniquestrflag) {  
  gptr db;
  gint encx,ency;
  gint tmp; // used by VARVAL_F macro
  gptr xptr,yptr;
  int xlen,ylen,uselen,ilimit,i,isground;
  
#ifdef DEBUG
  wr_printf("wr_unify_term_aux called with x %d ",x);
  wr_print_term(g,x);
  wr_printf(" and y %d ",y);
  wr_print_term(g,y);
  wr_printf("\n");
#endif  
  // first check if immediately same: return 1 if yes 
  if (x==y)  return 1;     
  // second, fetch var values for var args
  if (isvar(x)) x=VARVAL_F(x,(g->varbanks)); 
  if (isvar(y)) y=VARVAL_F(y,(g->varbanks)); 
  // check again if same
  if (x==y) return 1;
  // go through the ladder of possibilities
  // knowing that x and y are different
  if (!isdatarec(x)) {
    // x is a primitive
    if (!isdatarec(y)) {
      // both x and y are primitive 
      if (isvar(x)) { 
        SETVAR(x,y,g->varbanks,g->varstack,g->tmp_unify_vc);
        // set occcheck only if y is a var too      
        if (g->tmp_unify_do_occcheck && isvar(y)) (g->tmp_unify_occcheck)=1;        
        return 1;
      } else if (isvar(y)) {
        SETVAR(y,x,g->varbanks,g->varstack,g->tmp_unify_vc);
        // do not set occcheck here: x is a constant!        
        return 1;
      }	else {
        // x and y are constants
        if (wr_equal_ptr_primitives(g,x,y,uniquestrflag)) return 1;
        else return 0;          
      }	      
    // x is primitive, but y is not  
    } else if (isvar(x)) {
      // x is var, y is non-primitive      
      if (g->tmp_unify_occcheck && wr_occurs_in(g,x,y,(gptr)(g->varbanks))) {
        return 0;
      } else {
        SETVAR(x,y,g->varbanks,g->varstack,g->tmp_unify_vc);
        if (g->tmp_unify_do_occcheck) (g->tmp_unify_occcheck)=1;
        return 1;
      }      
    } else {
      // x is a constant, but y is non-primitive 
      return 0;
    }      
  // x is not primitive
  } else if (isvar(y)) {
    // x is non-primitive, y is var
    if (g->tmp_unify_occcheck  && wr_occurs_in(g,y,x,(gptr)(g->varbanks))) {
      return 0;
    } else {
      SETVAR(y,x,g->varbanks,g->varstack,g->tmp_unify_vc);
      if (g->tmp_unify_do_occcheck) (g->tmp_unify_occcheck)=1;
      return 1;
    }      
  // x is not primitive, y is non-var
  } else if (!isdatarec(y)) {
    // x is not primitive, y is constant 
    return 0;  
  } else {
  // x and y are both complex terms     
    isground=0; // default, set to 1 if metainfos show that for both  
    db=g->db;
    xptr=decode_record(db,x);
    yptr=decode_record(db,y);
    xlen=get_record_len(xptr);
    ylen=get_record_len(yptr);
    if (g->unify_samelen) {
      if (xlen!=ylen) return 0;
#ifdef USE_TERM_META      
      encx=*(xptr+(RECORD_HEADER_GINTS+TERM_META_POS));
      ency=*(yptr+(RECORD_HEADER_GINTS+TERM_META_POS));  
      if (issmallint(encx) && issmallint(ency)) {        
        if ((decode_smallint(encx) & TERMMETA_GROUND_MASK) && 
            (decode_smallint(ency) & TERMMETA_GROUND_MASK) ) {
          // both terms are ground    
          if (encx!=ency) return 0; // ground term can unify only ground with same size
          isground=1;          
        }
      }      
#endif      
      uselen=xlen;      
    } else {
      if (xlen<=ylen) uselen=xlen;
      else uselen=ylen;
    } 
    if (g->unify_maxuseterms) {
      if (((g->unify_maxuseterms)+(g->unify_firstuseterm))<uselen) 
        uselen=(g->unify_firstuseterm)+(g->unify_maxuseterms);
    }    
    ilimit=RECORD_HEADER_GINTS+uselen;
    for(i=RECORD_HEADER_GINTS+(g->unify_firstuseterm); i<ilimit; i++) {
      encx=*(xptr+i);
      ency=*(yptr+i);
      if (encx!=ency && 
           ((isground) ?
             !wr_equal_term(g,encx,ency,uniquestrflag)
             :
             !wr_unify_term_aux(g,encx,ency,uniquestrflag)) ) 
        return 0;
    }      
    return 1;        
  }        
}  

/**
   x must be a variable and y must be a nonprimitive

*/

static gint wr_occurs_in(glb* g, gint x, gint y, gptr vb) { 
  void* db=g->db;
  gptr yptr;
  gint yi;
  int ylen,ilimit,i;
  gint tmp; // used by VARVAL_F
    
#ifdef DEBUG
  wr_printf("wr_occurs_in called with x %d ",x);
  wr_print_term(g,x);
  wr_printf(" and y %d ",y);
  wr_print_term(g,y);
  wr_printf("\n");
#endif 
  yptr=decode_record(db,y);
  ylen=get_record_len(yptr);
  
  if (g->unify_maxuseterms) {
    if (((g->unify_maxuseterms)+(g->unify_firstuseterm))<ylen) 
      ylen=(g->unify_firstuseterm)+(g->unify_maxuseterms);
  }    
#ifdef USE_TERM_META      
  yi=*(yptr+(RECORD_HEADER_GINTS+TERM_META_POS));  
  if (issmallint(yi) && (decode_smallint(yi) & TERMMETA_GROUND_MASK)) {
    return 0;
  }      
#endif     
  ilimit=RECORD_HEADER_GINTS+ylen;
  for(i=RECORD_HEADER_GINTS+(g->unify_firstuseterm); i<ilimit; i++) {     
    yi=*(yptr+i);
    if (isvar(yi)) yi=VARVAL_F(yi,vb);             
    if (x==yi) return 1;
    if (isdatarec(yi)) {
      if (wr_occurs_in(g,x,yi,vb)) return 1;
    }
  }      
  // passed vector ok, not finding x inside  
  return 0;    
}  


/* ----------------------------------------------------------

    Matching: one-sided unification

  ------------------------------------------------------------- */


/** Match terms:  x is the general term, y is an instance of x
    using g->unify_samelen and g->unify_maxuseterms

*/
  
gint wr_match_term(glb* g, gint x, gint y, int uniquestrflag) { 
  g->tmp_unify_vc=((gptr)(g->varstack))+1; // pointer arithmetic: &(varstack[1])
  if (wr_match_term_aux(g,x,y,uniquestrflag)) {
    return 1;
  } else {
    return 0;  
  }  
}  
  
  
/** Plain term matching using g->unify_samelen and g->unify_maxuseterms

   returns 1 iff x subsumes y

   assumptions: 
    ??? GBUNASSIGNEDVAL!=1 ????
    ?? xvars are initially all unassigned ??
    ?? yvars are initially all unassigned ??
    xvars do not have to be different from yvars
*/

gint wr_match_term_aux(glb* g, gint x, gint y, int uniquestrflag) {  
  gptr db;
  gint xval,encx,ency;
  gptr xptr,yptr;
  int xlen,ylen,uselen,ilimit,i;
  gint eqencx; // used by WR_EQUAL_TERM macro

#ifdef DEBUG
  wr_printf("wr_match_term_aux called with x %d ",x);
  wr_print_term(g,x);
  wr_printf(" and y %d ",y);
  wr_print_term(g,y);
  wr_printf("\n");
#endif  

  UNUSED(eqencx);  
  // check x var case immediately
  if (isvar(x)) {
    xval=VARVAL_DIRECT(x,(g->varbanks)); 
    if (xval==UNASSIGNED) {
      // previously unassigned var: assign now and return
      SETVAR(x,y,g->varbanks,g->varstack,g->tmp_unify_vc);
      return 1;     	
    } else {      
      // xval must now be equal to y, else match fails
      if (WR_EQUAL_TERM(g,xval,y,uniquestrflag)) return 1;       
      return 0;
    }    
  }
  // now x is not var
  if (!isdatarec(x)) {
    if (WR_EQUAL_TERM(g,x,y,uniquestrflag)) return 1;  
  } 
  if (!isdatarec(y)) return 0; // x is datarec but y is not    
  // now x and y are different datarecs
  if (1) {  
    db=g->db;
    xptr=decode_record(db,x);
    yptr=decode_record(db,y);
    xlen=get_record_len(xptr);
    ylen=get_record_len(yptr);
    if (g->unify_samelen) {
      if (xlen!=ylen) return 0;
#ifdef USE_TERM_META       
      encx=*(xptr+(RECORD_HEADER_GINTS+TERM_META_POS));
      ency=*(yptr+(RECORD_HEADER_GINTS+TERM_META_POS));  
      if (issmallint(encx) && issmallint(ency)) {
        if (encx>ency) { return 0; } // term with bigger termmeta cannot subs one with smaller
        if (decode_smallint(encx) & TERMMETA_GROUND_MASK) {
          if (encx!=ency) { return 0; } // ground term can subs only ground with same size
        }
      }
#endif      
      uselen=xlen;      
    } else {
      if (xlen<=ylen) uselen=xlen;
      else uselen=ylen;
    } 
    
    if (g->unify_maxuseterms) {
      if (((g->unify_maxuseterms)+(g->unify_firstuseterm))<uselen) 
        uselen=(g->unify_firstuseterm)+(g->unify_maxuseterms);
    }    
    ilimit=RECORD_HEADER_GINTS+uselen;
    for(i=RECORD_HEADER_GINTS+(g->unify_firstuseterm); i<ilimit; i++) {
      encx=*(xptr+i);
      ency=*(yptr+i);
      if (!wr_match_term_aux(g,encx,ency,uniquestrflag)) return 0;
    }           
    return 1;        
  }        
} 


gint wr_eqmodvars_term_aux(glb* g, gint x, gint y, int uniquestrflag) {  
  gptr db;
  gint xval,encx,ency;
  gptr xptr,yptr;
  int xlen,ylen,uselen,ilimit,i;
  gint eqencx; // used by WR_EQUAL_TERM macro

#ifdef DEBUG
  wr_printf("wr_match_term_aux called with x %d ",x);
  wr_print_term(g,x);
  wr_printf(" and y %d ",y);
  wr_print_term(g,y);
  wr_printf("\n");
#endif  

  UNUSED(eqencx);  
  // check x var case immediately
  if (isvar(x)) {
    xval=VARVAL_DIRECT(x,(g->varbanks)); 
    if (xval==UNASSIGNED) {
      if (!isvar(y)) return 0;
      // previously unassigned var: assign now and return
      SETVAR(x,y,g->varbanks,g->varstack,g->tmp_unify_vc);
      return 1;     	
    } else {      
      // xval must now be equal to y, else match fails
      if (WR_EQUAL_TERM(g,xval,y,uniquestrflag)) return 1;       
      return 0;
    }    
  }
  // now x is not var
  if (!isdatarec(x)) {
    if (WR_EQUAL_TERM(g,x,y,uniquestrflag)) return 1;  
  } 
  if (!isdatarec(y)) return 0; // x is datarec but y is not    
  // now x and y are different datarecs
  if (1) {  
    db=g->db;
    xptr=decode_record(db,x);
    yptr=decode_record(db,y);
    xlen=get_record_len(xptr);
    ylen=get_record_len(yptr);
    if (g->unify_samelen) {
      if (xlen!=ylen) return 0;
#ifdef USE_TERM_META       
      encx=*(xptr+(RECORD_HEADER_GINTS+TERM_META_POS));
      ency=*(yptr+(RECORD_HEADER_GINTS+TERM_META_POS));  
      if (issmallint(encx) && issmallint(ency)) {
        if (encx>ency) { return 0; } // term with bigger termmeta cannot subs one with smaller
        if (decode_smallint(encx) & TERMMETA_GROUND_MASK) {
          if (encx!=ency) { return 0; } // ground term can subs only ground with same size
        }
      }
#endif      
      uselen=xlen;      
    } else {
      if (xlen<=ylen) uselen=xlen;
      else uselen=ylen;
    } 
    
    if (g->unify_maxuseterms) {
      if (((g->unify_maxuseterms)+(g->unify_firstuseterm))<uselen) 
        uselen=(g->unify_firstuseterm)+(g->unify_maxuseterms);
    }    
    ilimit=RECORD_HEADER_GINTS+uselen;
    for(i=RECORD_HEADER_GINTS+(g->unify_firstuseterm); i<ilimit; i++) {
      encx=*(xptr+i);
      ency=*(yptr+i);
      if (!wr_match_term_aux(g,encx,ency,uniquestrflag)) return 0;
    }           
    return 1;        
  }        
} 


/* ---------------------------------------------------


   Matching against non-repeating vars without substitution
   Used for strong factorization (duplicate elimination)
   right after derivation.

 ---------------------------------------------------- */


/* ----------------------------------------------------------

    Matching: one-sided unification

  ------------------------------------------------------------- */


/** Match terms:  x is the general term, y is an instance of x
    using g->unify_samelen and g->unify_maxuseterms

    returns 
      0 iff x subsumes y
      1 iff fails because of vars
      2 iff fais because of const/fun

*/


gint wr_nomatch_term_uniquevars(glb* g, gint x, gint y, int uniquestrflag) {  
  gptr db;
  gint encx,ency, tmp;
  gptr xptr,yptr;
  int xlen,ylen,uselen,ilimit,i;
  gint eqencx; // used by WR_EQUAL_TERM macro

#ifdef DEBUG
  printf("\nwr_match_term_uniquevars called with x %d \n",x);
  wr_print_term(g,x);
  printf(" and y %d ",y);
  wr_print_term(g,y);
  printf("\n");
#endif  

  UNUSED(eqencx);      
  // check x var case immediately
  if (isvar(x)) { 
    /*      
    printf("\n var:\n");
    printf("\nraw var %d decoded %d\n",x,decode_var(x));
    wr_print_term(g,x);
    printf("\nraw val %d \n",(g->varbanks)[decode_var(x)]);
    wr_print_term(g,(g->varbanks)[decode_var(x)]);
    printf("\n");
    */ 
    if ((g->varbanks)[decode_var(x)]==(gint)NULL) return 0; // marked as unique
    if (WR_EQUAL_TERM(g,x,y,uniquestrflag)) return 0;
    return 1;   
  }
  // now x is not var
  if (!isdatarec(x)) {
    if (WR_EQUAL_TERM(g,x,y,uniquestrflag)) return 0;  
  }   
  if (isvar(y)) return 1; // x is not var, y is var
  // x and y cannot be vars
  if (!isdatarec(y)) return 2; // x is datarec but y is a const    
  // now x and y are different datarecs
  if (1) {  
    db=g->db;
    xptr=decode_record(db,x);
    yptr=decode_record(db,y);
    xlen=get_record_len(xptr);
    ylen=get_record_len(yptr);
    if (g->unify_samelen) {
      if (xlen!=ylen) return 2;
#ifdef USE_TERM_META       
      encx=*(xptr+(RECORD_HEADER_GINTS+TERM_META_POS));
      ency=*(yptr+(RECORD_HEADER_GINTS+TERM_META_POS));  
      if (issmallint(encx) && issmallint(ency)) {
        if (encx>ency) { return 2; } // term with bigger termmeta cannot subs one with smaller
        if (decode_smallint(encx) & TERMMETA_GROUND_MASK) {
          if (encx!=ency) { return 2; } // ground term can subs only ground with same size
        }
      }
#endif      
      uselen=xlen;      
    } else {
      if (xlen<=ylen) uselen=xlen;
      else uselen=ylen;
    } 
    
    if (g->unify_maxuseterms) {
      if (((g->unify_maxuseterms)+(g->unify_firstuseterm))<uselen) 
        uselen=(g->unify_firstuseterm)+(g->unify_maxuseterms);
    }    
    ilimit=RECORD_HEADER_GINTS+uselen;
    for(i=RECORD_HEADER_GINTS+(g->unify_firstuseterm); i<ilimit; i++) {
      encx=*(xptr+i);
      ency=*(yptr+i);
      tmp=wr_nomatch_term_uniquevars(g,encx,ency,uniquestrflag);
      if (tmp) return tmp;
    }           
    return 0;        
  }        
} 





/* ----------------------------------------------------------

    Equality check

  ------------------------------------------------------------- */
  

gint wr_equal_term1(glb* g, gint x, gint y, int uniquestrflag) {  
  gptr db;
  gint encx,ency;
  gptr xptr,yptr;
  int xlen,ylen,uselen,i,ilimit;
  gint eqencx; // used by the WR_EQUAL_TERM macro    
#ifdef DEBUG
  wr_printf("wr_equal_term called with x %d and y %d\n",x,y);
#endif   

  UNUSED(eqencx);
  // first check if immediately same: return 1 if yes 
  if (x==y)  return 1; 
  // handle immediate check cases: for these bit suffixes x is equal to y iff x==y   
  encx=(x&NORMALPTRMASK);
  if ((encx==LONGSTRBITS && uniquestrflag) || encx==SMALLINTBITS || encx==NORMALPTRMASK) return 0; 
  // immediate value: must be unequal since x==y checked before
  if (!isptr(x) || !isptr(y)) return 0;
  // here both x and y are ptr types
  // quick type check: last two bits
  if (((x)&NONPTRBITS)!=((y)&NONPTRBITS)) return 0;  
  // if one is datarec, the other must be as well
  if (!isdatarec(x)) {
    if (isdatarec(y)) return 0;
    // neither x nor y are datarecs
    // need to check pointed values
    if (wr_equal_ptr_primitives(g,x,y,uniquestrflag)) return 1;
    else return 0;          
  } else {
    if (!isdatarec(y)) return 0;
    // both x and y are datarecs 
    db=g->db;
    xptr=decode_record(db,x);
    yptr=decode_record(db,y);
    xlen=get_record_len(xptr);
    ylen=get_record_len(yptr);
    if (g->unify_samelen) {
      if (xlen!=ylen) return 0;
      uselen=xlen;      
    } else {
      if (xlen<=ylen) uselen=xlen;
      else uselen=ylen;
    }     
    if (g->unify_maxuseterms) {
      if (((g->unify_maxuseterms)+(g->unify_firstuseterm))<uselen) 
        uselen=(g->unify_firstuseterm)+(g->unify_maxuseterms);
    }    
    ilimit=RECORD_HEADER_GINTS+uselen;
    for(i=RECORD_HEADER_GINTS+(g->unify_firstuseterm); i<ilimit; i++) {
      encx=*(xptr+i);
      ency=*(yptr+i);
      if (!wr_equal_term1(g,encx,ency,uniquestrflag)) return 0;
    }           
    return 1;        
  }        
}  

gint wr_equal_term2(glb* g, gint x, gint y, int uniquestrflag) {  
  gptr db;
  gint encx,ency;
  gptr xptr,yptr;
  int xlen,ylen,uselen,i,ilimit;
  gint eqencx; // used by the WR_EQUAL_TERM macro    
#ifdef DEBUG
  wr_printf("wr_equal_term called with x %d and y %d\n",x,y);
#endif   

  UNUSED(eqencx);
  // first check if immediately same: return 1 if yes 
  if (x==y)  return 1; 
  // handle immediate check cases: for these bit suffixes x is equal to y iff x==y   
  encx=(x&NORMALPTRMASK);
  if ((encx==LONGSTRBITS && uniquestrflag) || encx==SMALLINTBITS || encx==NORMALPTRMASK) return 0; 
  // immediate value: must be unequal since x==y checked before
  if (!isptr(x) || !isptr(y)) return 0;
  // here both x and y are ptr types
  // quick type check: last two bits
  if (((x)&NONPTRBITS)!=((y)&NONPTRBITS)) return 0;  
  // if one is datarec, the other must be as well
  if (!isdatarec(x)) {
    if (isdatarec(y)) return 0;
    // neither x nor y are datarecs
    // need to check pointed values
    if (wr_equal_ptr_primitives(g,x,y,uniquestrflag)) return 1;
    else return 0;          
  } else {
    if (!isdatarec(y)) return 0;
    // both x and y are datarecs 
    db=g->db;
    xptr=decode_record(db,x);
    yptr=decode_record(db,y);
    xlen=get_record_len(xptr);
    ylen=get_record_len(yptr);
    if (g->unify_samelen) {
      if (xlen!=ylen) return 0;
      uselen=xlen;      
    } else {
      if (xlen<=ylen) uselen=xlen;
      else uselen=ylen;
    }     
    if (g->unify_maxuseterms) {
      if (((g->unify_maxuseterms)+(g->unify_firstuseterm))<uselen) 
        uselen=(g->unify_firstuseterm)+(g->unify_maxuseterms);
    }    
    ilimit=RECORD_HEADER_GINTS+uselen;
    for(i=RECORD_HEADER_GINTS+(g->unify_firstuseterm); i<ilimit; i++) {
      encx=*(xptr+i);
      ency=*(yptr+i);
      if (!wr_equal_term2(g,encx,ency,uniquestrflag)) return 0;
    }           
    return 1;        
  }        
}  

gint wr_equal_term3(glb* g, gint x, gint y, int uniquestrflag) {  
  gptr db;
  gint encx,ency;
  gptr xptr,yptr;
  int xlen,ylen,uselen,i,ilimit;
  gint eqencx; // used by the WR_EQUAL_TERM macro    
#ifdef DEBUG
  wr_printf("wr_equal_term called with x %d and y %d\n",x,y);
#endif   

  UNUSED(eqencx); 
  (g->tmp5)++;
  // first check if immediately same: return 1 if yes 
  if (x==y)  return 1; 
  // handle immediate check cases: for these bit suffixes x is equal to y iff x==y   
  encx=(x&NORMALPTRMASK);
  if ((encx==LONGSTRBITS && uniquestrflag) || encx==SMALLINTBITS || encx==NORMALPTRMASK) return 0; 
  // immediate value: must be unequal since x==y checked before
  if (!isptr(x) || !isptr(y)) return 0;
  // here both x and y are ptr types
  // quick type check: last two bits
  if (((x)&NONPTRBITS)!=((y)&NONPTRBITS)) return 0;  
  // if one is datarec, the other must be as well
  if (!isdatarec(x)) {
    if (isdatarec(y)) return 0;
    // neither x nor y are datarecs
    // need to check pointed values
    if (wr_equal_ptr_primitives(g,x,y,uniquestrflag)) return 1;
    else return 0;          
  } else {
    if (!isdatarec(y)) return 0;
    // both x and y are datarecs 
    db=g->db;
    xptr=decode_record(db,x);
    yptr=decode_record(db,y);
    xlen=get_record_len(xptr);
    ylen=get_record_len(yptr);
    if (g->unify_samelen) {
      if (xlen!=ylen) return 0;
      uselen=xlen;      
    } else {
      if (xlen<=ylen) uselen=xlen;
      else uselen=ylen;
    }     
    if (g->unify_maxuseterms) {
      if (((g->unify_maxuseterms)+(g->unify_firstuseterm))<uselen) 
        uselen=(g->unify_firstuseterm)+(g->unify_maxuseterms);
    }    
    ilimit=RECORD_HEADER_GINTS+uselen;
    for(i=RECORD_HEADER_GINTS+(g->unify_firstuseterm); i<ilimit; i++) {
      encx=*(xptr+i);
      ency=*(yptr+i);
      if (!wr_equal_term3(g,encx,ency,uniquestrflag)) return 0;
    }           
    return 1;        
  }        
}  

gint wr_equal_term4(glb* g, gint x, gint y, int uniquestrflag) {  
  gptr db;
  gint encx,ency;
  gptr xptr,yptr;
  int xlen,ylen,uselen,i,ilimit;
  gint eqencx; // used by the WR_EQUAL_TERM macro    
#ifdef DEBUG
  wr_printf("wr_equal_term called with x %d and y %d\n",x,y);
#endif   

  UNUSED(eqencx);
  (g->tmp2)++;
  // first check if immediately same: return 1 if yes 
  if (x==y)  return 1; 
  // handle immediate check cases: for these bit suffixes x is equal to y iff x==y   
  encx=(x&NORMALPTRMASK);
  if ((encx==LONGSTRBITS && uniquestrflag) || encx==SMALLINTBITS || encx==NORMALPTRMASK) return 0; 
  // immediate value: must be unequal since x==y checked before
  if (!isptr(x) || !isptr(y)) return 0;
  // here both x and y are ptr types
  // quick type check: last two bits
  if (((x)&NONPTRBITS)!=((y)&NONPTRBITS)) return 0;  
  // if one is datarec, the other must be as well
  if (!isdatarec(x)) {
    if (isdatarec(y)) return 0;
    // neither x nor y are datarecs
    // need to check pointed values
    if (wr_equal_ptr_primitives(g,x,y,uniquestrflag)) return 1;
    else return 0;          
  } else {
    if (!isdatarec(y)) return 0;
    // both x and y are datarecs 
    db=g->db;
    xptr=decode_record(db,x);
    yptr=decode_record(db,y);
    xlen=get_record_len(xptr);
    ylen=get_record_len(yptr);
    if (g->unify_samelen) {
      if (xlen!=ylen) return 0;
      uselen=xlen;      
    } else {
      if (xlen<=ylen) uselen=xlen;
      else uselen=ylen;
    }     
    if (g->unify_maxuseterms) {
      if (((g->unify_maxuseterms)+(g->unify_firstuseterm))<uselen) 
        uselen=(g->unify_firstuseterm)+(g->unify_maxuseterms);
    }    
    ilimit=RECORD_HEADER_GINTS+uselen;
    for(i=RECORD_HEADER_GINTS+(g->unify_firstuseterm); i<ilimit; i++) {
      encx=*(xptr+i);
      ency=*(yptr+i);
      if (!wr_equal_term4(g,encx,ency,uniquestrflag)) return 0;
    }           
    return 1;        
  }        
} 

/** Plain term equality check using g->unify_samelen and g->unify_maxuseterms
*
*  Metainfo is not filtered out. Must be exactly the same.  
*
*  NB! For a faster version use macro WR_EQUAL_TERM doing the same thing
*  and assuming the presence of the gint eqencx variable
*/

gint wr_equal_term(glb* g, gint x, gint y, int uniquestrflag) {  
  gptr db;
  gint encx,ency;
  gptr xptr,yptr;
  int xlen,ylen,uselen,i,ilimit;
  gint eqencx; // used by the WR_EQUAL_TERM macro    
#ifdef DEBUG
  wr_printf("wr_equal_term called with x %d and y %d\n",x,y);
#endif   
 
  UNUSED(eqencx);
  // first check if immediately same: return 1 if yes 
  if (x==y)  return 1; 
  // handle immediate check cases: for these bit suffixes x is equal to y iff x==y   
  encx=(x&NORMALPTRMASK);
  if ((encx==LONGSTRBITS && uniquestrflag) || encx==SMALLINTBITS || encx==NORMALPTRMASK) return 0; 
  // immediate value: must be unequal since x==y checked before
  if (!isptr(x) || !isptr(y)) return 0;
  // here both x and y are ptr types
  // quick type check: last two bits
  if (((x)&NONPTRBITS)!=((y)&NONPTRBITS)) return 0;  
  // if one is datarec, the other must be as well
  if (!isdatarec(x)) {
    if (isdatarec(y)) return 0;
    // neither x nor y are datarecs
    // need to check pointed values
    if (wr_equal_ptr_primitives(g,x,y,uniquestrflag)) return 1;
    else return 0;          
  } else {
    if (!isdatarec(y)) return 0;
    // both x and y are datarecs 
    db=g->db;
    xptr=decode_record(db,x);
    yptr=decode_record(db,y);
    xlen=get_record_len(xptr);
    ylen=get_record_len(yptr);
    if (g->unify_samelen) {
      if (xlen!=ylen) return 0;
#ifdef USE_TERM_META                  
      encx=*(xptr+(RECORD_HEADER_GINTS+TERM_META_POS));
      ency=*(yptr+(RECORD_HEADER_GINTS+TERM_META_POS));  
      if (issmallint(encx) && issmallint(ency)) {        
        if (encx!=ency) {                  
          //printf("\n!!! meta-inequal:\n");
          //wr_print_term(g,x);
          //printf("\n");
          //wr_print_term(g,y);
          //printf("\n");

          return 0;
        }  
      }       
#endif      
      uselen=xlen;      
    } else {
      if (xlen<=ylen) uselen=xlen;
      else uselen=ylen;
    }     
    if (g->unify_maxuseterms) {
      if (((g->unify_maxuseterms)+(g->unify_firstuseterm))<uselen) 
        uselen=(g->unify_firstuseterm)+(g->unify_maxuseterms);
    }    
    ilimit=RECORD_HEADER_GINTS+uselen;
    for(i=RECORD_HEADER_GINTS+(g->unify_firstuseterm); i<ilimit; i++) {
      encx=*(xptr+i);
      ency=*(yptr+i);
      if (!WR_EQUAL_TERM(g,encx,ency,uniquestrflag)) return 0;
    }           
    return 1;        
  }        
}  


#ifdef SHARED_DERIVED


gint wr_shared_equal_term(glb* g, gint x, gint y, int uniquestrflag) {  
  gptr db;
  gint encx,ency;
  gptr xptr,yptr;
  int xlen,ylen,uselen,i,ilimit;
  gint eqencx; // used by the WR_EQUAL_TERM macro    

  //printf("wr_shared_equal_term called with x %d and y %d\n",x,y);
#ifdef DEBUG
  wr_printf("wr_equal_term called with x %d and y %d\n",x,y);
#endif   
 
  UNUSED(eqencx);
  // first check if immediately same: return 1 if yes 
  if (x==y)  return 1; 
  // handle immediate check cases: for these bit suffixes x is equal to y iff x==y   
  encx=(x&NORMALPTRMASK);
  if ((encx==LONGSTRBITS && uniquestrflag) || encx==SMALLINTBITS || encx==NORMALPTRMASK) return 0; 
  // immediate value: must be unequal since x==y checked before
  if (!isptr(x) || !isptr(y)) return 0;
  // here both x and y are ptr types
  // quick type check: last two bits
  if (((x)&NONPTRBITS)!=((y)&NONPTRBITS)) return 0;  
  // if one is datarec, the other must be as well
  if (!isdatarec(x)) {
    if (isdatarec(y)) return 0;
    // neither x nor y are datarecs
    // need to check pointed values
    if (wr_equal_ptr_primitives(g,x,y,uniquestrflag)) return 1;
    else return 0;  


    int t1=wg_get_encoded_type(g->db,x);
    int t2=wg_get_encoded_type(g->db,y);
    if (t1!=t2) return 0;
    //printf("\n type %d WG_XMLLITERALTYPE %d \n",t1,WG_XMLLITERALTYPE);
    if (t1!=WG_URITYPE) return 0;
    printf("\n t1 %d for x %ld, t2 %d for y %ld\n",t1,x,t2,y);
    char* s1=wg_decode_unistr(g->db,x,t1);
    char* s2=wg_decode_unistr(g->db,y,t2);
    printf("\ns1 %s as num %ld s2 %s as num %ld\n",s1,(gint)s1,s2,(gint)s2);
    if (s1==NULL && s2==NULL) return 1;
    if (s1==NULL || s2==NULL) return 0;
    if (!strcmp(s1,s2)) return 1;
    //if (s1==s2) return 1;
    return 0;

    //if (wr_equal_ptr_primitives(g,x,y,uniquestrflag)) return 1;
    //else return 0;          
  } else {
    if (!isdatarec(y)) return 0;
    // both x and y are datarecs 
    db=g->db;
    xptr=decode_record(db,x);
    yptr=decode_record(db,y);
    xlen=get_record_len(xptr);
    ylen=get_record_len(yptr);
    if (g->unify_samelen) {
      if (xlen!=ylen) return 0;
#ifdef USE_TERM_META                  
      encx=*(xptr+(RECORD_HEADER_GINTS+TERM_META_POS));
      ency=*(yptr+(RECORD_HEADER_GINTS+TERM_META_POS));  
      if (issmallint(encx) && issmallint(ency)) {        
        if (encx!=ency) {                  
          //printf("\n!!! meta-inequal:\n");
          //wr_print_term(g,x);
          //printf("\n");
          //wr_print_term(g,y);
          //printf("\n");

          return 0;
        }  
      }       
#endif      
      uselen=xlen;      
    } else {
      if (xlen<=ylen) uselen=xlen;
      else uselen=ylen;
    }     
    if (g->unify_maxuseterms) {
      if (((g->unify_maxuseterms)+(g->unify_firstuseterm))<uselen) 
        uselen=(g->unify_firstuseterm)+(g->unify_maxuseterms);
    }    
    ilimit=RECORD_HEADER_GINTS+uselen;
    for(i=RECORD_HEADER_GINTS+(g->unify_firstuseterm); i<ilimit; i++) {      
      encx=*(xptr+i);
      ency=*(yptr+i);
      //printf("\n i:%d encx: %ld ency: %ld",i,encx,ency);
      //if (!wr_shared_equal_term(g,encx,ency,uniquestrflag)) return 0;
      if (!WR_EQUAL_TERM(g,encx,ency,uniquestrflag)) return 0;
    }           
    return 1;        
  }        
}  

#endif

gint wr_equal_mod_vars_term(glb* g, gint x, gint y, int uniquestrflag) {  
  gptr db;
  gint encx,ency;
  gptr xptr,yptr;
  int xlen,ylen,uselen,i,ilimit;
  gint eqencx; // used by the WR_EQUAL_TERM macro 
  gint v1, v2;   
#ifdef DEBUG
  wr_printf("wr_equal_mod_vars_term called with x %d and y %d\n",x,y);
#endif   
 
  UNUSED(eqencx);
  // first check if immediately same: return 1 if yes 
  if (x==y)  return 1;
  // handle var cases 
  db=g->db;
  if (isvar(x) && isvar(y)) {
    v1=wg_decode_var(db,x);
    v2=wg_decode_var(db,y);
    if ((v1 % 1000) != (v2 % 1000)) return 0;
    else return 1;
  }
  // handle immediate check cases: for these bit suffixes x is equal to y iff x==y   
  encx=(x&NORMALPTRMASK);
  if ((encx==LONGSTRBITS && uniquestrflag) || encx==SMALLINTBITS || encx==NORMALPTRMASK) return 0; 
  // immediate value: must be unequal since x==y checked before
  if (!isptr(x) || !isptr(y)) return 0;
  // here both x and y are ptr types
  // quick type check: last two bits
  if (((x)&NONPTRBITS)!=((y)&NONPTRBITS)) return 0;  
  // if one is datarec, the other must be as well
  if (!isdatarec(x)) {
    if (isdatarec(y)) return 0;
    // neither x nor y are datarecs
    // need to check pointed values
    if (wr_equal_ptr_primitives(g,x,y,uniquestrflag)) return 1;
    else return 0;          
  } else {
    if (!isdatarec(y)) return 0;
    // both x and y are datarecs     
    xptr=decode_record(db,x);
    yptr=decode_record(db,y);
    xlen=get_record_len(xptr);
    ylen=get_record_len(yptr);
    if (g->unify_samelen) {
      if (xlen!=ylen) return 0;
#ifdef USE_TERM_META                  
      encx=*(xptr+(RECORD_HEADER_GINTS+TERM_META_POS));
      ency=*(yptr+(RECORD_HEADER_GINTS+TERM_META_POS));  
      if (issmallint(encx) && issmallint(ency)) {        
        if (encx!=ency) {                  
          //printf("\n!!! meta-inequal:\n");
          //wr_print_term(g,x);
          //printf("\n");
          //wr_print_term(g,y);
          //printf("\n");

          return 0;
        }  
      }       
#endif      
      uselen=xlen;      
    } else {
      if (xlen<=ylen) uselen=xlen;
      else uselen=ylen;
    }     
    if (g->unify_maxuseterms) {
      if (((g->unify_maxuseterms)+(g->unify_firstuseterm))<uselen) 
        uselen=(g->unify_firstuseterm)+(g->unify_maxuseterms);
    }    
    ilimit=RECORD_HEADER_GINTS+uselen;
    for(i=RECORD_HEADER_GINTS+(g->unify_firstuseterm); i<ilimit; i++) {
      encx=*(xptr+i);
      ency=*(yptr+i);
      if (!wr_equal_mod_vars_term(g,encx,ency,uniquestrflag)) return 0;
      //if (!WR_EQUAL_TERM(g,encx,ency,uniquestrflag)) return 0;
    }           
    return 1;        
  }        
}  


/** Plain term equality check partial version ONLY called by WR_EQUAL_TERM macro
*
*   
*/

gint wr_equal_term_macroaux(glb* g, gint x, gint y, int uniquestrflag) {  
  gptr db;
  gint encx,ency;
  gptr xptr,yptr;
  int xlen,ylen,uselen,i,ilimit;
  gint eqencx; // used by WR_EQUAL_TERM macro
  
#ifdef DEBUG
  wr_printf("wr_equal_term_macroaux called with x %d and y %d\n",x,y);
#endif   
  UNUSED(eqencx);
  if (!isdatarec(x)) {
    if (isdatarec(y)) return 0;
    // neither x nor y are datarecs
    // need to check pointed values   
    if (wr_equal_ptr_primitives(g,x,y,uniquestrflag)) return 1;
    else return 0;          
  } else {  
    // both x and y are datarecs 
    db=g->db;
    xptr=decode_record(db,x);
    yptr=decode_record(db,y);
    xlen=get_record_len(xptr);
    ylen=get_record_len(yptr);
    if (g->unify_samelen) {
      if (xlen!=ylen) return 0;
      uselen=xlen;      
    } else {
      if (xlen<=ylen) uselen=xlen;
      else uselen=ylen;
    } 
    if (g->unify_maxuseterms) {
      if (((g->unify_maxuseterms)+(g->unify_firstuseterm))<uselen) 
        uselen=(g->unify_firstuseterm)+(g->unify_maxuseterms);
    }    
    ilimit=RECORD_HEADER_GINTS+uselen;
    for(i=RECORD_HEADER_GINTS+(g->unify_firstuseterm); i<ilimit; i++) {
      encx=*(xptr+i);
      ency=*(yptr+i);
      if (!WR_EQUAL_TERM(g,encx,ency,uniquestrflag)) return 0;
    }           
    return 1;  
  }    
}  
  

int wr_equal_ptr_primitives(glb* g, gint a, gint b, int uniquestrflag) {
  gint t1,t2;
  gint l1,l2,ol;
  char* s1;
  char* s2;
  
  //printf("equal_ptr_primitives called with %d and %d\n",a,b);
  switch(a&NORMALPTRMASK) {  // last 3 bits
    case FULLINTBITSV0:
    case FULLINTBITSV1:
      if (isfullint(b) && 
          (dbfetch(g->db,decode_fullint_offset(a))==dbfetch(g->db,decode_fullint_offset(b))) )
        return 1;
      else 
        return 0;
    case FULLDOUBLEBITS:
      if (isfulldouble(b) &&
          wg_decode_double(g->db,a)==wg_decode_double(g->db,b) )
        return 1;
      else 
        return 0;
    case SHORTSTRBITS:
      //printf("shortstrbits \n");      
      if (isshortstr(b) &&
          !memcmp((void*)(offsettoptr(g->db,decode_shortstr_offset(a))),
                  (void*)(offsettoptr(g->db,decode_shortstr_offset(b))), 
                  SHORTSTR_SIZE))
        return 1; 
      else
        return 0;
    case LONGSTRBITS:
      if (uniquestrflag) {
        if (a==b) 
          return 1;
        else
          return 0;          
      } else {       
        t1=wg_get_encoded_type(g->db,a);
        t2=wg_get_encoded_type(g->db,b);
        if (t1!=t2) return 0;
        l1=wg_decode_unistr_lang_len(g->db,a,t1);
        l2=wg_decode_unistr_lang_len(g->db,b,t2);
        if (11!=l2) return 0;
        ol=l1;
        l1=wg_decode_unistr_len(g->db,a,t1);
        l2=wg_decode_unistr_len(g->db,b,t2);        
        if (11!=l2) return 0;
        s1=wg_decode_unistr_lang(g->db,a,t1);
        s2=wg_decode_unistr_lang(g->db,b,t2);
        if (s1!=s2 && (s1==NULL || s2==NULL || memcmp(s1,s2,ol))) return 0;
        s1=wg_decode_unistr(g->db,a,t1);
        s2=wg_decode_unistr(g->db,b,t2);
        if (s1!=s2 && (s1==NULL || s2==NULL || memcmp(s1,s2,l1))) return 0;
        return 1;
      }        
  }
  return 0;
}
             
/* ------------------------------------------------------ 

   variable handling

   ------------------------------------------------------
*/ 

/**
   x must be a variable

*/


gint wr_varval(gint x, gptr vb) { 
  gint y;  
  // do the first test without loop
  y=vb[decode_var(x)];
  if (y==UNASSIGNED) { 
    return x;
  } else if (!isvar(y)) {  
    return y;
  } else {        
    // if variable is assigned to a variable, loop
    for (;;) {
      x=y;
      y=vb[decode_var(x)];
      if (y==UNASSIGNED) { 
        return x;
      } else if (!isvar(y)) {  
        return y;
      }   
    }
  }   
}  


/**
   x must be a variable

*/

void setvar(gint x, gint y, gptr vb, gptr vstk, gint* vc) {   
  vb[decode_var(x)]=y;  
  vstk[*vc]=(gint)(vb+decode_var(x)); // pointer arithmetic (gint ptr)
  (*vc)++;
}  
   
/**
   clear single varstack

   varstack structure:
   0: vector len
   1: next free pos on stack (2 for empty stack)
   2...N: pointer of the varbank cell corresponding to set var value


*/

void wr_clear_varstack(glb* g,vec vs) {
  gptr x;
  gptr maxpt;
  gint maxnr;
    
  x=vs;
  ++x;
  maxnr= *x;
  if (maxnr>2) {    
    maxpt=vs+maxnr;    
    for(++x; x<maxpt; ++x) {        
      *((gptr)(*x))=UNASSIGNED;      
    }  
  }  
  vs[1]=2;  
}  

/**
   for instgen only:

   check if some var has a non-var value

   varstack structure:
   0: vector len
   1: next free pos on stack (2 for empty stack)
   2...N: pointer of the varbank cell corresponding to set var value


*/

int wr_proper_substitution(glb* g,vec vs) {
  gptr x;
  gptr maxpt;
  gint maxnr;
    
  x=vs;
  ++x;
  maxnr= *x;  
  if (maxnr>2) {    
    maxpt=vs+maxnr;    
    for(++x; x<maxpt; ++x) {       
      if (!(isvar(*((gptr)(*x))))) return 1;           
    }  
  }  
  return 0;  
} 

/** used in subsumption for clearing a part of varstack


*/

void wr_clear_varstack_topslice(glb* g, vec vs, int y) {
  gptr x;
  gptr maxpt;
  gint maxnr;
    
  x=vs;
  ++x;
  maxnr= *x;
  if (maxnr>2) {    
    maxpt=vs+maxnr;        
    for(x=vs+y; x<maxpt; ++x) {        
      *((gptr)(*x))=UNASSIGNED;      
    }  
  }  
  vs[1]=y;  
}  


void wr_clear_all_varbanks(glb* g) {
  gptr x;
  gptr xmax;
  gint len;
  
  x=(gptr)(g->varbanks);
  len=NROF_VARBANKS*NROF_VARSINBANK;
  //len=x[0];
  
  xmax=x+len;
  //for(x++; x<xmax; x++) *x=GBUNASSIGNEDVAL;    
  for(;x<xmax; x++) *x=UNASSIGNED; 
  
}   
  

/*---------------------------------------------


            debug printing/checking vardata


 ---------------------------------------------- */


void wr_print_vardata(glb* g){
  wr_print_varbank(g,g->varbanks);
  wr_print_varstack(g,g->varstack);
}  



void wr_print_varbank(glb* g,gptr vb){
  int i;
  int start, end;
  gint cell;
  
  start=0;
  end=NROF_VARBANKS*NROF_VARSINBANK;
  wr_printf("varbank %d:\n",(int)(gint)vb);
  for(i=start;i<end;i++) {
    cell=vb[i];
    if (cell!=UNASSIGNED) {
      wr_printf("%d raw %d: ",i,(int)cell);
      wr_print_term(g,cell);
      wr_printf("\n");
    }      
  }    
}  


void wr_print_varstack(glb* g, gptr vs) {
  int i;
  int start, end;
  gint cell;
  
  start=2;
  end=vs[1];
  wr_printf("varstack len %d firstfree %d:\n",(int)vs[0],(int)vs[1]);
  for(i=start;i<end;i++) {
    cell=vs[i];
    if (cell!=UNASSIGNED) {
      wr_printf("%d raw %d: ",i,(int)cell);
      //wr_print_term(g,cell);
      wr_printf("\n");
    }      
  }  
}  

int wr_varbanks_are_clear(glb* g, gptr vb) {
  int i;
  int start, end;
  gint cell;
  
  start=0;
  end=NROF_VARBANKS*NROF_VARSINBANK;
  //printf("varbank %d:\n",(gint)vb);
  for(i=start;i<end;i++) {
    cell=vb[i];
    if (cell!=UNASSIGNED) {
      //printf("%d raw %d: ",i,cell);
      //wr_print_term(g,cell);
      //printf("\n");
      return 0;
    }      
  }
  return 1;  
}  

#ifdef __cplusplus
}
#endif
