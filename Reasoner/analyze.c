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
  int tmp_inkb;
  
  if (db!=child_db) haveextdb=1;
  else haveextdb=0;
  wr_clear_all_varbanks(g);
  //printf("\n(g->inkb) %d (g->tmp_uriinfo) %d\n",(g->inkb),(g->tmp_uriinfo));
  if (1) { //!(g->tmp_uriinfo)) {
    tmp_inkb=(g->inkb);
    (g->inkb)=0;
    //printf("\n(g->inkb) %d\n",(g->inkb));
    (g->tmp_uriinfo)=wr_cvec_new(g,INITIAL_URITMPVEC_LEN);
    (g->inkb)=tmp_inkb;
  }
  /*
  cvec occs;
  int i;
  occs=wr_cvec_new(g,INITIAL_SINE_OCCVECLEN);
  for(i=0;i<100;i++) {
    printf("\nbefore push occs %ld occs[0] %ld occs[1] %ld i %d\n",(gint)occs,occs[0],occs[1],i);
    occs=wr_cvec_push(g,occs,i);
    printf("\nafter push occs %ld occs[0] %ld occs[1] %ld i %d\n",(gint)occs,occs[0],occs[1],i);
  }  
  exit(0);
  */    

  cell=(dbmemsegh(child_db)->clauselist);
  while(cell) {     
    cellptr=(gcell *) offsettoptr(child_db, cell);
    rec=offsettoptr(child_db,cellptr->car);
    tmp=wr_analyze_clause(g,rec,haveextdb);
    if (!tmp) {
      // something wrong with the clause
      wr_clear_all_varbanks(g);
      if (g->tmp_uriinfo) {
        tmp_inkb=(g->inkb);
        (g->inkb)=0;
        wr_vec_free(g,(g->tmp_uriinfo));
        (g->inkb)=tmp_inkb; 
      }  
      return 0;
    }
    n++;
    cell=cellptr->cdr;
  } 
  if (g->tmp_uriinfo) {
    tmp_inkb=(g->inkb);
    (g->inkb)=0;
    wr_vec_free(g,(gptr)(g->tmp_uriinfo));  
    (g->inkb)=tmp_inkb;
  }  
  return 1;  
}

int wr_analyze_clause(glb* g, gptr cl, int haveextdb) {
  void* db=g->db;
  gint history;
  gptr historyptr;
  int hlen, i; // j
  gint meta,atom=0;
  gint vc_tmp;
  int tmp;
  gint ctmp;
  int ruleflag=0, len=0, anslit=0, neglit=0, poslit=0;
  int poseq=0, negeq=0, uniteq=0;
  int size=0,maxdepth=0,varcount=0;
  gint priority, decprior=0; 
#ifdef REASONER_SINE
  cvec uriinfo;
  cvec occs;
#endif  
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

  //wr_print_clause(g,cl); 
  //wr_printf("\n");
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
#ifdef REASONER_SINE
  //printf("\n(g->tmp_uriinfo) initial %ld\n",(gint)(g->tmp_uriinfo));
  uriinfo=(gptr)(g->tmp_uriinfo); 
  //printf("\nuriinfo initial %ld\n",(gint)uriinfo);
  uriinfo[1]=2; // initialize  
#endif  
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
    // this is a recursive call into atom and then terms,
    // also adds new found uris to uriinfo cvec
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
      // this is a recursive call into atom and then terms,
      // also adds new found uris to uriinfo cvec
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
#ifdef REASONER_SINE
  //printf("\n(g->tmp_uriinfo) initial %ld\n",(gint)(g->tmp_uriinfo));
  // uriinfo cvec may have been reallocated while collecting uris
  uriinfo=(gptr)(g->tmp_uriinfo); 
  //printf("\nuriinfo initial %ld\n",(gint)uriinfo);  
  // add 1 to all elements of uriinfo cvec (uris present in clause)
  for(i=2;i<uriinfo[1];i++) {
    //printf("\nuri: ");    
    //wr_print_term_otter(g,uriinfo[i],100);
    // increase count
    //printf("\nbefore wg_decode_uri_scount i %d uriinfo[i] %ld\n",i,uriinfo[i]);
    ctmp=wg_decode_uri_scount(db,uriinfo[i]);
    //printf("\nwg_decode_uri_scount gave tmp %ld \n",ctmp);
    wg_set_uri_scount(db,uriinfo[i],ctmp+1);
    //printf("\nwg_decode_uri_scount after setting is %ld \n",wg_decode_uri_scount(db,uriinfo[i]));
    //printf(" count %ld ",wg_decode_uri_scount(db,uriinfo[i]));
    // add clause to occs list    
    tmp=(g->inkb);
    (g->inkb)=0; // use malloc temporarily
    //printf("\nbefore wg_decode_uri_occs i %d uriinfo[i] %ld\n",i,uriinfo[i]);
    occs=wg_decode_uri_occs(db,uriinfo[i]);
    //printf("\nwg_decode_uri_occs gave occs %ld \n",(gint)occs);
    //printf("\nuriinfo[i] %ld decoded %ld\n",uriinfo[i],occs);      
    if (!occs) {
      // no occs so far
      occs=wr_cvec_new(g,INITIAL_SINE_OCCVECLEN);
      if (!occs) {
        printf("\nfailed to alloc occs cvec while analyzing clause\n");
        (g->inkb)=1;
        return 1;
      } else {
        //printf("\nbefore occs1 occs %ld occs[0] %ld occs[1] %ld rpto(g,cl) %ld\n",(gint)occs,occs[0],occs[1],rpto(g,cl));
        occs=wr_cvec_push(g,occs,rpto(g,cl));
        //printf("\nafter occs1 occs %ld occs[0] %ld occs[1] %ld rpto(g,cl) %ld\n",(gint)occs,occs[0],occs[1],rpto(g,cl));
        if (!occs) {
          printf("\nfailed(1) to alloc occs after push cvec while analyzing clause\n");
          (g->inkb)=tmp;
          return 1;         
        }
        wg_set_uri_occs(db,uriinfo[i],occs);
      }  
      //printf("\nafter create occs[0] %ld occs[1] %ld\n",occs[0],occs[1]);
    } else {
      // add a new occ
      //printf("\nbefore occs2 occs %ld occs[0] %ld occs[1] %ld rpto(g,cl) %ld\n",(gint)occs,occs[0],occs[1],rpto(g,cl));
      occs=wr_cvec_push(g,occs,rpto(g,cl));
      //printf("\nafter occs2 occs %ld occs[0] %ld occs[1] %ld rpto(g,cl) %ld\n",(gint)occs,occs[0],occs[1],rpto(g,cl));     
      if (!occs) {
        printf("\nfailed(2) to alloc occs after push cvec while analyzing clause\n");
        (g->inkb)=tmp;
        return 1;
      } else {        
        wg_set_uri_occs(db,uriinfo[i],occs);
      }
    }
    // print occs elements
    //occs=wg_decode_uri_occs(db,uriinfo[i]);
    /*
    if (occs) {
      //printf("\noccs[0] %ld occs[1] %ld\n",occs[0],occs[1]);
      for(j=2;j<occs[1];j++) {
        printf("\n");
        wr_print_clause(g,rotp(g,occs[j]));
      }
      printf("\n");
    }
    */
    (g->inkb)=tmp;
  }
#endif  
  return 1;
}



