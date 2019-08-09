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
 /** @file clterm.c
 *  Procedures for managing clause history.
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

// #define HISTORY_MEMPOOL_SIZE 1000000
//static int show_history_error(glb* g, char* errmsg);
//static int show_history_error_nr(glb* g, char* errmsg, int nr); 
/*
#define HISTORY_PREFIX_LEN 2
#define HISTORY_DERIVATION_TAG_POS HISTORY_PREFIX_LEN+0

#define HISTORY_PARENT1_POS HISTORY_PREFIX_LEN+1
#define HISTORY_PARENT2_POS HISTORY_PREFIX_LEN+2

#define HISTORY_PATH_POS HISTORY_PREFIX_LEN+1
#define HISTORY_PARA_PARENT1_POS HISTORY_PREFIX_LEN+2
#define HISTORY_PARA_PARENT2_POS HISTORY_PREFIX_LEN+3
*/




#define EXPAND(X) X

#define wrr_printf(...) EXPAND(printf(__VA_ARGS__))




/* ======= Private protos ================ */
  
void wr_print_history_extra(glb* g, gint history);
gint wr_encode_priority(glb* g, gint priority);



/* ====== Functions ============== */

/* ------- building history ----------- */

gint wr_build_input_history(glb* g, gptr cl1, char* name, gint priority) {  
  void* db=g->db;
  gptr rec; 
  int datalen=HISTORY_PREFIX_LEN;
  gint encpriority;
  gint encname;

  if (g->store_history) {
    rec=wr_create_raw_history_record(g,datalen,g->build_buffer);
    if (rec==NULL) return wg_encode_null(db,NULL);
    encpriority=wr_encode_priority(g,priority);
    if (name==NULL) encname=(gint)NULL;
    else {
      encname=wg_encode_str(db,name,NULL); 
      if (!encname) return wg_encode_null(db,NULL);
    }
    wr_set_history_record_field(g,rec,HISTORY_PRIORITY_POS,encpriority);
    wr_set_history_record_field(g,rec,HISTORY_NAME_POS,encname);       
    wr_set_history_record_derived_order(g,rec);
    return wg_encode_record(db,rec); 
  } else {
    return wg_encode_int(g->db,1);
  }  
}


gint wr_build_resolve_history(glb* g, gptr cl1, gptr cl2, int pos1, int pos2, gptr cut_clvec) {  
  void* db=g->db;
  gptr rec;
  gint tag;
  int i,cutn=0;  
  int datalen=HISTORY_PREFIX_LEN+3;

  if (g->store_history) {
    if (cut_clvec) {
      for(cutn=1; cutn<(int)(cut_clvec[0]) && (gptr)(cut_clvec[cutn])!=NULL; cutn++) {}  
      cutn=cutn-1;      
    }
    // cutn is now the nr of cut literals

    rec=wr_create_raw_history_record(g,datalen+cutn,g->build_buffer);
    if (rec==NULL) return wg_encode_null(db,NULL);

    tag=wr_encode_history_resolve(g,pos1,pos2);
    wr_set_history_record_field(g,rec,HISTORY_DERIVATION_TAG_POS,tag);
    wr_set_history_record_field(g,rec,HISTORY_PARENT1_POS,wg_encode_record(db,cl1));
    wr_set_history_record_field(g,rec,HISTORY_PARENT2_POS,wg_encode_record(db,cl2));
    wr_set_history_record_field(g,rec,HISTORY_PRIORITY_POS,wr_calc_history_priority2(g,cl1,cl2)); 
    for(i=0; i<cutn; i++) {
      wr_set_history_record_field(g,rec,datalen+i,wg_encode_record(db,(gptr)(cut_clvec[i+1])));
    }      
    wr_set_history_record_derived_order(g,rec);
    return wg_encode_record(db,rec); 
    //return wg_encode_int(g->db,g->stat_built_cl); 
  } else {
    return wg_encode_int(g->db,1);
  }  
}


gint wr_build_instgen_history(glb* g, gptr cl1, gptr cl2, int pos1, int pos2, gptr cut_clvec) {  
  void* db=g->db;
  gptr rec;
  gint tag;
  int i,cutn=0;  
  int datalen=HISTORY_PREFIX_LEN+3;

  if (g->store_history) {
    if (cut_clvec) {
      for(cutn=1; cutn<(int)(cut_clvec[0]) && (gptr)(cut_clvec[cutn])!=NULL; cutn++) {}  
      cutn=cutn-1;      
    }
    // cutn is now the nr of cut literals

    rec=wr_create_raw_history_record(g,datalen+cutn,g->build_buffer);
    if (rec==NULL) return wg_encode_null(db,NULL);

    tag=wr_encode_history_instgen(g,pos1,pos2);
    wr_set_history_record_field(g,rec,HISTORY_DERIVATION_TAG_POS,tag);
    wr_set_history_record_field(g,rec,HISTORY_PARENT1_POS,wg_encode_record(db,cl1));
    wr_set_history_record_field(g,rec,HISTORY_PARENT2_POS,wg_encode_record(db,cl2));
    wr_set_history_record_field(g,rec,HISTORY_PRIORITY_POS,wr_calc_history_priority2(g,cl1,cl2)); 
    for(i=0; i<cutn; i++) {
      wr_set_history_record_field(g,rec,datalen+i,wg_encode_record(db,(gptr)(cut_clvec[i+1])));
    }      
    wr_set_history_record_derived_order(g,rec);
    return wg_encode_record(db,rec); 
    //return wg_encode_int(g->db,g->stat_built_cl); 
  } else {
    return wg_encode_int(g->db,1);
  }  
}

// input priority stores original markup / role

// just encode int, WR_HISTORY_USED_BIT_POS is 0 to indicate "not given yet"

gint wr_encode_priority(glb* g, gint priority) {
  gint res;
  
  res=wg_encode_int(g->db,priority);
  return res;
}

// decode int, and clear WR_HISTORY_USED_BIT_POS (if 1, indicates used / given)

gint wr_decode_priority(glb* g, gint encpriority) {
  gint res;
  
  res=wg_decode_int(g->db,encpriority);
  res=res & ~((gint)(1<<WR_HISTORY_USED_BIT_POS)); // mask used bit
  return res;
}


// used priority stores our preference for how we use the role

// just encode int, 
// WR_HISTORY_USED_BIT_POS is 0 to indicate "not given yet"

gint wr_encode_used_priority(glb* g, gint priority, gint oldpriority) {
  gint res;
  
  res=wg_encode_int(g->db,priority);
  return res;
}

// decode int, and clear WR_HISTORY_USED_BIT_POS (if 1, indicates used / given)

gint wr_decode_used_priority(glb* g, gint encpriority) {
  gint res;
  
  res=wg_decode_int(g->db,encpriority);
  res=res & ~((gint)(1<<WR_HISTORY_USED_BIT_POS)); // mask used bit
  return res;
}


gint wr_calc_history_priority2(glb* g, gptr cl1, gptr cl2) {
  //void *db=g->db;
  gint hist1, hist2;
  gptr hist1ptr, hist2ptr;
  gint prior1=0, prior2=0;
  gint decprior1=WR_HISTORY_FROMAXIOM_ROLENR, decprior2=WR_HISTORY_FROMAXIOM_ROLENR;
  gint resprior=WR_HISTORY_FROMAXIOM_ROLENR;

  if (cl1!=NULL) {  
    hist1=wr_get_history(g,cl1);
    if (hist1) {
      hist1ptr=rotp(g,hist1);
      prior1=wr_get_history_record_field(g,hist1ptr,HISTORY_PRIORITY_POS);
      if (prior1) decprior1=wr_decode_priority(g,prior1);
    }
  } else decprior1=0;

  if (cl2!=NULL) {  
    hist2=wr_get_history(g,cl2);
    if (hist2) {
      hist2ptr=rotp(g,hist2);
      prior2=wr_get_history_record_field(g,hist2ptr,HISTORY_PRIORITY_POS);
      if (prior2) decprior2=wr_decode_priority(g,prior2);
    }
  } else decprior2=0;

  // modify some priorities according to our strat

  if ((g->cl_pick_queue_strategy)==2) {
    // make non-included axioms assumptions and positive conjecture part assumptions
    if (decprior1==WR_HISTORY_GOAL_ROLENR && wr_is_positive_unit_cl(g, cl1)) 
      decprior1=WR_HISTORY_ASSUMPTION_ROLENR;  
    else if (decprior1==WR_HISTORY_AXIOM_ROLENR && (g->parse_is_included_file))
       decprior1=WR_HISTORY_ASSUMPTION_ROLENR;    
    if (decprior2==WR_HISTORY_GOAL_ROLENR && wr_is_positive_unit_cl(g, cl2)) 
      decprior2=WR_HISTORY_ASSUMPTION_ROLENR;      
    else if (decprior2==WR_HISTORY_AXIOM_ROLENR && (g->parse_is_included_file))
      decprior2=WR_HISTORY_ASSUMPTION_ROLENR;
  } else if ((g->cl_pick_queue_strategy)==3) {
    // only fully negative clauses of goal made goal and no assumptions (ie made axioms)
    if (decprior1==WR_HISTORY_GOAL_ROLENR && !wr_is_negative_cl(g,cl1))
      decprior1=WR_HISTORY_AXIOM_ROLENR;  
    else if (decprior1==WR_HISTORY_ASSUMPTION_ROLENR)  
      decprior1=WR_HISTORY_AXIOM_ROLENR; //!! previously had no effect      
    if (decprior2==WR_HISTORY_GOAL_ROLENR  && !wr_is_negative_cl(g,cl2))
      decprior2=WR_HISTORY_AXIOM_ROLENR;    
    else if (decprior2==WR_HISTORY_ASSUMPTION_ROLENR)  
      decprior2=WR_HISTORY_AXIOM_ROLENR; //!! previously had no effect
  }

  // calculate new priorities 

  // WR_HISTORY_FROMGOALASSUMPTION_ROLENR
  if ( decprior1==WR_HISTORY_FROMGOALASSUMPTION_ROLENR ||
       decprior2==WR_HISTORY_FROMGOALASSUMPTION_ROLENR ||
      ((decprior1==WR_HISTORY_GOAL_ROLENR || decprior1==WR_HISTORY_FROMGOAL_ROLENR) &&
       (decprior2==WR_HISTORY_ASSUMPTION_ROLENR || decprior2==WR_HISTORY_FROMASSUMPTION_ROLENR)) ||
      ((decprior2==WR_HISTORY_GOAL_ROLENR || decprior2==WR_HISTORY_FROMGOAL_ROLENR) &&
       (decprior1==WR_HISTORY_ASSUMPTION_ROLENR || decprior1==WR_HISTORY_FROMASSUMPTION_ROLENR)) ) {     
    resprior=WR_HISTORY_FROMGOALASSUMPTION_ROLENR;
  // #define WR_HISTORY_FROMGOAL_ROLENR 4  // some parents are goals, some are assumptions or axioms
  } else if (decprior1==WR_HISTORY_GOAL_ROLENR || decprior2==WR_HISTORY_GOAL_ROLENR ||
      decprior1==WR_HISTORY_FROMGOAL_ROLENR || decprior2==WR_HISTORY_FROMGOAL_ROLENR) {
    resprior=WR_HISTORY_FROMGOAL_ROLENR;
  // #define WR_HISTORY_FROMASSUMPTION_ROLENR 5        // some parents are assumptions, some are axioms      
  } else if (decprior1==WR_HISTORY_ASSUMPTION_ROLENR || decprior2==WR_HISTORY_ASSUMPTION_ROLENR ||
             decprior1==WR_HISTORY_FROMASSUMPTION_ROLENR || decprior2==WR_HISTORY_FROMASSUMPTION_ROLENR) {
    resprior=WR_HISTORY_FROMASSUMPTION_ROLENR;    
  // #define WR_HISTORY_FROMAXIOM_ROLENR 11            // parents are only axioms   
  } else {
    resprior=WR_HISTORY_FROMAXIOM_ROLENR;
  }
  
  //printf("\n wr_calc_history_priority2 got %d and %d and res %d\n",(int)decprior1,(int)decprior2,(int)resprior);
  return wr_encode_priority(g,resprior);
}

