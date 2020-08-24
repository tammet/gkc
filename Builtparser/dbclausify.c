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

 /** @file dbclausify.c
 *  Formula clausifier.
 *
 */


/* ====== Includes =============== */


#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include "../Db/dbdata.h"
#include "../Db/dbmem.h"
#include "../Db/dballoc.h"
#include "../Db/dbdata.h"
#include "../Db/dbmpool.h"
#include "../Printer/dbotterprint.h"
#include "../Reasoner/clterm.h"
#include "dbparse.h"
#include "dbgenparse.h"
#include "dbotterparse.h"
#include "dbprologparse.h"

#include "dbparse.h"
#include "dbclausify.h"
               
/* ====== Private headers and defs ======== */


//#undef DEBUG
//#define DEBUG
//#define TDEBUG

//static void* show_clausify_error(glb* g, char* format, ...);
static void* show_clausify_warning(glb* g, char* format, ...);

#define VARCOLLECT_SIZE 1000

#define MAKEDEF_COMPLEXITY_THRESHOLD 10 // NORMAL 10, TESTING 100

#define PRINTERR

/* ======== Data ========================= */



/* ====== Private protos ======== */



/* ====== Functions ============== */


void* wr_clausify_formula(glb* g, void* mpool, void* frm, 
      void* clrole, void* skname) {
  void* db=g->db;
  void* head;
  void* res;
  void* defs=NULL;
  int varnr=0;
  void* op;
  void* skclause;
  gint cell;
  gcell *cellptr;
  int clauseflag=0;
  void* freevars=NULL;

#ifdef TDEBUG   
  printf("wr_clausify_formula starting with frm\n");  
  wg_mpool_print(db,frm); 
  printf("\n");
  wg_tptp_print(db,frm); 
  printf("\n");
#endif  
  if (g->parse_errflag) return NULL;
  if (wg_isatom(db,frm)) {
    // constant
    return frm;
  }  
  head=wg_first(db,frm);
  if (!wg_ispair(db,head) && !wg_islogconnective(db,head)) {
    // nonlogical term
    return frm;
  }
  // collect freevars
  freevars=wr_parse_freeoccs(g,mpool,NULL,frm,&clauseflag);
  /*
  printf("\nfreevars\n");
  wg_mpool_print(db,freevars);
  printf("\n");
  */
  if (freevars) frm=wg_mklist3(db,mpool,wg_makelogall(db,mpool),freevars,frm); 
  if (!wg_isatom(db,head)) {
    // first el is a list itself
    if (!wg_rest(db,frm)) {
      // pointless parenthesis around formula: lift up
      return wr_clausify_formula(g,mpool,head,clrole,skname);
    } else {
      // seems to be wrong syntax
      return show_clausify_warning(g,"formula has a complex term leading a term");     
    }
  }
  // here we have a logical formula with head being connector
  res=wr_clausify_negpush(g,mpool,frm,1);
  if (g->parse_errflag) return NULL;
#ifdef TDEBUG  
  printf("\nwr_clausify_negpush ending with res\n");  
  wg_mpool_print(db,res); 
  printf("\n\n");
#endif
  res=wr_clausify_miniscope(g,mpool,res);
  if (g->parse_errflag) return NULL;
#ifdef TDEBUG  
  printf("\nwr_clausify_miniscope ending with res\n");  
  wg_mpool_print(db,res); 
  printf("\n\n");
#endif 
  //exit(0);
  res=wr_clausify_skolemize(g,mpool,res,NULL,&varnr);
  if (g->parse_errflag) return NULL;
#ifdef STORE_SKOLEM_STEPS  
  if (g->store_fof_skolem_steps) {
    if (res && skname) {
      skclause=wg_mklist4(db,mpool,NULL,skname,clrole,res);         
      if (g->parse_errflag) return NULL;
      cell=alloc_listcell(db);
      if (!cell) {
        //wr_show_parse_error(g,"failed to allocate a cell for storing skolemized formula");        
        return NULL;
      }  
      cellptr = (gcell *) offsettoptr(db, cell);             
      (cellptr->car) = (gint)skclause; //ptrtooffset(db, res);
      (cellptr->cdr) = (dbmemsegh(db)->infrmlist);
      (dbmemsegh(db)->infrmlist) = cell; 
    }
  }  
#endif  
#ifdef TDEBUG  
  printf("\nwr_clausify_skolemize ending with res and varnr %d sknr %d\n",
     varnr,dbmemsegh(db)->parse_skolem_nr);  
  wg_mpool_print(db,res); 
  printf("\nstarting to distribute\n");
#endif
  res=wr_clausify_distribute(g,mpool,res,&defs,0);
  if (g->parse_errflag) return NULL;
#ifdef TDEBUG
  printf("\nwr_clausify_formula ending with res\n");  
  wg_mpool_print(db,res); 
  printf("\n\n");
  printf("\nand defs\n");  
  wg_mpool_print(db,defs); 
  printf("\n\n");
#endif  
  if (res && defs) {
    op=wg_makelogand(db,mpool);
    res=wg_mklist3(db,mpool,op,res,defs);
    if (!op || !res) return show_clausify_warning(g,"cannot create a new list");
  }
#ifdef TDEBUG
  printf("\nfinal res\n");  
  wg_mpool_print(db,res); 
  printf("\n\n");
  wg_tptp_print(db,res); 
  printf("\n");
#endif    
  return res;  
}


/* 
   Push negation in and convert 
     implication -> 
     equivalence <->
     xor +
   to disjunctions and conjunctions.
   Takes:
      frm: a tree like ["-",["&",["&",["->","a",2],"a"],["-",2]]]
      sign: true or false 
   Returns:
      a new tree containing only & and V and the negation pushed to variables.
*/

