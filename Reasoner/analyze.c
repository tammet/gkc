/*
*
* Copyright (c) Tanel Tammet 2019
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

 /** @file analyze.c
 * Analyze the clause set before search.
 */

/* ====== Includes =============== */


#ifdef __cplusplus
extern "C" {
#endif

#include "rincludes.h"   
 

/* ====== Private defs =========== */


//#define DEBUG
//#undef DEBUG  


/* ====== Private headers ======== */

  
/* ====== Functions ============== */


/* =====================================================

 

====================================================== */



int wr_analyze_clause_list(glb* g, void* db, void* child_db) {
  gint cell;
  gcell *cellptr;
  gptr rec;
  int n=0, tmp, haveextdb=0;

  if (db!=child_db) haveextdb=1;
  else haveextdb=0;
  wr_clear_all_varbanks(g);

  cell=(dbmemsegh(child_db)->clauselist);
  while(cell) {     
    cellptr=(gcell *) offsettoptr(child_db, cell);
    rec=offsettoptr(child_db,cellptr->car);
    tmp=wr_analyze_clause(g,rec,haveextdb);
    if (!tmp) {
      // something wrong with the clause
      wr_clear_all_varbanks(g);
      return 0;
    }
    n++;
    cell=cellptr->cdr;
  }  
  return 1;  
}

int wr_analyze_clause(glb* g, gptr cl, int haveextdb) {
  void* db=g->db;
  gint history;
  gptr historyptr;
  int i, hlen;
  gint meta,atom=0;
  gint vc_tmp;
  int tmp;
  int ruleflag=0, len=0, anslit=0, neglit=0, poslit=0;
  int poseq=0, negeq=0, uniteq=0;
  int size=0,maxdepth=0,varcount=0;
  gint priority, decprior=0; 
#ifdef DEBUG  
  gint name;
  char* namestr;
#endif

#ifdef DEBUG
  wr_printf("\n wr_analyze_clause \n");
  wr_print_clause(g,cl); 
  wr_printf("\n");
  wg_print_record(g->db,cl); 
  wr_printf("\n");
#endif

  history=wr_get_history(g,cl);
  if (history) {
    
    historyptr=otp(db,history);
    hlen=wg_get_record_len(db,historyptr);
      
    if (hlen==HISTORY_PREFIX_LEN) {
      // input clause
#ifdef DEBUG
      name = wr_get_history_record_field(db,historyptr,HISTORY_NAME_POS);
      if (name && wg_get_encoded_type(db,name)==WG_STRTYPE) {
        namestr=wg_decode_str(db,name);
        wr_printf("name: %s",namestr);
      } else if (name && wg_get_encoded_type(db,name)==WG_INTTYPE) {
        wr_printf("name as int: %d",(int)(wg_decode_int(db,name)));
      }
      //wr_print_history_extra(g,history);
#endif      
      
      priority = wr_get_history_record_field(db,historyptr,HISTORY_PRIORITY_POS);
      decprior=wr_decode_priority(g,priority);
      
#ifdef DEBUG      
      if (decprior==WR_HISTORY_GOAL_ROLENR) wr_printf(",goal] ");
      else if (decprior==WR_HISTORY_ASSUMPTION_ROLENR) wr_printf(",assumption] ");
      else if (decprior==WR_HISTORY_FROMGOALASSUMPTION_ROLENR) wr_printf(",fromga] "); 
      else if (decprior==WR_HISTORY_FROMGOAL_ROLENR) wr_printf(",fromgoal] ");
      else if (decprior==WR_HISTORY_FROMASSUMPTION_ROLENR) wr_printf(",fromassumption] ");
      else if (decprior==WR_HISTORY_AXIOM_ROLENR) wr_printf(",axiom] ");
      else if (decprior==WR_HISTORY_FROMAXIOM_ROLENR) wr_printf(",fromaxiom] ");
      else if (decprior==WR_HISTORY_EXTAXIOM_ROLENR) wr_printf(",extaxiom] ");
      else if (decprior) wr_printf(", dp %d] ",(int)decprior);
#endif      
    }
       
  }
  g->tmp_unify_vc=((gptr)(g->varstack))+1;
  vc_tmp=*(g->tmp_unify_vc);

  if (wg_rec_is_fact_clause(db,cl)) {
    ruleflag=0;
    len=1;
    neglit=0;
    poslit=1;
    atom=encode_record(db,cl);
    if (wr_answer_lit(g,atom)) {
      //printf("\n!!! Warning: single ans clause\n");
      anslit++;
    }       
    if (wr_equality_atom(g,atom)) {
      poseq++;
      uniteq++;
    } 
    tmp=wr_analyze_term(g,pto(db,cl),0,&size,&maxdepth,1,haveextdb,0);
  } else if (wg_rec_is_rule_clause(db,cl)) {   
    ruleflag=1;
    len=wg_count_clause_atoms(db, cl);
    for(i=0; i<len; i++) {
      meta=wg_get_rule_clause_atom_meta(db,cl,i);
      atom=wg_get_rule_clause_atom(db,cl,i);
      if (wg_atom_meta_is_neg(db,meta)) neglit++;
      else poslit++;
      if (wr_answer_lit(g,atom)) {        
        anslit++;
      }
      if (wr_equality_atom(g,atom)) {
        if (wg_atom_meta_is_neg(db,meta)) negeq++;
        else {
          poseq++;
          if (len<2) uniteq++;
        }  
      } 
      tmp=wr_analyze_term(g,atom,0,
            &size,&maxdepth,!wg_atom_meta_is_neg(db,meta),haveextdb,0);
      if (!tmp) {
        // something wrong with the atom
        wr_printf("\nError: found an incorrectly encoded clause\n");
        wg_print_record(db,cl);
        return 0; 
      }
    }
  }

  varcount=*(g->tmp_unify_vc)-vc_tmp;

  if (vc_tmp!=*(g->tmp_unify_vc)) {    
    wr_clear_varstack_topslice(g,g->varstack,vc_tmp);
  }  
 
  if (len==2 && poslit==1 && neglit==1 && !anslit && 
      varcount==1 && size==4 && maxdepth==1) {
    // this is likely a chain clause
    check_process_chain_clause(g,cl);
  }

  (g->in_clause_count)++;
  if (ruleflag) (g->in_rule_clause_count)++;
  else (g->in_fact_clause_count)++;  
  if (anslit) (g->in_answer_clause_count)++;
  if (!varcount) (g->in_ground_clause_count)++;
  if (len==1) (g->in_unit_clause_count)++;
  if (poslit<=1) (g->in_horn_clause_count)++;
  if (!poslit) (g->in_neg_clause_count)++;
  if (!neglit) (g->in_pos_clause_count)++;
  if (poseq) (g->in_poseq_clause_count)++;
  if (negeq) (g->in_negeq_clause_count)++;
  if (uniteq) (g->in_unitposeq_clause_count)++;
  if (len<(g->in_min_length)) (g->in_min_length)=len;
  if (len>(g->in_max_length)) (g->in_max_length)=len;
  if (maxdepth<(g->in_min_depth)) (g->in_min_depth)=maxdepth;
  if (maxdepth>(g->in_max_depth)) (g->in_max_depth)=maxdepth;
  if (size<(g->in_min_size)) (g->in_min_size)=size;
  if (size>(g->in_max_size)) (g->in_max_size)=size;
  if (varcount<(g->in_min_vars)) (g->in_min_vars)=varcount;
  if (varcount>(g->in_max_vars)) (g->in_max_vars)=varcount;

  if (decprior==WR_HISTORY_GOAL_ROLENR) {
    (g->in_goal_count)++;
    if (!poslit) (g->in_neg_goal_count)++;
    if (!neglit) (g->in_pos_goal_count)++;
    if (!neglit && len==1) (g->in_posunit_goal_count)++;
  }  
  else if (decprior==WR_HISTORY_ASSUMPTION_ROLENR) (g->in_assumption_count)++;
  else if (decprior==WR_HISTORY_FROMGOALASSUMPTION_ROLENR) (g->in_goal_count)++; 
  else if (decprior==WR_HISTORY_FROMGOAL_ROLENR) (g->in_goal_count)++;
  else if (decprior==WR_HISTORY_FROMASSUMPTION_ROLENR) (g->in_assumption_count)++;
  else if (decprior==WR_HISTORY_AXIOM_ROLENR) (g->in_axiom_count)++;
  else if (decprior==WR_HISTORY_FROMAXIOM_ROLENR) (g->in_axiom_count)++;
  else if (decprior==WR_HISTORY_EXTAXIOM_ROLENR) (g->in_extaxiom_count)++;
  else (g->in_axiom_count)++;

  /*
  (g->in_average_length)=0;
  (g->in_average_depth)=0;
  (g->in_predicate_count)=0;
  (g->in_funsymb_count)=0;
  (g->in_axiom_count)=0;
  (g->in_assumption_count)=0;
  (g->in_goal_count)=0;
  */
  
  return 1;
}



int wr_analyze_term(glb* g, gint x, 
      int depth, int* size, int* maxdepth, int polarity, int haveextdb, int argpos) {
  void* db;
  gptr xptr;
  int i, start, end;  
  int w, dtype;
  gint ucount, ucountpos, ucountneg;

#ifdef DEBUG
  wr_printf("wr_analyze_term called with x %d type %d depth %d size %d maxdepth %d argpos %d\n",
         x,wg_get_encoded_type(g->db,x),depth,*size,*maxdepth,argpos);
  wr_print_term(g,x);
  wr_printf("\n");
#endif
  if (!isdatarec(x)) {
    // now we have a simple value  
    (*size)++;
    if (!isvar(x)) {
      db=g->db;
      dtype=wg_get_encoded_type(db,x);
      
      if (dtype==WG_URITYPE && !haveextdb) {
#ifdef DEBUG                
        wr_printf("\nuri: ");
        wr_printf(" %s \n", wg_decode_unistr(db,x,WG_URITYPE));
#endif        
        ucount=wg_decode_uri_count(db,x);
        ucountpos=ucount >> URI_COUNT_POSCOUNT_SHIFT;        
        ucountneg=ucount & URI_COUNT_NEGCOUNT_MASK;     
        if (polarity) {
          ucountpos++;
          if (ucountpos>(1<<URI_COUNT_POSCOUNT_SHIFT)) ucountpos=(1<<URI_COUNT_POSCOUNT_SHIFT);
        } else {
          ucountneg++;
          if (ucountneg>URI_COUNT_NEGCOUNT_MASK) ucountneg=URI_COUNT_NEGCOUNT_MASK;
        }                  
        ucount=ucountpos << URI_COUNT_POSCOUNT_SHIFT;        
        ucount=ucount | ucountneg;
        wg_set_uri_count(db,x,ucount);
        // finding the most used constant
        if (argpos!=0) {
          if ((ucountpos+ucountneg)>(g->in_max_const_ucount)) {
            (g->in_max_const_ucount)=(ucountpos+ucountneg);
            (g->in_max_occ_const)=x;
#ifdef DEBUG                
            wr_printf("\nmax occ const occs %d : ",(g->in_max_const_ucount));
            wr_printf(" %s \n", wg_decode_unistr(db,(g->in_max_occ_const),WG_URITYPE));
#endif             
          }
        }

      }
      
      return 10;
    }  
    // here x is a var    
    if (VARVAL_DIRECT(x,(g->varbanks))==UNASSIGNED) {
      // a new var 
      SETVAR(x,encode_smallint(1),g->varbanks,g->varstack,g->tmp_unify_vc);
      return 1;
    } else {
      // a var seen before
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
  if (depth>(*maxdepth)) *maxdepth=depth;
  for(i=start;i<end;i++) {
    if (!xptr[i]) return 0; // should not have 0 in args
    w=w+wr_analyze_term(g,xptr[i],depth,size,maxdepth,polarity,haveextdb,i-start);      
  }   
  return 1;
}  

// ====== make auto guide ====================

/*

very large over 50000

small and medium almost-pure-uniteq

small horn under 100

small non-horn under 100

medium horn up to 1000

all the rest



------

a really large part is goal or assumptions

----------

contains eq or not?

neg or unit or both?

several types of query or not?

minimal sensible length and depth limit?

query preference handling:

   - nr 1 handles assumptions and goals as-is: always use
   - if no non-external axioms or no positive unit goals, nr 2 is useless
   - if no assumptions and no non-negative goal clauses, nr 3 is useless

     if ((g->cl_pick_queue_strategy)==2) {
      // make non-included axioms assumptions and positive conjecture part assumptions
      if (decprior==WR_HISTORY_GOAL_ROLENR && wr_is_positive_unit_cl(g,cl)) 
        decprior=WR_HISTORY_ASSUMPTION_ROLENR;        
      else if (decprior==WR_HISTORY_AXIOM_ROLENR) // && (g->parse_is_included_file))
        decprior=WR_HISTORY_ASSUMPTION_ROLENR;       
    } else if ((g->cl_pick_queue_strategy)==3) {
      // only fully negative clauses of goal made goal and no assumptions (ie made axioms)
      if (decprior==WR_HISTORY_GOAL_ROLENR && !wr_is_negative_cl(g,cl))
        decprior=WR_HISTORY_AXIOM_ROLENR;  
      else if (decprior==WR_HISTORY_ASSUMPTION_ROLENR)  
        decprior=WR_HISTORY_AXIOM_ROLENR; // !! had no effect before
    } else if ((g->cl_pick_queue_strategy)==4) {
      // everything in one, axiom queue, regardless of polarity or role
      decprior=WR_HISTORY_AXIOM_ROLENR;   
    }   

*/

char* make_auto_guide(glb* g, glb* kb_g) { 
  char *buf=NULL,*pref; 
  int i,j,pos,iterations=4,secs;
  int eq,depth,length,size,bigratio;
  int qp1ok=1, qp2ok=1, qp3ok=1; // query preference strats default ok
  int tmp;

  if (kb_g) {
    wr_printf("\nglobal stats:\n");
    wr_show_in_stats(kb_g);
  }   
  make_sum_input_stats(g,kb_g);
  wr_show_in_summed_stats(g);
  if ((g->sin_poseq_clause_count)+(g->sin_negeq_clause_count)) {
    // equality present
    eq=1;  
  } else {
    // no equality
    eq=0;  
  }
  if (!(g->sin_axiom_count) &&
      ((g->sin_goal_count)==1 ||
       !(g->sin_posunit_goal_count))) {
    // query pref 2 is useless
    qp2ok=0;
  }
  if (!(g->sin_assumption_count) && 
       ((g->sin_goal_count)==1 ||
        (g->sin_goal_count)==(g->sin_neg_goal_count))) {
    // query pref 3 is useless
    qp3ok=0;
  } 
  tmp=(g->sin_goal_count)+(g->sin_assumption_count);
  if ( (g->sin_goal_count)*2 > (g->in_clause_count)) {
    // huge goal, default query pref is useless
    qp1ok=0;
  }
  if ( tmp*2 > (g->in_clause_count)) {
    // huge goal+assumptions, default query pref + assumption-rich is useless
    qp1ok=0; 
    qp2ok=0;
  }

  buf=(char*)wr_malloc(g,10000);

  pref="{\n"
    "\"print\":1,\n"
    "\"print_level\": 15,\n"
    "\"max_size\": 0,\n"
    "\"max_depth\": 0,\n"
    "\"max_length\": 0,\n"
    "\"max_seconds\": 0,\n";
 
  pos=sprintf(buf,"%s",pref);
  if (!eq) {
    pos+=sprintf(buf+pos,"\"equality\":0,\n");
  }
  pos+=sprintf(buf+pos,"\"runs\":[\n");
  
  if ((g->sin_clause_count)>500000) {
    // very large
    secs=10;
  } else {
    secs=1;
  }
  depth=2;
  length=(g->sin_min_length);
  size=3;
  //smallratio=2;
  bigratio=20;
  for(i=0;i<iterations;i++) {

    // start of a block
    if ((g->sin_clause_count)>500000) {
      // very large    
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 1},\n",secs);
      }
      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2},\n",secs);
      }
      if (qp3ok) {         
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 3},\n",secs);          
      }
      // unit mod
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"unit\"], \"query_preference\": 1},\n",secs);
      }  
      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"unit\"], \"query_preference\": 2},\n",secs);
      }  
      if (qp3ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"unit\"], \"query_preference\": 3},\n",secs);
      }
      // positive pref mod
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"positive_pref\"], \"query_preference\": 1},\n",secs);      
      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"positive_pref\"], \"query_preference\": 2},\n",secs);
      }  
      if (qp3ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"positive_pref\"], \"query_preference\": 3},\n",secs);
      }
      // size limit mod
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 1, \"max_depth\": %d},\n",secs,depth);
      }  
      //if (qp2ok) {
      //  pos+=sprintf(buf+pos,
      //  "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2, \"max_depth\": %d},\n",secs,depth);
      //}  
      //pos+=sprintf(buf+pos,
      //"{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 1, \"max_size\": %d},\n",secs,size);
      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2, \"max_size\": %d},\n",secs,size);
      }  
      //if (qp3ok) {
      //  pos+=sprintf(buf+pos,
      //  "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 3, \"max_depth\": %d},\n",secs,depth);
      //}
      // triple mod
      /* 
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"triple\"], \"query_preference\": 1},\n",secs);
      }  
      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"triple\"], \"query_preference\": 2},\n",secs);
      }  
      if (qp3ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"triple\"], \"query_preference\": 3},\n",secs);
      }
      */

    } else if ((g->sin_clause_count)>50000) {
      // very large
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 1},\n",secs);
      }  
      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2},\n",secs);
      }
      if (qp3ok) {         
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 3},\n",secs);          
      }
      // unit mod
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"unit\"], \"query_preference\": 1},\n",secs);
      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"unit\"], \"query_preference\": 2},\n",secs);
      }  
      if (qp3ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"unit\"], \"query_preference\": 3},\n",secs);
      }
      // positive pref mod
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"positive_pref\"], \"query_preference\": 1},\n",secs);      
      }  
      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"positive_pref\"], \"query_preference\": 2},\n",secs);
      }  
      if (qp3ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"positive_pref\"], \"query_preference\": 3},\n",secs);
      }
      // size limit mod
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 1, \"max_depth\": %d},\n",secs,depth);
      //if (qp2ok) {
      //  pos+=sprintf(buf+pos,
      //  "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2, \"max_depth\": %d},\n",secs,depth);
      //}  
      //if (qp1ok) {
      //  pos+=sprintf(buf+pos,
      // "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 1, \"max_size\": %d},\n",secs,size);
      //} 
      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2, \"max_size\": %d},\n",secs,size);
      }  
      //if (qp3ok) {
      //  pos+=sprintf(buf+pos,
      //  "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 3, \"max_depth\": %d},\n",secs,depth);
      //}
      // triple mod
      /* 
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"triple\"], \"query_preference\": 1},\n",secs);
      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"triple\"], \"query_preference\": 2},\n",secs);
      }  
      if (qp3ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"triple\"], \"query_preference\": 3},\n",secs);
      }
      */

    } else if ((g->sin_clause_count)>1000) {
      // large
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 1},\n",secs);
      }  
      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2},\n",secs);
      }
      if (qp3ok) {         
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 3},\n",secs);          
      }
      // unit mod
      if (1) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"unit\"], \"query_preference\": 0},\n",secs);
      }
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"unit\"], \"query_preference\": 1},\n",secs);
      }  
      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"unit\"], \"query_preference\": 2},\n",secs);
      }  
      if (qp3ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"unit\"], \"query_preference\": 3},\n",secs);
      }
      // positive pref mod
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"positive_pref\"], \"query_preference\": 0},\n",secs);
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"positive_pref\"], \"query_preference\": 1},\n",secs);      
      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"positive_pref\"], \"query_preference\": 2},\n",secs);
      }  
      if (qp3ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"positive_pref\"], \"query_preference\": 3},\n",secs);
      }
      // size limit mod
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 1, \"max_depth\": %d},\n",secs,depth);
      }  
      //if (qp2ok) {
      //  pos+=sprintf(buf+pos,
      //  "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2, \"max_depth\": %d},\n",secs,depth);
      //}  
      //pos+=sprintf(buf+pos,
      //"{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 1, \"max_size\": %d},\n",secs,size);
      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2, \"max_size\": %d},\n",secs,size);
      }  
      //if (qp3ok) {
      //  pos+=sprintf(buf+pos,
      //  "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 3, \"max_depth\": %d},\n",secs,depth);
      //}
      // triple mod
      /* 
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"triple\"], \"query_preference\": 1},\n",secs);
      }  
      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"triple\"], \"query_preference\": 2},\n",secs);
      }  
      if (qp3ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"triple\"], \"query_preference\": 3},\n",secs);
      }  
      */
      // non-query strats

      if ((g->sin_horn_clause_count)==(g->sin_clause_count)) {
        // horn
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 1},\n",secs);
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0},\n",secs);
        if (g->sin_max_length>1) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 0},\n",secs);   
          //pos+=sprintf(buf+pos,
          //"{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 1},\n",secs);                   
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"hyper\"], \"query_preference\": 0},\n",secs);
          //pos+=sprintf(buf+pos,
          //"{\"max_seconds\": %d, \"strategy\":[\"hyper\"], \"query_preference\": 1},\n",secs);
        }
      } else {
        // non-horn
        if (g->sin_max_length>1) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 1},\n",secs);
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 0},\n",secs);
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"hardness_pref\"], \"query_preference\": 1},\n",secs);
        }  
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0},\n",secs);        
        //pos+=sprintf(buf+pos,
        //"{\"max_seconds\": %d, \"strategy\":[\"positive_pref\"], \"query_preference\": 0},\n",secs);        
        if (g->sin_max_length>1) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"hyper\"], \"query_preference\": 0},\n",secs);
          //pos+=sprintf(buf+pos,
          //"{\"max_seconds\": %d, \"strategy\":[\"hyper\"], \"query_preference\": 1},\n",secs);
        }
      } 

    } else if ((g->sin_clause_count)>100) {
      // medium size
      if ((g->sin_horn_clause_count)==(g->sin_clause_count)) {
        // horn
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 1},\n",secs);
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0},\n",secs);
        if (g->sin_max_length>1) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 0},\n",secs);   
          //pos+=sprintf(buf+pos,
          //"{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 1},\n",secs);                   
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"hyper\"], \"query_preference\": 0},\n",secs);
          //pos+=sprintf(buf+pos,
          //"{\"max_seconds\": %d, \"strategy\":[\"hyper\"], \"query_preference\": 1},\n",secs);
        }
      } else {
        // non-horn
        if (g->sin_max_length>1) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 1},\n",secs);
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 0},\n",secs);
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"hardness_pref\"], \"query_preference\": 0},\n",secs);
        }  
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0},\n",secs);        
        //pos+=sprintf(buf+pos,
        //"{\"max_seconds\": %d, \"strategy\":[\"positive_pref\"], \"query_preference\": 0},\n",secs);        
        if (g->sin_max_length>1) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"hyper\"], \"query_preference\": 0},\n",secs);
          //pos+=sprintf(buf+pos,
          //"{\"max_seconds\": %d, \"strategy\":[\"hyper\"], \"query_preference\": 1},\n",secs);
        }
      }           
      // query block
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 1},\n",secs);
      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2},\n",secs);
      }        
      if (qp3ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 3},\n",secs);
      }
      // query block with pos order
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"positive_pref\"], \"query_preference\": 0},\n",secs);
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"query_focus\", \"positive_pref\"], \"query_preference\": 1},\n",secs);
      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\", \"positive_pref\"], \"query_preference\": 2},\n",secs);
      }        
      if (qp3ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\", \"positive_pref\"], \"query_preference\": 3},\n",secs);
      }
      // reverse clause list order block
      if (g->sin_max_length>1) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 1, \"reverse_clauselist\": 1},\n",secs);
        //pos+=sprintf(buf+pos,
        //"{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 0, \"reverse_clauselist\": 1},\n",secs);
      }
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 1, \"reverse_clauselist\": 1},\n",secs); 
      //pos+=sprintf(buf+pos,
      //"{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0, \"reverse_clauselist\": 1},\n",secs);        
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"positive_pref\"], \"query_preference\": 0, \"reverse_clauselist\": 1},\n",secs);     
      // different weight ratio block
      //if (g->sin_max_length>1)  {
      //  pos+=sprintf(buf+pos,
      //  "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 0, \"weight_select_ratio\": %d},\n",secs,bigratio);
      //}          
      //pos+=sprintf(buf+pos,
      //  "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 1, \"weight_select_ratio\": %d},\n",secs,bigratio);
      //pos+=sprintf(buf+pos,
      //  "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 0, \"weight_select_ratio\": %d},\n",secs,bigratio);
      //pos+=sprintf(buf+pos,
      //  "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 1, \"weight_select_ratio\": %d},\n",secs,bigratio);  
      //if (g->sin_max_length>1)  {
      //  pos+=sprintf(buf+pos,
      //    "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 1, \"weight_select_ratio\": %d},\n",secs,smallratio);
      //}          
      //pos+=sprintf(buf+pos,
      //  "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0, \"weight_select_ratio\": %d},\n",secs,smallratio);
      //pos+=sprintf(buf+pos,
      //  "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 0, \"weight_select_ratio\": %d},\n",secs,smallratio);
      //pos+=sprintf(buf+pos,
      //  "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 1, \"weight_select_ratio\": %d},\n",secs,smallratio);      
      // no equality block
      if (eq) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 1, \"equality\":0},\n",secs);
      }
      // size limit block
      if ((g->sin_max_depth)>1) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 1, \"max_depth\": %d},\n",secs,depth);
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 1, \"max_depth\": %d},\n",secs,depth);
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 0, \"max_depth\": %d},\n",secs,depth);
      }
      /* 
      if ((g->sin_horn_clause_count)!=(g->sin_clause_count)) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0, \"max_size\": %d},\n",secs,size);
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 0, \"max_size\": %d},\n",secs,size);
      }
      */

    } else if ((g->sin_max_length)>1) {
      // small non-ueq
      if ((g->sin_horn_clause_count)==(g->sin_clause_count)) {
        // horn
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0},\n",secs);
        pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 0},\n",secs);          
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 1},\n",secs);
        if ((g->sin_max_depth)>1) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 1},\n",secs);  
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"hyper\"], \"query_preference\": 0},\n",secs);
          //pos+=sprintf(buf+pos,
          //"{\"max_seconds\": %d, \"strategy\":[\"hyper\"], \"query_preference\": 1},\n",secs);
        }
      } else {
        // non-horn
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 0},\n",secs);        
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0},\n",secs);    
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 1},\n",secs);    
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 1},\n",secs);
        if (g->sin_max_length>1) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"hyper\"], \"query_preference\": 0},\n",secs);
          //pos+=sprintf(buf+pos,
          //"{\"max_seconds\": %d, \"strategy\":[\"hyper\"], \"query_preference\": 1},\n",secs);
          //pos+=sprintf(buf+pos,
          //"{\"max_seconds\": %d, \"strategy\":[\"positive_pref\"], \"query_preference\": 0},\n",secs);     
        }       
      }
      // reverse clause list order block
      pos+=sprintf(buf+pos,      
      "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 1, \"reverse_clauselist\": 1},\n",secs);
      //pos+=sprintf(buf+pos,
      //"{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 0, \"reverse_clauselist\": 1},\n",secs);
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 1, \"reverse_clauselist\": 1},\n",secs); 
      //pos+=sprintf(buf+pos,
      //"{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0, \"reverse_clauselist\": 1},\n",secs);        
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"positive_pref\"], \"query_preference\": 0, \"reverse_clauselist\": 1},\n",secs);     
      // query block
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 1},\n",secs);      
      // no equality block
      if (eq) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 1, \"equality\":0},\n",secs);
      }
      // size limit block     
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 0, \"max_depth\": %d},\n",secs,depth);
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0, \"max_depth\": %d},\n",secs,depth);
      /* 
      if ((g->sin_horn_clause_count)!=(g->sin_clause_count)) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0, \"max_size\": %d},\n",secs,size);
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 0, \"max_size\": %d},\n",secs,size);
      }
      */
      // ratio block
      //pos+=sprintf(buf+pos,
      //"{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 0, \"weight_select_ratio\": %d},\n",secs,bigratio);        
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0, \"weight_select_ratio\": %d},\n",secs,bigratio);    
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 1, \"weight_select_ratio\": %d},\n",secs,bigratio);    
      //pos+=sprintf(buf+pos,
      //"{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 1, \"weight_select_ratio\": %d},\n",secs,bigratio);
    } else {
      // small ueq
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0},\n",secs);
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 1},\n",secs);
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0, \"weight_select_ratio\": %d},\n",secs,20);
        //pos+=sprintf(buf+pos,
        //"{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0, \"weight_select_ratio\": %d},\n",secs,100);
        //pos+=sprintf(buf+pos,
        //"{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0, \"weight_select_ratio\": %d},\n",secs,2);
        if ((g->sin_max_depth)>1) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0, \"max_depth\": %d},\n",secs,depth);
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0, \"max_depth\": %d},\n",secs,depth+1);
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0, \"max_depth\": %d},\n",secs,depth+2);
          depth+=2;
        }  
        /* 
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0, \"max_size\": %d},\n",secs,size);
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0, \"max_size\": %d},\n",secs,size+3);
        size=size+3;
        */
    }
    // end of one block
    if (i<iterations-1) {
      pos+=sprintf(buf+pos,"\n");      
    }
    // limit mods
    secs=secs*5;
    if (((g->in_max_depth)>1) && (g->sin_max_depth)>1) {
      depth=depth+1;
    } 
    length=length+1; 
    size=size*2;
  }

  // overwrite last comma

  for(j=pos;j>0;j--) {
    if (*(buf+j)==',') {
      *(buf+j)=' ';
      break;
    }
  }

  pos+=sprintf(buf+pos,"\n]}\n");
  
  wr_printf("\nauto guide:\n-----------\n%s\n",buf);
   
  //guide=wr_parse_guide_str(buf);
  //printf("Using default strategy.");
  return buf;
}

