/*
*
* Copyright (c) Tanel Tammet 2004-2019
*
* Contact: tanel.tammet@gmail.com                 
*
* This file is part of WhiteDB
*
* WhiteDB is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
* 
* WhiteDB is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License
* along with WhiteDB.  If not, see <http://www.gnu.org/licenses/>.
*
*/

 /** @file strat.c
 *  Derivation strategy implementations/filters.
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
 

/* ====== Private headers and defs ======== */

//#define DEBUG
#undef DEBUG
//#define EQORDER_DEBUG
#define QUIET
//#undef QUIET

#define EQ_ORDER_VAR_WEIGHT 10 // should be minimal value of any const/funs weight

/* ======= Private protos ================ */


// term ordering for equality

static int wr_order_eqterms_occurs_in(glb* g, gint x, gint y, gptr vb);
static int wr_order_eqterms_const_weight(glb* g, gint a);
static int wr_order_eqterms_weight_vars(glb* g, gint x, gptr vars, gptr vb);
static int wr_order_eqterms_lex_order(glb* g, gint x, gint y, gptr vb);
static int wr_order_eqterms_const_lex_smaller(glb* g, gint x, gint y);

/* ====== Functions ============== */


int wr_cl_is_goal(glb* g, gptr cl) {
  gint hist, prior, decprior;
  gptr histptr;
  int htype;

  
  printf("\n wr_cl_is_goal:");
  wr_print_clause(g,cl);
  printf("\n");
  
  hist=wr_get_history(g,cl);
  if (hist) {
    htype=wg_get_encoded_type(g->db,hist);  
    if (htype!=WG_RECORDTYPE) return 0;
    histptr=rotp(g,hist);
    prior=wr_get_history_record_field(g,histptr,HISTORY_PRIORITY_POS);
    if (!prior) return 0;
    decprior=wr_decode_priority(g,prior);
    if (decprior==WR_HISTORY_GOAL_ROLENR) {
      printf("\n is goal!\n");
      return 1;
    } else {
      //printf("\n not marked given\n");
      return 0;
    }
  }
  return 0;
}

int wr_cl_is_assumption(glb* g, gptr cl) {
  gint hist, prior, decprior;
  gptr histptr;
  int htype;

  /*
  printf("\n wr_cl_is_assumptionl:");
  wr_print_clause(g,cl);
  printf("\n");
  */

  if (hist) {
    htype=wg_get_encoded_type(g->db,hist);  
    if (htype!=WG_RECORDTYPE) return 0;
    histptr=rotp(g,hist);
    prior=wr_get_history_record_field(g,histptr,HISTORY_PRIORITY_POS);
    if (!prior) return 0;
    decprior=wr_decode_priority(g,prior);
    if (decprior==WR_HISTORY_ASSUMPTION_ROLENR) {
      printf("\n is assumption!\n");
      return 1;
    } else {
      //printf("\n not marked given\n");
      return 0;
    }
  }
  return 0;
}

int wr_is_unit_cl(glb* g, gptr cl, int ruleflag, int len) {
  void* db;  
  int i, anscount;
  gint atom;

  if (!ruleflag) return 1;
  if (len<2) return 1;      
  anscount=0;
  db=g->db; 
  for(i=0; i<len; i++) {          
    atom=wg_get_rule_clause_atom(db,cl,i);
    if (wr_answer_lit(g,atom)) anscount++;
  }
  if ((len-anscount)<2) return 1;
  return 0;  
}

int wr_isunit_cl(glb* g, gptr cl) {
  void* db=g->db;  
  int i, anscount;
  gint atom;
  int len;

  if (!wg_rec_is_rule_clause(db,cl)) return 1;
  len=wg_count_clause_atoms(db,cl);
  if (len<2) return 1;      
  anscount=0; 
  for(i=0; i<len; i++) {          
    atom=wg_get_rule_clause_atom(db,cl,i);
    if (wr_answer_lit(g,atom)) anscount++;
  }
  if ((len-anscount)<2) return 1;
  return 0;  
}