gint wr_build_propagate_history(glb* g, gptr cl1, gptr cl2, int pos1, int pos2) {  
  void* db=g->db;
  gptr rec;
  gint tag;
  int cutn=0;  
  int datalen=HISTORY_PREFIX_LEN+3;

  if (g->store_history) {    

    rec=wr_create_raw_history_record(g,datalen+cutn,g->build_buffer);
    if (rec==NULL) return wg_encode_null(db,NULL);

    tag=wr_encode_history_propagate(g,pos1,pos2);
    wr_set_history_record_field(g,rec,HISTORY_DERIVATION_TAG_POS,tag);
    wr_set_history_record_field(g,rec,HISTORY_PARENT1_POS,wg_encode_record(db,cl1));
    wr_set_history_record_field(g,rec,HISTORY_PARENT2_POS,wg_encode_record(db,cl2)); 
    wr_set_history_record_field(g,rec,HISTORY_PRIORITY_POS,wr_calc_history_priority2(g,cl1,cl2));   
    wr_set_history_record_derived_order(g,rec);
    return wg_encode_record(db,rec); 
  } else {
    return wg_encode_int(g->db,1);
  }  
}

gint wr_build_factorial_history(glb* g, gptr cl1, int pos1, int pos2, gptr cut_clvec) {
  void* db=g->db;
  gptr rec;
  gint tag;
  int i,cutn=0;
  int datalen=HISTORY_PREFIX_LEN+2;

  if (g->store_history) {
    if (cut_clvec) {
      for(cutn=1; cutn<(int)(cut_clvec[0]) && (gptr)(cut_clvec[cutn])!=NULL; cutn++) {}  
      cutn=cutn-1;      
    }

    rec=wr_create_raw_history_record(g,datalen+cutn,g->build_buffer);
    if (rec==NULL) return wg_encode_null(db,NULL);
    //tag=wr_encode_history_factorial(g,pos1,pos2);
    if (pos2>=0) tag=wr_encode_history_factorial(g,pos1,pos2);
    else tag=wr_encode_history_equality_reflexive(g,pos1);
    wr_set_history_record_field(g,rec,HISTORY_DERIVATION_TAG_POS,tag);
    wr_set_history_record_field(g,rec,HISTORY_PARENT1_POS,wg_encode_record(db,cl1));
    wr_set_history_record_field(g,rec,HISTORY_PRIORITY_POS,wr_calc_history_priority2(g,cl1,NULL));
    for(i=0; i<cutn; i++) {
      wr_set_history_record_field(g,rec,datalen+i,wg_encode_record(db,(gptr)(cut_clvec[i+1])));
    }
    wr_set_history_record_derived_order(g,rec);
    return wg_encode_record(db,rec);     
  } else {
    return wg_encode_int(g->db,1);
  }  
}


gint wr_build_propinst_history(glb* g, gptr cl1, gint term, gptr cut_clvec) {
  void* db=g->db;
  gptr rec;
  gint tag;
  int i,cutn=0;
  int datalen=HISTORY_PREFIX_LEN+3;

  if (g->store_history) {
    if (cut_clvec) {
      for(cutn=1; cutn<(int)(cut_clvec[0]) && (gptr)(cut_clvec[cutn])!=NULL; cutn++) {}  
      cutn=cutn-1;      
    }
    rec=wr_create_raw_history_record(g,datalen+cutn,g->build_buffer);
    if (rec==NULL) return wg_encode_null(db,NULL);
    //tag=wr_encode_history_factorial(g,pos1,pos2);
    tag=wr_encode_history_propinst(g);   
    wr_set_history_record_field(g,rec,HISTORY_DERIVATION_TAG_POS,tag);
    wr_set_history_record_field(g,rec,HISTORY_PARENT1_POS,wg_encode_record(db,cl1));
    wr_set_history_record_field(g,rec,HISTORY_PARENT2_POS,term);
    wr_set_history_record_field(g,rec,HISTORY_PRIORITY_POS,wr_calc_history_priority2(g,cl1,NULL));
    for(i=0; i<cutn; i++) {
      wr_set_history_record_field(g,rec,datalen+i,wg_encode_record(db,(gptr)(cut_clvec[i+1])));
    }
    wr_set_history_record_derived_order(g,rec);
    return wg_encode_record(db,rec);     
  } else {
    return wg_encode_int(g->db,1);
  }  
}


gint wr_build_extprop_history(glb* g) {  
  void* db=g->db;
  gptr rec; 
  gint tag;
  int datalen=HISTORY_PREFIX_LEN;
  gint encpriority;
  //gint encname;

  if (g->store_history) {
    rec=wr_create_raw_history_record(g,datalen+1,g->build_buffer);
    if (rec==NULL) return wg_encode_null(db,NULL);
    tag=wr_encode_history_extprop(g);
    wr_set_history_record_field(g,rec,HISTORY_DERIVATION_TAG_POS,tag);
    encpriority=wr_encode_priority(g,0);    
    wr_set_history_record_field(g,rec,HISTORY_PRIORITY_POS,encpriority);         
    wr_set_history_record_derived_order(g,rec);
    return wg_encode_record(db,rec); 
  } else {
    return wg_encode_int(g->db,1);
  }  
}

gint wr_build_para_history(glb* g, gptr cl1, gptr cl2, int pos1, int pos2, gptr cut_clvec,
                          gint path, int leftflag, int fromflag) {  
  void* db=g->db;
  gptr rec;
  gint tag;
  int i,cutn=0;  
  int datalen=HISTORY_PREFIX_LEN+4;

  if (g->store_history) {
    if (cut_clvec) {
      for(cutn=1; cutn<(int)(cut_clvec[0]) && (gptr)(cut_clvec[cutn])!=NULL; cutn++) {}  
      cutn=cutn-1;      
    }    
    // cutn is now the nr of cut literals 
    rec=wr_create_raw_history_record(g,datalen+cutn,g->build_buffer);
    if (rec==NULL) return wg_encode_null(db,NULL);
    tag=wr_encode_history_para(g,pos1,pos2,leftflag);
    wr_set_history_record_field(g,rec,HISTORY_DERIVATION_TAG_POS,tag);
    wr_set_history_record_field(g,rec,HISTORY_PATH_POS,wg_encode_int(db,path));
    wr_set_history_record_field(g,rec,HISTORY_PARA_PARENT1_POS,wg_encode_record(db,cl1));
    wr_set_history_record_field(g,rec,HISTORY_PARA_PARENT2_POS,wg_encode_record(db,cl2));
    wr_set_history_record_field(g,rec,HISTORY_PRIORITY_POS,wr_calc_history_priority2(g,cl1,cl2));
    for(i=0; i<cutn; i++) {
      wr_set_history_record_field(g,rec,datalen+i,wg_encode_record(db,(gptr)(cut_clvec[i+1])));
    }
    wr_set_history_record_derived_order(g,rec);
    return wg_encode_record(db,rec); 
    //return wg_encode_int(g->db,g->stat_built_cl); 
  } else {
    return wg_encode_int(g->db,1);
  }  
}