void make_sum_input_stats(glb* g, glb* kb_g) {
  float s1,s2;

  (g->sin_clause_count)=(g->in_clause_count);
  (g->sin_rule_clause_count)=(g->in_rule_clause_count);
  (g->sin_fact_clause_count)=(g->in_fact_clause_count);
  (g->sin_answer_clause_count)=(g->in_answer_clause_count);
  (g->sin_ground_clause_count)=(g->in_ground_clause_count);
  (g->sin_unit_clause_count)=(g->in_unit_clause_count);
  (g->sin_horn_clause_count)=(g->in_horn_clause_count);
  (g->sin_pos_clause_count)=(g->in_pos_clause_count);
  (g->sin_neg_clause_count)=(g->in_neg_clause_count);
  (g->sin_poseq_clause_count)=(g->in_poseq_clause_count);
  (g->sin_negeq_clause_count)=(g->in_negeq_clause_count);
  (g->sin_unitposeq_clause_count)=(g->in_unitposeq_clause_count);
  (g->sin_chain_clause_count)=(g->in_chain_clause_count);
  (g->sin_min_length)=(g->in_min_length);
  (g->sin_max_length)=(g->in_max_length);
  (g->sin_min_depth)=(g->in_min_depth);
  (g->sin_max_depth)=(g->in_max_depth);
  (g->sin_min_size)=(g->in_min_size);
  (g->sin_max_size)=(g->in_max_size);
  (g->sin_min_vars)=(g->in_min_vars);
  (g->sin_max_vars)=(g->in_max_vars);
  (g->sin_average_length)=(g->in_average_length);
  (g->sin_average_depth)=(g->in_average_depth);
  (g->sin_predicate_count)=(g->in_predicate_count);
  (g->sin_funsymb_count)=(g->in_funsymb_count);
  (g->sin_extaxiom_count)=(g->in_extaxiom_count);
  (g->sin_axiom_count)=(g->in_axiom_count);
  (g->sin_assumption_count)=(g->in_assumption_count);
  (g->sin_goal_count)=(g->in_goal_count);
  (g->sin_neg_goal_count)=(g->in_neg_goal_count);
  (g->sin_pos_goal_count)=(g->in_pos_goal_count);
  (g->sin_posunit_goal_count)=(g->in_posunit_goal_count);
  if ((g->in_max_const_ucount)>(g->sin_max_const_ucount)) {
    (g->sin_max_const_ucount)=(g->in_max_const_ucount);
    (g->sin_max_occ_const)=(g->in_max_occ_const);
  } 

  if (kb_g) {
    (g->sin_clause_count)+=(kb_g->in_clause_count);
    (g->sin_rule_clause_count)+=(kb_g->in_rule_clause_count);
    (g->sin_fact_clause_count)+=(kb_g->in_fact_clause_count);
    (g->sin_answer_clause_count)+=(kb_g->in_answer_clause_count);
    (g->sin_ground_clause_count)+=(kb_g->in_ground_clause_count);
    (g->sin_unit_clause_count)+=(kb_g->in_unit_clause_count);
    (g->sin_horn_clause_count)+=(kb_g->in_horn_clause_count);
    (g->sin_pos_clause_count)+=(kb_g->in_pos_clause_count);
    (g->sin_neg_clause_count)+=(kb_g->in_neg_clause_count);
    (g->sin_poseq_clause_count)+=(kb_g->in_poseq_clause_count);
    (g->sin_negeq_clause_count)+=(kb_g->in_negeq_clause_count);
    (g->sin_unitposeq_clause_count)+=(kb_g->in_unitposeq_clause_count);
    (g->sin_chain_clause_count)+=(kb_g->in_chain_clause_count);
    if ((kb_g->in_min_length)<(g->sin_min_length))
      (g->sin_min_length)=(kb_g->in_min_length);
    if ((kb_g->in_max_length)>(g->sin_max_length))  
      (g->sin_max_length)=(kb_g->in_max_length);
    if ((kb_g->in_min_depth)<(g->sin_min_depth))  
      (g->sin_min_depth)=(kb_g->in_min_depth);
    if ((kb_g->in_max_depth)>(g->sin_max_depth))  
      (g->sin_max_depth)=(kb_g->in_max_depth);
    if ((kb_g->in_min_size)<(g->sin_min_size))  
      (g->sin_min_size)=(kb_g->in_min_size);
    if ((kb_g->in_max_size)>(g->sin_max_size))  
      (g->sin_max_size)=(kb_g->in_max_size);
    if ((kb_g->in_min_vars)<(g->sin_min_vars))  
      (g->sin_min_vars)=(kb_g->in_min_vars);
    if ((kb_g->in_max_vars)>(g->sin_max_vars))  
      (g->sin_max_vars)=(kb_g->in_max_vars);

    s1=((g->in_clause_count)/((g->in_clause_count)+(kb_g->in_clause_count)))*
       (g->in_average_length);
    s2=((kb_g->in_clause_count)/((g->in_clause_count)+(kb_g->in_clause_count)))*
       (kb_g->in_average_length);   
    (g->sin_average_length)=s1+s2;

    s1=((g->in_clause_count)/((g->in_clause_count)+(kb_g->in_clause_count)))*
       (g->in_average_depth);
    s2=((kb_g->in_clause_count)/((g->in_clause_count)+(kb_g->in_clause_count)))*
       (kb_g->in_average_depth);  
    (g->sin_average_depth)=s1+s2;

    (g->sin_predicate_count)=(kb_g->in_predicate_count); // estimate, assuming kb is bigger
    (g->sin_funsymb_count)=(kb_g->in_funsymb_count);  // estimate, assuming kb is bigger

    (g->sin_extaxiom_count)+=(kb_g->in_extaxiom_count);
    (g->sin_axiom_count)+=(kb_g->in_axiom_count);
    (g->sin_assumption_count)+=(kb_g->in_assumption_count);
    (g->sin_goal_count)+=(kb_g->in_goal_count);
    (g->sin_neg_goal_count)+=(kb_g->in_neg_goal_count);
    (g->sin_pos_goal_count)+=(kb_g->in_pos_goal_count);
    (g->sin_posunit_goal_count)+=(kb_g->in_posunit_goal_count);

  
    if ((kb_g->in_max_const_ucount)>(g->sin_max_const_ucount)) {
      (g->sin_max_const_ucount)=(kb_g->in_max_const_ucount);
      (g->sin_max_occ_const)=(kb_g->in_max_occ_const);
    }    
  }
}

