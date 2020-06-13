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

 /** @file strat.c
 *  Derivation strategy implementations/filters.
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

//#define DEBUG
//#undef DEBUG
//#define EQORDER_DEBUG
#define QUIET
//#undef QUIET

#define NORMAL_CONSTANT_ORDER
#define EQ_ORDER_VAR_WEIGHT 10 // should be minimal value of any const/funs weight

#define SUBLISTS_FIXED // if defined, important june fix used
//#define OCC_LEX_ORDER // if defined, lex ordering is based on occurrences
//#define KB_WEIGHTS_ORDER // if defined, kb weights are based on occurrences
//#define KB_WEIGHTS_ORDER_REVERSE

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

  
  //printf("\n wr_cl_is_goal:");
  //wr_print_clause(g,cl);
  //printf("\n");
  
  hist=wr_get_history(g,cl);
  if (hist) {
    htype=wg_get_encoded_type(g->db,hist);  
    if (htype!=WG_RECORDTYPE) return 0;
    histptr=rotp(g,hist);
    prior=wr_get_history_record_field(g,histptr,HISTORY_PRIORITY_POS);
    if (!prior) return 0;
    decprior=wr_decode_priority(g,prior);
    if (decprior==WR_HISTORY_GOAL_ROLENR) {
      //printf("\n is goal!\n");
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
  wr_printf("\n wr_cl_is_assumptionl:");
  wr_print_clause(g,cl);
  wr_printf("\n");
  */

  hist=wr_get_history(g,cl); // !!! was wrong: hist was uninitialized
  if (hist) {
    htype=wg_get_encoded_type(g->db,hist);  
    if (htype!=WG_RECORDTYPE) return 0;
    histptr=rotp(g,hist);
    prior=wr_get_history_record_field(g,histptr,HISTORY_PRIORITY_POS);
    if (!prior) return 0;
    decprior=wr_decode_priority(g,prior);
    if (decprior==WR_HISTORY_ASSUMPTION_ROLENR) {
      //printf("\n is assumption!\n");
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

  UNUSED(db);
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

  UNUSED(db);
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

  UNUSED(db);
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
  wr_printf("\n(g->queryfocusneg_strat): %d\n",(g->queryfocusneg_strat));
#endif

  history=wr_get_history(g,cl); 
  if (!history) return 1;
  htype=wg_get_encoded_type(db,history);  
  if (htype!=WG_RECORDTYPE) return 1;

  if (g->unit_active_strat) {
    if (wr_count_cl_nonans_atoms(g,cl)<2) return 1;
  }

  priority=wr_get_history_record_field(g,otp(db,history),HISTORY_PRIORITY_POS);
  decpriority=wg_decode_int(db,priority);

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
      decpriority=WR_HISTORY_AXIOM_ROLENR; // !! wrong had no effect
  } 

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
  }
  // here we have queryfocusneg_strat 
 
  ruleflag=wg_rec_is_rule_clause(db,cl);
  //printf("\n ruleflag: %d\n",ruleflag);
  if (!ruleflag) {
    len=1;  
    poscount=1;
    if ((g->cl_pick_queue_strategy)==1)
    return 0;
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
  if (anscount>0 || negcount==len)  res=0;
  else res=1;

  return res;
}

/*
 
  resolvability ordering

*/