void* wr_clausify_negpush(glb* g, void* mpool, void* frm, int sign) {
  void* db=g->db;
  void *res, *op, *newop, *newop2;
  void *part, *tmp;
  void *arg, *arg1, *arg2, *arg1cp, *arg2cp; 
  void *res1, *res2, *res3, *res4, *res5, *res6; 
  int len;  

  //printf("wr_clausify_negpush starting with sign %d and frm\n",sign);  
  //wg_mpool_print(db,frm); 
  //printf("\n");

  if (wg_isatom(db,frm)) {
    // constant
    if (sign) return frm;
    else return wg_mklist2(db,mpool,wg_makelogneg(db,mpool),frm);
  }  
  op=wg_first(db,frm);
  if (wg_ispair(db,op) && !wg_rest(db,frm)) {
    // pointless parenthesis around formula: lift up
    return wr_clausify_negpush(g,mpool,op,sign);
  }  
  if (!wg_islogconnective(db,op)) {
    // nonlogical term
    if (sign) return frm;
    else return wg_mklist2(db,mpool,wg_makelogneg(db,mpool),frm);
  } 
  // frm is a logical term
  len=wg_list_len(db,frm);
  // must have at least one arg
  if (len<2) return show_clausify_warning(g,"formula contains op with no args");
  if (wg_islogneg(db,op)) {
    arg1=wg_first(db,wg_rest(db,frm));
    if (wg_isatom(db,arg1) || 
        (wg_ispair(db,arg1) && 
         wg_isatom(db,wg_first(db,arg1)) &&
         !wg_islogconnective(db,wg_first(db,arg1))) ) {
      // nonlogical argument to negation
      if (sign) return frm;
      else return arg1;
    } else {      
      // logical argument to negation
      return wr_clausify_negpush(g,mpool,arg1,!sign);
    }  
  }  
  // we have a binary operator: must have at least two args
  if (len<3) return show_clausify_warning(g,"binary op in a formula has less than 2 args");
  if (wg_islogand(db,op) || wg_islogor(db,op)) {
    // op is and / or 
    // newop changes if negative sign
    if (sign) newop=op;
    else if (wg_islogand(db,op)) newop=wg_makelogor(db,mpool);
    else newop=wg_makelogand(db,mpool);
    res=NULL;
    for(part=wg_rest(db,frm); part!=NULL; part=wg_rest(db,part)) {
      arg=wg_first(db,part);
      tmp=wr_clausify_negpush(g,mpool,arg,sign);
      res=wg_mkpair(db,mpool,tmp,res);
      if (!tmp || !res) return show_clausify_warning(g,"memory pool full in negation push");
    }
    res=wg_mkpair(db,mpool,newop,res);
    if (!newop || !res) return show_clausify_warning(g,"memory pool full in negation push");
    return res;
  } else if (wg_islogimp(db,op)) {
    // op is =>
    if (sign) newop=wg_makelogor(db,mpool);
    else newop=wg_makelogand(db,mpool);   
    res1=NULL;
    for(part=wg_rest(db,frm); part!=NULL; part=wg_rest(db,part)) {
      arg=wg_first(db,part);
      if (!wg_rest(db,part)) {
        // last elem: implication result
        tmp=wr_clausify_negpush(g,mpool,arg,sign);
        res1=wg_mkpair(db,mpool,tmp,res1);         
      } else {
        // not a last elem: implication argument
        tmp=wr_clausify_negpush(g,mpool,arg,!sign);        
        res1=wg_mkpair(db,mpool,tmp,res1);
        if (!tmp || !res1) return show_clausify_warning(g,"memory pool full in negation push");             
      }     
    }  
    res=wg_mkpair(db,mpool,newop,res1);
    if (!newop || !tmp || !res || !res1) return show_clausify_warning(g,"memory pool full in negation push");
    return res;
  } else if (wg_islogall(db,op) || wg_islogexists(db,op)) {
    // op is a quantifer all or exists
    if (len!=3) return show_clausify_warning(g,"quantifier in a formula does not have 2 args");
    if (sign) newop=op;
    else if (wg_islogall(db,op)) newop=wg_makelogexists(db,mpool);
    else newop=wg_makelogall(db,mpool);
    arg1=wg_first(db,wg_rest(db,frm));
    arg2=wg_first(db,wg_rest(db,wg_rest(db,frm)));
    res1=wr_clausify_negpush(g,mpool,arg2,sign);
    res=wg_mklist3(db,mpool,newop,arg1,res1);
    if (!newop || !res1 || !res) return show_clausify_warning(g,"memory pool full in negation push");
    return res;  
  } else if (wg_islogeqv(db,op)) {
    // op is <=>
    if (len!=3) return show_clausify_warning(g,"equivalence op in a formula does not have 2 args");
    //in positive case:
    //res=["&",["V",formula_negpush(frm[1],false),
    //               formula_negpush(frm[2],true)],
    //          ["V",formula_negpush(frm[1],true),
    //               formula_negpush(frm[2],false)] ];
    if (sign) {     
      newop=wg_makelogand(db,mpool);
      newop2=wg_makelogor(db,mpool);      
    } else {
      newop=wg_makelogor(db,mpool);
      newop2=wg_makelogand(db,mpool);
    }  
    arg1=wg_first(db,wg_rest(db,frm));
    arg2=wg_first(db,wg_rest(db,wg_rest(db,frm)));
    res1=wr_clausify_negpush(g,mpool,arg1,!sign);
    res2=wr_clausify_negpush(g,mpool,arg2,sign);
    arg1cp=wr_clausify_copy(g,mpool,arg1);
    arg2cp=wr_clausify_copy(g,mpool,arg2);
    if (!arg1cp || !arg2cp) return show_clausify_warning(g,"memory pool full in negation push");
    res3=wr_clausify_negpush(g,mpool,arg1cp,sign);   
    res4=wr_clausify_negpush(g,mpool,arg2cp,!sign);
    res5=wg_mklist3(db,mpool,newop2,res1,res2);
    res6=wg_mklist3(db,mpool,newop2,res3,res4);
    res=wg_mklist3(db,mpool,newop,res5,res6);
    if (!newop || !newop2 || !res1 || !res2 || !res3 || !res4 || !res5 || !res6 || !res) {
      return show_clausify_warning(g,"memory pool full in negation push");
    }  
    return res; 
  } 
  // should not happen
  return show_clausify_warning(g,"unexpected formula structure at a negpush phase");
}

void* wr_clausify_copy(glb* g, void* mpool, void* term) {
  void* db=g->db;
  void *el, *part;
  void *res, *tmp;

#ifdef DEBUG
  printf("\nwr_clausify_copy starting with term\n");  
  wg_mpool_print(db,term); 
  printf("\n");
#endif
  if (!term) return term;
  if (wg_isatom(db,term)) {
    res=term;   
  } else { 
    res=NULL;    
    for(part=term; part!=NULL; part=wg_rest(db,part)) {
      el=wg_first(db,part);    
      if (wg_isatom(db,el)) {
        res=wg_mkpair(db,mpool,el,res);
        if (res==NULL) return show_clausify_warning(g,"failed to copy term");
      } else {
        tmp=wr_clausify_copy(g,mpool,el);
        if (tmp==NULL) return show_clausify_warning(g,"failed to copy term");
        res=wg_mkpair(db,mpool,tmp,res);      
        if (res==NULL) return show_clausify_warning(g,"failed to copy term");
      }
    }
    res=wg_reverselist(db,mpool,res);
  }  
#ifdef DEBUG
  printf("\nwr_clausify_copy ending with res\n");  
  wg_mpool_print(db,res); 
  printf("\n");
#endif  
  return res;  
}  



/* 
   Miniscope: 
     build a new res with quantifiers pushed as far inside as possible
     (all [X,..,Y] frm) and (exists [X,...,Y] frm)   
   Assumes negation has been previously pushed inside, but distribution
   and skolemization not done. 

  fof(main,axiom,(
    ! [x] : ? [z] : (p(x,z) | q(x,z))  )).
  gives 
    ! [x] : ((? [z] : p(x,z)) | (? [z] : q(x,z))) 
   p(x,sk1(x)) | q(x,sk2(x)) 

  fof(main,axiom,(
    ! [x,y] : ? [z] : (p(z) | q(x,y))  )).
  gives
    ((? [z] : p(z)) | q(x,y)) 
    ((? [z] : p(z)) | (! [x,y] : q(x,y))) 
    p(sk) | q(x,y)

*/

void* wr_clausify_miniscope(glb* g, void* mpool, void* frm) {
  void* db=g->db;
  void *res, *op, *op3_1;
  void *arg2, *arg3, *arg3_1, *arg3_2;
  void *vars3_1, *vars3_2;
  void *res1, *res2; 
  int len;  
  void *newvars;

#ifdef DEBUG
  printf("wr_clausify_miniscope starting on\n");  
  wg_mpool_print(db,frm); 
  printf("\n");
#endif  

  if (wg_isatom(db,frm)) {
    // simple atom
    return frm;
  }  
  op=wg_first(db,frm);
  len=wg_list_len(db,frm);
  if (wg_ispair(db,op) && len<2) {
    // pointless parenthesis around formula: lift up
    return wr_clausify_miniscope(g,mpool,op);
  }
  if (wg_ispair(db,op)) {
    // should not happen
    return show_clausify_warning(g,"nonatomic head detected in a miniscope phase");
  }
  if (!wg_islogconnective(db,op) || wg_islogneg(db,op)) {
    // simple nonlogical term or a negated atom
    return frm;
  }
  if (wg_islogall(db,op) || wg_islogexists(db,op)) {
    arg2=wg_first(db,wg_rest(db,frm));
    arg3=wg_first(db,wg_rest(db,wg_rest(db,frm)));
    //newvars=wr_clausify_append_vars(g,mpool,vars,arg2,1,varnr);
#ifdef DEBUG
    printf("newvars for forall became \n");  
    wg_mpool_print(db,newvars); 
    printf("\n");
#endif     
    arg3=wr_clausify_miniscope(g,mpool,arg3);
    if (!wg_ispair(db,arg3)) {
      return arg3;
    } else {
      op3_1=wg_first(db,arg3);
      if (wg_islogand(db,op3_1) || wg_islogor(db,op3_1)) {
        // distribute over or and and
        arg3_1=wg_first(db,wg_rest(db,arg3));
        arg3_2=wg_first(db,wg_rest(db,wg_rest(db,arg3)));
        vars3_1=wr_miniscope_freeoccs(g,mpool,arg2,arg3_1);
        vars3_2=wr_miniscope_freeoccs(g,mpool,arg2,arg3_2);
        if (vars3_1==NULL && vars3_2==NULL) {
          res=arg3;
        } else if (vars3_1==NULL) {
          res1=wg_mklist3(db,mpool,op,vars3_2,arg3_2);
          res=wg_mklist3(db,mpool,op3_1,arg3_1,res1);
        } else if (vars3_2==NULL) {
          res1=wg_mklist3(db,mpool,op,vars3_1,arg3_1);
          res=wg_mklist3(db,mpool,op3_1,res1,arg3_2);
        } else if ((wg_islogand(db,op3_1) && wg_islogall(db,op)) ||
                   (wg_islogor(db,op3_1) && wg_islogexists(db,op)) ) {          
          // can always distribute
          res1=wg_mklist3(db,mpool,op,vars3_1,arg3_1);
          res2=wg_mklist3(db,mpool,op,vars3_2,arg3_2);    
          res=wg_mklist3(db,mpool,op3_1,res1,res2);
        } else {          
          // cannot distribute
          // remove not-occurring vars from quantifier
          newvars=wr_add_freeoccs(g,mpool,vars3_1,vars3_2);         
          if (newvars==NULL) res=arg3;
          else res=wg_mklist3(db,mpool,op,newvars,arg3);
        } 
      } else {
        // connective after quantifier was not or or and
        // remove not-occurring vars from quantifier
        newvars=wr_miniscope_freeoccs(g,mpool,arg2,arg3);
        if (newvars==NULL) res=arg3;
        else res=wg_mklist3(db,mpool,op,newvars,arg3);
      }
      return res;
    }
  } else {
    // connective but not all or exists
    if (len!=3) return frm;
    arg2=wg_first(db,wg_rest(db,frm));
    arg3=wg_first(db,wg_rest(db,wg_rest(db,frm)));
    res1=wr_clausify_miniscope(g,mpool,arg2);
    res2=wr_clausify_miniscope(g,mpool,arg3);   
    res=wg_mklist3(db,mpool,op,res1,res2);
    return res;
  }  
}  