gint wr_build_simplify_history(glb* g, gptr cl1, gptr cut_clvec, gptr rewrite_clvec) {
  void* db=g->db;
  gptr rec;
  gint tag;
  int i,j,cutn=0,rewriten=0;
  int datalen=HISTORY_PREFIX_LEN+2;
  if (g->store_history) {
    if (cut_clvec) {
      for(cutn=1; cutn<(int)(cut_clvec[0]) && (gptr)(cut_clvec[cutn])!=NULL; cutn++) {}
      cutn=cutn-1;      
    }
    if (rewrite_clvec) rewriten=rewrite_clvec[1]-2;
    rec=wr_create_raw_history_record(g,datalen+cutn+rewriten,g->build_buffer);
    if (rec==NULL) return wg_encode_null(db,NULL);
    //tag=wr_encode_history_factorial(g,pos1,pos2);
    tag=wr_encode_history_simplify(g);   
    wr_set_history_record_field(g,rec,HISTORY_DERIVATION_TAG_POS,tag);
    wr_set_history_record_field(g,rec,HISTORY_PARENT1_POS,wg_encode_record(db,cl1));
    wr_set_history_record_field(g,rec,HISTORY_PRIORITY_POS,wr_calc_history_priority2(g,cl1,NULL));
    for(i=0; i<cutn; i++) {
      wr_set_history_record_field(g,rec,datalen+i,wg_encode_record(db,(gptr)(cut_clvec[i+1])));
    }
    for(j=0;j<rewriten;j++) {
      wr_set_history_record_field(g,rec,datalen+i+j,wg_encode_record(db,(gptr)(rewrite_clvec[j+2])));
    }
    wr_set_history_record_derived_order(g,rec);
     return wg_encode_record(db,rec); 
    //return wg_encode_int(g->db,g->stat_built_cl); 
  } else {
    return wg_encode_int(g->db,1);
  }  
}

gint wr_build_cut_history(glb* g, gptr cl, int pos1, int pos2) {
  return wg_encode_int(g->db,2);
}

void wr_set_history(glb* g, gptr clause, gint history) {
  if (g->store_history) {
    *((gint*)clause+RECORD_HEADER_GINTS+CLAUSE_HISTORY_POS)=history;
  }  
}


gint wr_get_history(glb* g, gptr clause) {
  return *((gint*)clause+RECORD_HEADER_GINTS+CLAUSE_HISTORY_POS);
}


void wr_set_history_record_derived_order(glb* g, gptr rec) {
#ifdef RECORD_HISTORY_ORDER
  wr_set_history_record_field(g,rec,HISTORY_DERIVED_ORDER_POS,
    wg_encode_int(g->db,g->stat_derived_cl));  
  wr_set_history_record_field(g,rec,HISTORY_GIVEN_ORDER_POS,
     wg_encode_int(g->db,0));    
#endif
}

void wr_set_history_record_given_order(glb* g, gptr rec) {
#ifdef RECORD_HISTORY_ORDER
  if (g->store_history) {
    wr_set_history_record_field(g,rec,HISTORY_GIVEN_ORDER_POS,
       wg_encode_int(g->db,g->stat_given_used));  
  }   
#endif
}


/* -------- encodings ------- */

gint wr_encode_history_resolve(glb* g, int pos1, int pos2) {
  int n1,n2,n;

  n1=pos1;
  if (n1>255) n1=255;
  n2=pos2;
  if (n2>255) n2=255;
  n=WR_HISTORY_TAG_RESOLVE<<WR_HISTORY_TAG_SHIFT | n1<<WR_HISTORY_POS1_SHIFT | n2;    
  return wg_encode_int(g->db,n);
}

gint wr_encode_history_instgen(glb* g, int pos1, int pos2) {
  int n1,n2,n;

  n1=pos1;
  if (n1>255) n1=255;
  n2=pos2;
  if (n2>255) n2=255;
  n=WR_HISTORY_TAG_INSTGEN<<WR_HISTORY_TAG_SHIFT | n1<<WR_HISTORY_POS1_SHIFT | n2;    
  return wg_encode_int(g->db,n);
}

gint wr_encode_history_propagate(glb* g, int pos1, int pos2) {
  int n1,n2,n;

  n1=pos1;
  if (n1>255) n1=255;
  n2=pos2;
  if (n2>255) n2=255;
  n=WR_HISTORY_TAG_PROPAGATE<<WR_HISTORY_TAG_SHIFT | n1<<WR_HISTORY_POS1_SHIFT | n2;    
  return wg_encode_int(g->db,n);
}

gint wr_encode_history_factorial(glb* g, int pos1, int pos2) {
  int n1,n2,n;

  n1=pos1;
  if (n1>255) n1=255;
  n2=pos2;
  if (n2<0) n2=0;
  if (n2>255) n2=255;
  n=WR_HISTORY_TAG_FACTORIAL<<WR_HISTORY_TAG_SHIFT | n1<<WR_HISTORY_POS1_SHIFT | n2;    
  return wg_encode_int(g->db,n);
}

gint wr_encode_history_propinst(glb* g) {
  int n;
 
  n=WR_HISTORY_TAG_PROPINST<<WR_HISTORY_TAG_SHIFT;    
  return wg_encode_int(g->db,n);
}

gint wr_encode_history_extprop(glb* g) {
  int n;
 
  n=WR_HISTORY_TAG_EXTPROP<<WR_HISTORY_TAG_SHIFT;    
  return wg_encode_int(g->db,n);
}


gint wr_encode_history_equality_reflexive(glb* g, int pos1) {
  int n1,n;

  n1=pos1;
  if (n1>255) n1=255;
  n=WR_HISTORY_TAG_EQUALITY_REFLEXIVE<<WR_HISTORY_TAG_SHIFT | n1<<WR_HISTORY_POS1_SHIFT;
  return wg_encode_int(g->db,n);
}

gint wr_encode_history_para(glb* g, int pos1, int pos2, int leftflag) {
  int n1,n2,n;

  n1=pos1;
  if (n1>255) n1=255;
  n2=pos2;
  if (n2>255) n2=255;
  n=WR_HISTORY_TAG_PARA<<WR_HISTORY_TAG_SHIFT | n1<<WR_HISTORY_POS1_SHIFT | n2;   
  if (leftflag) n=n | (1<<WR_HISTORY_EXTRA_SHIFT);
  return wg_encode_int(g->db,n);
}

gint wr_encode_history_simplify(glb* g) {
  int n;

  n=WR_HISTORY_TAG_SIMPLIFY<<WR_HISTORY_TAG_SHIFT;    
  return wg_encode_int(g->db,n);
}

int wr_get_history_tag(glb* g, gint head) {
  return (head>>WR_HISTORY_TAG_SHIFT)&WR_HISTORY_TAG_BITS;
}

int wr_get_history_extra(glb* g, gint head) {
  return (head>>WR_HISTORY_EXTRA_SHIFT)&WR_HISTORY_EXTRA_BITS;
}

int wr_get_history_pos1(glb* g, gint head) {
  return (head>>WR_HISTORY_POS1_SHIFT)&WR_HISTORY_TAG_BITS;
}

int wr_get_history_pos2(glb* g, gint head) {
  return head&WR_HISTORY_TAG_BITS;
}


/* --------- printing history --------- */


void wr_show_history(glb* g, gint history) {
  void* db=g->db;
  int htype;
  void* assoc=NULL;
  void* mpool;
  int clnr=1;
  char namebuf[20];
  gptr ans;
  //gint anshistory;

  //if (!(g->store_history)) return;

  wr_show_result(g,history);
  return;
 
  if (((g->answers)[1])>2 && (gptr)((g->answers)[2])!=NULL) {
    printf("\nanswer: ");
    ans=(gptr)((g->answers)[2]);
    wr_print_clause(g,ans);
  }

  if (!(g->store_history)) return;
  
  mpool=wg_create_mpool(db,1000000);
  wr_printf("\nproof:\n");
  htype=wg_get_encoded_type(db,history);  
  if (htype!=WG_RECORDTYPE) {
    wr_print_simpleterm_otter(g,history,1);
  } else {
    wr_flatten_history(g,mpool,history,NULL,0,&clnr,&assoc);
  }  
  assoc=wg_reverselist(db,mpool,assoc); 
  wr_print_flat_history(g,mpool,clnr,&assoc);
  // print last step
  snprintf(namebuf,19,"%d",clnr++);
  if (((g->answers)[1])>2 && (gptr)((g->answers)[2])!=NULL) {
    wr_print_one_history(g,mpool,history,(gptr)((g->answers)[2]),namebuf,clnr-1,&assoc);
  } else {
    wr_print_one_history(g,mpool,history,NULL,namebuf,clnr-1,&assoc);
  }    
  wr_printf("\n\n");
 
  wg_free_mpool(db,mpool);
}
 
/*
  build alist of enumerations of clauses found in history with nrs from 1...N
    ((clX . 1)....(clZ . N))
*/