int wr_calc_clause_resolvability(glb* g, gptr cl, int allowall, int hyperpartial) {
  void* db=g->db;
  int atomnr,i, polarity, hardness; 
  int max_hardness=MIN_HARDNESS;   
  int max_neg_hardness=MIN_HARDNESS;
  int max_pos_hardness=MIN_HARDNESS;
  gint meta, atom; 
  int poscount=0,negcount=0,anscount=0;
  int allowedflag, tmp;

  UNUSED(db);
  if (!wg_rec_is_rule_clause(db,cl)) {
    (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,1,1);
    return 1;
  }  
  atomnr=wg_count_clause_atoms(db,cl);
  if (atomnr==1) {
    (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,1,1);
    return 1;
  }

  // from here clause length at least 2

  if (!allowall && (g->knuthbendixpref_strat)) {
    // use a separate knuthbendix resolvability marker procedure
    tmp=wr_calc_clause_knuthbendix_resolvability(g,cl,(g->varbanks));
    if (!tmp) return 0; // error case
    return 1;
  }

  if (!allowall && (g->hyperres_strat)) {
    // initially set all literals as allowed
    for(i=0; i<atomnr; i++) {
      (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,1);
    }  
    // count pos and neg
    poscount=0;    
    anscount=0;
    for(i=0; i<atomnr; i++) {          
      meta=wg_get_rule_clause_atom_meta(db,cl,i);
      atom=wg_get_rule_clause_atom(db,cl,i);
      if (wr_answer_lit(g,atom)) anscount++;       
      else if (wg_atom_meta_is_neg(db,meta)) negcount++; 
      else poscount++;           
    }
    // if not hyperpartial and some negative, make positive ones prohibited
    if (negcount && !hyperpartial) {
      for(i=0; i<atomnr; i++) {          
        meta=wg_get_rule_clause_atom_meta(db,cl,i);            
        if (!wg_atom_meta_is_neg(db,meta)) {
          // prohibit pos atoms since there are some negative ones
          (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,0);        
        }
      }
      return 1;
    }
    // here either all pos or hyperpartial: use hardness in the next part          
  }  

  // from here cannot be knuthbendix or hyper with some negative literals
  // count neg and pos and hardnesses for polarityorder and queryfocus

  if (!allowall && 
       ( (g->negpref_strat) || (g->pospref_strat) || (g->hyperres_strat) ||
         (g->queryfocus_strat) || (g->hardnesspref_strat))) {  
    // count pos and neg
    for(i=0; i<atomnr; i++) {          
      meta=wg_get_rule_clause_atom_meta(db,cl,i);
      atom=wg_get_rule_clause_atom(db,cl,i);
      if (wr_answer_lit(g,atom)) anscount++;       
      else if (wg_atom_meta_is_neg(db,meta)) negcount++; 
      else poscount++;           
    }  

    // hyperres start: does everything for neg-containing non-partial
    if (g->hyperres_strat) {
      // initially set all literals as allowed
      for(i=0; i<atomnr; i++) {
        (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,1);
      }        
      // if not hyperpartial and some negative, make positive ones prohibited
      if (negcount && !hyperpartial) {
        for(i=0; i<atomnr; i++) {          
          meta=wg_get_rule_clause_atom_meta(db,cl,i);            
          if (!wg_atom_meta_is_neg(db,meta)) {
            // prohibit pos atoms since there are some negative ones
            (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,0);        
          }
        }
        return 1;
      }
      // here either all pos or hyperpartial: use hardness in the next part          
    }  

    // calc hardnesses 
    for(i=0; i<atomnr; i++) {          
      meta=wg_get_rule_clause_atom_meta(db,cl,i);
      atom=wg_get_rule_clause_atom(db,cl,i);
      if (wr_answer_lit(g,atom)) continue;
      // count pos/neg
      if (wg_atom_meta_is_neg(db,meta)) {
        // negative lit
        polarity=0;        
        if ((g->negpref_strat) || (g->queryfocus_strat) || (g->hardnesspref_strat) ||
            (g->hyperres_strat)) {
          // for negative literals only needed for negpref and hyper
          hardness=wr_calc_atom_hardness(g,polarity,atom);
          (g->tmp_hardnessinf_vec)=wr_vec_store(g,g->tmp_hardnessinf_vec,i+1,hardness);            
          if (hardness>max_neg_hardness) max_neg_hardness=hardness;
          if (hardness>max_hardness) max_hardness=hardness;
        } else {
          // mark that hardness not calced
          (g->tmp_hardnessinf_vec)=wr_vec_store(g,g->tmp_hardnessinf_vec,i+1,-1000);
        }
      } else {    
        // positive lit 
        polarity=1; 
        if ((g->pospref_strat) || (g->queryfocus_strat) || (g->hardnesspref_strat)) {
          // for positive literals only needed for pospref
          hardness=wr_calc_atom_hardness(g,polarity,atom);
          (g->tmp_hardnessinf_vec)=wr_vec_store(g,g->tmp_hardnessinf_vec,i+1,hardness);             
          if (hardness>max_pos_hardness) max_pos_hardness=hardness;
          if (hardness>max_hardness) max_hardness=hardness;
        } else {
          // mark that hardness not calced
          (g->tmp_hardnessinf_vec)=wr_vec_store(g,g->tmp_hardnessinf_vec,i+1,-1000);
        }  
      }          
    }    
  }
  
  // TESTING: normal no 0 blocker
  if (!allowall && negcount==0 && ((g->negpref_strat) || (g->hyperres_strat))) { 
    // pure positive for neg order:
    // use a separate knuthbendix resolvability marker procedure
    tmp=wr_calc_clause_knuthbendix_resolvability(g,cl,(g->varbanks));
    if (!tmp) return 0; // error case
    return 1;
  }
  // TESTING: normal no 0 blocker
  if (!allowall && poscount==0 && (g->pospref_strat)) {
    // pure negative for pos order:
    // use a separate knuthbendix resolvability marker procedure
    tmp=wr_calc_clause_knuthbendix_resolvability(g,cl,(g->varbanks));
    if (!tmp) return 0; // error case
    return 1;
  }

  // next loop over clause marks allowed and prohibited literals

  allowedflag=0;
  for(i=0;i<atomnr;i++) {
    meta=wg_get_rule_clause_atom_meta(db,cl,i);  
    atom=wg_get_rule_clause_atom(db,cl,i);  
    if (wg_atom_meta_is_neg(db,meta)) polarity=0;
    else polarity=1;            
    if (wr_answer_lit(g,atom)) {
      (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,0);
      continue;
    } 
    if (allowall) {
      (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,1);
      continue;
    }
    if (g->knuthbendixpref_strat) {
      // order clause by knuth bendix: no hardness
    } else {
      // order clause by pos/neg, using additionally hardness      
      if (g->negpref_strat) {
        if (polarity) {
          // positive lit
          if (negcount>0) {
            // neg is preferred to current atom, prohibit
            (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,0);
          } else {
            // no neg, ok to resolve
            if (g->queryfocus_strat) { // TESTING: normally no 1 enforcer
              // resolve only hardest pos
              if (allowedflag || (((g->tmp_hardnessinf_vec)[i+1])<max_pos_hardness)) {
                // prohibit
                (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,0);
              } else {               
                (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,1);
                allowedflag=1;                
              }
            } else {  
              // here always resolve pos
              (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,1);
              allowedflag=1;
            }  
          }  
        } else {
          // negative: use hardness to determine whether to prohibit
          if (((g->tmp_hardnessinf_vec)[i+1])<max_neg_hardness) {
            // prohibit
            (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,0);
          } else {
            // allow if not already allowed (since may have equal hardnesses)
            if (!allowedflag) {
              (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,1);
              allowedflag=1;
            } else {
              (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,0);
            }
          }
        }               
      } else if (g->pospref_strat) {
        if (!polarity) {
          // negative lit
          if (poscount>0) {
            // pos is preferred to current atom, prohibit
            (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,0);
          } else {
            // no pos, ok to resolve
            if (g->queryfocus_strat) { // TESTING: normally no 1 enforcer
              // resolve only hardest neg
              if (allowedflag || (((g->tmp_hardnessinf_vec)[i+1])<max_neg_hardness)) {
                // prohibit
                (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,0);
              } else {                 
                (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,1);
                allowedflag=1;                
              }
            } else {  
              // always resolve neg
              (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,1);
              allowedflag=1;
            }
          }  
        } else {
          // positive: use hardness to determine whether to prohibit
          if (((g->tmp_hardnessinf_vec)[i+1])<max_pos_hardness) {
            // prohibit
            (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,0);
          } else {
            // allow if not already allowed (since may have equal hardnesses)
            if (!allowedflag) {
              (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,1);
              allowedflag=1; 
            } else {
              (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,0);
            }
          }
        }       
      } else if (g->queryfocus_strat) {
        // pure queryfocus: use just hardness
        if (((g->tmp_hardnessinf_vec)[i+1])<max_hardness) {
          (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,0);      
        } else {
          if (!allowedflag) {
            (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,1);
            allowedflag=1;   
          } else {
            (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,0);
          }    
        }  
      } else if ((g->hyperres_strat) &&  hyperpartial && negcount) {
        // here we assume some negative present
        // use just hardness for negative, prohibit positive
        if (!wg_atom_meta_is_neg(db,meta)) {
          (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,0);
        } else if (((g->tmp_hardnessinf_vec)[i+1])<max_neg_hardness) {
          (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,0);      
        } else {
          if (!allowedflag) {
            (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,1);
            allowedflag=1;   
          } else {
            (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,0);
          }    
        }                          
      } else if (g->hardnesspref_strat) {
        // use just hardness
        if (((g->tmp_hardnessinf_vec)[i+1])<max_hardness) {
          (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,0);      
        } else {
          if (!allowedflag) {
            (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,1);
            allowedflag=1;   
          } else {
            (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,0);
          }    
        }                 
      } else {
        // no known ordering: allow all literals except ans
        (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,1);

      }
    }   
  }
  return 1;
} 