void *wr_miniscope_freeoccs(glb* g, void* mpool, void* vars, void* frm) {
  void *db=g->db;
  void *res, *arg2, *arg3;
  void *op, *newvars, *termoccs, *freeoccs;
  void *term, *termpart;
  int len;

  //printf("\nwr_miniscope_freeoccs on ");
  //wg_mpool_print(db,frm);
  //printf("\n");

  if (vars==NULL) return NULL;
  if (wg_isatom(db,frm)) {
    // simple atom
    //return wg_mkpair(db,mpool,frm,NULL);
    if (wr_freeoccs_invars(g,mpool,vars,frm)) {
      res=wg_mkpair(db,mpool,frm,NULL);
      return res;
    } else {
      return NULL;
    }    
  }  
  op=wg_first(db,frm);
  len=wg_list_len(db,frm);
  if (wg_ispair(db,op) && len<2) {
    // pointless parenthesis around formula: lift up
    return wr_miniscope_freeoccs(g,mpool,vars,op);
  }
  if (wg_ispair(db,op)) {
    // should not happen
    return show_clausify_warning(g,"nonatomic head detected in a miniscope phase");
  }
  if (wg_islogall(db,op) || wg_islogexists(db,op)) {
    // quantifier
    arg2=wg_first(db,wg_rest(db,frm));
    arg3=wg_first(db,wg_rest(db,wg_rest(db,frm)));
    newvars=wr_miniscope_varssubset(g,mpool,vars,arg2);
    termoccs=wr_miniscope_freeoccs(g,mpool,newvars,arg3);
    return termoccs;  
  } else {
    // logical or a nonlogical term: loop over
    freeoccs=NULL;
    if (wg_islogconnective(db,op)) {
      termpart=wg_rest(db,frm);
    } else {
      termpart=frm;
    }  
    for(; termpart!=NULL; termpart=wg_rest(db,termpart)) {
      term=wg_first(db,termpart);
      termoccs=wr_miniscope_freeoccs(g,mpool,vars,term);
      //printf("\ntermoccs ");
      //wg_mpool_print(db,termoccs);
      //printf("\n");
      if (termoccs) freeoccs=wr_add_freeoccs(g,mpool,freeoccs,termoccs);      
    }
    return freeoccs;
  } 
}

int wr_freeoccs_invars(glb* g, void* mpool, void* vars, void* frm) {  
  void* db;
  void *boundvar, *boundpart;
  char* str;

  if (vars==NULL) return 0;
  db=g->db;
  str=wg_atomstr1(db,frm);
  for(boundpart=vars; boundpart!=NULL; boundpart=wg_rest(db,boundpart)) {
    boundvar=wg_first(db,boundpart); 
    if (!strcmp(str,wg_atomstr1(db,boundvar))) {
      // same vars      
      return 1;
    }  
  }
  return 0;
}

void* wr_miniscope_varssubset(glb* g, void* mpool, void* vars, void* boundvars) {
  void* db=g->db;
  void *res=NULL, *part, *boundpart, *var, *boundvar;
  int found=0;

  if (vars==NULL) return NULL;
  if (boundvars==NULL) return vars;
  for(part=vars; part!=NULL; part=wg_rest(db,part)) { 
    var=wg_first(db,part); 
    for(boundpart=boundvars; boundpart!=NULL; boundpart=wg_rest(db,boundpart)) {
      boundvar=wg_first(db,boundpart); 
      if (!strcmp(wg_atomstr1(db,var),wg_atomstr1(db,boundvar))) {
        // same vars
        if (wg_rest(db,vars)==NULL) return NULL; // no other vars
        found=1;
        break;
      }  
    }
    if (found) break;  
  }
  if (!found) return vars;  
  // overlap found, construct the subset
  for(part=vars; part!=NULL; part=wg_rest(db,part)) { 
    var=wg_first(db,part); 
    found=0;
    for(boundpart=boundvars; boundpart!=NULL; boundpart=wg_rest(db,boundpart)) {
      boundvar=wg_first(db,boundpart); 
      if (!strcmp(wg_atomstr1(db,var),wg_atomstr1(db,boundvar))) {
        // same vars
        found=1;
        break;
      }  
    }
    if (!found) {
      // add to the ok subset
      res=wg_mkpair(db,mpool,var,res);
    }
  }
  return res;
}

void* wr_add_freeoccs(glb* g, void* mpool, void* vars, void* termoccs) {
  void* db=g->db;
  void *res, *part, *boundpart, *var, *boundvar;
  int found=0;

  if (termoccs==NULL) return vars;
  if (vars==NULL) return termoccs; 
  res=vars;
  for(part=termoccs; part!=NULL; part=wg_rest(db,part)) { 
    var=wg_first(db,part); 
    // check if already present in vars
    found=0;
    for(boundpart=vars; boundpart!=NULL; boundpart=wg_rest(db,boundpart)) {
      boundvar=wg_first(db,boundpart); 
      if (!strcmp(wg_atomstr1(db,var),wg_atomstr1(db,boundvar))) {
        // same vars
        found=1;
        break;
      }  
    }
    if (!found) {
      // add to the ok subset
      res=wg_mkpair(db,mpool,var,res);
    }
  }
  return res;
}

/* 
   Skolemize: 
     destructively convert
     (all [X,..,Y] frm) and (exists [X,...,Y] frm)
     renaming and skolemizing as necessary:
     resulting frm has vars and constants and no quantifiers.
   Terms are converted by replacing vars/atoms in-place,
   formula layer around is built anew (i.e. nondestructively).
   Assumes negation has been previously pushed inside, but distribution
   not done.  
*/