int wr_count_cl_nonans_atoms(glb* g, gptr cl) {
  void* db=g->db;  
  int i, anscount;
  gint atom;
  int len;

  if (!wg_rec_is_rule_clause(db,cl)) return 1;
  len=wg_count_clause_atoms(db,cl);
  if (len<2) return len;      
  anscount=0; 
  for(i=0; i<len; i++) {          
    atom=wg_get_rule_clause_atom(db,cl,i);
    if (wr_answer_lit(g,atom)) anscount++;
  }
  return len-anscount;  
}


/*

  always set nonanslen

  default assume *posok and *negok are 1:
  set to 0 only if prohibited

*/

void wr_set_stratlimits_cl(glb* g, gptr cl, int ruleflag, int len, int* posok, int* negok, int* nonanslen) {
  void* db;
  int poscount, negcount, anscount;
  int i;
  gint meta, atom;

  if (!ruleflag) {
    *nonanslen=1;
    return;
  } 
  db=g->db;
  poscount=0; // count non-ans positive atoms
  negcount=0; 
  anscount=0; // count all ans (assume non-neg)
  for(i=0; i<len; i++) {          
    meta=wg_get_rule_clause_atom_meta(db,cl,i);
    if (wg_atom_meta_is_neg(db,meta)) negcount++;
    else {
      atom=wg_get_rule_clause_atom(db,cl,i);
      if (wr_answer_lit(g,atom)) anscount++; 
      else poscount++;
    } 
  }
  *nonanslen=len-anscount;
  // prohibit pos or neg       
  if (g->negpref_strat) {
    if (poscount>0 && negcount>0) *posok=0;
  }      
  if (g->pospref_strat) {
    if (poscount>0 && negcount>0) *negok=0;
  }
}

int wr_order_resolvable_atom(glb* g, int negflag,  
      int negok, int posok, int negadded, int posadded) {
  if (g->negpref_strat) {
    if (negflag && negok && !negadded) return 1;
    else if (!negflag && posok) return 1;
    else return 0;
  }
  if (g->pospref_strat) {
    if (!negflag && posok && !posadded) return 1;
    else if (negflag && negok) return 1;
    else return 0;
  }
  return 1;
}