gptr wr_flatten_history(glb* g, void* mpool, gint history, gptr cl, int depth, int* clnr, void **assoc) {
  void* db=g->db;
  int htype,tag,dechead;
  gint head, cl1, h1;
  int len, tmp1, i, start;
   
  // get history type and components
  /* 
  printf("\nflatten called depth %d\n",depth);  
  wr_print_clause(g,cl);
  printf("\n");
  wg_print_record(g->db,cl);
  printf("\n");
  */
  if (!history) return NULL;
  len=wg_get_record_len(db,otp(db,history));   
  head=wr_get_history_record_field(db,otp(db,history),HISTORY_DERIVATION_TAG_POS);
  htype=wg_get_encoded_type(db,head);
  if (htype!=WG_INTTYPE) return NULL;
  dechead=wg_decode_int(db,head);
  //printf("\n wr_flatten_history got dechead %d\n",dechead);
  tag=wr_get_history_tag(g,dechead);
  //printf("\nflatten cp1\n");
  // process parent clauses found in history
  //printf("\n wr_flatten_history got tag %d\n",tag);
  if (tag==WR_HISTORY_TAG_RESOLVE || tag==WR_HISTORY_TAG_FACTORIAL ||
      tag==WR_HISTORY_TAG_PARA || tag==WR_HISTORY_TAG_EQUALITY_REFLEXIVE || 
      tag==WR_HISTORY_TAG_SIMPLIFY || tag==WR_HISTORY_TAG_PROPAGATE || 
      tag==WR_HISTORY_TAG_PROPINST || tag==WR_HISTORY_TAG_INSTGEN) {
    if (tag==WR_HISTORY_TAG_PARA) start=HISTORY_PARA_PARENT1_POS;
    else start=HISTORY_PARENT1_POS;    
    for(i=start;i<len;i++) {
      //printf("\nflatten i %d depth %d\n",i,depth);
      if (tag==WR_HISTORY_TAG_PROPINST && i==HISTORY_PARENT2_POS) {
        // instantiation term, not a clause
        continue;
      }
      cl1=wr_get_history_record_field(g,otp(db,history),i);      
      h1=wr_get_history(g,otp(db,cl1));
      if (wg_get_assoc(db,(void *)cl1,*assoc)) {
        //printf("\nC");
        continue;
      }  
      wr_flatten_history(g,mpool,h1,otp(db,cl1),depth+1,clnr,assoc);
      /* 
      printf("\n flattening adding cl nr %d\n",*clnr);
      wr_print_clause(g,rotp(g,cl1));
      printf("\n");
      */
      tmp1=wr_flatten_history_addcl(g,mpool,cl1,clnr,assoc);
      if (!tmp1) return NULL;         
    }   
  }
  return *assoc;   
}

int wr_flatten_history_addcl(glb* g, void* mpool, gint cl, int* clnr, void **assoc) {
  void* db=g->db;
  char* n;
  void *tmp, *newassoc;
  char namebuf[20];
  
  tmp=wg_get_assoc(db,(void *)cl,*assoc);
  if (tmp) {
    //printf("\nfound\n");
    n=wg_rest(db,tmp);
  } else {
    //printf("\not found\n");
    snprintf(namebuf,19,"%d",(*clnr)++);
    //n=wg_mklist2(db,mpool,wg_mkatom(db,mpool,WG_STRTYPE,namebuf,NULL),(*clnr)-1);
    n=wg_mklist2(db,mpool,wg_mkatom(db,mpool,WG_STRTYPE,namebuf,NULL),(void *)(gint)((*clnr)-1));
    newassoc=wg_add_assoc(db,mpool,(void *)cl,n,*assoc);
    if (!newassoc) return 0;
    *assoc=newassoc;
  } 
  return 1;
}  


char* wr_print_flat_history(glb* g, void* mpool, int maxclnr, void **assoc) {
  void* db=g->db;  
  void *flat, *part, *tmp;
  gint cl, history; 
  char namebuf[20];
  int num;
  
  // print all proof steps before last
  flat=*assoc; 
  if (flat) {
    for(part=flat; wg_ispair(db,part); part=wg_rest(db,part)) {
      tmp=wg_first(db,part);   
      cl=(gint)(wg_first(db,tmp));
      history=wr_get_history(g,rotp(g,cl));      
      num=(int)(gint)(wg_nth(db,tmp,2));
      snprintf(namebuf,19,"%d",num);
      wr_print_one_history(g,mpool,history,rotp(g,cl),namebuf,maxclnr,assoc);     
    }
  }
  return NULL;
}

char* wr_print_one_history
        (glb* g, void* mpool, gint history, gptr cl, char* clns, int maxclnr, void **assoc) {
  void* db=g->db;
  int htype,tag,dechead;
  void *tmp1;
  gint head, cl1, path, name;
  char* namestr;
  //char *n, *n1;
  int pos1, pos2, num, len, i, termpos, leftflag;
  char namebuf1[20];  
  char orderbuf[80];
  gptr historyptr;
#ifdef SHOW_HISTORY_ORDER  
  int o1,o2;
#endif
  if (!history) {
    // empty history
    wr_printf("\n %s: [] ",clns);
    if (!cl) {
      wr_printf("false");
    } else {
      wr_print_clause(g,cl);   
    } return clns;
  } 
#ifdef SHOW_HISTORY_ORDER
  o1=wr_get_history_record_field(g,otp(db,history),HISTORY_DERIVED_ORDER_POS);
  o2=wr_get_history_record_field(g,otp(db,history),HISTORY_GIVEN_ORDER_POS);
  snprintf(orderbuf,79," %8ld %6ld",wg_decode_int(db,o1),wg_decode_int(db,o2));
#else
  orderbuf[0]=(char)0;      
#endif  
  len=wg_get_record_len(db,otp(db,history)); 
  if (len==HISTORY_PREFIX_LEN) {
    // input clause
    wr_printf("\n %s:%s [in",clns,orderbuf);
    historyptr=otp(db,history);    
    name = wr_get_history_record_field(db,historyptr,HISTORY_NAME_POS);
    if (name && wg_get_encoded_type(db,name)==WG_STRTYPE) {
      namestr=wg_decode_str(db,name);
      wr_printf(",%s",namestr);
    } else if (name && wg_get_encoded_type(db,name)==WG_INTTYPE) {
      wr_printf(",%d",(int)(wg_decode_int(db,name)));
    }   
    if (g->print_history_extra) wr_print_history_extra(g,history);
    else {
      wr_printf("] ");    
    }
    if (!cl) {
      wr_printf("false");
    } else {
      wr_print_clause(g,cl);   
    } 
    return clns;
  } 
  head=wr_get_history_record_field(db,otp(db,history),HISTORY_DERIVATION_TAG_POS);
  htype=wg_get_encoded_type(db,head);
  if (htype!=WG_INTTYPE) return NULL;
  dechead=wg_decode_int(db,head);
  tag=wr_get_history_tag(g,dechead);
  pos1=wr_get_history_pos1(g,dechead);
  pos2=wr_get_history_pos2(g,dechead);
  //printf("\n dechead %d tag %d pos1 %d pos2 %d \n",dechead,tag,pos1,pos2);
  if (tag==WR_HISTORY_TAG_RESOLVE || tag==WR_HISTORY_TAG_PROPAGATE 
     || tag==WR_HISTORY_TAG_INSTGEN) {
    if (tag==WR_HISTORY_TAG_RESOLVE) {
      wr_printf("\n %s:%s [mp, ",clns,orderbuf);
    } else if (tag==WR_HISTORY_TAG_INSTGEN) {
      wr_printf("\n %s:%s [instgen, ",clns,orderbuf);
    } else {
      wr_printf("\n %s:%s [fmp, ",clns,orderbuf);
    }
    for(i=HISTORY_PARENT1_POS;i<len;i++) {
      cl1=wr_get_history_record_field(g,otp(db,history),i);
      tmp1=wg_get_assoc(db,(void *)cl1,*assoc);
      num=(int)(gint)(wg_nth(db,tmp1,2));
      snprintf(namebuf1,19,"%d",num);
      if (i==1+HISTORY_DERIVATION_TAG_POS) {
        if (pos1) {
          wr_printf("%s.%d",namebuf1,pos1);
        } else {
          wr_printf("%s",namebuf1);
        }  
      } else if (i==2+HISTORY_DERIVATION_TAG_POS) {
        if (pos2) {
          wr_printf("%s.%d",namebuf1,pos2);
        } else {
          wr_printf("%s",namebuf1);
        }  
      } else {
       wr_printf("%s",namebuf1);
      }    
      if (i<len-1) wr_printf(", ");
    }    
    if (g->print_history_extra) wr_print_history_extra(g,history);
    else {
      wr_printf("] ");
    }
    if (!cl) {
      wr_printf("false");
    } else wr_print_clause(g,cl);   
   
  } else if (tag==WR_HISTORY_TAG_FACTORIAL) {
    wr_printf("\n %s:%s [merge, ",clns,orderbuf);
    for(i=HISTORY_PARENT1_POS;i<len;i++) {
      cl1=wr_get_history_record_field(g,otp(db,history),i);
      tmp1=wg_get_assoc(db,(void *)cl1,*assoc);
      num=(int)(gint)(wg_nth(db,tmp1,2));
      snprintf(namebuf1,19,"%d",num);
      if (i==1+HISTORY_DERIVATION_TAG_POS) {
        wr_printf("%s.%d.%d",namebuf1,pos1,pos2);       
      } else {
       wr_printf("%s",namebuf1);
      }    
      if (i<len-1) wr_printf(", ");
    }    
    if (g->print_history_extra) wr_print_history_extra(g,history);
    else {
      wr_printf("] ");  
    } if (!cl) {
      wr_printf("false");
    } else wr_print_clause(g,cl);  
   
   } else if (tag==WR_HISTORY_TAG_PROPINST) {
    wr_printf("\n %s:%s [propinst, ",clns,orderbuf);
    for(i=HISTORY_PARENT1_POS;i<len;i++) {
      cl1=wr_get_history_record_field(g,otp(db,history),i);
      if (i==HISTORY_PARENT2_POS) {
        // instantiation term
        wr_print_term(g,cl1);
      } else {
        tmp1=wg_get_assoc(db,(void *)cl1,*assoc);
        num=(int)(gint)(wg_nth(db,tmp1,2));
        snprintf(namebuf1,19,"%d",num);
        if (i==1+HISTORY_DERIVATION_TAG_POS) {
          wr_printf("%s.%d.%d",namebuf1,pos1,pos2);       
        } else {
        wr_printf("%s",namebuf1);
        }    
      }  
      if (i<len-1) wr_printf(", ");
    }    
    if (g->print_history_extra) wr_print_history_extra(g,history);
    else {
      wr_printf("] ");  
    } if (!cl) {
      wr_printf("false");
    } else wr_print_clause(g,cl);  

   } else if (tag==WR_HISTORY_TAG_EQUALITY_REFLEXIVE) {
    wr_printf("\n %s:%s [r=, ",clns,orderbuf);
    for(i=HISTORY_PARENT1_POS;i<len;i++) {
      cl1=wr_get_history_record_field(g,otp(db,history),i);
      tmp1=wg_get_assoc(db,(void *)cl1,*assoc);
      num=(int)(gint)(wg_nth(db,tmp1,2));
      snprintf(namebuf1,19,"%d",num);
      if (i==1+HISTORY_DERIVATION_TAG_POS) {
        wr_printf("%s.%d",namebuf1,pos1);       
      } else {
       wr_printf("%s",namebuf1);
      }    
      if (i<len-1) {
        wr_printf(", ");
      }  
    }     
    if (g->print_history_extra) wr_print_history_extra(g,history);
    else {
      wr_printf("] ");  
    } 
    if (!cl) {
      wr_printf("false");
    } else wr_print_clause(g,cl);    

  } else if (tag==WR_HISTORY_TAG_PARA) {
    wr_printf("\n %s:%s [=, ",clns,orderbuf);    
    leftflag=wr_get_history_extra(g,dechead);
    path=wr_get_history_record_field(g,otp(db,history),HISTORY_PATH_POS);   
    termpos=wr_decode_para_termpath_pos(g,wg_decode_int(db,path));    
    for(i=HISTORY_PARA_PARENT1_POS;i<len;i++) {
      cl1=wr_get_history_record_field(g,otp(db,history),i);
      tmp1=wg_get_assoc(db,(void *)cl1,*assoc);
      num=(int)(gint)(wg_nth(db,tmp1,2));
      snprintf(namebuf1,19,"%d",num);     
      if (i==2+HISTORY_DERIVATION_TAG_POS) {        
        if (pos1) {
          if (!leftflag) {
            wr_printf("%s.%d.R",namebuf1,pos1);
          } else {
            wr_printf("%s.%d.L",namebuf1,pos1);
          }  
        } else {
          if (leftflag) {
            wr_printf("%s",namebuf1);
          } else {
            wr_printf("%s.0.R",namebuf1);
          }  
        }  
      } else if (i==3+HISTORY_DERIVATION_TAG_POS) {
        if (pos2) {
          wr_printf("%s.%d.%d",namebuf1,pos2,termpos);
        } else {
          wr_printf("%s.0.%d",namebuf1,termpos);
        }  
      } else {
       wr_printf("%s",namebuf1);
      }    
      if (i<len-1) wr_printf(", ");
    }   
    if (g->print_history_extra) wr_print_history_extra(g,history);
    else {
      wr_printf("] ");  
    }
    if (!cl) {
      wr_printf("false");
    } else {
      wr_print_clause(g,cl);   
      //wg_print_record(db,cl);
    }  
  } else if (tag==WR_HISTORY_TAG_SIMPLIFY) {
    wr_printf("\n %s:%s [simp, ",clns,orderbuf);    
    for(i=HISTORY_PARENT1_POS;i<len;i++) {
      cl1=wr_get_history_record_field(g,otp(db,history),i);
      tmp1=wg_get_assoc(db,(void *)cl1,*assoc);
      num=(int)(gint)(wg_nth(db,tmp1,2));
      snprintf(namebuf1,19,"%d",num);
      wr_printf("%s",namebuf1);          
      if (i<len-1) wr_printf(", ");
    }      
    if (g->print_history_extra) wr_print_history_extra(g,history);
    else {
      wr_printf("] ");  
    }
    if (!cl) {
      wr_printf("false");
    } else wr_print_clause(g,cl); 
  }
  return clns;
}