void* wr_clausify_skolemize(glb* g, void* mpool, void* frm, void* vars, int* varnr) {
  void* db=g->db;
  void *res, *op;
  void *arg2, *arg3;
  void *res2, *res3; 
  int len;  
  void *newvars;

#ifdef DEBUG
  printf("wr_clausify_skolemize starting on\n");  
  wg_mpool_print(db,frm); 
  printf("\n");
#endif 

  if (wg_isatom(db,frm)) {
    // simple atom
    return frm;
  }  
  op=wg_first(db,frm);
  len=wg_list_len(db,frm);
  if (wg_ispair(db,op) && len<2) {
    // pointless parenthesis around formula: lift up
    return wr_clausify_skolemize(g,mpool,op,vars,varnr);
  }
  if (wg_ispair(db,op)) {
    // should not happen
    return show_clausify_warning(g,"nonatomic head detected in a skolemize phase");
  }
  if (!wg_islogconnective(db,op) || wg_islogneg(db,op)) {
    // simple nonlogical term or a negated atom
    // destructively replace
    wr_clausify_replace_vars(g,mpool,vars,frm);
    return frm;
  }
  if (wg_islogall(db,op)) {
    arg2=wg_first(db,wg_rest(db,frm));
    arg3=wg_first(db,wg_rest(db,wg_rest(db,frm)));
    newvars=wr_clausify_append_vars(g,mpool,vars,arg2,1,varnr);
#ifdef DEBUG
    printf("newvars for forall became \n");  
    wg_mpool_print(db,newvars); 
    printf("\n");
#endif     
    res=wr_clausify_skolemize(g,mpool,arg3,newvars,varnr);
    return res;
  } else if (wg_islogexists(db,op)) {
    arg2=wg_first(db,wg_rest(db,frm));
    arg3=wg_first(db,wg_rest(db,wg_rest(db,frm)));
    newvars=wr_clausify_append_vars(g,mpool,vars,arg2,0,varnr);
#ifdef DEBUG
    printf("newvars for exists became \n");  
    wg_mpool_print(db,newvars); 
    printf("\n");
#endif    
    res=wr_clausify_skolemize(g,mpool,arg3,newvars,varnr);
    return res;
  } else {
    // conventional logical term: here we assume two args
    if (len!=3) return show_clausify_warning(g,"non-not logical op should have two arguments");
    arg2=wg_first(db,wg_rest(db,frm));
    arg3=wg_first(db,wg_rest(db,wg_rest(db,frm)));
    res2=wr_clausify_skolemize(g,mpool,arg2,vars,varnr);
    res3=wr_clausify_skolemize(g,mpool,arg3,vars,varnr);
    res=wg_mklist3(db,mpool,op,res2,res3);
    if (!res || !res2 || !res3) return show_clausify_warning(g,"clausification of subformula failed");
    return res;
  }
}  

void* wr_clausify_append_vars
        (glb* g, void* mpool, void* assoc, void* varlist, int allflag, int *varnr) {
  void* db=g->db;        
  void *var, *val, *pair, *apart;
  void *fun, *term, *newassoc;
  int type;
  char buf[20]; // new fun name / var value buffer

#ifdef DEBUG
  printf("\nwr_clausify_append_vars starting on\n");  
  printf("assoc \n");
  wg_mpool_print(db,assoc); 
  printf("\nvarlist \n");
  wg_mpool_print(db,varlist); 
  printf("\n allflag %d varnr %d \n",allflag,*varnr);
#endif    
  if (!varlist) return assoc;
  newassoc=assoc;
  for(; varlist!=NULL; varlist=wg_rest(db,varlist)) {
    var=wg_first(db,varlist);
    if (!wg_isatom(db,var)) return show_clausify_warning(g,"nonatomic var found");
    type=wg_atomtype(db,var);
    if (type!=WG_URITYPE && type!=WG_VARTYPE) 
      return show_clausify_warning(g,"abnormal var found 1");    
    if (wg_atomstr2(db,var)!=NULL) 
      return show_clausify_warning(g,"abnormal var found 2");
    if (allflag) {
      // universal quantifier: 
      // pick a fresh var name
      (*varnr)++;
      snprintf(buf,19,"%s%d",NEWVAR_PREFIX,*varnr);      
      val=wg_mkatom(db,mpool,WG_VARTYPE,buf,NULL);      
      pair=wg_mkpair(db,mpool,var,val);
      newassoc=wg_mkpair(db,mpool,pair,newassoc);      
    } else {
      // existential quantifier:
      // pick a fresh skolem name and create a skolem fun
      (dbmemsegh(db)->parse_skolem_nr)++;      
      snprintf(buf,19,"%s%d",g->parse_skolem_prefix,dbmemsegh(db)->parse_skolem_nr);      
      fun=wg_mkatom(db,mpool,WG_URITYPE,buf,NULL);
      term=NULL;
      // find universal vars in outer scope
      for(apart=assoc; apart!=NULL; apart=wg_rest(db,apart)) {
        pair=wg_first(db,apart);
        if (wg_atomtype(db,wg_rest(db,pair))==WG_VARTYPE) {
          // universal quantifier found
          term=wg_mkpair(db,mpool,wg_rest(db,pair),term); // append univ var to skolem term
        }
      }
      if (!term) val=fun;
      else val=wg_mkpair(db,mpool,fun,term);
      pair=wg_mkpair(db,mpool,var,val);
      newassoc=wg_mkpair(db,mpool,pair,newassoc);
    }
  }
  return newassoc;
}

void wr_clausify_replace_vars(glb* g, void* mpool, void* assoc, void* term) {
  void* db=g->db;
  void *el, *part, *apart, *ael, *avar;
  int type;

  //printf("wr_clausify_replace_vars called with\n");  
  //wg_mpool_print(db,assoc);   
  //printf("\n");
  //wg_mpool_print(db,term);   
  //printf("\n");
  if (!assoc) return;
  if (!term) return;
  if (wg_isatom(db,term)) {
    // simple atom, should not happen
    show_clausify_warning(g,"wr_clausify_replace_vars called on atom");
    return;   
  }  
  for(part=term; part!=NULL; part=wg_rest(db,part)) {
    el=wg_first(db,part);    
    if (!wg_isatom(db,el)) {
      // not an atom: pair
      wr_clausify_replace_vars(g,mpool,assoc,el);
    } else {
      // here is atom
      type=wg_atomtype(db,el);
      if (type!=WG_URITYPE && type!=WG_VARTYPE) continue;
      if (wg_atomstr2(db,el)!=NULL) continue;
      for(apart=assoc; apart!=NULL; apart=wg_rest(db,apart)) {
        ael=wg_first(db,apart);
        avar=wg_first(db,ael);
        if (wg_atomtype(db,avar)==type &&
            !strcmp(wg_atomstr1(db,avar),wg_atomstr1(db,el)) ) {
          // var found in assoc, replace
          wg_replacepair_first(db,part,wg_rest(db,ael));
          break;
        }            
      }
    }
  }
}  



/* 
  Convert formula to CNF (clause normal form) assuming
  formula has negation pushed to literals and contains
  otherwise only operators & and V.
  Essentially, each V is pushed inside so that all 
  V arguments are literals.
  Also, all & arguments in the result are either literals
  or disjunctions (V) of literals (i.e. nested &-s are
  flattened).
  Conversion uses only distribution laws: no new variables
  are introduced, tautologies are not removed nor any 
  other optimizations attempted.  

(v a (v b c)) kept as is
(& a (v b c) d) kept as is
(v (& a b) c) -> (& (v a c) (v b c))
(v (& a b) (& c d)) -> (& (v a c) (v a d) (v b c) (v b d))
(v (& a (& b d)) c) -> (& (v a c) (v (& b d) c)) -> 
    (& (v a c)  (& (v b c) (v d c)))
*/