int wr_initial_select_active_cl(glb* g, gptr cl) {
  void* db=g->db;
  int ruleflag, len, i;
  int negcount=0, poscount=0, anscount=0;
  gint meta, atom;
  int res;
  gint history,priority,decpriority;
  int htype;

#ifdef DEBUG  
  dp("\ncheck initial cl: ");
  wr_print_clause(g,cl);
  dprintf("\n");
#endif
  /*
  printf("\ncheck initial cl: ");
  wr_print_clause(g,cl);
  printf("\n");
  */
  history=wr_get_history(g,cl); 
  if (!history) return 1;
  htype=wg_get_encoded_type(db,history);  
  if (htype!=WG_RECORDTYPE) return 1;
  priority=wr_get_history_record_field(g,otp(db,history),HISTORY_PRIORITY_POS);
  decpriority=wg_decode_int(db,priority);

  //printf("\ndecpriority before: %d \n",decpriority);

  if ((g->cl_pick_queue_strategy)==2) {
      // make non-included axioms assumptions and positive conjecture part assumptions
      if (decpriority==WR_HISTORY_GOAL_ROLENR && wr_is_positive_unit_cl(g,cl)) 
        decpriority=WR_HISTORY_ASSUMPTION_ROLENR;        
      else if (decpriority==WR_HISTORY_AXIOM_ROLENR && (g->parse_is_included_file))
        decpriority=WR_HISTORY_ASSUMPTION_ROLENR;       
  } else if ((g->cl_pick_queue_strategy)==3) {
    // only fully negative clauses of goal made goal and no assumptions (ie made axioms)
    if (decpriority==WR_HISTORY_GOAL_ROLENR && !wr_is_negative_cl(g,cl))
      decpriority=WR_HISTORY_AXIOM_ROLENR;  
    else if (decpriority==WR_HISTORY_ASSUMPTION_ROLENR)  
      decpriority==WR_HISTORY_AXIOM_ROLENR;
  } 

  //printf("\ndecpriority after: %d \n",decpriority);

  if (!(g->queryfocusneg_strat)) {
    if (decpriority==WR_HISTORY_EXTAXIOM_ROLENR) {
      //printf("\n returning 1, case 1\n");
      return 1;
    } else if (decpriority==WR_HISTORY_AXIOM_ROLENR && (g->cl_pick_queue_strategy)==1) {
      //printf("\n returning 1, case 2\n");
      return 1;
    } else if (decpriority==WR_HISTORY_AXIOM_ROLENR && (g->cl_pick_queue_strategy)==2) {
      //printf("\n returning 0, case 3\n");
      return 0;  
    } else if (decpriority==WR_HISTORY_AXIOM_ROLENR && (g->cl_pick_queue_strategy)==3) {
      //printf("\n returning 1, case 4\n");
      return 1;  
    } else if (decpriority==WR_HISTORY_ASSUMPTION_ROLENR && (g->cl_pick_queue_strategy)==3) {
      //printf("\n returning 1, case 5\n");
      return 1;   
    } else if (decpriority==WR_HISTORY_GOAL_ROLENR  && (g->cl_pick_queue_strategy)==3 &&
             !wr_is_negative_cl(g,cl)) {
      //printf("\n returning 1, case 6\n");         
      return 1;       
    } else {
      //printf("\n returning 0\n");
      return 0;         
    }  
    //if (!decpriority || decpriority>5) return 1;
    //else return 0;
  }
  // here we have queryfocusneg_strat
  if (decpriority>1) return 1;
  ruleflag=wg_rec_is_rule_clause(db,cl);
  if (!ruleflag) {
    len=1;  
    poscount=1;
  } else {
    len = wg_count_clause_atoms(db, cl);
    for(i=0; i<len; i++) {          
      meta=wg_get_rule_clause_atom_meta(db,cl,i);
      if (wg_atom_meta_is_neg(db,meta)) negcount++;
      else {
        atom=wg_get_rule_clause_atom(db,cl,i);
        if (wr_answer_lit(g,atom)) anscount++; 
        else poscount++;
      } 
    }
  }  
  //if (len==1) res=1;  
  if (anscount>0 || negcount==len)  res=0;
  else res=1;
  
  /*
  dp("\ncheck initial cl res is %d \n",res)
  */
  return res;
}

/* -------------

  equality ordering and restrictions 

---------------- */

// result:
//
// 0: none bigger, neither ok for para
// 1: a bigger than b (prohibits b)
// 2: b bigger than a (prohibits a)
// 3: none bigger, both ok for para