void wr_print_history_extra(glb* g, gint history) {
  void* db=g->db;
  gint priority, decprior;
  gptr historyptr;

  historyptr=otp(db,history);
  priority = wr_get_history_record_field(db,historyptr,HISTORY_PRIORITY_POS);
  decprior=wr_decode_priority(g,priority);
  if (decprior==WR_HISTORY_GOAL_ROLENR) {
    wr_printf(",goal] ");
  } else if (decprior==WR_HISTORY_ASSUMPTION_ROLENR) {
    wr_printf(",assumption] ");
  } else if (decprior==WR_HISTORY_FROMGOALASSUMPTION_ROLENR) {
    wr_printf(",fromga] "); 
  } else if (decprior==WR_HISTORY_FROMGOAL_ROLENR) {
    wr_printf(",fromgoal] ");
  } else if (decprior==WR_HISTORY_FROMASSUMPTION_ROLENR) {
    wr_printf(",fromassumption] ");
  } else if (decprior==WR_HISTORY_AXIOM_ROLENR) {
    wr_printf(",axiom] ");
  } else if (decprior==WR_HISTORY_FROMAXIOM_ROLENR) {
    wr_printf(",fromaxiom] ");
  } else if (decprior==WR_HISTORY_EXTAXIOM_ROLENR) {
    wr_printf(",extaxiom] ");
  } else if (decprior) {
    wr_printf(", dp %d] ",(int)decprior);
  } else {
    wr_printf("] ");
  }  
}

/*
  print a history without operation or parents: just a name and role

  this is used in otterprint

*/

void wr_print_clause_name_history(glb* g, gint history) {
  void* db=g->db; 
  int len;
  gptr historyptr;  
  char* namestr;
  gint name, priority, decprior;

  if (!history) {
    // empty history
    wr_printf("[] ");   
    return;
  } 
  historyptr=otp(db,history);
  len=wg_get_record_len(db,historyptr); 
  if (len==HISTORY_PREFIX_LEN) {
    // input clause    
    priority = wr_get_history_record_field(db,historyptr,HISTORY_PRIORITY_POS);
    name = wr_get_history_record_field(db,historyptr,HISTORY_NAME_POS);
    if (name && wg_get_encoded_type(db,name)==WG_STRTYPE) {
      namestr=wg_decode_str(db,name);
      wr_printf("[%s",namestr);
    } else if (name && wg_get_encoded_type(db,name)==WG_INTTYPE) {
      wr_printf("[%d",(int)(wg_decode_int(db,name)));
    } else {
      wr_printf("[");
    }
    decprior=wr_decode_priority(g,priority);
    if (decprior==WR_HISTORY_GOAL_ROLENR) {
      wr_printf(",goal] ");
    } else if (decprior==WR_HISTORY_ASSUMPTION_ROLENR) {
      wr_printf(",assumption] ");
    } else if (decprior==WR_HISTORY_FROMGOALASSUMPTION_ROLENR) {
      wr_printf(",fromga] ");
    } else if (decprior==WR_HISTORY_FROMASSUMPTION_ROLENR) {
      wr_printf(",fromassumption] ");
    } else if (decprior==WR_HISTORY_AXIOM_ROLENR) {
      wr_printf(",axiom] ");
    } else if (decprior==WR_HISTORY_FROMAXIOM_ROLENR) {
      wr_printf(",fromaxiom] ");
    } else if (decprior==WR_HISTORY_EXTAXIOM_ROLENR) {
      wr_printf(",extaxiom] ");
    } else if (decprior) {
      wr_printf(", dp %d] ",(int)decprior);
    } else {
      wr_printf("] ");     
    }  
    return;
  } else {
    // not an input clause
    wr_printf("[] ");
  }
}


/* --------- printing history to a string --------- */