void* wr_clausify_distribute(glb* g, void* mpool, void* frm, void **defs, int indef) {
  void* db=g->db;
  void *res, *op, *logand, *logor;
  void *arg1, *arg2, *arg3, *arg4;
  void *res1, *res2, *res3, *res4, *res5, *res6, *res7, *res8; 
  void *def1=NULL, *def2=NULL;
  int len, and1flag, and2flag, hardcount;

#ifdef DEBUG
  printf("wr_clausify_distribute called on frm\n");  
  wg_mpool_print(db,frm); 
  printf("\n");
#endif
  if (g->parse_errflag) return NULL;
  if (wg_isatom(db,frm)) {
    // simple atom
    return frm;  
  }  
  op=wg_first(db,frm);
  len=wg_list_len(db,frm);
  if (wg_ispair(db,op) && len<2) {
    // pointless parenthesis around formula: lift up
    return wr_clausify_distribute(g,mpool,op,defs,indef);
  }
  if (wg_ispair(db,op)) {
    // should not happen
    return show_clausify_warning(g,"nonatomic head detected in a distr phase");
  }
  if (!wg_islogconnective(db,op)) {
    // simple nonlogical term
    return frm;
  }
  if (wg_islogneg(db,op)) {
    // negation, we assume at the atom 
    return frm;
  }
  if (len!=3) 
    return show_clausify_warning(g,"wrong subformula length in a distr phase: %d",len);
  if (wg_islogand(db,op)) {
    // this should stay at the top level
    res1=wr_clausify_distribute(g,mpool,wg_first(db,wg_rest(db,frm)),defs,indef);
    if (!res1) return show_clausify_warning(g,"memory pool full in simple distribution");
    res2=wr_clausify_distribute(g,mpool,wg_first(db,wg_rest(db,wg_rest(db,frm))),defs,indef);
    if (!res2) return show_clausify_warning(g,"memory pool full in simple distribution");
    res=wg_mklist3(db,mpool,op,res1,res2);
    if (!res1 || !res2 || !res) return show_clausify_warning(g,"memory pool full in simple distribution");
    return res;
  }
  if (wg_islogor(db,op)) {
    //printf("wr_clausify_distribute  logor case on frm\n");  
    //wg_mpool_print(db,frm); 
    //printf("\n");
    and1flag=0;
    and2flag=0;
    // this should be distributed lower if & is found below
    res1=wr_clausify_distribute(g,mpool,wg_first(db,wg_rest(db,frm)),defs,indef);
    if (!res1) return show_clausify_warning(g,"memory pool full in simple distribution");
    res2=wr_clausify_distribute(g,mpool,wg_first(db,wg_rest(db,wg_rest(db,frm))),defs,indef);    
    /* 
    printf("wr_clausify_distribute subres res1\n");  
    wg_mpool_print(db,res1); 
    printf("\n");
    printf("wr_clausify_distribute subres res2\n");  
    wg_mpool_print(db,res2); 
    printf("\n");
    */
    if (!res1 || !res2) return show_clausify_warning(g,"memory pool full in simple distribution");
    // here res1 and res2 may contain & only at top layers
    if (wg_ispair(db,res1) && wg_islogand(db,wg_first(db,res1))) {
      // res1 top level is &, must distribute
      and1flag=1;      
    }
    if (wg_ispair(db,res2) && wg_islogand(db,wg_first(db,res2))) {
      // res2 top level is &, must distribute
      and2flag=1;
    }
    logand=wg_makelogand(db,mpool);
    logor=op;
    if (!and1flag && !and2flag) {
      // none of res1 and res2 is an &-clause, no distribution needed
      res=wg_mklist3(db,mpool,logor,res1,res2);
      if (!res) return show_clausify_warning(g,"memory pool full in simple distribution");
    } else if ((and1flag && !and2flag) || (!and1flag && and2flag)) {
      // res1 is an & clause, res2 is not or vice versa
      // (v (& a b) c) -> (& (v a c) (v b c))
      if (and1flag && !and2flag) {
        // create def or distribute?
        //def1=wr_clausify_makedef(g,mpool,res1,defs);
        // parts 
        arg1=wg_nth(db,res1,1); // arg1 is an &-clause (& a b)     
        arg2=wg_nth(db,res1,2);    
        arg3=res2;              // arg3 is not an &-clause c 
      } else {
        //  create def or distribute?
        //def1=wr_clausify_makedef(g,mpool,res2,defs);
        // parts
        arg1=wg_nth(db,res2,2); // arg1 is an &-clause (& a b)     
        arg2=wg_nth(db,res2,1); 
        arg3=res1;              // arg3 is not an &-clause c 
      }      
      // new part with def created
      if (def1!=NULL) res=def1;      
      else {
        // old part
        res3=wg_mklist3(db,mpool,logor,arg1,arg3);  // (v a c)
        if (!res3) return show_clausify_warning(g,"memory pool full");
        res4=wg_mklist3(db,mpool,logor,arg2,arg3);  // (v b c)
        if (!res3 || !res4) return show_clausify_warning(g,"memory pool full");
        // normalize newly created res5 and res6
        res3=wr_clausify_distribute(g,mpool,res3,defs,indef);
        if (!res3) return show_clausify_warning(g,"memory pool full");
        res4=wr_clausify_distribute(g,mpool,res4,defs,indef);
        // build a final result
        res=wg_mklist3(db,mpool,logand,res3,res4); // (& (v a c) (v b c))
        if (!res3 || !res4 || !res) {
          return show_clausify_warning(g,"memory pool full");
        }       
      }  
    } else {
      // both res1 and res2 are &-clauses
      //(v (& a b) (& c d)) -> (& (v a c) (v a d) (v b c) (v b d))
      //(v (& a b) (& c d)) -> (& (v a c) (& (v a d) (& (v b c) (v b d))))
      // parts
      arg1=wg_nth(db,res1,1); // a      
      arg2=wg_nth(db,res1,2); // b      
      arg3=wg_nth(db,res2,1); // c      
      arg4=wg_nth(db,res2,2); // d
      // create defs
      hardcount=0;
      //printf("\nCP0\n");
      
      if ((g->parse_newpred_strat) && !indef) {
        hardcount+=wr_clausify_dist_complexity(g,arg1,0);
        hardcount+=wr_clausify_dist_complexity(g,arg2,0);
        hardcount+=wr_clausify_dist_complexity(g,arg3,0);
        hardcount+=wr_clausify_dist_complexity(g,arg4,0);
      }
      
      //printf("\nCP4\n");
      //printf("\nwr_clausify_distribute case hardcount %d on frm\n", hardcount);  
      //wg_mpool_print(db,frm); 
      //printf("\n");

      if ((g->parse_newpred_strat) && 
          //(hardcount>=MAKEDEF_COMPLEXITY_THRESHOLD) ) {
          (indef ||  (hardcount>=MAKEDEF_COMPLEXITY_THRESHOLD)) ) {
          //!indef && (hardcount>=MAKEDEF_COMPLEXITY_THRESHOLD)) {
        // introduce new pred as definition
        //printf("\nhardcount %d\n",hardcount);
        def1=wr_clausify_makedef(g,mpool,res1,defs);
        if (!def1) return show_clausify_warning(g,"memory pool full while making a new def");
        def2=wr_clausify_makedef(g,mpool,res2,defs);
        if (!def1 || !def2) {
          return show_clausify_warning(g,"could not create a def pair in a distr phase");
        } 
        res=wg_mklist3(db,mpool,logor,def1,def2);
        if (!res) return show_clausify_warning(g,"memory pool full in a distr phase");
      } else {        
        // build new v clauses
        res3=wg_mklist3(db,mpool,logor,arg1,arg3);  // (v a c)
        if (!res3) return show_clausify_warning(g,"memory pool full in a distr phase");
        res4=wg_mklist3(db,mpool,logor,arg1,arg4);  // (v a d)
        if (!res4) return show_clausify_warning(g,"memory pool full in a distr phase");
        res5=wg_mklist3(db,mpool,logor,arg2,arg3);  // (v b c)
        if (!res5) return show_clausify_warning(g,"memory pool full in a distr phase");
        res6=wg_mklist3(db,mpool,logor,arg2,arg4);  // (v b d)
        if (!res6) return show_clausify_warning(g,"memory pool full in a distr phase");
        // normalize new v clauses
        res3=wr_clausify_distribute(g,mpool,res3,defs,indef);
        if (!res3) return show_clausify_warning(g,"memory pool full in a distr phase");
        res4=wr_clausify_distribute(g,mpool,res4,defs,indef);
        if (!res4) return show_clausify_warning(g,"memory pool full in a distr phase");
        res5=wr_clausify_distribute(g,mpool,res5,defs,indef);
        if (!res5) return show_clausify_warning(g,"memory pool full in a distr phase");
        res6=wr_clausify_distribute(g,mpool,res6,defs,indef);
        if (!res6) return show_clausify_warning(g,"memory pool full in a distr phase");
        // build a final result
        // (& (v a c) (& (v a d) (& (v b c) (v b d))))
        res7=wg_mklist3(db,mpool,logand,res5,res6);
        if (!res7) return show_clausify_warning(g,"memory pool full in a distr phase");
        res8=wg_mklist3(db,mpool,logand,res4,res7);
        res=wg_mklist3(db,mpool,logand,res3,res8);
        if (!res3 || !res4 || !res5 || !res6 || !res7 || !res8 || !res) {
          return show_clausify_warning(g,"memory pool full in a distr phase");
        }      
      }  
    }  
#ifdef DEBUG
    printf("wr_clausify_distribute returns\n");  
    wg_mpool_print(db,res); 
    printf("\n");
#endif
    return res;
  } else {
    // should not happen
    return show_clausify_warning(g,"unexpected formula structure at a distr phase");
  }
  // should not happen
  return show_clausify_warning(g,"unexpected error at a distr phase");
}  