void wr_print_clause_resolvability(glb* g, gptr cl) {
  int len,i;

  wr_printf("\nresolvability for ");
  wr_print_clause(g,cl);
  wr_printf("\n");  
  if (!wg_rec_is_rule_clause(db,cl)) {
    wr_printf("1, since fact clause\n");
  } else {
    len=wg_count_clause_atoms(db,cl);
    for(i=0;i<len;i++) {
      wr_printf("%ld ",(g->tmp_resolvability_vec)[i+1]);
    }
    wr_printf("\n");
  }
}

void wr_print_clause_hardnesses(glb* g, gptr cl) {
  int len,i;

  wr_printf("\nhardness for ");
  wr_print_clause(g,cl);
  wr_printf("\n");  
  if (!wg_rec_is_rule_clause(db,cl)) {
    wr_printf("fact clause\n");
  } else {
    len=wg_count_clause_atoms(db,cl);
    for(i=0;i<len;i++) {
      wr_printf("%ld ",(g->tmp_hardnessinf_vec)[i+1]);
    }
    wr_printf("\n");
  }
}

/*

 hardness 

*/

int wr_calc_clause_hardnesses(glb* g, gptr cl, 
      int* max_pos_hardness, int* max_neg_hardness) {
  void* db=g->db;
  int atomnr,i, polarity,hardness;  
  int max_hardness=MIN_HARDNESS;
  gint meta, atom; 

  UNUSED(db);
  if (!wg_rec_is_rule_clause(db,cl)) return MIN_HARDNESS;
  /*
  wr_printf("\nwr_calc_clause_hardnesses for \n");  
  wr_print_clause(g,cl);
  wr_printf("\n");
  */
  atomnr=wg_count_clause_atoms(db,cl);
  for(i=0;i<atomnr;i++) {
    meta=wg_get_rule_clause_atom_meta(db,cl,i);  
    atom=wg_get_rule_clause_atom(db,cl,i);  
    if (wg_atom_meta_is_neg(db,meta)) polarity=0;
    else polarity=1;
    
    
    if (wr_answer_lit(g,atom)) hardness=MIN_HARDNESS;
    else hardness=wr_calc_atom_hardness(g,polarity,atom);

    //printf("\n  atom ");
    //wr_print_term(g,atom);
    //printf("\n  hardness %d\n",hardness);
    
    //hardness=0;

    (g->tmp_hardnessinf_vec)=wr_vec_store(g,g->tmp_hardnessinf_vec,i+1,hardness);
    if (hardness>max_hardness) max_hardness=hardness;
    if (!polarity) {if (hardness>(*max_neg_hardness)) (*max_neg_hardness)=hardness;}
    else {if (hardness>(*max_pos_hardness)) (*max_pos_hardness)=hardness;}

    //printf("%d\n",hardness);
  }
  return max_hardness;
}


int wr_calc_atom_hardness(glb* g, int polarity, gint atom) {
  //void* db=g->db;  
  int vc_tmp;
  atom_hardnesscalc hdata;
  int hardness=0;

  /*
  wr_printf("\n wr_calc_atom_hardness for ");
  wr_print_atom_otter(g,atom,1);
  wr_printf("\n");
  */
  /*
  if (g->hardnesspref_strat){
    wr_printf("\n wr_calc_atom_hardness for ");
    wr_print_atom_otter(g,atom,1);
    wr_printf("\n");
  }
  */

  hdata.size=0;
  hdata.maxdepth=0;
  hdata.newvars=0;
  hdata.repvars=0;
  hdata.atomposocc=0;
  hdata.atomnegocc=0;
  hdata.internposocc=0;
  hdata.internnegocc=0;  

  vc_tmp=*(g->tmp_unify_vc);
  wr_calc_atom_hardness_aux(g,atom,0,0,&hdata,polarity,0);
  if (vc_tmp!=*(g->tmp_unify_vc)) {
    wr_clear_varstack_topslice(g,g->varstack,vc_tmp);
  }  

  hardness=(hdata.size-hdata.newvars)*5;
  //printf("result hardness cp0 %d\n",hardness);
  if (polarity) {
    // positive
    if (!hdata.atomnegocc) {
      hardness=100*hardness; 
    } else hardness-=(int)(round(log10((double)hdata.atomnegocc)));
    if (!hdata.internnegocc) {
      //hardness+=5;
    } else hardness-=(int)(round(log10((double)hdata.internnegocc)/(double)4));  
  } else {
    // negative
    if (!hdata.atomposocc) {
      hardness=100*hardness; 
    } else hardness-=(int)(round(log10((double)hdata.atomposocc)));
    if (!hdata.internposocc) {
      //hardness+=5;
    } else hardness-=(int)(round(log10((double)hdata.internposocc)/(double)4));
  }
  /*
  wr_printf("\nresult hardness %d polarity %d maxdepth %d size %d newvars %d repvars %d\n",
    hardness, polarity,hdata.maxdepth,hdata.size,hdata.newvars,hdata.repvars);
  wr_printf("\n       atomposocc %ld,atomnegocc %ld,internposocc %ld,internnegocc %ld;\n",
    hdata.atomposocc,hdata.atomnegocc,hdata.internposocc,hdata.internnegocc); 
  */
  /*
  if (g->hardnesspref_strat){
    wr_printf("\nresult hardness %d polarity %d maxdepth %d size %d newvars %d repvars %d\n",
      hardness, polarity,hdata.maxdepth,hdata.size,hdata.newvars,hdata.repvars);
    wr_printf("\n       atomposocc %ld,atomnegocc %ld,internposocc %ld,internnegocc %ld;\n",
      hdata.atomposocc,hdata.atomnegocc,hdata.internposocc,hdata.internnegocc);
  }
  */
  
  return hardness;
}