int wr_show_result(glb* g, gint history) {
  void* db=g->db;
  int htype;
  void* assoc=NULL;
  void* mpool;
  int clnr=1;
  char namebuf[20];
  gptr ans;
  char* buf;
  int ansnr, blen, bpos;
  FILE* outfile;

  // create buf for printing

  namebuf[0]=0;
  blen=1000;
  bpos=0;
  buf=wr_malloc(g,blen);
  if (buf==NULL) {   
    return -1;
  }   

  if (!wr_str_guarantee_space(g,&buf,&blen,bpos+100)) return -1;
  if (((g->answers)[1])<=2) {
    // no results
    if (g->print_json) {
      bpos+=snprintf(buf+bpos,blen-bpos,"\n{\"result\": \"unknown\"}\n");
    } else {
      bpos+=snprintf(buf+bpos,blen-bpos,"\nresult: unknown\n");
    }  
    return 0;
  } 
  // here we have some results
  if (g->print_json) {
    bpos+=snprintf(buf+bpos,blen-bpos,"\n{\"result\": \"proof found\",\n");
    bpos+=snprintf(buf+bpos,blen-bpos,"\n{\"answers\": [\n");
  } else {
    if ((g->print_level_flag)>1) bpos+=snprintf(buf+bpos,blen-bpos,"\n");
    if (g->in_has_fof) {
      bpos+=snprintf(buf+bpos,blen-bpos,"\nresult: proof found\n%% SZS status Theorem for %s.",g->filename);
    } else {
      bpos+=snprintf(buf+bpos,blen-bpos,"\nresult: proof found\n%% SZS status Unsatisfiable for %s.",g->filename);
    }  
    if ((g->required_answer_nr)<2) {
      //bpos+=snprintf(buf+bpos,blen-bpos,"\nproof:");
    } else {
      bpos+=snprintf(buf+bpos,blen-bpos,"\nanswers and proofs:");
    }  
  }  
  for(ansnr=2; ansnr<((g->answers)[1]); ansnr+=2) {
    // loop over all proofs 
    if (g->print_json) {
      bpos+=snprintf(buf+bpos,blen-bpos,"[\n");
    } else {
      //bpos+=snprintf(buf+bpos,blen-bpos,"\n");
    }
    // first check if answer present    
    if ((gptr)((g->answers)[ansnr])!=NULL) {
      if (!wr_str_guarantee_space(g,&buf,&blen,bpos+100)) return -1;
      if (g->print_json) {
        bpos+=snprintf(buf+bpos,blen-bpos,"{\"answer\": ");
        ans=(gptr)((g->answers)[ansnr]);
        bpos=wr_strprint_clause(g,ans,&buf,&blen,bpos);
        if (bpos<0) return bpos;
        if (!wr_str_guarantee_space(g,&buf,&blen,bpos+100)) return -1;
        bpos+=snprintf(buf+bpos,blen-bpos,"},");
      } else {
        bpos+=snprintf(buf+bpos,blen-bpos,"\n\nanswer: ");
        ans=(gptr)((g->answers)[ansnr]);
        bpos=wr_strprint_clause(g,ans,&buf,&blen,bpos);
        if (bpos<0) return bpos;
      }       
    }
    // second, print proof
    if (!(g->store_history)) continue;
    history=(g->answers)[ansnr+1];
    //printf("\nansnr %d history %ld\n",ansnr,history);
     
    // create mpool 
    mpool=wg_create_mpool(db,1000000);
    if (mpool==NULL) return -1;
    
    bpos+=snprintf(buf+bpos,blen-bpos,"\n");
    //if (!wr_hist_print(g,histstr,"\nproof:\n")) return -1;
    assoc=NULL;
    clnr=1;
    htype=wg_get_encoded_type(db,history);  
    if (htype!=WG_RECORDTYPE) {
      bpos=wr_strprint_simpleterm_otter(g,history,1,&buf,&blen,bpos,0);
      if (bpos<0) return bpos;
    } else {
      wr_flatten_history(g,mpool,history,NULL,0,&clnr,&assoc);
    }  
    assoc=wg_reverselist(db,mpool,assoc); 
    if (!(g->print_json)) {
      if (!wr_str_guarantee_space(g,&buf,&blen,bpos+100)) return -1;
      bpos+=snprintf(buf+bpos,blen-bpos,
        "\n%% SZS output start CNFRefutation for %s",g->filename);
    } 
    bpos=wr_strprint_flat_history(g,mpool,&buf,&blen,bpos,clnr,&assoc);
    if (bpos<0) return bpos;

    // print last step
    snprintf(namebuf,19,"%d",clnr++);
    if (((g->answers)[1])>ansnr && (gptr)((g->answers)[ansnr])!=NULL) {    
      bpos=wr_strprint_one_history(g,mpool,&buf,&blen,bpos,history,
            (gptr)((g->answers)[ansnr]),namebuf,clnr-1,&assoc);           
    } else {
      bpos=wr_strprint_one_history(g,mpool,&buf,&blen,bpos,history,NULL,namebuf,clnr-1,&assoc);
    }      
    if (bpos<0) return bpos;
    if (!wr_str_guarantee_space(g,&buf,&blen,bpos+10)) return -1;
    if (g->print_json) {
      bpos+=snprintf(buf+bpos,blen-bpos,"\n]}\n]\n"); // end one answer/proof struct
    } 

    wg_free_mpool(db,mpool); 
  }
  if (g->print_json) {
    bpos+=snprintf(buf+bpos,blen-bpos,"]}\n"); // end all answers
    bpos+=snprintf(buf+bpos,blen-bpos,"}\n");
  } else {
    bpos+=snprintf(buf+bpos,blen-bpos,
        "\n%% SZS output end CNFRefutation for %s",g->filename);
    bpos+=snprintf(buf+bpos,blen-bpos,"\n");
  }  
  if (g->outfilename) {
    outfile=fopen(g->outfilename, "w");
    if (!outfile) {
      printf("{\"error\": \"cannot open outfile to print proof %s\"}\n",(g->outfilename));               
    } else {
      fprintf(outfile,"%s",buf);
      fclose(outfile);
    }  
  } else {
    printf("%s",buf);
  }    
  return bpos;
}
 

int wr_strprint_flat_history(glb* g, void* mpool, char** buf, int* blen, int bpos,
                             int maxclnr, void **assoc) {
  void* db=g->db;  
  void *flat, *part, *tmp;
  gint cl, history; 
  char namebuf[20];
  int num;
  
  namebuf[0]=0;
  if (g->print_json) {
    bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"{\"proof\":\n[");
  }
  // print all proof steps before last
  flat=*assoc; 
  if (flat) {
    for(part=flat; wg_ispair(db,part); part=wg_rest(db,part)) {
      tmp=wg_first(db,part);   
      cl=(gint)(wg_first(db,tmp));
      history=wr_get_history(g,rotp(g,cl));      
      num=(int)(gint)(wg_nth(db,tmp,2));
      snprintf(namebuf,19,"%d",num);
      bpos=wr_strprint_one_history(g,mpool,buf,blen,bpos,history,rotp(g,cl),namebuf,maxclnr,assoc);
      if (g->print_json) { //&& wg_ispair(db,wg_rest(db,part))) {
        bpos+=snprintf((*buf)+bpos,(*blen)-bpos,",");
      }
      if (bpos<0) return bpos;
    }
  }
  //if (g->print_json) {
  //    bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"\n]");
  //}
  return bpos;
}

int wr_strprint_one_history
        (glb* g, void* mpool, char** buf, int* blen, int bpos, 
         gint history, gptr cl, char* clns, int maxclnr, void **assoc) {
  void* db=g->db;
  int htype,tag,dechead;
  void *tmp1;
  gint head, cl1, path, name;
  char* namestr;
  //char *n, *n1;
  int pos1, pos2, num, len, i, termpos, leftflag;
  char namebuf1[20];  
  char orderbuf[80];
  gptr historyptr;
#ifdef SHOW_HISTORY_ORDER  
  int o1,o2;
#endif
  /* 
  printf("\nprinting one history for clns %s\n",clns);
  wr_print_clause(g,cl);
  printf("\n");
  head=wr_get_history_record_field(db,otp(db,history),HISTORY_DERIVATION_TAG_POS);
  htype=wg_get_encoded_type(db,head);
  printf("\nhead %ld htype %d and WG_INTTYPE %d\n",head,htype,WG_INTTYPE);
  */
  namebuf1[0]=(char)0;  
  if (!wr_str_guarantee_space(g,buf,blen,bpos+100)) return -1;
  //if (g->print_json) {
  //  bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"[");
  //}
  if (!history) {
    // empty history
    if (g->print_json) {
      bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s, []]",clns);
    } else {
      bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"\n %s: [] ",clns);
    }
    bpos=wr_strprint_clause(g,cl,buf,blen,bpos);   
    return bpos;
  } 
#ifdef SHOW_HISTORY_ORDER
  o1=wr_get_history_record_field(g,otp(db,history),HISTORY_DERIVED_ORDER_POS);
  o2=wr_get_history_record_field(g,otp(db,history),HISTORY_GIVEN_ORDER_POS);
  snprintf(orderbuf,79," %8ld, %6ld,",wg_decode_int(db,o1),wg_decode_int(db,o2));
#else
  orderbuf[0]=(char)0;      