int wr_clausify_isliteral(glb* g, void* frm) {
  void* db=g->db;
  void *op;
  if (frm==NULL) return 1;
  if (wg_isatom(db,frm)) return 1;
  op=wg_first(db,frm);
  if (!wg_islogconnective(db,op)) return 1;
  if (wg_islogneg(db,op)) return 1;
  return 0;
}

int wr_clausify_dist_complexity(glb* g, void* term, int reverses) {
  void* db=g->db;
  void *op;
  int len,tmp1,tmp2;

  //printf("wr_clausify_dist_complexity on term\n");  
  //wg_mpool_print(db,term); 
  //printf("\n");

  if (!term) return 0;  
  if (wg_isatom(db,term)) return 0;

  //printf("\nCP0\n");

  op=wg_first(db,term);
  len=wg_list_len(db,term);
  if (wg_ispair(db,op) && len<2) {
    // pointless parenthesis around formula: lift up
    return wr_clausify_dist_complexity(g,op,reverses);
  }
  if (wg_ispair(db,op)) {
    // should not happen
    return 0;
  }
  if (!wg_islogconnective(db,op)) {
    // simple nonlogical term
    return 0;
  }
  if (wg_islogneg(db,op)) {
    // negation, we assume at the atom 
    return 0;
  }
  if (len!=3) {
    return 0;
  }  
  //printf("\nCP1\n");
  if (wg_islogand(db,op)) { 
    tmp1=wr_clausify_dist_complexity(g,wg_first(db,wg_rest(db,term)),reverses);
    tmp2=wr_clausify_dist_complexity(g,wg_first(db,wg_rest(db,wg_rest(db,term))),reverses);
    //printf("\n res %d\n",1+tmp1+tmp2);
    return 1+tmp1+tmp2;    
  }
  if (wg_islogor(db,op)) {
    tmp1=wr_clausify_dist_complexity(g,wg_first(db,wg_rest(db,term)),reverses);
    tmp2=wr_clausify_dist_complexity(g,wg_first(db,wg_rest(db,wg_rest(db,term))),reverses);
    //printf("\n res %d\n",1+tmp1+tmp2);
    return 1+tmp1+tmp2;    
  }
  //printf("\n should not happen res\n");
  return 0;
}

void* wr_clausify_makedef(glb* g, void* mpool, void* frm, void **defs) {
  void* db=g->db;
  void *def, *negdef, *negfrm, *res1, *res2; //, *res2d;
  void *logand, *logor; //, *logneg;
  int count,i,check;  
  void *fun, *term, *deflst, *tmp;
  char buf[20]; // new fun name / var value buffer
  gptr vars[VARCOLLECT_SIZE]; // collected vars

#ifdef DEBUG
  printf("wr_clausify_makedef called on frm\n");  
  wg_mpool_print(db,frm); 
  printf("\n");
#endif
  if (g->parse_errflag) return NULL;
  logand=wg_makelogand(db,mpool);
  logor=wg_makelogor(db,mpool);
  if (!logand || !logor) {
    return show_clausify_warning(g,"could not create elem wr_clausify_makedef");
  }
  // make new def atom
  (dbmemsegh(db)->parse_newpred_nr)++;
  snprintf(buf,19,"%s%d",g->parse_newpred_prefix,dbmemsegh(db)->parse_newpred_nr);   
  fun=wg_mkatom(db,mpool,WG_URITYPE,buf,NULL);
  if (!fun) return show_clausify_warning(g,"could not create elem wr_clausify_makedef");
  term=NULL;
  // build term as as a list of vars
  count=0;
  check=wr_clausify_collect_vars(g,frm,vars,&count,VARCOLLECT_SIZE-1); 
  if (check<0) return show_clausify_warning(g,"could not create full varlist in wr_clausify_makedef");
  term=NULL;
  for(i=0;i<count;i++) {
    term=wg_mkpair(db,mpool,(void*)(vars[i]),term);
    if (!term) return show_clausify_warning(g,"could not create extend vars wr_clausify_makedef");
  }
  if (!term) def=fun;
  else def=wg_mkpair(db,mpool,fun,term);
  if (!def) return show_clausify_warning(g,"could not create elem wr_clausify_makedef");
  negdef=wr_clausify_negpush(g,mpool,def,0);
  if (!negdef) return show_clausify_warning(g,"could not create elem wr_clausify_makedef");
  // res1: -def | (andarg1 & andarg2)
  res1=wg_mklist3(db,mpool,logor,negdef,frm); 
  if (!res1) return show_clausify_warning(g,"could not create elem wr_clausify_makedef");

  res1=wr_clausify_distribute(g,mpool,res1,defs,1);

  if (!res1) return show_clausify_warning(g,"could not subdist 1 in wr_clausify_makedef");
  
  // res2: def | -andarg1 | -andarg2
  negfrm=wr_clausify_negpush(g,mpool,frm,0);
  if (!negfrm) return show_clausify_warning(g,"could not create elem wr_clausify_makedef");
  /* 
  res2d=wr_clausify_distribute(g,mpool,negfrm,NULL,1);
  if (!res2d) return show_clausify_warning(g,"could not create elem wr_clausify_makedef");
  // res2: def | -andarg1 | -andarg2
  res2=wg_mklist3(db,mpool,logor,def,res2d);
  res2=wr_clausify_distribute(g,mpool,res2,defs,1);
  */
   
  // negfrm: neg pushed inside for:  -(andarg1 & andarg2)
  if (!negfrm) return show_clausify_warning(g,"could not create elem wr_clausify_makedef");
  // res2: def | -andarg1 | -andarg2
  res2=wg_mklist3(db,mpool,logor,def,negfrm);
  // res2: def | (neg pushed inside for:  -(andarg1 & andarg2))
  if (!res2) return show_clausify_warning(g,"could not create elem wr_clausify_makedef");
  /* 
  printf("\nstarting to distr in makedef for\n");
  wg_mpool_print(db,res2); 
  printf("\n");
  */
  res2=wr_clausify_distribute(g,mpool,res2,defs,1);
  /* 
  printf("\nended distr in makedef for\n");
  wg_mpool_print(db,res2); 
  printf("\n");
  */
  if (!res2) return show_clausify_warning(g,"could not subdist 2 in wr_clausify_makedef");
  deflst=wg_mklist3(db,mpool,logand,res1,res2);

  //deflst=res1;
  if (!deflst) return show_clausify_warning(g,"could not create elem wr_clausify_makedef");
  if (*defs==NULL) {
    *defs=deflst;
  } else {
    tmp=wg_mklist3(db,mpool,logand,*defs,deflst);
    if (!tmp) return show_clausify_warning(g,"could not create elem wr_clausify_makedef");
    *defs=tmp;
  }
#ifdef DEBUG
  printf("wr_clausify_makedef creates\n");  
  wg_mpool_print(db,res1); 
  printf("\n");
  wg_mpool_print(db,res2); 
  printf("\n");
  printf("and defs\n");  
  wg_mpool_print(db,*defs); 
  printf("and returns\n");  
  wg_mpool_print(db,def); 
  printf("\n\n");
#endif
  return def;
}

int wr_clausify_collect_vars(glb* g, void* term, gptr* vars, int *count, int max) {
  void* db=g->db;
  void *part, *el;
  int type,i, check;  
#ifdef DEBUG
  printf("\n wr_clausify_collect_vars called with count %d on term\n",*count);  
  wg_mpool_print(db,term); 
  printf("\n");
#endif
  if (!term) return 1;  
  if (wg_isatom(db,term)) {
    type=wg_atomtype(db,term);
    if (type!=WG_VARTYPE) return 1;
    // do we have this var already?
    for(i=0;i<*count;i++) {
      if ((void*)(vars[i])==term) return 1;
    }
    // new var: add
    if (*count>=max) {
      show_clausify_warning(g,"too many vars found by wr_clausify_makedef");
      return -1;
    }
#ifdef DEBUG
    printf("\n wr_clausify_collect_vars count %d found var\n",*count);  
    wg_mpool_print(db,term); 
    printf("\n");
#endif
    vars[*count]=term;    
    (*count)++;
    return 1;
  }
  for(part=term; part!=NULL; part=wg_rest(db,part)) {
    el=wg_first(db,part); 
    check=wr_clausify_collect_vars(g,el,vars,count,max);
    if (check<0) return -1;  
  }  
  return 1;   
}  