void wr_copy_sin_stats(glb* fromg, glb* tog) {

  (tog->sin_clause_count)=(fromg->in_clause_count);
  (tog->sin_rule_clause_count)=(fromg->in_rule_clause_count);
  (tog->sin_fact_clause_count)=(fromg->in_fact_clause_count);
  (tog->in_answer_clause_count)=(fromg->in_answer_clause_count);
  (tog->in_ground_clause_count)=(fromg->in_ground_clause_count);
  (tog->in_unit_clause_count)=(fromg->in_unit_clause_count);
  (tog->in_horn_clause_count)=(fromg->in_horn_clause_count);
  (tog->in_pos_clause_count)=(fromg->in_pos_clause_count);
  (tog->in_neg_clause_count)=(fromg->in_neg_clause_count);
  (tog->in_poseq_clause_count)=(fromg->in_poseq_clause_count);
  (tog->in_negeq_clause_count)=(fromg->in_negeq_clause_count);
  (tog->in_unitposeq_clause_count)=(fromg->in_unitposeq_clause_count);
  (tog->in_chain_clause_count)=(fromg->in_chain_clause_count);
  (tog->in_min_length)=(fromg->in_min_length);
  (tog->in_max_length)=(fromg->in_max_length);
  (tog->in_min_depth)=(fromg->in_min_depth);
  (tog->in_max_depth)=(fromg->in_max_depth);
  (tog->in_min_size)=(fromg->in_min_size);
  (tog->in_max_size)=(fromg->in_max_size);
  (tog->in_min_vars)=(fromg->in_min_vars);
  (tog->in_max_vars)=(fromg->in_max_vars);
  (tog->in_average_length)=(fromg->in_average_length);
  (tog->in_average_depth)=(fromg->in_average_depth);
  (tog->in_predicate_count)=(fromg->in_predicate_count);
  (tog->in_funsymb_count)=(fromg->in_funsymb_count);
  (tog->in_extaxiom_count)=(fromg->in_extaxiom_count);
  (tog->in_axiom_count)=(fromg->in_axiom_count);
  (tog->in_assumption_count)=(fromg->in_assumption_count);
  (tog->in_goal_count)=(fromg->in_goal_count);
  (tog->in_neg_goal_count)=(fromg->in_neg_goal_count);
  (tog->in_pos_goal_count)=(fromg->in_pos_goal_count);
  (tog->in_posunit_goal_count)=(fromg->in_posunit_goal_count);
  (tog->in_max_const_ucount)=(fromg->in_max_const_ucount);
  (tog->in_max_occ_const)=(fromg->in_max_occ_const); 
}