int wr_order_eqterms(glb* g, gint a, gint b, gptr vb) {  
  int res=-1;
  gint aweight, bweight;
  //gptr avars, bvars;
  int xw, yw, lexorder;
  gint tmp; // for VARVAL_F

  //return 3;

#ifdef EQORDER_DEBUG  
  dp("\n *+*+*+*+*+*+*+* wr_order_eqterms called on terms a and b: ");
  wr_print_term(g,a);
  dprintf("\n");
  wr_print_term(g,b);
  dprintf("\n");
#endif

  if (isvar(a) && vb!=NULL) a=VARVAL_F(a,vb);
  if (isvar(b) && vb!=NULL) b=VARVAL_F(b,vb);

  // first analyze simple cases: a or b is a var or constant

  if (isvar(a)) {
    if (isvar(b)) {
      // both are vars
      res=3;
    } else if (isdatarec(b) && wr_order_eqterms_occurs_in(g,a,b,vb)) {
      // b is a function term and a is inside b
      res=2;      
    } else {
      // b is a constant or fun term and a does not occur in b
      res=3;
    }
  } else if (isvar(b)) {
    // b is a var, a is not a var
    if (isdatarec(a) && wr_order_eqterms_occurs_in(g,b,a,vb)) {
      // a is a function term and b is inside a
      res=1;
    } else {
      // a is a constant or fun term and b does not occur in a
      res=3;
    }
  } else if (!isdatarec(a)) {
    // a is a constant
    if (isdatarec(b)) {
      // b is ok, a is prohibited
      res=2;
    } else {
      // b is a constant
      aweight=wr_order_eqterms_const_weight(g,a);
      bweight=wr_order_eqterms_const_weight(g,b);
      if (aweight>bweight) res=1;
      else if (bweight>aweight) res=2;
      else res=3;     
    } 
  } else if (!isdatarec(b)) {
    // b is a constant
    // here a must be a datarec   
    res=1;  
  }
  if (res>=0) {
#ifdef EQORDER_DEBUG  
    dp("\n wr_order_eqterms returns on simple comparison: %d\n",res); 
#endif    
    return res;
  }

  // here a complex case: both a and b are datarecs
  
  wr_clear_countedvarlist(g,(g->xcountedvarlist));
  xw=wr_order_eqterms_weight_vars(g, a, (g->xcountedvarlist), vb);
#ifdef EQORDER_DEBUG    
  wr_show_countedvarlist(g,(g->xcountedvarlist));
#endif 
  wr_clear_countedvarlist(g,(g->ycountedvarlist));
  yw=wr_order_eqterms_weight_vars(g, b, (g->ycountedvarlist), vb);
#ifdef EQORDER_DEBUG    
  wr_show_countedvarlist(g,(g->ycountedvarlist));
#endif   
  
#ifdef EQORDER_DEBUG  
    dp("\n wr_order_eqterms calcs xw and yw: %d %d\n",xw,yw); 
    if (wr_countedvarlist_is_subset(g,(g->xcountedvarlist),(g->ycountedvarlist))) {
      printf(" xlist is a subset of ylist \n");
    } else if (wr_countedvarlist_is_subset(g,(g->ycountedvarlist),(g->xcountedvarlist))) {
      printf(" ylist is a subset of xlist \n");
    } else {
      printf(" xlist and ylist are not subsets of each other\n");
    }
#endif   

  if (xw>yw && wr_countedvarlist_is_subset(g,(g->ycountedvarlist),(g->xcountedvarlist))) {
    res=1;
  } else if (yw>xw && wr_countedvarlist_is_subset(g,(g->xcountedvarlist),(g->ycountedvarlist))) {
    res=2;
  } else if (xw==yw) {
    // here sizes are the same
    lexorder=wr_order_eqterms_lex_order(g,a,b,vb);
#ifdef EQORDER_DEBUG    
    printf("\nlexorder result: %d\n",lexorder);
#endif       
    // lexorder:
    // 1 if x is smaller than y
    // 2 if y is smaller than x
    if (lexorder==2 && wr_countedvarlist_is_subset(g,(g->ycountedvarlist),(g->xcountedvarlist)))
       res=1; // a bigger than b (prohibits b)
    else if (lexorder==1 && wr_countedvarlist_is_subset(g,(g->xcountedvarlist),(g->ycountedvarlist)))   
       res=2; // b bigger than a (prohibits a)
    else 
       res=3;
  } else {
    res=3;
  }        

  #ifdef EQORDER_DEBUG  
    dp("\n wr_order_eqterms returns on complex comparison: %d\n",res); 
  #endif    

  return res;
}

static int wr_order_eqterms_const_weight(glb* g, gint a) {
  return 10;
}


/*
#define VARVAL(x,vb) (wr_varval(x,vb))
#define VARVAL_F(x,vb) (tmp=vb[decode_var(x)],
 ((tmp==UNASSIGNED) ? x : (!isvar(tmp) ? tmp : wr_varval(tmp,vb)))) 
#define VARVAL_DIRECT(x,vb) (vb[decode_var(x)])
*/