/*
void* orig_wr_clausify_distribute(glb* g, void* mpool, void* frm, void* defs) {
  void* db=g->db;
  void *res, *op, *logand, *logor;
  void *arg1, *arg2, *arg3, *arg4;
  void *res1, *res2, *res3, *res4, *res5, *res6, *res7, *res8; 
  int len, and1flag, and2flag;

  printf("wr_clausify_distribute called on frm\n");  
  wg_mpool_print(db,frm); 
  printf("\n");

  if (wg_isatom(db,frm)) {
    // simple atom
    return frm;  
  }  
  op=wg_first(db,frm);
  len=wg_list_len(db,frm);
  if (wg_ispair(db,op) && len<2) {
    // pointless parenthesis around formula: lift up
    return wr_clausify_distribute(g,mpool,op,defs);
  }
  if (wg_ispair(db,op)) {
    // should not happen
    return show_clausify_warning(g,"nonatomic head detected in a distr phase");
  }
  if (!wg_islogconnective(db,op)) {
    // simple nonlogical term
    return frm;
  }
  if (wg_islogneg(db,op)) {
    // negation, we assume at the atom 
    return frm;
  }
  if (len!=3) 
    return show_clausify_warning(g,"wrong subformula length in a distr phase: %d",len);
  if (wg_islogand(db,op)) {
    // this should stay at the top level
    res1=wr_clausify_distribute(g,mpool,wg_first(db,wg_rest(db,frm)),defs);
    res2=wr_clausify_distribute(g,mpool,wg_first(db,wg_rest(db,wg_rest(db,frm))),defs);
    res=wg_mklist3(db,mpool,op,res1,res2);
    if (!res1 || !res2 || !res) return show_clausify_warning(g,"memory pool full");
    return res;
  }
  if (wg_islogor(db,op)) {
    and1flag=0;
    and2flag=0;
    // this should be distributed lower if & is found below
    res1=wr_clausify_distribute(g,mpool,wg_first(db,wg_rest(db,frm)),defs);
    res2=wr_clausify_distribute(g,mpool,wg_first(db,wg_rest(db,wg_rest(db,frm))),defs);
    if (!res1 || !res2) return show_clausify_warning(g,"memory pool full");
    // here res1 and res2 may contain & only at top layers
    if (wg_ispair(db,res1) && wg_islogand(db,wg_first(db,res1))) {
      // res1 top level is &, must distribute
      and1flag=1;      
    }
    if (wg_ispair(db,res2) && wg_islogand(db,wg_first(db,res2))) {
      // res2 top level is &, must distribute
      and2flag=1;
    }
    logand=wg_makelogand(db,mpool);
    logor=op;
    if (!and1flag && !and2flag) {
      // none of res1 and res2 is an &-clause, no distribution needed
      res=wg_mklist3(db,mpool,logor,res1,res2);
      if (!res) return show_clausify_warning(g,"memory pool full");
    } else if ((and1flag && !and2flag) || (!and1flag && and2flag)) {
      // res1 is an & clause, res2 is not or vice versa
      // (v (& a b) c) -> (& (v a c) (v b c))
      if (and1flag && !and2flag) {
        arg1=wg_nth(db,res1,1); // arg1 is an &-clause (& a b)     
        arg2=wg_nth(db,res1,2);    
        arg3=res2;              // arg3 is not an &-clause c 
      } else {
        arg1=wg_nth(db,res2,2); // arg1 is an &-clause (& a b)     
        arg2=wg_nth(db,res2,1); 
        arg3=res1;              // arg3 is not an &-clause c 
      }
      res3=wg_mklist3(db,mpool,logor,arg1,arg3);  // (v a c)
      res4=wg_mklist3(db,mpool,logor,arg2,arg3);  // (v b c)
      if (!res3 || !res4) return show_clausify_warning(g,"memory pool full");
      // normalize newly created res5 and res6
      res3=wr_clausify_distribute(g,mpool,res3,defs);
      res4=wr_clausify_distribute(g,mpool,res4,defs);
      // build a final result
      res=wg_mklist3(db,mpool,logand,res3,res4); // (& (v a c) (v b c))
      if (!res3 || !res4 || !res) {
        return show_clausify_warning(g,"memory pool full");
      }       
    } else {
      // both res1 and res2 are &-clauses
      //(v (& a b) (& c d)) -> (& (v a c) (v a d) (v b c) (v b d))
      //(v (& a b) (& c d)) -> (& (v a c) (& (v a d) (& (v b c) (v b d))))
      arg1=wg_nth(db,res1,1); // a      
      arg2=wg_nth(db,res1,2); // b      
      arg3=wg_nth(db,res2,1); // c      
      arg4=wg_nth(db,res2,2); // d
      // build new v clauses
      res3=wg_mklist3(db,mpool,logor,arg1,arg3);  // (v a c)
      res4=wg_mklist3(db,mpool,logor,arg1,arg4);  // (v a d)
      res5=wg_mklist3(db,mpool,logor,arg2,arg3);  // (v b c)
      res6=wg_mklist3(db,mpool,logor,arg2,arg4);  // (v b d)
      // normalize new v clauses
      res3=wr_clausify_distribute(g,mpool,res3,defs);
      res4=wr_clausify_distribute(g,mpool,res4,defs);
      res5=wr_clausify_distribute(g,mpool,res5,defs);
      res6=wr_clausify_distribute(g,mpool,res6,defs);
      // build a final result
      // (& (v a c) (& (v a d) (& (v b c) (v b d))))
      res7=wg_mklist3(db,mpool,logand,res5,res6);
      res8=wg_mklist3(db,mpool,logand,res4,res7);
      res=wg_mklist3(db,mpool,logand,res3,res8);
      if (!res3 || !res4 || !res5 || !res6 || !res7 || !res8 || !res) {
        return show_clausify_warning(g,"memory pool full in a distr phase");
      }      
    }  

    printf("wr_clausify_distribute returns\n");  
    wg_mpool_print(db,res); 
    printf("\n");

    return res;
  } else {
    // should not happen
    return show_clausify_warning(g,"unexpected formula structure at a distr phase");
  }
  // should not happen
  return show_clausify_warning(g,"unexpected error at a distr phase");
}  

*/

/*
  
  (& a b) ->       ( (a)
                     (b))
  (& p(1) r(2)) -> ( ((p 1))
                     ((r 2)) )
  (v p(1) r(2)) -> ( ((p 1)(r 2)) )

  (v p(1) (v r(2) s(3))) -> ( ((p 1)(r 2)(s 3)) )

  (& k(e)  (v p(1) r(2))  ->  ( ((k e))
                                ((p 1)(r 2)) )                   
   
*/

void* wr_flatten_logclause(glb* g,void* mpool, void* cl) {
  void* db=g->db;
  void *op, *arg1, *arg2;
  void *res1, *res2, *res;

  //printf("\nwr_flatten_logclause starts with:\n");
  //wg_mpool_print(db,cl); 
  //printf("\n");

  if (!cl) return NULL;
  if (!wg_ispair(db,cl)) {

    //printf("\n in wr_flatten_logclause !wg_ispair(db,cl):\n");
    //wg_mpool_print(db,cl); 
    //printf("\n");

    res1=wr_flatten_logclause_or(g,mpool,cl);
    res=wg_mklist1(db,mpool,res1);
   
    //printf("\n in wr_flatten_logclause !wg_ispair(db,cl) gave res:\n");
    //wg_mpool_print(db,res); 
    //printf("\n");

    if (!res1 || !res) 
      return show_clausify_warning(g,"memory pool full in a flatten phase");
    return res;  
  }    
  op=wg_first(db,cl);
  if (wg_ispair(db,op) && !wg_rest(db,cl)) {
    res=wr_flatten_logclause(g,mpool,op);
  } else if (wg_islogor(db,op)) {
    res1=wr_flatten_logclause_or(g,mpool,cl);
    res=wg_mklist1(db,mpool,res1);   
    if (!res1) 
      return show_clausify_warning(g,"memory pool full in a flatten phase");
  } else if (wg_islogand(db,op)) {
    arg1=wg_first(db,wg_rest(db,cl));
    arg2=wg_first(db,wg_rest(db,wg_rest(db,cl)));
    res1=wr_flatten_logclause(g,mpool,arg1);
    res2=wr_flatten_logclause(g,mpool,arg2);
    res=wg_appendlist(db,mpool,res1,res2);
    if (!res1 || !res2 || !res) 
     return show_clausify_warning(g,"memory pool full in a flatten phase");
  } else {
    res1=wr_flatten_logclause_or(g,mpool,cl);
    res=wg_mklist1(db,mpool,res1);
    if (!res1) 
      return show_clausify_warning(g,"memory pool full in a flatten phase");
  }   
  if (!res) 
     return show_clausify_warning(g,"memory pool full in a flatten phase");

  //printf("\n in wr_flatten_logclause gave res:\n");
  //wg_mpool_print(db,res); 
  //printf("\n");

  return res;
}