int wr_analyze_term(glb* g, gint x, 
      int depth, int* size, int* maxdepth, int polarity, int haveextdb, int argpos) {
  void* db;
  gptr xptr;
  int i, start, end, j, urifound;  
  int w, dtype;
  gint ucount, ucountpos, ucountneg;
  int tmp_inkb;

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
#ifdef REASONER_SINE      
        urifound=0;
        cvec uriinfo=(gptr)(g->tmp_uriinfo);
        for(j=2; j<uriinfo[1]; j++) {
          if (uriinfo[j]==x) {
            urifound=1;
            break;
          }
        }        
        if (!urifound) {         
          //printf("\nuriinfo1 %ld\n",(gint)uriinfo);
          //printf("\nbefore uriinfo %ld uriinfo[0] %ld uriinfo[1] %ld x %ld\n",(gint)uriinfo,uriinfo[0],uriinfo[1],x);
          tmp_inkb=(g->inkb);
          (g->inkb)=0;
          uriinfo=wr_cvec_push(g,uriinfo,x);
          (g->inkb)=tmp_inkb;
          //printf("\nafter uriinfo %ld uriinfo[0] %ld uriinfo[1] %ld x %ld\n",(gint)uriinfo,uriinfo[0],uriinfo[1],x);
          //printf("\nuriinfo2 %ld\n",(gint)uriinfo);
          //printf("\n(g->tmp_uriinfo) %ld\n",(gint)(g->tmp_uriinfo));
          //printf("\nrpto(g,uriinfo) %ld\n",(gint)(rpto(g,uriinfo)));
          (g->tmp_uriinfo)=uriinfo;
        }                
#endif        

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

// ======= get symbols of a clause ===========




int wr_analyze_sine(glb* g, void* db, void* child_db) {
  gint cell;
  gcell *cellptr;
  gptr rec;
  int n, k, i, j, s, tmpinkb, sine_k, uri_k, inuri_k; //, haveextdb=0;
  cvec uriinfo, cl_uriinfo, occs;
  gint uri, uri_id, inuri,scount; //  inuri_id
  gptr cl;
  int sine_maxk;
  gint sine_common;
  gint sine_g;
  int sine_maxtriggers, sine_maxcommon;
  float sine_tolerance;
  int common_found=0;
  int triggers=0;
  int usedn, common_n;
  gint maxcount;
  int weak_sine;


  if ((g->sine_strat)==2) weak_sine=0;
  else weak_sine=1;

  //if (db!=child_db) haveextdb=1;
  //else haveextdb=0;
  
  //printf("\nwr_analyze_sine starts (g->sine_k_bytes) %ld \n",(g->sine_k_bytes));
  if (!(g->sine_k_values) || !(g->sine_k_bytes)) {
    return 0;
  }  

  // setup
  memset((g->sine_k_values),0,(g->sine_k_bytes));
  memset((g->sine_uri_k_values),0,(g->sine_uri_k_bytes));
  tmpinkb=(g->inkb);
  (g->inkb)=0; // use malloc temporarily
  uriinfo=wr_cvec_new(g,INITIAL_URITMPVEC_LEN);
  cl_uriinfo=wr_cvec_new(g,INITIAL_URITMPVEC_LEN);

  // params
  sine_maxk=120; // should fit into char
  sine_common=4; // initial value: this and above are common, smaller number means more pure-common clauses
  sine_tolerance=2; // bigger number means more triggering
  sine_g=5; // bigger number means more triggering
  sine_maxtriggers=1000; // how many are allowed to be triggered

  // loop over all clauses
  /*
  n=0;
  cell=(dbmemsegh(child_db)->clauselist);
  while(0 && cell) {     
    cellptr=(gcell *) offsettoptr(child_db, cell);
    rec=offsettoptr(child_db,cellptr->car);
    uriinfo[1]=2; // initialize uriinfo
    wr_get_clause_symbols(g,rec,&uriinfo);   
    n++;
    cell=cellptr->cdr;
  }  
  */
  // loop over symbols

  cell=(dbmemsegh(child_db)->urilist); 
  n=0;
  usedn=0;
  maxcount=0;
  while(cell) {
    cellptr=(gcell *) offsettoptr(child_db, cell);
    //rec=offsettoptr(child_db,cellptr->car); 
    uri=cellptr->car;
    //wr_printf("\nn %d %s \n",n,wg_decode_unistr(child_db,uri,WG_URITYPE));
    scount=wg_decode_uri_scount(db,uri);
    if (scount>0) {
      //wr_print_term_otter(g,uri,100);          
      //printf(" scount: %ld\n",scount);
      if (scount>maxcount) maxcount=scount;
      usedn++;
    }  
    cell=cellptr->cdr;         
    n++;
  }   
  // update sine_common

  // originally was
  //if (maxcount<10) sine_common=(gint)(maxcount*0.6);
  //if (maxcount<50) sine_common=(gint)(maxcount*0.7);
  //else sine_common=10; // normal 10
  // ie
  //if (maxcount<50) sine_common=(gint)(maxcount*0.7);
  //else sine_common=10; // normal 10


  // trying for strong and weak 
  /*
  if (maxcount<10) sine_common=(gint)(maxcount*0.6);
  else if (maxcount<50) sine_common=(gint)(maxcount*0.7);
  else if (maxcount<1000) sine_common=10;
  else if (maxcount<10000) sine_common=100;
  //else sine_common=1000;
  else sine_common=1000; //(gint)(maxcount*0.9);
  */

  //else sine_common=1000; // normal 10
  //sine_common=10000000;


  if (weak_sine) {
    if (maxcount<50) sine_common=(gint)(maxcount*0.7);
    else sine_common=10; // normal 10

    sine_maxcommon=1000000;
  } else {
    if (maxcount<50) { sine_common=(gint)(maxcount*0.7); }   
    sine_common=10; 
    
    sine_maxcommon=(int)(n/10.0);
    if (sine_maxcommon>10000) { sine_maxcommon=10000; }
    else if (sine_maxcommon<10) { sine_maxcommon=10; }
  }

  if (g->print_sine) {
    printf("\nn: %d usedn: %d maxcount: %ld sine_common: %ld sine_maxcommon: %d\n",
          n,usedn,maxcount,sine_common,sine_maxcommon);
  }        

  // collect initial symbols from goal and set goal k to 1
  // also check commonness

  cell=(dbmemsegh(child_db)->clauselist);
  uriinfo[1]=2; // initialize uriinfo
  n=0;
  while(cell) {
    cellptr=(gcell *) offsettoptr(child_db, cell);
    rec=offsettoptr(child_db,cellptr->car); 
    //printf("\n clid %ld cl: ",wr_get_clid(g,rec));
    //wr_print_clause(g,rec);
    //printf("\n");
    if (wr_cl_is_goal(g, rec)) {
      //printf("clause is goal ");
      wr_set_cl_sine_k(g,rec,1);     
      wr_get_clause_symbols(g,rec,&uriinfo);       
    } else if (sine_common && sine_maxcommon>common_found) {
      // check if a fully common-symbol clause      
      if (wr_sine_check_trigger_cl(g,rec,1,sine_tolerance,sine_common)) {
        wr_set_cl_sine_k(g,rec,sine_maxk+1);
        common_found++;
      }
    }
    cell=cellptr->cdr;         
    n++;
  }

  // update sine_maxtriggers
  if (weak_sine) {
    sine_maxtriggers=1000000;
  } else {
    sine_maxtriggers=(int)(n/10.0);
    if (sine_maxtriggers>10000) sine_maxtriggers=10000;
    else if (sine_maxtriggers<10) sine_maxtriggers=10;
  }

  // update sine_tolerance and g

  //sine1:

  if (n<100) {
    sine_tolerance=3.5; // bigger number means more triggering
    sine_g=7;
  } else if (n<1000) {
    sine_tolerance=3; // bigger number means more triggering
    sine_g=6;
  } else if (n<10000) {
    sine_tolerance=2; // bigger number means more triggering
    sine_g=5;
  } else {
    sine_tolerance=1.5; // bigger number means more triggering
    sine_g=4;
  }

  //sine2:
  
  if (n<100) {
    sine_tolerance=4; // bigger number means more triggering
    sine_g=6;
  } else if (n<1000) {
    sine_tolerance=3.5; // bigger number means more triggering
    sine_g=5;
  } else if (n<10000) {
    sine_tolerance=2; // bigger number means more triggering
    sine_g=3;
  } else {
    sine_tolerance=1.5; // bigger number means more triggering
    sine_g=2;
  }

  //sine3:
  
  if (n<100) {
    sine_tolerance=2; // bigger number means more triggering
    sine_g=4;
  } else if (n<1000) {
    sine_tolerance=2.5; // bigger number means more triggering
    sine_g=3;
  } else if (n<10000) {
    sine_tolerance=1.5; // bigger number means more triggering
    sine_g=2;
  } else {
    sine_tolerance=1.5; // bigger number means more triggering
    sine_g=1;
  }

  //sine4:
  
  if (n<100) {
    sine_tolerance=4; // bigger number means more triggering
    sine_g=4;
  } else if (n<1000) {
    sine_tolerance=4; // bigger number means more triggering
    sine_g=3;
  } else if (n<10000) {
    sine_tolerance=4; // bigger number means more triggering
    sine_g=2;
  } else if (n<100000) {
    sine_tolerance=4; // bigger number means more triggering
    sine_g=1;
  } else {
    sine_tolerance=1.5;
    sine_g=1;
  }
  //if (n>100000) sine_tolerance=1.5; // large csrs with sine_g=1;
 
  //sine5:
  
  /*
  if (n<100) {
    sine_tolerance=4; // bigger number means more triggering
    sine_g=3;
  } else if (n<1000) {
    sine_tolerance=4; // bigger number means more triggering
    sine_g=2;
  } else if (n<10000) {
    sine_tolerance=2; // bigger number means more triggering
    sine_g=2;
  } else {
    sine_tolerance=2; // bigger number means more triggering
    sine_g=1;
  }
  */

  //sine6:
  /*
  if (n<100) {
    sine_tolerance=2; // bigger number means more triggering
    sine_g=2;
  } else if (n<1000) {
    sine_tolerance=1.5; // bigger number means more triggering
    sine_g=2;
  } else if (n<10000) {
    sine_tolerance=1.5; // bigger number means more triggering
    sine_g=1;
  } else {
    sine_tolerance=1.5; // bigger number means more triggering
    sine_g=1;
  }
  */
  if (g->print_sine) {
    printf("\nclauses %d sine_tolerance %f sine_g %ld\n",n,sine_tolerance,sine_g);
  }  

  // set the goal uri k-s to 1

  //printf("\n=== setting goal uri k-s to 1 ===\n");

  for(s=2;s<uriinfo[1];s++) {
    uri=uriinfo[s];
    uri_id=wg_decode_uri_id(db,uri);
    if (uri_id>=(g->sine_uri_k_bytes)) continue;
    wr_set_uri_sine_k(g,uri,1);

    //printf("\ngoal uri id %ld k %d ",uri_id,wr_get_uri_sine_k(g,uri));
    //wr_print_term_otter(g,uri,100);
    //printf("\n");       
  }

  // do the main sine loop

  k=1;
  int dummy=1;
  while(dummy<2) {
    dummy=2;
    //printf("\n=== outer loop k: %d starts ===\n",k);
    // loop over k-level uris 
    //printf("\nuriinfo[1]: %ld",uriinfo[1]);
    for(i=2;i<uriinfo[1];i++) {
      if (triggers>=sine_maxtriggers) break;
      uri=uriinfo[i];
      uri_id=wg_decode_uri_id(db,uri);
      uri_k=wr_get_uri_sine_k(g,uri);
      k=uri_k;
      if (k>sine_maxk) continue; // cannot fit into char
      scount=wg_decode_uri_scount(db,uri);

      //printf("\n -- start symbol loop for i: %d for k: %d scount: %ld with uri: --- \n",i,k,scount);
      //wr_print_term_otter(g,uri,100); 
      //printf("\n");
      
      occs=wg_decode_uri_occs(db,uri);
      if (!occs) continue;   
      //printf("\noccs[1]: %ld\n",occs[1]);
      // loop over all occs of the uri
      for(j=2;j<occs[1];j++) {
        //printf("\nocc j: %d ",j);
        cl=rotp(g,occs[j]);
        //printf("clid %ld k %d cl: ",wr_get_clid(g,cl),wr_get_cl_sine_k(g,cl));
        //wr_print_clause(g,cl);
        // if not set or bigger, set clause sine_k to k+1
        sine_k=wr_get_cl_sine_k(g,cl);
        if (!sine_k || sine_k>(k+1)) {
          // check if uri triggers          
          if ((scount>sine_g) && 
              (!wr_sine_check_trigger_cl(g,cl,scount,sine_tolerance,0)) ) {
            continue; 
          }  
          // triggered ok
          //printf("\nuri triggers cl!\n");
          wr_set_cl_sine_k(g,cl,k+1);
          triggers++;
          if (triggers>=sine_maxtriggers) break;
          // get this clause uris
          cl_uriinfo[1]=2;
          wr_get_clause_symbols(g,cl,&cl_uriinfo);
          //printf("\n- loop over uris in occ clause -");
          // loop over uris in the occ clause
          for(s=2;s<cl_uriinfo[1];s++) {
            inuri=cl_uriinfo[s];
            //inuri_id=wg_decode_uri_id(db,inuri);
            inuri_k=wr_get_uri_sine_k(g,inuri);
            //printf("\ninuri id %ld k %d inuri: ",inuri_id,inuri_k);
            //wr_print_term_otter(g,inuri,100);
            //printf("\n");                      
            if (!inuri_k || inuri_k>k+1) {
              // set inuri (uri in occurring clause) to the same k+1 as clause
              wr_set_uri_sine_k(g,inuri,k+1);
              // push uri to uri list to handle later              
              uriinfo=wr_cvec_push(g,uriinfo,inuri);             
              //printf("\n* inuri_k set to %d\n",wr_get_uri_sine_k(g,inuri));
            }
          }            
        }
      }
      //printf("\n");
    }
  }

  // show result
  if (g->print_sine) {
    printf("\n=== sine result ==== \n");
    printf("\ncommon: %d",common_found);
    printf("\ntriggers: %d\n",triggers);
  }  
  cell=(dbmemsegh(child_db)->clauselist); 
  n=0;
  usedn=0;
  common_n=0;
  while(cell) {
    cellptr=(gcell *) offsettoptr(child_db, cell);
    rec=offsettoptr(child_db,cellptr->car); 
    if (wr_get_cl_sine_k(g,rec)) usedn++;
    if (wr_get_cl_sine_k(g,rec)>=120) common_n++;
    /*
    printf("clid %ld k %d cl: ",wr_get_clid(g,rec),wr_get_cl_sine_k(g,rec));
    wr_print_clause(g,rec);    
    if (wr_cl_is_goal(g, rec)) {
      printf(" clause is goal ");            
    }
    printf("\n");
    */
    cell=cellptr->cdr;         
    n++;
  }
  if (g->print_sine) {
    printf("\nclauses %d used by sine %d of these common %d\n",n,usedn,common_n);
  }  
 
  // final cleanup
  wr_vec_free(g,uriinfo);
  wr_vec_free(g,cl_uriinfo);
  (g->inkb)=tmpinkb;  

  //printf("\nwr_analyze_sine ends\n");
  //exit(0);  
  return 1;
}

/*
int wr_sine_uri_triggers_cl(glb* g, gint uri, gint scount, gptr cl, cvec cl_uriinfo) {    
  void* db=g->db;
  gint cluri,sc,minsc,urioccs;
  int i;

  minsc=100000000;
  for(i=2;i<cl_uriinfo[1];i++) {
    cluri=cl_uriinfo[i];
    sc=wg_decode_uri_scount(db,cluri);
    printf("\nsc %ld",sc)
    if (!sc) continue;
    if (sc>scount) return 0; // fails!
    if (sc<minsc) minsc=sc;
  }
  printf("scount %ld minsc %ld\n",scount,minsc);
  return 1;
}
*/

void wr_set_cl_sine_k(glb* g,gptr cl,int k) {
  gint clid;

  clid=wr_get_clid(g,cl);
  if (clid>=(g->sine_k_bytes)) return;
  (g->sine_k_values)[clid]=(char)k;
}

int wr_get_cl_sine_k(glb* g,gptr cl) {
  gint clid;

  clid=wr_get_clid(g,cl);
  if (clid>=(g->sine_k_bytes)) return 0;
  return (int)((g->sine_k_values)[clid]);
}

void wr_set_uri_sine_k(glb* g,gint uri,int k) {
  gint id;

  id=wg_decode_uri_id(g->db,uri);
  if (id>=(g->sine_uri_k_bytes)) return;
  (g->sine_uri_k_values)[id]=(char)k;
}

int wr_get_uri_sine_k(glb* g,gint uri) {
  gint id;

  id=wg_decode_uri_id(g->db,uri);
  if (id>=(g->sine_uri_k_bytes)) return 100;
  return (int)((g->sine_uri_k_values)[id]);
}


/*

for initialization do 
  uriinfo[1]=2; // initialize  
before calling  


*/


void wr_get_clause_symbols(glb* g, gptr cl, cvec* uriinfo) {
  void* db=g->db;
  int i, len;
  gint atom=0;   
  //gint uri;

  //printf("\nwr_get_clause_symbols called for \n");
  //wr_print_clause(g,cl); 
  //printf("\n");

  if (wg_rec_is_fact_clause(db,cl)) {   
    wr_get_term_symbols(g,pto(db,cl),uriinfo);
  } else if (wg_rec_is_rule_clause(db,cl)) {      
    len=wg_count_clause_atoms(db, cl);
    for(i=0; i<len; i++) {    
      atom=wg_get_rule_clause_atom(db,cl,i);     
      wr_get_term_symbols(g,atom,uriinfo);            
    }
  }
  /*
  for(i=2;i<(*uriinfo)[1];i++) {
    uri=(*uriinfo)[i];
    printf("\nuri: ");    
    wr_print_term_otter(g,uri,100); 
    printf("\nid %ld scount %ld",wg_decode_uri_id(db,uri),wg_decode_uri_scount(db,uri));     
  }
  */
  return;
}



void wr_get_term_symbols(glb* g, gint x, cvec* uriinfo) {
  void* db;
  gptr xptr;
  int i, start, end, j;
  gint dtype;
  cvec uinfo;

  if (!isdatarec(x)) {
    // now we have a simple value   
    if (isvar(x)) return;      
    db=g->db;
    dtype=wg_get_encoded_type(db,x);      
    if (dtype!=WG_URITYPE) return;    
#ifdef DEBUG                
    wr_printf("\nuri: ");
    wr_printf(" %s \n", wg_decode_unistr(db,x,WG_URITYPE));
#endif                     
    uinfo = *uriinfo;
    for(j=2; j<uinfo[1]; j++) {
      if (uinfo[j]==x) {
        return;
      }
    }        
    uinfo=wr_cvec_push(g,uinfo,x);
    if (uinfo!=*uriinfo) *uriinfo=uinfo;                 
    return;    
  }   
  // now we have a datarec
  db=g->db;
  xptr=decode_record(db,x);
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,xptr);
  for(i=start;i<end;i++) {
    if (!xptr[i]) return; // should not have 0 in args
    wr_get_term_symbols(g,xptr[i],uriinfo);
  }   
  return;
}