#endif  
  len=wg_get_record_len(db,otp(db,history));  
  if (len==HISTORY_PREFIX_LEN) {
    // input clause
    if (g->print_json) {
      bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"\n[%s,%s [\"in\"",clns,orderbuf);         
    } else {  
      bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"\n %s:%s [in",clns,orderbuf);
    }  
    historyptr=otp(db,history);    
    name = wr_get_history_record_field(db,historyptr,HISTORY_NAME_POS);
    if (name && wg_get_encoded_type(db,name)==WG_STRTYPE) {
      namestr=wg_decode_str(db,name);
      if (!wr_str_guarantee_space(g,buf,blen,bpos+100+strlen(namestr))) return -1;
      if (g->print_json) bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", \"%s\"",namestr);
      else bpos+=snprintf((*buf)+bpos,(*blen)-bpos,",%s",namestr);
    } else if (name && wg_get_encoded_type(db,name)==WG_INTTYPE) {
      bpos+=snprintf((*buf)+bpos,(*blen)-bpos,",%d",(int)(wg_decode_int(db,name)));
    }
    if (!wr_str_guarantee_space(g,buf,blen,bpos+100)) return -1;   
    if (g->print_history_extra) {
      bpos=wr_strprint_history_extra(g,buf,blen,bpos,history);
      if (bpos<0) return bpos;
    } else {
      if (g->print_json) bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"], ");
      else bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"] ");
    }   
    bpos=wr_strprint_clause(g,cl,buf,blen,bpos);  
    if (bpos<0) return bpos;
    if (g->print_json) bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"]");
    //else bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"] ");
    return bpos;

  } 
  head=wr_get_history_record_field(db,otp(db,history),HISTORY_DERIVATION_TAG_POS);
  htype=wg_get_encoded_type(db,head);
  if (htype!=WG_INTTYPE) return bpos;
  dechead=wg_decode_int(db,head);
  tag=wr_get_history_tag(g,dechead);
  pos1=wr_get_history_pos1(g,dechead);
  pos2=wr_get_history_pos2(g,dechead);
  //printf("\n dechead %d tag %d pos1 %d pos2 %d \n",dechead,tag,pos1,pos2);
  if (!wr_str_guarantee_space(g,buf,blen,bpos+100)) return -1;
  if (tag==WR_HISTORY_TAG_RESOLVE || tag==WR_HISTORY_TAG_PROPAGATE
      || tag==WR_HISTORY_TAG_INSTGEN) {
    if (tag==WR_HISTORY_TAG_RESOLVE) {
      if (g->print_json) {
        bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"\n[%s,%s [\"mp\", ",clns,orderbuf); 
      } else {
        bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"\n %s:%s [mp, ",clns,orderbuf);
      }
    } else if (tag==WR_HISTORY_TAG_INSTGEN) {
      if (g->print_json) {
        bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"\n[%s,%s [\"instgen\", ",clns,orderbuf); 
      } else {
        bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"\n %s:%s [instgen, ",clns,orderbuf);
      }      
    } else {
      if (g->print_json) {
        bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"\n[%s,%s [\"fmp\", ",clns,orderbuf); 
      } else {
        bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"\n %s:%s [fmp, ",clns,orderbuf);
      }
    }
    for(i=HISTORY_PARENT1_POS;i<len;i++) {
      cl1=wr_get_history_record_field(g,otp(db,history),i);
      tmp1=wg_get_assoc(db,(void *)cl1,*assoc);
      num=(int)(gint)(wg_nth(db,tmp1,2));      
      snprintf(namebuf1,19,"%d",num);
      if (!wr_str_guarantee_space(g,buf,blen,bpos+100+strlen(namebuf1))) return -1;
      if (i==1+HISTORY_DERIVATION_TAG_POS) {
        if (pos1) {
          if (g->print_json) {
            bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"[%s,%d]",namebuf1,pos1);
          } else {
            bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s.%d",namebuf1,pos1);
          }
        } else {
          if (g->print_json) {
            bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s",namebuf1);
          } else {
            bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s",namebuf1);
          }
        }  
      } else if (i==2+HISTORY_DERIVATION_TAG_POS) {
        if (pos2) {
          if (g->print_json) {
            bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"[%s,%d]",namebuf1,pos2);
          } else {
            bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s.%d",namebuf1,pos2);
          }         
        } else {
          bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s",namebuf1);
        }  
      } else {
       bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s",namebuf1);
      }    
      if (!wr_str_guarantee_space(g,buf,blen,bpos+100)) return -1;
      if (i<len-1) bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", ");
    }    
    if (!wr_str_guarantee_space(g,buf,blen,bpos+100)) return -1;
    if (g->print_history_extra) {
      bpos=wr_strprint_history_extra(g,buf,blen,bpos,history);
      if (bpos<0) return bpos;
    } else {
      if (g->print_json) {
        bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"], ");
      } else {
        bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"]");
      }      
    }
    bpos=wr_strprint_clause(g,cl,buf,blen,bpos);            
    if (bpos<0) return bpos;
   
  } else if (tag==WR_HISTORY_TAG_FACTORIAL) {
    if (!wr_str_guarantee_space(g,buf,blen,bpos+100)) return -1;
    if (g->print_json) {
      bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"\n[%s,%s [\"merge\", ",clns,orderbuf);       
    } else {
      bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"\n %s:%s [merge, ",clns,orderbuf); 
    }      
    for(i=HISTORY_PARENT1_POS;i<len;i++) {
      cl1=wr_get_history_record_field(g,otp(db,history),i);
      tmp1=wg_get_assoc(db,(void *)cl1,*assoc);
      num=(int)(gint)(wg_nth(db,tmp1,2));
      snprintf(namebuf1,19,"%d",num);
      if (i==1+HISTORY_DERIVATION_TAG_POS) {
        if (!wr_str_guarantee_space(g,buf,blen,bpos+100+strlen(namebuf1))) return -1;
        if (g->print_json) {
          bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"[%s,%d,%d]",namebuf1,pos1,pos2);          
        } else {
          bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s.%d.%d",namebuf1,pos1,pos2);
        }               
      } else {
       bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s",namebuf1);
      }    
      if (i<len-1) bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", ");
    }    
    if (!wr_str_guarantee_space(g,buf,blen,bpos+100)) return -1;
    if (g->print_history_extra) {
      bpos=wr_strprint_history_extra(g,buf,blen,bpos,history);
      if (bpos<0) return bpos;
    } else {
      bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"] ");  
    } 
    bpos=wr_strprint_clause(g,cl,buf,blen,bpos);        
    if (bpos<0) return bpos;

   } else if (tag==WR_HISTORY_TAG_PROPINST) {
    //printf("\n prop clause in history for clns %s: \n",clns);    
    //wr_print_clause(g,cl);
    //printf("\n");
    if (!wr_str_guarantee_space(g,buf,blen,bpos+100)) return -1;
    if (g->print_json) {
      bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"\n[%s,%s [\"inst\", ",clns,orderbuf);       
    } else {
      bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"\n %s:%s [inst, ",clns,orderbuf); 
    }      
    for(i=HISTORY_PARENT1_POS;i<len;i++) {
      cl1=wr_get_history_record_field(g,otp(db,history),i);      
      if (i==HISTORY_PARENT2_POS) {
        // instantiation term       
        bpos=wr_strprint_term_otter(g,cl1,(g->print_clause_detaillevel),buf,blen,bpos);               
        if (bpos<0) return bpos;
      } else {
        tmp1=wg_get_assoc(db,(void *)cl1,*assoc);
        num=(int)(gint)(wg_nth(db,tmp1,2));
        snprintf(namebuf1,19,"%d",num);
        if (!wr_str_guarantee_space(g,buf,blen,bpos+100+strlen(namebuf1))) return -1;
        if (g->print_json) {
          bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s",namebuf1);          
        } else {
          bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s",namebuf1);
        }               
      } 
      if (i<len-1) { bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", "); }
    }    
    if (!wr_str_guarantee_space(g,buf,blen,bpos+100)) return -1;
    if (g->print_history_extra) {
      bpos=wr_strprint_history_extra(g,buf,blen,bpos,history);
      if (bpos<0) return bpos;
    } else {
      bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"] ");  
    } 
    bpos=wr_strprint_clause(g,cl,buf,blen,bpos);        
    if (bpos<0) return bpos;

    } else if (tag==WR_HISTORY_TAG_EXTPROP) {    
    if (!wr_str_guarantee_space(g,buf,blen,bpos+100)) return -1;
    if (g->print_json) {
      bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"\n[%s,%s [\"extprop\"], 0]",clns,orderbuf);       
    } else {
      bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"\n %s:%s [extprop] false.",clns,orderbuf); 
    }                  
    if (bpos<0) return bpos;

   } else if (tag==WR_HISTORY_TAG_EQUALITY_REFLEXIVE) {
    if (!wr_str_guarantee_space(g,buf,blen,bpos+100)) return -1;
    if (g->print_json) {
      bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"\n[%s,%s [\"r=\", ",clns,orderbuf);          
    } else {
      bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"\n %s:%s [r=, ",clns,orderbuf); 
    }     
    for(i=HISTORY_PARENT1_POS;i<len;i++) {
      cl1=wr_get_history_record_field(g,otp(db,history),i);
      tmp1=wg_get_assoc(db,(void *)cl1,*assoc);
      num=(int)(gint)(wg_nth(db,tmp1,2));      
      snprintf(namebuf1,19,"%d",num);
      if (!wr_str_guarantee_space(g,buf,blen,bpos+100+strlen(namebuf1))) return -1;
      if (i==1+HISTORY_DERIVATION_TAG_POS) {
        if (g->print_json) {
          bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"[%s,%d]",namebuf1,pos1);               
        } else {
          bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s.%d",namebuf1,pos1); 
        }               
      } else {
       bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s",namebuf1);
      }    
      if (i<len-1) {
        bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", ");
      }  
    }     
    if (!wr_str_guarantee_space(g,buf,blen,bpos+100)) return -1;
    if (g->print_history_extra) {
      bpos=wr_strprint_history_extra(g,buf,blen,bpos,history);
      if (bpos<0) return bpos;
    } else {
      if (g->print_json) {
        bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"], ");
      } else {
        bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"]");
      }      
    } 
    bpos=wr_strprint_clause(g,cl,buf,blen,bpos);    
    if (bpos<0) return bpos;      

  } else if (tag==WR_HISTORY_TAG_PARA) {
    if (!wr_str_guarantee_space(g,buf,blen,bpos+100)) return -1;
    if (g->print_json) {
      bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s,%s [\"=\", ",clns,orderbuf);  
    } else {
      bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"\n %s:%s [=, ",clns,orderbuf); 
    }       
    leftflag=wr_get_history_extra(g,dechead);
    path=wr_get_history_record_field(g,otp(db,history),HISTORY_PATH_POS);   
    termpos=wr_decode_para_termpath_pos(g,wg_decode_int(db,path));    
    for(i=HISTORY_PARA_PARENT1_POS;i<len;i++) {
      cl1=wr_get_history_record_field(g,otp(db,history),i);
      tmp1=wg_get_assoc(db,(void *)cl1,*assoc);
      num=(int)(gint)(wg_nth(db,tmp1,2));
      snprintf(namebuf1,19,"%d",num);     
      if (!wr_str_guarantee_space(g,buf,blen,bpos+100+strlen(namebuf1))) return -1;
      if (i==2+HISTORY_DERIVATION_TAG_POS) {        
        if (pos1) {
          if (!leftflag) {
            if (g->print_json) {
              bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"[%s,%d,\"R\"]",namebuf1,pos1);
            } else {
              bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s.%d.R",namebuf1,pos1);
            }             
          } else {
            if (g->print_json) {
              bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"[%s,%d,\"L\"]",namebuf1,pos1);
            } else {
              bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s.%d.L",namebuf1,pos1);
            }
          }  
        } else {
          if (leftflag) {
            if (g->print_json) {
              bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s",namebuf1);
            } else {
              bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s",namebuf1);
            }           
          } else {
            if (g->print_json) {
              bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"[%s,0,\"R\"]",namebuf1);
            } else {
              bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s.0.R",namebuf1);
            }           
          }  
        }  
      } else if (i==3+HISTORY_DERIVATION_TAG_POS) {
        if (pos2) {
          if (g->print_json) {
            bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"[%s,%d,%d]",namebuf1,pos2,termpos);
          } else {
            bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s.%d.%d",namebuf1,pos2,termpos);
          }            
        } else {
          if (g->print_json) {
            bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"[%s,0,%d]",namebuf1,termpos);
          } else {
            bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s.0.%d",namebuf1,termpos);
          }            
        }  
      } else {
       bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s",namebuf1);
      }    
      if (i<len-1) bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", ");
    }   
    if (!wr_str_guarantee_space(g,buf,blen,bpos+100)) return -1;
    if (g->print_history_extra) {
      bpos=wr_strprint_history_extra(g,buf,blen,bpos,history);
      if (bpos<0) return bpos;
    } else {
      if (g->print_json) {
        bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"], ");
      } else {
        bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"]");
      }      
    }
    if (!wr_str_guarantee_space(g,buf,blen,bpos+100)) return -1;
    bpos=wr_strprint_clause(g,cl,buf,blen,bpos);      
    if (bpos<0) return bpos;

  } else if (tag==WR_HISTORY_TAG_SIMPLIFY) {
    if (!wr_str_guarantee_space(g,buf,blen,bpos+100)) return -1;
    if (g->print_json) {
      bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"[%s,%s [\"simp\", ",clns,orderbuf);  
    } else {
      bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"\n %s:%s [simp, ",clns,orderbuf);
    }          
    for(i=HISTORY_PARENT1_POS;i<len;i++) {
      cl1=wr_get_history_record_field(g,otp(db,history),i);
      tmp1=wg_get_assoc(db,(void *)cl1,*assoc);
      num=(int)(gint)(wg_nth(db,tmp1,2));
      snprintf(namebuf1,19,"%d",num);
      if (!wr_str_guarantee_space(g,buf,blen,bpos+100+strlen(namebuf1))) return -1;
      bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"%s",namebuf1);          
      if (i<len-1) bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", ");
    }      
    if (!wr_str_guarantee_space(g,buf,blen,bpos+100)) return -1;
    if (g->print_history_extra) {
      bpos=wr_strprint_history_extra(g,buf,blen,bpos,history);
      if (bpos<0) return bpos;
    } else {
      if (g->print_json) {
        bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"], ");
      } else {
        bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"]");
      }      
    }
    bpos=wr_strprint_clause(g,cl,buf,blen,bpos); 
    if (bpos<0) return bpos;
      
  }
  if (g->print_json) {
    bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"]");
  }
  return bpos;
}