// ==================== show stats ====================

void wr_show_in_stats(glb* g) {
  if (!(g->print_stats)) return;
    
  wr_printf("\ninput clause set statistics:\n");
  wr_printf("----------------------------------\n");

  wr_printf("in_clause_count:         %13d\n",g->in_clause_count);  
  wr_printf("in_rule_clause_count:    %13d\n",g->in_rule_clause_count);
  wr_printf("in_fact_clause_count:    %13d\n",g->in_fact_clause_count);
  wr_printf("in_answer_clause_count:  %13d\n",g->in_answer_clause_count);
  wr_printf("in_ground_clause_count:  %13d\n",g->in_ground_clause_count);
  wr_printf("in_unit_clause_count:    %13d\n",g->in_unit_clause_count);
  wr_printf("in_horn_clause_count:    %13d\n",g->in_horn_clause_count);
  wr_printf("in_pos_clause_count:     %13d\n",g->in_pos_clause_count);
  wr_printf("in_neg_clause_count:     %13d\n",g->in_neg_clause_count);
  wr_printf("in_poseq_clause_count:   %13d\n",g->in_poseq_clause_count);
  wr_printf("in_negeq_clause_count:   %13d\n",g->in_negeq_clause_count);
  wr_printf("in_unitposeq_clause_count:  %10d\n",g->in_unitposeq_clause_count);
  wr_printf("in_chain_clause_count:   %13d\n", g->in_chain_clause_count);
  wr_printf("in_min_length: %13d\n",g->in_min_length);
  wr_printf("in_max_length: %13d\n",g->in_max_length);
  wr_printf("in_min_depth:  %13d\n",g->in_min_depth);
  wr_printf("in_max_depth:  %13d\n",g->in_max_depth);
  wr_printf("in_min_size:   %13d\n",g->in_min_size);
  wr_printf("in_max_size:   %13d\n",g->in_max_size);
  wr_printf("in_min_vars:   %13d\n",g->in_min_vars);
  wr_printf("in_max_vars:   %13d\n",g->in_max_vars);
  //wr_printf("in_average_length:     %f\n",g->in_average_length);
  //wr_printf("in_average_depth:      %f\n",g->in_average_depth);
  //wr_printf("in_predicate_count:    %13d\n",g->in_predicate_count);
  //wr_printf("in_funsymb_count:      %13d\n",g->in_funsymb_count);
  wr_printf("in_extaxiom_count:     %13d\n",g->in_extaxiom_count);
  wr_printf("in_axiom_count:        %13d\n",g->in_axiom_count);
  wr_printf("in_assumption_count:   %13d\n",g->in_assumption_count);
  wr_printf("in_goal_count:         %13d\n",g->in_goal_count);
  wr_printf("in_neg_goal_count:     %13d\n",g->in_neg_goal_count);
  wr_printf("in_pos_goal_count:     %13d\n",g->in_pos_goal_count);
  wr_printf("in_posunit_goal_count: %13d\n",g->in_posunit_goal_count);
  //wr_printf("in_max_const_ucount:   %13d\n",g->in_max_const_ucount);
  //if (g->in_max_occ_const) {
  //  wr_printf("in_max_occ_const: %s \n", wg_decode_unistr(g->db,(g->in_max_occ_const),WG_URITYPE));
  //}
}