int wr_calc_atom_hardness_aux(glb* g, gint x, int depth, int pos,
      atom_hardnesscalc* hptr, int polarity, int haveextdb) {
  void* db;
  gptr xptr;
  int i, start, end, apos;
  int w, dtype;
  gint ucount, ucountpos, ucountneg;

  /*
  wr_printf("wr_calc_atom_hardness_aux called with x %d type %d depth %d size %d maxdepth %d\
         newvars,repvars\n",
         x,wg_get_encoded_type(g->db,x),depth,size,maxdepth,newvars,repvars);
  wr_print_term(g,x);
  wr_printf("\n");
  */

  if (!isdatarec(x)) {
    // now we have a simple value  
    (hptr->size)++;
    if (!isvar(x)) {
      db=g->db;
      dtype=wg_get_encoded_type(db,x);
      
      if (dtype==WG_URITYPE && !haveextdb) {    

        //printf("\nuri: ");
        //printf(" %s \n", wg_decode_unistr(db,x,WG_URITYPE));

        ucount=wg_decode_uri_count(db,x);
        ucountpos=ucount >> URI_COUNT_POSCOUNT_SHIFT;
        if (ucountpos>30000) ucountpos=30000;
        ucountneg=ucount & URI_COUNT_NEGCOUNT_MASK;
        if (ucountneg>10000) ucountneg=10000;

        if (depth==1 && pos==0) {
          (hptr->atomposocc)=ucountpos;
          (hptr->atomnegocc)=ucountneg;
        } else {
          (hptr->internposocc)+=ucountpos;
          (hptr->internnegocc)+=ucountneg;
        }
        /*
        wr_printf("\npolarity %d depth %d ucountpos %ld ucountneg %ld\n",polarity,depth,ucountpos,ucountneg);
        wr_printf("\n(hptr->atomposocc) %d (hptr->atomnegocc) %d \n",
          (hptr->atomposocc),(hptr->atomnegocc));
        */
      }
      return 10;
    }  
    // here x is a var    
    if (VARVAL_DIRECT(x,(g->varbanks))==UNASSIGNED) {
      // a new var 
      (hptr->newvars)++;
      //printf("\n a new var %ld where *(g->tmp_unify_vc) %ld\n",x,*(g->tmp_unify_vc));
      SETVAR(x,encode_smallint(1),g->varbanks,g->varstack,g->tmp_unify_vc);
      //printf("\n after setvar *(g->tmp_unify_vc) %ld\n",*(g->tmp_unify_vc));
      //printf("\n new var\n");
      return 1;
    } else {
      (hptr->repvars)++;
      // a var seen before
      //printf("\n old var\n");
      //printf("\n an old var %ld\n",x);
      return 2;
    }       
  }   
  // now we have a datarec
  db=g->db;
  xptr=decode_record(db,x);
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,xptr);
  w=0;    
  depth++;
  if (depth>(hptr->maxdepth)) (hptr->maxdepth)=depth;
  apos=0;
  for(i=start;i<end;i++) {
    w=w+wr_calc_atom_hardness_aux(g, xptr[i], depth, apos, hptr, polarity, haveextdb);
    apos++;      
  }   
  return 1;
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
  //gint aweight, bweight;
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
      // b is a constant: use offsets to compare between each other
      //aweight=a; 
      //bweight=b;      
      //if (aweight>bweight) res=1;
      //else if (bweight>aweight) res=2;
      //else res=3; 
      if (wr_order_eqterms_const_lex_smaller(g,a,b)) res=2;
      else if (wr_order_eqterms_const_lex_smaller(g,b,a)) res=1;
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
  printf("\nxcountedvarlist "); 
  wr_show_countedvarlist(g,(g->xcountedvarlist));
  printf("\n");
#endif 
  wr_clear_countedvarlist(g,(g->ycountedvarlist));
  yw=wr_order_eqterms_weight_vars(g, b, (g->ycountedvarlist), vb);
#ifdef EQORDER_DEBUG    
  printf("\nycountedvarlist ");
  wr_show_countedvarlist(g,(g->ycountedvarlist));
  printf("\n");
#endif   
  
#ifdef EQORDER_DEBUG  
    dp("\n wr_order_eqterms calcs xw and yw: %d %d\n",xw,yw); 
    if (wr_countedvarlist_is_subset(g,(g->xcountedvarlist),(g->ycountedvarlist))) {
      wr_printf(" xlist is a subset of ylist \n");
    } else if (wr_countedvarlist_is_subset(g,(g->ycountedvarlist),(g->xcountedvarlist))) {
      wr_printf(" ylist is a subset of xlist \n");
    } else {
      wr_printf(" xlist and ylist are not subsets of each other\n");
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
    wr_printf("\nlexorder result: %d\n",lexorder);
#endif       
    // lexorder:
    // 1 if x is smaller than y
    // 2 if y is smaller than x
    if (lexorder==2 && wr_countedvarlist_is_subset(g,(g->ycountedvarlist),(g->xcountedvarlist))) {
       res=1; // a bigger than b (prohibits b)
    } else if (lexorder==1 && wr_countedvarlist_is_subset(g,(g->xcountedvarlist),(g->ycountedvarlist))) {  
       res=2; // b bigger than a (prohibits a)
    } else {
       res=3;
    }   
  } else {
    res=3;
  }        

  #ifdef EQORDER_DEBUG  
    dp("\n wr_order_eqterms returns on complex comparison: %d\n",res); 
  #endif    

  return res;
}