int wr_strprint_history_extra(glb* g, char** buf, int* blen, int bpos, gint history) {
  void* db=g->db;
  gint priority, decprior;
  gptr historyptr;

  historyptr=otp(db,history);
  priority = wr_get_history_record_field(db,historyptr,HISTORY_PRIORITY_POS);
  decprior=wr_decode_priority(g,priority);
  if (!wr_str_guarantee_space(g,buf,blen,bpos+100)) return -1;
  if (decprior==WR_HISTORY_GOAL_ROLENR) {
    if (g->print_json) bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", \"goal\"], ");
    else bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", goal] ");
  } else if (decprior==WR_HISTORY_ASSUMPTION_ROLENR) {
    if (g->print_json) bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", \"assumption\"], ");
    else bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", assumption] ");
  } else if (decprior==WR_HISTORY_FROMGOALASSUMPTION_ROLENR) {
    if (g->print_json) bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", \"fromga\"], "); 
    else bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", fromga] "); 
  } else if (decprior==WR_HISTORY_FROMGOAL_ROLENR) {
    if (g->print_json) bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", \"fromgoal\"], ");
    else bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", fromgoal] ");
  } else if (decprior==WR_HISTORY_FROMASSUMPTION_ROLENR) {
    if (g->print_json) bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", \"fromassumption\"], ");
    else bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", fromassumption] ");
  } else if (decprior==WR_HISTORY_AXIOM_ROLENR) {
    if (g->print_json) bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", \"axiom\"], ");
    else bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", axiom] ");
  } else if (decprior==WR_HISTORY_FROMAXIOM_ROLENR) {
    if (g->print_json) bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", \"fromaxiom\"], ");
    else bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", fromaxiom] ");
  } else if (decprior==WR_HISTORY_EXTAXIOM_ROLENR) {
    if (g->print_json) bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", \"extaxiom\"], ");
    else bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", extaxiom] ");
  } else if (decprior) {
    if (g->print_json) bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", \"dp\", %d], ",(int)decprior);
    else bpos+=snprintf((*buf)+bpos,(*blen)-bpos,", dp %d] ",(int)decprior);
  } else {
    if (g->print_json)  bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"], ");
    else bpos+=snprintf((*buf)+bpos,(*blen)-bpos,"] ");   
  }  
  return bpos;
}


/* ------- raw data alloc and fields ----------- */

gptr wr_create_raw_history_record(glb* g, gint length, gptr buffer) {
  gptr rec;
  int i;
  
  if (buffer==NULL) {
    rec=wg_create_raw_record(g->db,length); 
    if (rec==NULL) return NULL;  
    for(i=0;i<HISTORY_PREFIX_LEN;i++) rec[RECORD_HEADER_GINTS+i]=(gint)0;
  } else {
    rec=wr_alloc_from_cvec(g,buffer,RECORD_HEADER_GINTS+length);
    if (rec==NULL) return NULL;
    rec[0]=(length+RECORD_HEADER_GINTS)*sizeof(gint);
  }    
  return rec;
}

void wr_set_history_record_field(glb* g, gptr rec, int pos, gint val) {
  rec[RECORD_HEADER_GINTS+pos]=val;
}

gint wr_get_history_record_field(glb* g, gptr rec, int pos) {
  return rec[RECORD_HEADER_GINTS+pos];
}

gptr wr_copy_raw_history_record(glb* g, gptr history, gptr buffer) {
  gptr rec;
  int length,i;

  if (history==NULL) return NULL;
  length=wg_get_record_len(g->db,history);
  if (buffer==NULL) {
    rec=wg_create_raw_record(g->db,length); 
    if (rec==NULL) return NULL;  
    for(i=0;i<length+RECORD_HEADER_GINTS;i++) rec[i]=history[i];
  } else {
    rec=wr_alloc_from_cvec(g,buffer,length+RECORD_HEADER_GINTS);
    if (rec==NULL) return NULL;
    for(i=0;i<length+RECORD_HEADER_GINTS;i++) rec[i]=history[i];
  }     
  return rec;
}

/* =============== answer and proof registration ================= */


int wr_register_answer(glb* g, gptr cl, gint history) {
  void *db=g->db;
  gint count;
  cvec answers;
  gptr hcopy;

  if (!(g->store_history)) return 0;
  answers=(g->answers);
  count=(g->answers)[1];
  hcopy=wr_copy_raw_history_record(g,wg_decode_record(db,history),g->queue_termbuf);
  if (hcopy==NULL) return -1;
  (g->answers)=wr_cvec_store(g,answers,count,(gint)cl);
  if (!(g->answers)) return -1;
  (g->answers)=wr_cvec_store(g,answers,count+1,wg_encode_record(db,hcopy));
  if (!(g->answers)) return -1;
  return (((g->answers)[1])-2)/2;
}

int wr_enough_answers(glb* g) {
  if (!(g->store_history)) return 1;
  if (!(g->answers)) return 1;
  //printf("\n answer count %ld\n",((((g->answers)[1])-2)/2));
  if (((((g->answers)[1])-2)/2)>=(g->required_answer_nr)) {   
    return 1;
  } else {
    (g->proof_found)=0; // we need more
    return 0;
  }
}

int wr_have_answers(glb* g) {
  if (!(g->store_history)) return (g->proof_found);
  if (!(g->answers)) return (g->proof_found);
  if (g->proof_found) return 1;
  //printf("\n answer count %ld\n",((((g->answers)[1])-2)/2));
  if (((((g->answers)[1])-2)/2)>=1) {   
    return 1;
  } else {    
    return 0;
  }
}

/* ============== error handling ==================== */

/** called with err msg when an history alloc or printing error occurs
*
*  may print or log an error
*  does not do any jumps etc
*/

/*
static int show_history_error(glb* g, char* errmsg) {
  fprintf(stderr,"history error: %s\n",errmsg);
  return -1;
}
*/

/** called with err msg and err nr when an mpool allocation error occurs
*
*  may print or log an error
*  does not do any jumps etc
*/

/*
static int show_history_error_nr(glb* g, char* errmsg, int nr) {
  fprintf(stderr,"history error: %s %d\n",errmsg,nr);
  return -1;
} 
*/

#ifdef __cplusplus
}
#endif