void wr_show_in_summed_stats(glb* g) {
  if (!(g->print_stats)) return;
    
  wr_printf("\ninput clause set summed statistics:\n");
  wr_printf("----------------------------------\n");

  wr_printf("in_clause_count:         %13d\n",g->sin_clause_count);  
  wr_printf("in_rule_clause_count:    %13d\n",g->sin_rule_clause_count);
  wr_printf("in_fact_clause_count:    %13d\n",g->sin_fact_clause_count);
  wr_printf("in_answer_clause_count:  %13d\n",g->sin_answer_clause_count);
  wr_printf("in_ground_clause_count:  %13d\n",g->sin_ground_clause_count);
  wr_printf("in_unit_clause_count:    %13d\n",g->sin_unit_clause_count);
  wr_printf("in_horn_clause_count:    %13d\n",g->sin_horn_clause_count);
  wr_printf("in_pos_clause_count:     %13d\n",g->sin_pos_clause_count);
  wr_printf("in_neg_clause_count:     %13d\n",g->sin_neg_clause_count);
  wr_printf("in_poseq_clause_count:   %13d\n",g->sin_poseq_clause_count);
  wr_printf("in_negeq_clause_count:   %13d\n",g->sin_negeq_clause_count);
  wr_printf("in_unitposeq_clause_count:  %10d\n",g->sin_unitposeq_clause_count);
  wr_printf("in_chain_clause_count:   %13d\n", g->sin_chain_clause_count);
  wr_printf("in_min_length: %13d\n",g->sin_min_length);
  wr_printf("in_max_length: %13d\n",g->sin_max_length);
  wr_printf("in_min_depth:  %13d\n",g->sin_min_depth);
  wr_printf("in_max_depth:  %13d\n",g->sin_max_depth);
  wr_printf("in_min_size:   %13d\n",g->sin_min_size);
  wr_printf("in_max_size:   %13d\n",g->sin_max_size);
  wr_printf("in_min_vars:   %13d\n",g->sin_min_vars);
  wr_printf("in_max_vars:   %13d\n",g->sin_max_vars);
  //wr_printf("in_average_length:     %f\n",g->sin_average_length);
  //wr_printf("in_average_depth:      %f\n",g->sin_average_depth);
  //wr_printf("in_predicate_count:    %13d\n",g->sin_predicate_count);
  //wr_printf("in_funsymb_count:      %13d\n",g->sin_funsymb_count);
  wr_printf("in_extaxiom_count:     %13d\n",g->sin_extaxiom_count);
  wr_printf("in_axiom_count:        %13d\n",g->sin_axiom_count);
  wr_printf("in_assumption_count:   %13d\n",g->sin_assumption_count);
  wr_printf("in_goal_count:         %13d\n",g->sin_goal_count);
  wr_printf("in_neg_goal_count:     %13d\n",g->sin_neg_goal_count);
  wr_printf("in_pos_goal_count:     %13d\n",g->sin_pos_goal_count);
  wr_printf("in_posunit_goal_count: %13d\n",g->sin_posunit_goal_count);
  //wr_printf("in_max_const_ucount:   %13d\n",g->sin_max_const_ucount);
  //if (g->sin_max_occ_const) {
  //  wr_printf("in_max_occ_const: %s \n", wg_decode_unistr(g->db,(g->sin_max_occ_const),WG_URITYPE));
  //}
}

#ifdef __cplusplus
}
#endif