int wr_sine_check_trigger_cl(glb* g, gptr cl, gint scount, float tolerance, gint common) {
  void* db=g->db;
  int i, len;
  gint atom=0;   

  //printf("\nwr_sine_check_trigger_cl called for \n");
  //wr_print_clause(g,cl); 
  //printf("\n");

  if (wg_rec_is_fact_clause(db,cl)) {   
    return wr_sine_check_trigger_term(g,pto(db,cl),scount,tolerance,common);
  } else if (wg_rec_is_rule_clause(db,cl)) {      
    len=wg_count_clause_atoms(db, cl);
    for(i=0; i<len; i++) {    
      atom=wg_get_rule_clause_atom(db,cl,i);     
      if (!wr_sine_check_trigger_term(g,atom,scount,tolerance,common)) return 0;            
    }
  }
  return 1;  
}



int wr_sine_check_trigger_term(glb* g, gint x, gint scount, float tolerance, gint common) {
  void* db;
  gptr xptr;
  int i, start, end;
  gint dtype,sc;

  if (!isdatarec(x)) {
    // now we have a simple value   
    if (isvar(x)) return 1;      
    db=g->db;
    dtype=wg_get_encoded_type(db,x);      
    if (dtype!=WG_URITYPE) return 1;    
#ifdef DEBUG                
    wr_printf("\nuri: ");
    wr_printf(" %s \n", wg_decode_unistr(db,x,WG_URITYPE));
#endif        
    sc=wg_decode_uri_scount(db,x);
    if (common) {
      // commonness criteria
      if (sc<common) return 0;
      else return 1;
    } else {
      // normal sine
      if (scount>(sc*tolerance)) return 0;                    
      else return 1;    
    }  
  }   
  // now we have a datarec
  db=g->db;
  xptr=decode_record(db,x);
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,xptr);
  for(i=start;i<end;i++) {
    if (!xptr[i]) return 0; // should not have 0 in args
    if (!wr_sine_check_trigger_term(g,xptr[i],scount,tolerance,common)) return 0;   
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
  int eq,depth,length,size; //,bigratio;
  int qp1ok=1, qp2ok=1, qp3ok=1; // query preference strats default ok
  int tmp;

  if (kb_g &&  !(g->outfilename)) {
    wr_printf("\nglobal stats:\n");
    wr_show_in_stats(kb_g);
  }   
  make_sum_input_stats(g,kb_g);
  if (!(g->outfilename)) {
    wr_show_in_summed_stats(g);
  }  
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
  if ( (g->sin_goal_count)*3 > (g->in_clause_count)*2) {
    // huge goal, default query pref is useless
    qp1ok=0;
  }
  if ( tmp*3 > (g->in_clause_count)*2) {
    // huge goal+assumptions, default query pref + assumption-rich is useless     
    qp2ok=0;
  }

  buf=(char*)wr_malloc(g,10000);
  // normal "\"print_level\": 15,\n"
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
  //bigratio=20;
  for(i=0;i<iterations;i++) {

    // start of a block

    // ---

    if ((g->sin_clause_count)>100000) { // largest
      // very large          

      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2},\n",secs);
      }
      if (qp3ok) {         
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 3},\n",secs);          
      }
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 1},\n",secs);
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

      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"positive_pref\"], \"query_preference\": 2},\n",secs);
      }  

      // sine

      if (qp2ok && i<2) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2, \"sine\":1},\n",secs);
      }
      if (qp3ok && i<2) {         
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 3, \"sine\":1},\n",secs);          
      }
      if (qp1ok && i<2) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 1, \"sine\":2},\n",secs);
      }
      if (qp1ok && i<2) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 1, \"sine\":1},\n",secs);
      }


      // -- maybe --

      if (i<1) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"positive_pref\"], \"query_preference\": 1},\n",secs);      
        
        if (qp3ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"positive_pref\"], \"query_preference\": 3},\n",secs);
        }
        // size limit mod
        if (qp1ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 1, \"max_depth\": %d},\n",secs,depth);
        }  
      
        if (qp2ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2, \"max_size\": %d},\n",secs,size);
        }  
      }  
      
    // ---

    } else if ((g->sin_clause_count)>20000) {  // < 100000 
      // very large
      
      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2},\n",secs);
      }     
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"unit\"], \"query_preference\": 0},\n",secs);  

      // sine

      if (qp2ok && i<2) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2, \"sine\":1},\n",secs);
      }

      if (i<2) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"unit\"], \"query_preference\": 0, \"sine\":2},\n",secs);
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

      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2, \"max_size\": %d},\n",secs,size);  
      }  
      
      // -- maybe --

      if (i<1) {
        if (qp1ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 1},\n",secs);
        }  
        if (qp1ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 1,\"sine\":1},\n",secs);
        }
        if (qp3ok) {         
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 3},\n",secs);          
        }
        if (i<1) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"unit\",\"pure_unit\"],\"query_preference\":0},\n",secs);
        }
      }  

     // ---

     } else if ((g->sin_clause_count)>5000) { // < 20000
      // large       
      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2},\n",secs);
      }
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 1},\n",secs);
      } 
      if (qp3ok) {         
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 3},\n",secs);          
      }
      // unit mod
      
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"unit\"], \"query_preference\": 0},\n",secs);
      
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"unit\"], \"query_preference\": 1},\n",secs);
      }  

      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"positive_pref\"], \"query_preference\": 0},\n",secs);
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"positive_pref\"], \"query_preference\": 1},\n",secs);  
      }  

      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2, \"max_size\": %d},\n",secs,size);
      }
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d,\"strategy\":[\"query_focus\",\"positive_pref\"],\"query_preference\":0,\"max_size\":30},\n",secs);
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d,\"strategy\":[\"query_focus\",\"positive_pref\"],\"query_preference\":0,\"var_weight\":1, \"repeat_var_weight\":1},\n",secs);    
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d,\"strategy\":[\"query_focus\",\"positive_pref\"],\"query_preference\":0,\"max_size\":30,\"depth_penalty\":50,\"length_penalty\":100},\n",secs);
      if (eq) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"query_focus\",\"positive_pref\",\"prohibit_nested_para\"],\"query_preference\":0},\n",secs);
      }  

      // maybe

      if (i<1) {
        if (qp2ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"unit\"], \"query_preference\": 2},\n",secs);
        }  
        if (qp3ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"unit\"], \"query_preference\": 3},\n",secs);
        }
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"unit\"],\"query_preference\":0},\n",secs);
        if (i<1) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"unit\",\"pure_unit\"],\"query_preference\":0},\n",secs);
        }
        if (eq) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"query_focus\",\"posunitpara\"],\"query_preference\":1,\"depth_penalty\":50,\"length_penalty\":100, \"sine\":1},\n",secs);
        }
        if (qp1ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 1, \"sine\":1},\n",secs);
        }
        if (qp1ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 1, \"sine\":2},\n",secs);
        }
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":0,\"depth_penalty\":100, \"sine\":1},\n",secs);
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"unit\"],\"query_preference\":0,\"sine\":2},\n",secs);
        if (qp2ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2, \"sine\":1},\n",secs);
        }
        
      }
     
    // ---

     } else if ((g->sin_clause_count)>1000) { // < 5000
      // large
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 1},\n",secs);
      }  
      if (qp2ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2},\n",secs);
      }      
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"unit\"], \"query_preference\": 0},\n",secs);
      
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
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 1},\n",secs);
      }    
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 0},\n",secs);

      if (eq) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"hardness_pref\",\"posunitpara\"],\"query_preference\":1},\n",secs);
      }
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":0,\"depth_penalty\":100},\n",secs);
      if (eq) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"query_focus\",\"posunitpara\"],\"query_preference\":1,\"depth_penalty\":50,\"length_penalty\":100},\n",secs);
      }
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d,\"strategy\":[\"query_focus\",\"max_ground_weight\"],\"query_preference\":1},\n",secs);

      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\":1, \"reverse_clauselist\":1},\n",secs);
     
    
      // single

      if (i<2) {
          
        if (qp3ok) {  
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"query_focus\"],\"query_preference\":3},\n",secs);
        }
        if (qp2ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"query_focus\",\"unit\"],\"query_preference\":2},\n",secs);
        }
        if (qp1ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"query_focus\",\"positive_pref\"],\"query_preference\":1},\n",secs);
        }
        if (qp1ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"query_focus\"],\"query_preference\":1,\"max_depth\":2},\n",secs);
        }
        if (qp1ok) {  
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"positive_pref\",\"unit\"], \"query_preference\":1, \"reverse_clauselist\":1, \"weight_select_ratio\": 2},\n",secs);
        }
        
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"hardness_pref\"],\"query_preference\":1},\n",secs);  

        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"unit\"],\"query_preference\":0},\n",secs);    

        if (eq) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"query_focus\",\"posunitpara\"],\"query_preference\":1,\"depth_penalty\":50,\"length_penalty\":100, \"sine\":1},\n",secs);
        }

        if (qp1ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 1, \"sine\":1},\n",secs);
        }
        if (qp1ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 1, \"sine\":2},\n",secs);
        }
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":0,\"depth_penalty\":100, \"sine\":1},\n",secs);

        // maybe

        if (i<1) {
          if (qp3ok) {         
            pos+=sprintf(buf+pos,
            "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 3},\n",secs);  
            pos+=sprintf(buf+pos,
            "{\"max_seconds\": %d,\"strategy\":[\"unit\",\"pure_unit\"],\"query_preference\":0},\n",secs); 
            if (eq) {
              pos+=sprintf(buf+pos,
              "{\"max_seconds\": %d,\"strategy\":[\"unit\",\"posunitpara\"],\"query_preference\":0},\n",secs);        
            }  

          }
          if (qp2ok) {
            pos+=sprintf(buf+pos,
            "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2, \"sine\":1},\n",secs);
          }
          if (qp1ok) {
            pos+=sprintf(buf+pos,
            "{\"max_seconds\": %d,\"strategy\":[\"hardness_pref\"],\"query_preference\":1, \"sine\":1},\n",secs);
          }  
        }
      }
     
    // ---

    } else if ((g->sin_clause_count)>100) { // < 1000

      pos+=sprintf(buf+pos,     
      "{\"max_seconds\": %d,\"strategy\":[\"hardness_pref\"],\"query_preference\":0},\n",secs);      
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":0},\n",secs);      
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":1},\n",secs);
      }
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"query_focus\"],\"query_preference\":1},\n",secs);
      }
      if (qp3ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"query_focus\"],\"query_preference\":3},\n",secs);
      }
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d,\"strategy\":[\"query_focus\",\"positive_pref\"],\"query_preference\":0},\n",secs);      
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"query_focus\",\"positive_pref\"],\"query_preference\":1},\n",secs);
      }
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d,\"strategy\":[\"unit\"],\"query_preference\":0},\n",secs);
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"unit\"],\"query_preference\":1,\"reverse_clauselist\":1},\n",secs);
      }
      if (qp1ok) {
        pos+=sprintf(buf+pos, 
        "{\"max_seconds\": %d,\"strategy\":[\"query_focus\"],\"query_preference\":1,\"depth_penalty\": 100},\n",secs);
      }

      // single 

      if (i<2) {        
        if (qp1ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"unit\"],\"query_preference\":1,\"max_depth\":2},\n",secs);
        }
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":0,\"max_depth\":3},\n",secs);
        pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d, \"strategy\":[\"hardness_pref\"], \"query_preference\": 0,\"sine\":1},\n",secs);
        if (qp2ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"query_focus\"],\"query_preference\":2},\n",secs);
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"query_focus\",\"positive_pref\"],\"query_preference\":2},\n",secs);
        }              
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":1,\"length_penalty\":100},\n",secs);      
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"unit\"],\"query_preference\":0,\"depth_penalty\":100},\n",secs);       
        if (qp1ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"query_focus\"],\"query_preference\":1,\"depth_penalty\":50,\"length_penalty\":100},\n",secs);
        }      
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":1,\"depth_penalty\":50,\"length_penalty\":100},\n",secs);      
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":0,\"depth_penalty\":50,\"length_penalty\":100},\n",secs);      
        if (eq) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\",\"posunitpara\"],\"query_preference\":1,\"max_depth\":2,\"depth_penalty\":50,\"length_penalty\":100},\n",secs);
        }
        if (eq) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"hardness_pref\",\"posunitpara\"],\"query_preference\":0,\"depth_penalty\":50,\"length_penalty\":100},\n",secs);
        }
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"hardness_pref\",\"max_ground_weight\"],\"query_preference\":0},\n",secs);            
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":1,\"max_depth\": 2},\n",secs);            
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"query_focus\"],\"query_preference\":1,\"max_depth\": 2},\n",secs);      
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":0,\"max_depth\": 1},\n",secs);     
        //pos+=sprintf(buf+pos,
        //"{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":1,\"reverse_clauselist\": 1},\n",secs);
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"unit\",\"pure_unit\"],\"query_preference\":0},\n",secs);   

        pos+=sprintf(buf+pos,     
        "{\"max_seconds\": %d,\"strategy\":[\"hardness_pref\"],\"query_preference\":0, \"sine\":1},\n",secs);
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":0, \"sine\":1},\n",secs);   
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"unit\"],\"query_preference\":0, \"sine\":1},\n",secs); 
        if (qp1ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"query_focus\"],\"query_preference\":1, \"sine\":2},\n",secs);
        }  
        if (qp1ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":1, \"sine\":2},\n",secs);
        }                
      }
    
    // ---

    } else if ((g->sin_clause_count)>20) { // < 100

      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":1},\n",secs);
      }
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d,\"strategy\":[\"unit\"],\"query_preference\":0},\n",secs);
      if (qp1ok) {      
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"unit\"],\"query_preference\":1},\n",secs);
      }
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":0},\n",secs);
      if (qp1ok) {      
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"query_focus\"],\"query_preference\":1},\n",secs);
      }

      //dual:
      pos+=sprintf(buf+pos, 
      "{\"max_seconds\": %d,\"strategy\":[\"positive_pref\"],\"query_preference\":0,\"reverse_clauselist\":1},\n",secs);
      if (qp1ok && eq) {      
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":1, \"equality\":0},\n",secs);
      }

      //exp:

      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d,\"strategy\":[\"positive_pref\"],\"query_preference\":0,\"reverse_clauselist\":1,\"length_penalty\":100},\n",secs);      
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d,\"strategy\":[\"unit\"],\"query_preference\":0,\"depth_penalty\": 100},\n",secs);      
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d,\"strategy\":[\"query_focus\"],\"query_preference\":1,\"var_weight\":1, \"repeat_var_weight\":1},\n",secs);
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":1,\"var_weight\":1, \"repeat_var_weight\":1},\n",secs);
      }

      //single:
      
      if (i<2) {
        if (qp1ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":1,\"reverse_clauselist\":1},\n",secs);
        }
        if (qp1ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":1, \"weight_select_ratio\":20},\n",secs);
        }
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":0,\"max_depth\":2},\n",secs);      
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"unit\"],\"query_preference\":0,\"max_depth\":4},\n",secs);      
        if (qp1ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"unit\"],\"query_preference\":1,\"reverse_clauselist\":1},\n",secs);
        }
        

        //exp single:

        if (qp1ok) {
          pos+=sprintf(buf+pos, 
          "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":1,\"max_size\":30},\n",secs);
        }
        if (qp1ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"query_focus\",\"double\"],\"query_preference\":1},\n",secs);
        }
        if (eq) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"unit\",\"posunitpara\"],\"query_preference\":0},\n",secs);
        }
        if (qp1ok && eq) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"unit\",\"posunitpara\"],\"query_preference\":1},\n",secs);
        }
        if (qp1ok) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":1, \"var_weight\":1, \"repeat_var_weight\":1},\n",secs);
        }
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"], \"var_weight\":1, \"repeat_var_weight\":1,\
         \"depth_penalty\":100, \"length_penalty\":100, \"max_length\":3,\"max_depth\":3},\n",secs);
        if (i<1) {
          pos+=sprintf(buf+pos,
          "{\"max_seconds\": %d,\"strategy\":[\"unit\",\"pure_unit\"],\"query_preference\":0},\n",secs);
        }  
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":0, \"sine\":1},\n",secs);
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"unit\"],\"query_preference\":0, \"sine\":1},\n",secs);
      }  

    // ----


    } else  { // < 20
      // very small size

      pos+=sprintf(buf+pos,  
      "{\"max_seconds\": %d,\"strategy\":[\"unit\"],\"query_preference\":0},\n",secs);
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"unit\"],\"query_preference\":1},\n",secs);
      }
      pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":0},\n",secs);
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":1},\n",secs);
      }     

      // exp:
       
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":1,\"weight_select_ratio\":20},\n",secs);
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d,\"strategy\":[\"unit\"],\"query_preference\":0, \"var_weight\":1, \"repeat_var_weight\":1},\n",secs);      
      if (eq) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"unit\",\"prohibit_nested_para\"],\"query_preference\":1,\"reverse_clauselist\":1},\n",secs);
      }
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":0, \"depth_penalty\":100},\n",secs);
      if (eq) {       
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\",\"prohibit_nested_para\"],\"query_preference\":0},\n",secs);
      }
      pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":0, \"depth_penalty\":50, \"length_penalty\":100},\n",secs);      
      if (qp1ok) {
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":1,\"weight_select_ratio\":20,\"depth_penalty\": 50, \"length_penalty\":100},\n",secs);
      }
      if (i<2) {         
         pos+=sprintf(buf+pos,
         "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":0},\n",secs);
      }
      if (i<1) {
        pos+=sprintf(buf+pos,
         "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":0,\"max_depth\":4},\n",secs);
        pos+=sprintf(buf+pos,
         "{\"max_seconds\": %d,\"strategy\":[\"negative_pref\"],\"query_preference\":0,\"max_depth\":8},\n",secs); 
        pos+=sprintf(buf+pos,
        "{\"max_seconds\": %d,\"strategy\":[\"unit\",\"pure_unit\"],\"query_preference\":0},\n",secs);
      }
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
  
  if (!(g->outfilename)) {
    wr_printf("\nauto guide:\n-----------\n%s\n",buf);
  }     
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

    if (kb_g->in_has_fof) (g->in_has_fof)=1;

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

  if (fromg->in_has_fof) (tog->in_has_fof)=1;

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