static int wr_order_eqterms_const_weight(glb* g, gint a) {
#ifdef KB_WEIGHTS_ORDER
  void* db=g->db;
  gint dtypex=wg_get_encoded_type(db,a);
  if (dtypex==WG_URITYPE) {    
    
    gint ucountx=wg_decode_uri_count(db,a);
    gint ucountposx=ucountx >> URI_COUNT_POSCOUNT_SHIFT;
    if (ucountposx>30000) ucountposx=30000;
    gint ucountnegx=ucountx & URI_COUNT_NEGCOUNT_MASK;
    if (ucountnegx>10000) ucountnegx=10000;
    gint ucountallx=ucountposx+ucountnegx; 
    /*
    printf("\nx uri weight for %s ucountallx %ld id %ld: ",
      wg_decode_unistr(db,a,WG_URITYPE),
      ucountallx,
      wg_decode_uri_id(db,a));
    */  
#ifdef KB_WEIGHTS_ORDER_REVERSE    
    ucountallx=(1000000-ucountallx);
    if (ucountallx<=0) ucountallx=30;
    return ucountallx+20;   
#else
    if (ucountallx<=0) ucountallx=30;
    return ucountallx+20;   
#endif    
  } else {
    return 10;
  }  
#else
  return 10;
#endif  
}