void wr_clear_countedvarlist(gptr g, cvec varlist) {
  varlist[1]=4;
  varlist[2]=-1;
}

/*

  varlist[0]: size of cvec
  varlist[1]: ptr to next free, initially 4, increased by 2
  varlist[2]: max var stored, initially -1
  varlist[4]: first var stored
  varlist[5]: count of first var stored (initially 1)
  ...

*/

void wr_record_varocc(gptr g, gint x, cvec varlist) {
  int i, nxt;  

  if (x<=varlist[2]) {
    // possibly already added
    for(i=4; i<varlist[1]; i=i+2) {
      if (varlist[i]==x) {
        (varlist[i+1])++; // increase count
        return;
      }  
    }
  }  
  // here var was not found
  nxt=varlist[1];
  if ((nxt+1)>=varlist[0]) {
    // not enough space, do not store
    return;
  }
  if (x>varlist[2]) varlist[2]=x; // update max var if needed 
  varlist[nxt]=x;   // store var itself
  varlist[nxt+1]=(gint)1; // store count: initially 1
  varlist[1]=nxt+2; // increase freeptr
}

void wr_show_countedvarlist(gptr g, cvec varlist) {
  int i;
  
  printf("varlist len %d min var ",(varlist[1]-4)/2);
  if (varlist[2]>=0) wr_print_term(g,varlist[2]);
  else printf(" none");
  printf(" vars ");
  for(i=4; i<varlist[1]; i=i+2) {
    wr_print_term(g,varlist[i]);
    printf(": %d, ",varlist[i+1]);
  }
}

static int wr_order_eqterms_occurs_in(glb* g, gint x, gint y, gptr vb) { 
  void* db=g->db;
  gptr yptr;
  gint yi;
  int ylen,ilimit,i;
  gint tmp; // used by VARVAL_F
    
#ifdef DEBUG
  printf("wr_order_eqterms_occurs_in called with x %d ",x);
  wr_print_term(g,x);
  printf(" and y %d ",y);
  wr_print_term(g,y);
  printf("\n");
#endif 
  yptr=decode_record(db,y);
  ylen=get_record_len(yptr);
  
  if (g->unify_maxuseterms) {
    if (((g->unify_maxuseterms)+(g->unify_firstuseterm))<ylen) 
      ylen=(g->unify_firstuseterm)+(g->unify_maxuseterms);
  }    
  ilimit=RECORD_HEADER_GINTS+ylen;
  for(i=RECORD_HEADER_GINTS+(g->unify_firstuseterm); i<ilimit; i++) {     
    yi=*(yptr+i);
    if (isvar(yi) && vb!=NULL) yi=VARVAL_F(yi,vb);    
    if (x==yi) return 1;
    if (isdatarec(yi)) {
      if (wr_order_eqterms_occurs_in(g,x,yi,vb)) return 1;
    }
  }      
  // passed vector ok, not finding x inside  
  return 0;    
}  


static int wr_order_eqterms_weight_vars(glb* g, gint x, gptr vars, gptr vb) {
  void* db;
  gptr xptr;
  int i, start, end;  
  int w;
  gint tmp; // used by VARVAL_F

#ifdef DEBUG
  printf("wr_order_eqterms_weight_vars called with x %d ",x);
  wr_print_term(g,x);
  printf("\n");
#endif     

  if (isvar(x) && vb!=NULL) x=VARVAL_F(x,vb);
  if (!isdatarec(x)) {
    // now we have a simple value     
    if (isvar(x)) {
      // store var and return 0 as weight
      wr_record_varocc(g,x,vars);
      return EQ_ORDER_VAR_WEIGHT; 
    } else {
      return wr_order_eqterms_const_weight(g,x);   
    }  
  }   
  // now we have a datarec
  db=g->db;
  xptr=decode_record(db,x);
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,xptr);
  w=0;    
  for(i=start;i<end;i++) {
    w=w+wr_order_eqterms_weight_vars(g,xptr[i],vars,vb);      
  }   
  return w;
} 