/*
  
  1  -> (1)

  (p 1) -> ((p 1))

  (v (p 1) (r 2)) ->  ((p 1)(r 2)) 

  (v (p 1) (v (r 2) (s 3))) ->  ((p 1)(r 2)(s 3)) 
 
  

*/
void* wr_flatten_logclause_or(glb* g,void* mpool, void* cl) {
  void* db=g->db;
  void *op, *arg1, *arg2;
  void *res1, *res2, *res;
  int truth1, truth2;

#ifdef DEBUG
  printf("\n in wr_flatten_logclause_or cl:\n");
  wg_mpool_print(db,cl); 
  printf("\n");
#endif

  if (!cl) return NULL;
  if (!wg_ispair(db,cl)) {
    res=wg_mklist1(db,mpool,cl);
    if (!res) return show_clausify_warning(g,"memory pool full in a flatten phase");
    return res;
  }  
  op=wg_first(db,cl);
  if (wg_ispair(db,op) && !wg_rest(db,cl)) {
    /*
    truth1=wr_is_logtruth(g,op);
    if (!truth1) {
      res=wr_flatten_logclause_or(g,mpool,op);
    } else if (truth1==1) {      
      show_clausify_warning(g,"const true clause found in flatten phase");
      return NULL;
    } else if (truth1==-1) {
      show_clausify_warning(g,"const false clause found in flatten phase");
      return NULL;
    }
    */
    res=wr_flatten_logclause_or(g,mpool,op);
  } else if (wg_islogor(db,op)) {
    arg1=wg_first(db,wg_rest(db,cl));
    arg2=wg_first(db,wg_rest(db,wg_rest(db,cl)));
  
    truth1=wr_is_logtruth(g,arg1);
    truth2=wr_is_logtruth(g,arg2);

    if (!truth1 && !truth2) {
      res1=wr_flatten_logclause_or(g,mpool,arg1);
      res2=wr_flatten_logclause_or(g,mpool,arg2);
      if (wg_islogtrue(db,res1)) return res1;
      if (wg_islogtrue(db,res2)) return res2;
      res=wg_appendlist(db,mpool,res1,res2);
      if (!res1 || !res2 || !res) 
      return show_clausify_warning(g,"memory pool full in a flatten phase");
    } else if (truth1==1 || truth2==1) {
      //show_clausify_warning(g,"const true clause found in flatten phase");
      return wg_makelogtrue(db,mpool);
    } else if (truth1==-1) {
      res=wr_flatten_logclause_or(g,mpool,arg2);
    } else if (truth2==-1) {
       res=wr_flatten_logclause_or(g,mpool,arg1);
    } else {
      return show_clausify_warning(g,"error case in flatten phase");
    }
    /*
    res1=wr_flatten_logclause_or(g,mpool,arg1);
    res2=wr_flatten_logclause_or(g,mpool,arg2);
    res=wg_appendlist(db,mpool,res1,res2);
    if (!res1 || !res2 || !res) 
     return show_clausify_warning(g,"memory pool full in a flatten phase");
    */ 
  } else {
    res=wg_mklist1(db,mpool,cl);
  }
  if (!res) 
     return show_clausify_warning(g,"memory pool full in a flatten phase");
  return res;
}

// returns 0 if not known truth value, 1 if true, -1 if false

int wr_is_logtruth(glb* g, void* cl) {
  void* db=g->db;
  void *op, *rest, *arg;
  int sign=1,tmp,res;

#ifdef DEBUG
  printf("\n in wr_is_logtruth cl:\n");
  wg_mpool_print(db,cl); 
  printf("\n");
#endif

  if (wg_ispair(db,cl)) {
    op=wg_first(db,cl);
    if (wg_islogneg(db,op)) {
      sign=-1;
      rest=wg_rest(db,cl);
      if (!rest) return 0;
      arg=wg_first(db,rest);
    } else {
      return 0;
    }
  } else {
    arg=cl;
  }  
  tmp=wr_is_logconst(g,arg);
  if (!tmp) {
    res=0;
  } else if (tmp==sign) {
    res=1;
  } else res=-1;

#ifdef DEBUG
  printf("\n wr_is_logtruth returns %d:\n",res);
#endif
  return res;
}

// returns 0 if not a logconst, 1 if true, -1 if false

int wr_is_logconst(glb* g, void* arg) {
  void* db=g->db;
  //void *op, *rest;

#ifdef DEBUG
  printf("\n in wr_is_logconst cl:\n");
  wg_mpool_print(db,arg); 
  printf("\n");
#endif  
  /*
  if (!wg_ispair(db,cl)) return 0;
  op=wg_first(db,cl);
  if (!wg_isatom(db,op)) return 0;
  if (strcmp(wg_atomstr1(db,op),"$p")) return 0;
  rest=wg_rest(db,cl);
  if (!rest) return 0;
  arg=wg_first(db,rest);
  */
  if (!wg_isatom(db,arg)) return 0;
  if (wg_atomtype(db,arg)!=WG_URITYPE) return 0;
  if (!strcmp(wg_atomstr1(db,arg),"$true")) return 1;
  if (!strcmp(wg_atomstr1(db,arg),"$false")) return -1;
  return 0;    
}

/* ------------ errors ---------------- */

/*
void* show_clausify_error(glb* g, char* format, ...) {
  int tmp1,tmp2;
  va_list args;
  va_start (args, format);
#ifdef PRINTERR   
  //printf("*** Clausify error: ");
  //vprintf (format, args);
#endif  
  if (g->parse_errflag) return NULL;
  (g->parse_errflag)=1;  
  if (g->parse_errmsg) return NULL;
  (g->parse_errmsg)=wr_malloc(g,1000);
  if (!(g->parse_errmsg)) return NULL;
  tmp1=snprintf((g->parse_errmsg),50,"{\"error\": \"clausification hard error: ");
  tmp2=vsnprintf((g->parse_errmsg)+tmp1,800,format,args);
  snprintf((g->parse_errmsg)+tmp1+tmp2,50,"\"}");
  va_end (args);
  wr_free(g,malloc);
  return NULL;
}
*/

void* show_clausify_warning(glb* g, char* format, ...) {
  int tmp1,tmp2;
  va_list args;
  va_start (args, format);
#ifdef PRINTERR  
  //printf("*** Clausify warning: ");
  //vprintf (format, args);
#endif    
  if (g->parse_errflag) return NULL;
  (g->parse_errflag)=1;  
  if (g->parse_errmsg) return NULL;
  (g->parse_errmsg)=wr_malloc(g,1000);
  if (!(g->parse_errmsg)) return NULL;
  tmp1=snprintf((g->parse_errmsg),50,"{\"error\": \"clausification error: ");
  tmp2=vsnprintf((g->parse_errmsg)+tmp1,800,format,args);
  snprintf((g->parse_errmsg)+tmp1+tmp2,50,"\"}");
  va_end (args);
  //if (g && format && !strncmp(format,"could",5)) (g->parse_errflag)=1;
  //printf("\n(g->parse_errflag) %d\n",(g->parse_errflag));
  //printf("\n");
  return NULL;
}

/*
void* show_clausify_warning(glb* g, char* format, ...) {
  va_list args;
  va_start (args, format);
#ifdef PRINTERR  
  printf("*** Clausify warning: ");
  vprintf (format, args);
#endif  
  va_end (args);
  if (g && format && !strncmp(format,"could",5)) (g->parse_errflag)=1;
  //printf("\n(g->parse_errflag) %d\n",(g->parse_errflag));
  printf("\n");
  return NULL;
}
*/