void wr_clear_countedvarlist(glb* g, cvec varlist) {
  UNUSED(g);
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

void wr_record_varocc(glb* g, gint x, cvec varlist) {
  int i, nxt;  

  UNUSED(g);
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

void wr_show_countedvarlist(glb* g, cvec varlist) {
  int i;
  
  if (varlist==NULL) {
    wr_printf("\nvarlist is NULL\n");
    return;
  }
  wr_printf("varlist len %d min var ",(int)((varlist[1]-4)/2));
  if (varlist[2]>=0) wr_print_term(g,varlist[2]);
  else wr_printf(" none");
  wr_printf(" vars ");
  for(i=4; i<varlist[1]; i=i+2) {
    wr_print_term(g,(gint)(varlist[i]));
    wr_printf(": %d, ",(int)(varlist[i+1]));
  }
}

static int wr_order_eqterms_occurs_in(glb* g, gint x, gint y, gptr vb) { 
  void* db=g->db;
  gptr yptr;
  gint yi;
  int ylen,ilimit,i;
  gint tmp; // used by VARVAL_F
    
#ifdef DEBUG
  wr_printf("wr_order_eqterms_occurs_in called with x %ld ",x);
  wr_print_term(g,x);
  wr_printf(" and y %ld ",y);
  wr_print_term(g,y);
  wr_printf("\n");
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
  wr_printf("wr_order_eqterms_weight_vars called with x %ld ",x);
  wr_print_term(g,x);
  wr_printf("\n");
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

int wr_countedvarlist_is_subset(glb* g, cvec xlist, cvec ylist) {
  int i, j, var, found;  

  UNUSED(g);

#ifdef EQORDER_DEBUG  
  printf("\n");
  for(i=0; i<xlist[1]; i=i+1) {
    printf("xlist i %d: %ld\n",i,xlist[i]);
  }
  printf("\n");
  for(i=0; i<ylist[1]; i=i+1) {
    printf("ylist i %d: %ld\n",i,ylist[i]);
  }
#endif
  // NULL cases are for atom knuth bendix: for para cases no NULLs here
  if (xlist==NULL) return 1; // no xlist
  if (ylist==NULL) { // no ylist
    if (xlist[1]==4) return 1; // xlist also empty, xlist is sublist
    else return 0; // xlist not empty, xlist cannot be sublist
  }  
  // if xlist empty, then xlist is a sublist:
  if (xlist[1]==4) return 1;
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
#ifdef SUBLISTS_FIXED
        // this is correct, fixed in june
        if (xlist[i+1]>ylist[j+1]) return 0; // too many occs in xlist
#else 
        // this is old and incorrect
        if (xlist[i+1]>ylist[i+1]) return 0; // too many occs in xlist
#endif        
        
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
  wr_printf("wr_order_eqterms_lex_order called with x %ld and y %ld: ",x,y);
  wr_print_term(g,x);
  wr_printf("\n");
  wr_print_term(g,y);
  wr_printf("\n");
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
  // normal in May 2020
#ifdef NORMAL_CONSTANT_ORDER  
  if (xlen<ylen) return 1;
  else if (ylen<xlen) return 2;   
#else  
  // TESTING 
  if (xlen>ylen) return 1;
  else if (ylen>xlen) return 2;  
#endif  

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

/* 

  returns:
    0 if terms are lex-equal
    1 if x is smaller than y
    2 if y is smaller than x
    3 if terms are not lex-comparable

*/
/*

static int wr_order_eqterms_lex_order2(glb* g, gint x, gint y, gptr vb) {
  void* db;
  gptr xptr,yptr;
  //int i, start, end;  
  //int w;
  gint encx,ency,tmp; // used by VARVAL_F
  int xlen,ylen,uselen,ilimit,i;

#ifdef EQORDER_DEBUG
  wr_printf("wr_order_eqterms_lex_order2 called with x %ld and y %ld: ",x,y);
  wr_print_term(g,x);
  wr_printf("\n");
  wr_print_term(g,y);
  wr_printf("\n");
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
  // normal in May 2020
#ifdef NORMAL_CONSTANT_ORDER  
  if (xlen<ylen) return 1;
  else if (ylen<xlen) return 2;   
#else  
  // TESTING 
  if (xlen>ylen) return 1;
  else if (ylen>xlen) return 2;  
#endif  

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
*/

static int wr_order_eqterms_const_lex_smaller(glb* g, gint x, gint y) {
  //if (x<y) return 1;  // TESTING
  if (x>y) return 1; // Normal in may 2020
  else return 0; 
}


/*
 
 ============== literal knuth bendix ordering ================

*/

/* 

  sets (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,1,1) 

  in atom-to-atom comparison res code:

   0: none bigger, neither prohibits other
   1: xatom bigger than yatom (prohibits yatom)
   2: yatom bigger than xatom (prohibits xatom)
   3: none bigger, both ok 

*/

int wr_calc_clause_knuthbendix_resolvability(glb* g, gptr cl, gptr vb) {
  int atomnr,i,j,lexorder,res;
  gint xatom,xw;
  gint yatom,yw;
  cvec xvarlist,yvarlist;    

  atomnr=wr_calc_clause_size_countedvarlist(g,cl,vb);
  if (atomnr<=0) return 0; // error case
  if (atomnr==1) {
    // single literal
    (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,1,1);
    return 1;
  }
  // initially set all literals as allowed
  for(i=0; i<atomnr; i++) {
    (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,1);
  }    
  // TESTING BLOCK for getting foundground
  /*
  int foundground=0;
  for(i=0; i<atomnr; i++) {  
    xvarlist=(gptr)((g->tmp_clinfo)[(i*2)+2+1]); // IFF no vars, xvarlist will be NULL
    if (xvarlist==NULL) {
      foundground=1;
      break;
    }  
  } 
  */ 
  // TESTING BLOCK for getting foundeq
  /*
  int foundeq=0;
  for(i=0; i<atomnr; i++) {  
    xatom=wg_get_rule_clause_atom(db,cl,i);   
    if (wr_equality_atom(g,xatom)) {
      foundeq=1;
      break;
    }  
  }
  */

  // loop over all pairs of literals, setting prohibited to 0
  for(i=0; i<atomnr; i++) {          
    //xmeta=wg_get_rule_clause_atom_meta(db,cl,i);
    xatom=wg_get_rule_clause_atom(db,cl,i);
    xw=(g->tmp_clinfo)[(i*2)+2];   
    if (xw<0) { // marked as ans lit by countedvarlist calc
      (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,0);
      continue; 
    }  
    xvarlist=(gptr)((g->tmp_clinfo)[(i*2)+2+1]); // IFF no vars, xvarlist will be NULL
    for(j=i+1; j<atomnr; j++) {
      //ymeta=wg_get_rule_clause_atom_meta(db,cl,j);
      if (!(g->tmp_resolvability_vec)[j+1]) {
        // j atom is already marked nonresolvable: no point in comparing
        continue;
      }      
      yatom=wg_get_rule_clause_atom(db,cl,j);
      yw=(g->tmp_clinfo)[(j*2)+2]; 
      if (yw<0) { // marked as ans lit by countedvarlist calc
        (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,j+1,0);
        continue; 
      }  
      yvarlist=(gptr)((g->tmp_clinfo)[(j*2)+2+1]);

#ifdef EQORDER_DEBUG  
      dp("\n wr_calc_clause_knuthbendix_resolvability i %d and j %d\n",i,j);
      wr_printf("xatom\n");
      wr_print_term(g,xatom);
      wr_printf("\nyatom\n");
      wr_print_term(g,yatom);
      dp("\n calcs xw and yw: %ld %ld\n",xw,yw); 
      if (wr_countedvarlist_is_subset(g,xvarlist,yvarlist)) {
        wr_printf(" xvarlist is a subset of yvarlist \n");
      } else if (wr_countedvarlist_is_subset(g,yvarlist,xvarlist)) {
        wr_printf(" yvarlist is a subset of xvarlist \n");
      } else {
        wr_printf(" xvarlist and yvarlist are not subsets of each other\n");
      }
#endif   
      
      
      // TESTING block using foundground
      /*
      if (foundground) {
        if (xvarlist!=NULL && yvarlist==NULL) {
          // xatom prohibited
          //printf("\nxatom prohibited\n");
          (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,0);
          continue;
        } else if (xvarlist==NULL && yvarlist!=NULL) {
          // yatom prohibited
          //printf("\nyatom prohibited\n");
          (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,j+1,0);
          continue;
        }
      } 
      */    
      // TESTING block using foundeq
      /*
      if (foundeq) {
        if (wr_equality_atom(g,xatom) && !wr_equality_atom(g,yatom)) {
          // xatom prohibited
          (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,0);
          continue;
        } else if (!wr_equality_atom(g,xatom) && wr_equality_atom(g,yatom)) {
          // yatom prohibited
          (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,j+1,0);
          continue;
        }
      }
      */

      if (xw>yw && wr_countedvarlist_is_subset(g,yvarlist,xvarlist)) {
        res=1;
      } else if (yw>xw && wr_countedvarlist_is_subset(g,xvarlist,yvarlist)) {
        res=2;
      } else if (xw==yw) {
        // here sizes are the same
        lexorder=wr_order_atoms_lex_order(g,xatom,yatom,vb);
  #ifdef EQORDER_DEBUG    
         wr_printf("\nlexorder result: %d\n",lexorder);
  #endif       
        // lexorder:
        // 1 if x is smaller than y
        // 2 if y is smaller than x
        if (lexorder==2 && wr_countedvarlist_is_subset(g,yvarlist,xvarlist))
          res=1; // a bigger than b (prohibits b)
        else if (lexorder==1 && wr_countedvarlist_is_subset(g,xvarlist,yvarlist))   
          res=2; // b bigger than a (prohibits a)
        else 
          res=3;
      } else {
        res=3;
      }
  #ifdef EQORDER_DEBUG  
      dp("\n wr_order_eqterms returns on complex comparison: %d\n",res); 
  #endif    
      if (res==1) {
        // yatom prohibited
        (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,j+1,0);
      } else if (res==2) {
        // xatom prohibited
         (g->tmp_resolvability_vec)=wr_vec_store(g,g->tmp_resolvability_vec,i+1,0);
      }
    } // loop on j
  } // loop on i
  return 1;
}


/*

 build size and countedvarlist for each literal

 result: a vec of two-gint tuples weight, ptr_to_countedvarlist

*/



int wr_calc_clause_size_countedvarlist(glb* g, gptr cl, gptr vb) {
  void* db=g->db;
  int atomnr,i,j,xw;  
  gint meta, atom; 
  cvec gvvec;
  cvec vvec;


  UNUSED(db);
  UNUSED(meta);
  if (!wg_rec_is_rule_clause(db,cl)) {
    return 1; // mark no vec created
  }  
  atomnr=wg_count_clause_atoms(db,cl);
  if (atomnr==1) {
    return 1; // mark no vec created
  }

  // from here clause length at least 2
  (g->tmp_clinfo)[1]=2; // initialize
  for(i=0; i<atomnr; i++) {          
    meta=wg_get_rule_clause_atom_meta(db,cl,i);
    atom=wg_get_rule_clause_atom(db,cl,i);
    if (wr_answer_lit(g,atom)) {
      // mark dummy: should not be selected for resolution or comparison
      wr_cvec_store(g,(g->tmp_clinfo),(i*2)+2,-1);
      wr_cvec_store(g,(g->tmp_clinfo),(i*2)+2+1,(gint)NULL);
      continue;
    }
    wr_clear_countedvarlist(g,(g->xcountedvarlist));
    xw=wr_order_atom_weight_vars(g, atom, (g->xcountedvarlist), vb);
    gvvec=(g->xcountedvarlist);
#ifdef EQORDER_DEBUG    
    wr_printf("\natom nr %d\n",i);
    wr_print_term(g,atom);
    wr_printf("\n size %d \n",xw);
    wr_show_countedvarlist(g,gvvec);
#endif
    // copy size over
    (g->tmp_clinfo)=wr_cvec_store(g,(g->tmp_clinfo),(i*2)+2,xw);
    // copy var vec over
    if (gvvec[2]>3) {
      // vars present in atom: alloc a cvec and copy over
      vvec=wr_cvec_new(g,gvvec[1]-2);
      if (vvec==NULL) {
        (g->alloc_err)=1;
        wr_tmp_clinfo_free(g,(g->tmp_clinfo));
        wr_alloc_err2int(g,"Cannot allocate memory for a cvec with length",gvvec[1]-2);    
        return 0; // mark error
      }
      vvec[1]=gvvec[1];
      for(j=2;j<(gvvec[1]);j++) {
        vvec[j]=gvvec[j];
      }
      (g->tmp_clinfo)=wr_cvec_store(g,(g->tmp_clinfo),(i*2)+2+1,(gint)vvec);
    } else {
      (g->tmp_clinfo)=wr_cvec_store(g,(g->tmp_clinfo),(i*2)+2+1,(gint)NULL);
    }                 
#ifdef EQORDER_DEBUG 
    wr_printf("\n stored weight and countedvarlist %ld\n",(g->tmp_clinfo)[(i*2)+2]);
    wr_show_countedvarlist(g,(gptr)((g->tmp_clinfo)[(i*2)+2+1]));
    wr_printf("\n after storing (g->tmp_clinfo)[1] is %ld\n",(g->tmp_clinfo)[1]);
#endif
  } 
#ifdef EQORDER_DEBUG 
  wr_printf("\nfinal (g->tmp_clinfo)[0] is %ld and (g->tmp_clinfo)[1] is %ld\n",
    (g->tmp_clinfo)[0],(g->tmp_clinfo)[1]);
#endif  
  return atomnr; // mark vec is created
} 

void wr_tmp_clinfo_free(glb* g, cvec v) {
  int i;

#ifdef EQORDER_DEBUG
  wr_printf("\n wr_tmp_clinfo_free called with v[1] %ld\n",v[1]);
#endif  
  for(i=2;i<v[1];i+=2) {
#ifdef EQORDER_DEBUG    
    wr_printf("\n i %d v[i] %ld v[i+1] %ld \n",i,v[i],v[i+1]);
    wr_show_countedvarlist(g,(gptr)(v[i+1]));
#endif    
    if (v[i+1]!=(gint)NULL) wr_free(g,(gptr)(v[i+1]));
  }
}

int wr_order_atom_weight_vars(glb* g, gint x, gptr vars, gptr vb) {
  void* db;
  gptr xptr;
  int i, start, end, w;  

#ifdef DEBUG
  wr_printf("wr_order_atom_weight_vars called with x %ld ",x);
  wr_print_term(g,x);
  wr_printf("\n");
#endif     

  if (!isdatarec(x)) {
    // now we have a simple value     
    if (isvar(x)) wr_record_varocc(g,x,vars);        
    return 1;
  }   
  // now we have a datarec
  db=g->db;
  xptr=decode_record(db,x);
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,xptr);
  w=0;    
  for(i=start;i<end;i++) {
    w=w+wr_order_atom_weight_vars(g,xptr[i],vars,vb);      
  }   
  return w;
} 


/* 

  returns:
    0 if terms are lex-equal
    1 if x is smaller than y
    2 if y is smaller than x
    3 if terms are not lex-comparable

*/


int wr_order_atoms_lex_order(glb* g, gint x, gint y, gptr vb) {
  void* db;
  gptr xptr,yptr;
  gint encx,ency; 
  int xlen,ylen,uselen,ilimit,i,tmp;

#ifdef EQORDER_DEBUG
  wr_printf("wr_order_atoms_lex_order called with x %ld and y %ld: ",x,y);
  wr_print_term(g,x);
  wr_printf("\n");
  wr_print_term(g,y);
  wr_printf("\n");
#endif     

  if (x==y) return 0; // equal 
  if (isvar(x) || isvar(y)) return 3; // not comparable
  // here none can be a var   
  if (!isdatarec(x)) {
    // now x has a have a simple value     
    if (!isdatarec(y)) {
      if (wr_order_atoms_const_lex_smaller(g,x,y)) return 1;
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
    tmp=wr_order_atoms_lex_order(g,encx,ency,vb);
    if (tmp==0) continue;
    else return tmp;   
  }      
  return 0;        
}        

int wr_order_atoms_const_lex_smaller(glb* g, gint x, gint y) {

#ifdef OCC_LEX_ORDER
  void* db=g->db;
  gint dtypex=wg_get_encoded_type(db,x);
  gint dtypey=wg_get_encoded_type(db,y);
  if (dtypex==WG_URITYPE && dtypey==WG_URITYPE) {    
    
    gint ucountx=wg_decode_uri_count(db,x);
    gint ucountposx=ucountx >> URI_COUNT_POSCOUNT_SHIFT;
    if (ucountposx>30000) ucountposx=30000;
    gint ucountnegx=ucountx & URI_COUNT_NEGCOUNT_MASK;
    if (ucountnegx>10000) ucountnegx=10000;
    gint ucountallx=ucountposx+ucountnegx; 

    gint ucounty=wg_decode_uri_count(db,y);
    gint ucountposy=ucounty >> URI_COUNT_POSCOUNT_SHIFT;
    if (ucountposy>30000) ucountposy=30000;
    gint ucountnegy=ucounty & URI_COUNT_NEGCOUNT_MASK;
    if (ucountnegy>10000) ucountnegy=10000;
    gint ucountally=ucountposx+ucountnegy;
    /*
    printf("\nx uri %s ucountallx %ld id %ld: ",
      wg_decode_unistr(db,x,WG_URITYPE),
      ucountallx,
      wg_decode_uri_id(db,x));
    printf("\ny uri %s ucountally %ld id %ld: ",
      wg_decode_unistr(db,y,WG_URITYPE),
      ucountally,
      wg_decode_uri_id(db,y));  
    */
    if (ucountallx<ucountally) {
      return 1;
    } else if (ucountallx>ucountally) {
      return 0;    
    } else if (wg_decode_uri_id(db,x)<wg_decode_uri_id(db,y)) {
      return 1;
    } else {
      return 0;
    }
  } else {
     if (x>y) return 1;
      else return 0; 
  }  
#else
  /*
  // NORMAL block  
  if (x>y) return 1;
  else return 0; 
  */
  // NEW block june
  void* db=g->db;
  gint dtypex=wg_get_encoded_type(db,x);
  gint dtypey=wg_get_encoded_type(db,y);
  if (dtypex==WG_URITYPE && dtypey==WG_URITYPE) {    
    if (wg_decode_uri_id(db,x)<wg_decode_uri_id(db,y)) {
        return 1;
    } else {
        return 0;
    }
  } else {
    if (x>y) return 1;
    else return 0; 
  }  
#endif  

  // TESTING with count
  /*
  void* db=g->db;
  gint dtypex=wg_get_encoded_type(db,x);
  gint dtypey=wg_get_encoded_type(db,y);
  if (dtypex==WG_URITYPE && dtypey==WG_URITYPE) {    

    //printf("\nuri: ");
    //printf(" %s \n", wg_decode_unistr(db,x,WG_URITYPE));

    gint ucountx=wg_decode_uri_count(db,x);
    gint ucountposx=ucountx >> URI_COUNT_POSCOUNT_SHIFT;
    if (ucountposx>30000) ucountposx=30000;
    gint ucountnegx=ucountx & URI_COUNT_NEGCOUNT_MASK;
    if (ucountnegx>10000) ucountnegx=10000;
    gint ucountallx=ucountposx+ucountnegx; 

    gint ucounty=wg_decode_uri_count(db,y);
    gint ucountposy=ucounty >> URI_COUNT_POSCOUNT_SHIFT;
    if (ucountposy>30000) ucountposy=30000;
    gint ucountnegy=ucounty & URI_COUNT_NEGCOUNT_MASK;
    if (ucountnegy>10000) ucountnegy=10000;
    gint ucountally=ucountposx+ucountnegy;

    if (ucountallx<ucountally) return 1;
    else return 0;
      
  } else {
     if (x>y) return 1;
      else return 0; 
  }
  */
}



/*
 
 ================= printing =========================

*/

void wr_print_strat_flags(glb* g) {

  wr_printf("\nstrategy flags:\n");
  wr_printf("----------------------------------\n");

  wr_printf("pick_given_queue_ratio %d\n", (g->pick_given_queue_ratio));         // this is used for all queues to diff btw priority and simple
  wr_printf("pick_given_queue_ratio_counter %d\n", (g->pick_given_queue_ratio_counter)); // this is not used for queues
  wr_printf("next_pick_given_queue_block_nr %d\n", (g->next_pick_given_queue_block_nr));

  wr_printf("cl_depth_penalty %d\n", (g->cl_depth_penalty));
  wr_printf("cl_length_penalty %d\n", (g->cl_length_penalty));

  /* pre-given limits */
  wr_printf("max_run_seconds %d\n", (g->max_run_seconds)); // one run max seconds
  wr_printf("max_seconds %d\n", (g->max_seconds));     // total max seconds
  wr_printf("cl_maxkeep_weightlimit %d\n", (g->cl_maxkeep_weightlimit));
  wr_printf("cl_maxkeep_sizelimit %d\n", (g->cl_maxkeep_sizelimit));
  wr_printf("cl_maxkeep_depthlimit %d\n", (g->cl_maxkeep_depthlimit));
  wr_printf("cl_maxkeep_lengthlimit %d\n", (g->cl_maxkeep_lengthlimit));  

  wr_printf("cl_pick_queue_strategy %d\n", (g->cl_pick_queue_strategy)); // default a single 2nd level queue for all
  
  wr_printf("reverse_clauselist_strat %d\n", (g->reverse_clauselist_strat));
  wr_printf("queryfocus_strat %d\n", (g->queryfocus_strat));
  wr_printf("queryfocusneg_strat %d\n", (g->queryfocusneg_strat));  

  wr_printf("hyperres_strat %d\n", (g->hyperres_strat));
  wr_printf("weightorder_strat %d\n", (g->weightorder_strat));  
  wr_printf("negpref_strat %d\n", (g->negpref_strat));
  wr_printf("pospref_strat %d\n", (g->pospref_strat));
  wr_printf("knuthbendixpref_strat %d\n", (g->knuthbendixpref_strat));
  wr_printf("hardnesspref_strat %d\n", (g->hardnesspref_strat));
  wr_printf("res_shortarglen_limit %d\n", (g->res_shortarglen_limit)); // max non-ans len of the shortest res argument (generalization of unit)
  wr_printf("back_subsume %d\n", (g->back_subsume)); // 1 does not work any more
  wr_printf("propagate %d\n", (g->propagate));    // 1 does not work any more
  wr_printf("use_equality_strat %d\n", (g->use_equality_strat)); // general strategy
  wr_printf("use_equality %d\n", (g->use_equality)); // current principle
  wr_printf("posunitpara_strat %d\n", (g->posunitpara_strat)); // only paramodulate from unit equalities
  wr_printf("use_comp_funs_strat %d\n", (g->use_comp_funs_strat));
  wr_printf("use_comp_funs %d\n", (g->use_comp_funs));
  wr_printf("use_rewrite_terms_strat %d\n", (g->use_rewrite_terms_strat)); // general strategy
  wr_printf("have_rewrite_terms %d\n", (g->have_rewrite_terms)); // do we actually have rewrite terms
  
}


#ifdef __cplusplus
}
#endif