/*

  return 1 iff xlist is a counted subset of ylist.
  else return 0

*/

int wr_countedvarlist_is_subset(gptr g, cvec xlist, cvec ylist) {
  int i, j, var, found;  

  // if both empty, then xlist is a sublist:
  if (xlist[1]==4 && ylist[1]==4) return 1;
  // else if only ylist is empty, then xlist cannot be a sublist
  if (ylist[1]==4) return 0;
  // here both xlist and ylist are nonempty

  // longer cannot be a subset of shorter:
  if (xlist[1]>ylist[1]) return 0;
  // max of shorter list cannot be bigger than max of longer list
  if (xlist[2]>ylist[2]) return 0;
  // outer loop over shorter, i.e. xlist
  for(i=4; i<xlist[1]; i=i+2) {
    var=xlist[i];
    found=0;
    // inner loop over longer. i.e. ylist
    for(j=4; j<ylist[1]; j=j+2) {
      if (var==ylist[j]) {
        // var found
        if (xlist[i+1]>ylist[i+1]) return 0; // too many occs in xlist
        else {
          found=1;
          break;
        }
      }
    }
    if (!found) return 0;
  }
  return 1;
}

/* 

  returns:
    0 if terms are lex-equal
    1 if x is smaller than y
    2 if y is smaller than x
    3 if terms are not lex-comparable

*/


static int wr_order_eqterms_lex_order(glb* g, gint x, gint y, gptr vb) {
  void* db;
  gptr xptr,yptr;
  //int i, start, end;  
  //int w;
  gint encx,ency,tmp; // used by VARVAL_F
  int xlen,ylen,uselen,ilimit,i;

#ifdef EQORDER_DEBUG
  printf("wr_order_eqterms_lex_order called with x %d and y %d: ",x,y);
  wr_print_term(g,x);
  printf("\n");
  wr_print_term(g,y);
  printf("\n");
#endif     

  if (x==y) return 0; // equal
  if (vb!=NULL) {
    if (isvar(x)) x=VARVAL_F(x,vb);
    if (isvar(y)) y=VARVAL_F(y,vb);
    if (x==y) return 0; // equal
  } 
  if (isvar(x) || isvar(y)) return 3; // not comparable
  // here none can be a var   
  if (!isdatarec(x)) {
    // now x has a have a simple value     
    if (!isdatarec(y)) {
      if (wr_order_eqterms_const_lex_smaller(g,x,y)) return 1;
      else return 2;      
    } else {
      // consider function terms as lex bigger than constants
      return 1; 
    }
  } else if (!isdatarec(y)) {
    // x is a function term and y is a constant
    return 2;
  }
  // x and y are both complex terms     
  db=g->db;
  xptr=decode_record(db,x);
  yptr=decode_record(db,y);
  xlen=get_record_len(xptr);
  ylen=get_record_len(yptr);
  // let smaller-arity funs be lex-smaller
  if (xlen<ylen) return 1;
  else if (ylen<xlen) return 2;
  // here the arities are same
  uselen=xlen;
  if (g->unify_maxuseterms) {
    if (((g->unify_maxuseterms)+(g->unify_firstuseterm))<uselen) 
      uselen=(g->unify_firstuseterm)+(g->unify_maxuseterms);
  }    
  ilimit=RECORD_HEADER_GINTS+uselen;
  for(i=RECORD_HEADER_GINTS+(g->unify_firstuseterm); i<ilimit; i++) {
    encx=*(xptr+i);
    ency=*(yptr+i);    
    if (encx==ency) continue;
    tmp=wr_order_eqterms_lex_order(g,encx,ency,vb);
    if (tmp==0) continue;
    else return tmp;   
  }      
  return 0;        
}        

static int wr_order_eqterms_const_lex_smaller(glb* g, gint x, gint y) {
  if (x<y) return 1;
  else return 0; 
}


#ifdef __cplusplus
}
#endif