// ============ LTB specific ==================


void make_ltb_guide(glb* g, char** strats, int stratscount) { 
  char *buf=NULL,*pref;   
  int i,pos,secs,stratn;
  
  for(i=0;i<stratscount;i++) {
    strats[i]=NULL;
  }
  stratn=0;
  for(i=0;i<1;i++) {

    // first batch, 3 sec total
    secs=1;
    buf=(char*)malloc(10000);  
    pref="{\n"
        "\"print_level\": 5,\n"; 
    pos=sprintf(buf,"%s",pref);
    pos+=sprintf(buf+pos,"\"runs\":[\n");
    pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 1},\n",secs);
    pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 1},\n",secs);
    pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 1}\n",secs);    
    pos+=sprintf(buf+pos,"]}\n");    
    if (stratn<stratscount) strats[stratn++]=buf;

    // second batch, 5 sec total, ends at 8 sec
    secs=1;
    buf=(char*)malloc(10000);  
    pref="{\n"
        "\"print_level\": 5,\n"; 
    pos=sprintf(buf,"%s",pref);
    pos+=sprintf(buf+pos,"\"runs\":[\n");
    pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2},\n",secs);
    pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 0},\n",secs);
    pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"positive_pref\"], \"query_preference\": 3},\n",secs);
    pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"query_focus\",\"unit\"], \"query_preference\": 1},\n",secs);  
    pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 2, \"equality\":0}\n",secs);
    pos+=sprintf(buf+pos,"]}\n");    
    if (stratn<stratscount) strats[stratn++]=buf;

    // third batch, 15 sec total, ends at 23 sec
    secs=5;
    buf=(char*)malloc(10000);  
    pref="{\n"
        "\"print_level\": 5,\n"; 
    pos=sprintf(buf,"%s",pref);
    pos+=sprintf(buf+pos,"\"runs\":[\n");
    pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 1},\n",secs);
    pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 2},\n",secs);
    pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 0}\n",secs);    
    pos+=sprintf(buf+pos,"]}\n");
    if (stratn<stratscount) strats[stratn++]=buf;

    // fourth batch, 20 sec total, ends at 43 sec
    secs=5;
    buf=(char*)malloc(10000);  
    pref="{\n"
        "\"print_level\":  5,\n"; 
    pos=sprintf(buf,"%s",pref);
    pos+=sprintf(buf+pos,"\"runs\":[\n");
    pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2},\n",secs);
    pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 0, \"reverse_clauselist\": 1},\n",secs);
    pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 2},\n",secs);    
    pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"hyper\"], \"query_preference\": 1}\n",secs);  
    pos+=sprintf(buf+pos,"]}\n");
    if (stratn<stratscount) strats[stratn++]=buf;

    // fifth batch 
    secs=15;
    buf=(char*)malloc(10000);  
    pref="{\n"
        "\"print_level\":  5,\n"; 
    pos=sprintf(buf,"%s",pref);
    pos+=sprintf(buf+pos,"\"runs\":[\n");
    pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 1},\n",secs);
    pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"query_focus\"], \"query_preference\": 2},\n",secs);
    pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 1},\n",secs);  
    pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"negative_pref\"], \"query_preference\": 0, \"reverse_clauselist\": 1},\n",secs);
    pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"unit\"], \"query_preference\": 1},\n",secs);    
    pos+=sprintf(buf+pos,
      "{\"max_seconds\": %d, \"strategy\":[\"hyper\"], \"query_preference\": 1}\n",secs); 
    pos+=sprintf(buf+pos,"]}\n");
    if (stratn<stratscount) strats[stratn++]=buf;

    /*
    // end of one block
    if (i<iterations-1) {
      pos+=sprintf(buf+pos,"\n");      
    }
    // limit mods
    secs=secs*5;   
    */
  }
  /*
  for(i=0;i<stratscount;i++) {
    if (strats[i]) {
      printf("\n strat %d is \n%s\n",i,strats[i]);
    }    
  }
  */
  return;
}

#ifdef __cplusplus
}
#endif



