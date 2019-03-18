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

/* ======= Private protos ================ */
  
void wr_print_history_extra(glb* g, gint history);
gint wr_encode_priority(glb* g, gint priority);

/* ====== Functions ============== */

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

    //printf()    
    //printf("\n in wr_build_resolve_history pos1 %d pos2 %d \n",pos1,pos2);
    //wr_check_cl_memarea(g,cl1);
    //wr_check_cl_memarea(g,cl2);
    //printf("\n");
    rec=wr_create_raw_history_record(g,datalen+cutn,g->build_buffer);
    if (rec==NULL) return wg_encode_null(db,NULL);
    /*
    printf("\nraw rec: ");
    wr_print_glb_memarea(g,rec);
    printf("\n cl1 again:");
    wr_check_cl_memarea(g,cl1);
    printf("\n");
    printf("\n cl2 again:");
    wr_check_cl_memarea(g,cl2);
    printf("\n");
    */
    tag=wr_encode_history_resolve(g,pos1,pos2);
    wr_set_history_record_field(g,rec,HISTORY_DERIVATION_TAG_POS,tag);
    wr_set_history_record_field(g,rec,HISTORY_PARENT1_POS,wg_encode_record(db,cl1));
    wr_set_history_record_field(g,rec,HISTORY_PARENT2_POS,wg_encode_record(db,cl2));
    wr_set_history_record_field(g,rec,HISTORY_PRIORITY_POS,wr_calc_history_priority2(g,cl1,cl2)); 
    for(i=0; i<cutn; i++) {
      wr_set_history_record_field(g,rec,datalen+i,wg_encode_record(db,(gptr)(cut_clvec[i+1])));
    }      
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

    //printf()    
    //printf("\n in wr_build_resolve_history pos1 %d pos2 %d \n",pos1,pos2);
    //wr_check_cl_memarea(g,cl1);
    //wr_check_cl_memarea(g,cl2);
    //printf("\n");
    rec=wr_create_raw_history_record(g,datalen+cutn,g->build_buffer);
    if (rec==NULL) return wg_encode_null(db,NULL);
    /*
    printf("\nraw rec: ");
    wr_print_glb_memarea(g,rec);
    printf("\n cl1 again:");
    wr_check_cl_memarea(g,cl1);
    printf("\n");
    printf("\n cl2 again:");
    wr_check_cl_memarea(g,cl2);
    printf("\n");
    */
    tag=wr_encode_history_propagate(g,pos1,pos2);
    wr_set_history_record_field(g,rec,HISTORY_DERIVATION_TAG_POS,tag);
    wr_set_history_record_field(g,rec,HISTORY_PARENT1_POS,wg_encode_record(db,cl1));
    wr_set_history_record_field(g,rec,HISTORY_PARENT2_POS,wg_encode_record(db,cl2)); 
    wr_set_history_record_field(g,rec,HISTORY_PRIORITY_POS,wr_calc_history_priority2(g,cl1,cl2));   
    return wg_encode_record(db,rec); 
    //return wg_encode_int(g->db,g->stat_built_cl); 
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
    /*
    printf("\n in wr_build_factorial_history pos1 %d pos2 %d \n",pos1,pos2);
    wr_print_clause(g,cl1);
    */     
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
    /*
    printf("\nhistory with len %d:\n",wg_get_record_len(db,rec));    
    wr_print_record(g,rec);  
    printf("\nshown\n");  
    */

     return wg_encode_record(db,rec); 
    //return wg_encode_int(g->db,g->stat_built_cl); 
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
    //printf("\n in wr_build_para_history pos1 %d pos2 %d path %d cutn %d\n",pos1,pos2,path,cutn);
    //wr_check_cl_memarea(g,cl1);
    //wr_check_cl_memarea(g,cl2);
    //printf("\n");
    rec=wr_create_raw_history_record(g,datalen+cutn,g->build_buffer);
    if (rec==NULL) return wg_encode_null(db,NULL);
    /*
    printf("cl1:\n");
    wr_print_clause(g,cl1);
    printf("cl2:\n");
    wr_print_clause(g,cl2);
    printf("\n");
    */
    /*
    printf("\nraw rec: ");
    //wr_print_glb_memarea(g,rec);
    //printf("\n cl1 again:");
    //wr_check_cl_memarea(g,cl1);
    printf("\n");
    printf("\n cl2 again:");
    //wr_check_cl_memarea(g,cl2);
    printf("\n");
    */
    tag=wr_encode_history_para(g,pos1,pos2,leftflag);
    wr_set_history_record_field(g,rec,HISTORY_DERIVATION_TAG_POS,tag);
    wr_set_history_record_field(g,rec,HISTORY_PATH_POS,wg_encode_int(db,path));
    wr_set_history_record_field(g,rec,HISTORY_PARA_PARENT1_POS,wg_encode_record(db,cl1));
    wr_set_history_record_field(g,rec,HISTORY_PARA_PARENT2_POS,wg_encode_record(db,cl2));
    wr_set_history_record_field(g,rec,HISTORY_PRIORITY_POS,wr_calc_history_priority2(g,cl1,cl2));
    for(i=0; i<cutn; i++) {
      wr_set_history_record_field(g,rec,datalen+i,wg_encode_record(db,(gptr)(cut_clvec[i+1])));
    }
    return wg_encode_record(db,rec); 
    //return wg_encode_int(g->db,g->stat_built_cl); 
  } else {
    return wg_encode_int(g->db,1);
  }  
}

gint wr_build_simplify_history(glb* g, gptr cl1, gptr cut_clvec) {
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
    tag=wr_encode_history_simplify(g);   
    wr_set_history_record_field(g,rec,HISTORY_DERIVATION_TAG_POS,tag);
    wr_set_history_record_field(g,rec,HISTORY_PARENT1_POS,wg_encode_record(db,cl1));
    wr_set_history_record_field(g,rec,HISTORY_PRIORITY_POS,wr_calc_history_priority2(g,cl1,NULL));
    for(i=0; i<cutn; i++) {
      wr_set_history_record_field(g,rec,datalen+i,wg_encode_record(db,(gptr)(cut_clvec[i+1])));
    }
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


/* --------- printing --------- */


void wr_show_history(glb* g, gint history) {
  void* db=g->db;
  int htype;
  void* assoc=NULL;
  void* mpool;
  int clnr=1;
  char namebuf[20];

  //wr_show_clactive(g);
  //wr_show_clactivesubsume(g);

  //wr_print_termhash(g, gint* hasharr);
  /*
  printf("\n*+*+*+* hash_neg_active_groundunits\n");

  wr_print_termhash(g,rotp(g,g->hash_neg_active_groundunits));

  printf("\n*+*+*+* hash_pos_active_groundunits\n");

  wr_print_termhash(g,rotp(g,g->hash_pos_active_groundunits));

  printf("\n*+*+*+* hash_pos_groundunits\n");

  //wr_clterm_hashlist_print(g,rotp(g,g->hash_pos_groundunits));
  wr_print_termhash(g,rotp(g,g->hash_pos_groundunits));

  printf("\n*+*+*+* hash_neg_groundunits\n");

  //wr_clterm_hashlist_print(g,rotp(g,g->hash_neg_groundunits)); 
  wr_print_termhash(g,rotp(g,g->hash_neg_groundunits));
  */
  mpool=wg_create_mpool(db,1000000);
  printf("\nproof:\n");
  htype=wg_get_encoded_type(db,history);  
  if (htype!=WG_RECORDTYPE) {
    wr_print_simpleterm_otter(g,history,1);
  } else {
    //wr_print_record(g,otp(db,history))
    wr_flatten_history(g,mpool,history,NULL,0,&clnr,&assoc);
  }
  assoc=wg_reverselist(db,mpool,assoc); 
  wr_print_flat_history(g,mpool,clnr,&assoc);
  // print last step
  snprintf(namebuf,19,"%d",clnr++);
  wr_print_one_history(g,mpool,history,NULL,namebuf,clnr-1,&assoc);
  printf("\n\n");

  //wg_mpool_print(db,assoc); 
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
  
  //printf("\nflatten called depth %d\n",depth);
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
      tag==WR_HISTORY_TAG_SIMPLIFY || tag==WR_HISTORY_TAG_PROPAGATE) {
    if (tag==WR_HISTORY_TAG_PARA) start=HISTORY_PARA_PARENT1_POS;
    else start=HISTORY_PARENT1_POS;    
    for(i=start;i<len;i++) {
      //printf("\nflatten i %d depth %d\n",i,depth);
      cl1=wr_get_history_record_field(g,otp(db,history),i);      
      h1=wr_get_history(g,otp(db,cl1));
      if (wg_get_assoc(db,(void *)cl1,*assoc)) {
        //printf("\nC");
        continue;
      }  
      wr_flatten_history(g,mpool,h1,otp(db,cl1),depth+1,clnr,assoc);
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
  //gint cln;
  //char *clns;
  char namebuf[20];
  int num;
  
  // print all proof steps before last
  flat=*assoc; 
  if (flat) {
    for(part=flat; wg_ispair(db,part); part=wg_rest(db,part)) {
      tmp=wg_first(db,part);   
      cl=(gint)(wg_first(db,tmp));
      history=wr_get_history(g,rotp(g,cl));      
      //cln=(gint)(wg_nth(db,tmp,1));       
      //clns=wg_atomstr1(db,(void *)cln);
      //num=maxclnr-(int)(gint)(wg_nth(db,tmp,2));
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
  gptr historyptr;

  if (!history) {
    // empty history
    printf("\n %s: [] ",clns);
    if (!cl) printf("false");
    else wr_print_clause(g,cl);   
    return clns;
  } 
  len=wg_get_record_len(db,otp(db,history)); 
  if (len==HISTORY_PREFIX_LEN) {
    // input clause
    printf("\n %s: [in",clns);
    historyptr=otp(db,history);    
    name = wr_get_history_record_field(db,historyptr,HISTORY_NAME_POS);
    if (name && wg_get_encoded_type(db,name)==WG_STRTYPE) {
      namestr=wg_decode_str(db,name);
      printf(",%s",namestr);
    } else if (name && wg_get_encoded_type(db,name)==WG_INTTYPE) {
      printf(",%d",(int)(wg_decode_int(db,name)));
    }
    if (g->print_history_extra) wr_print_history_extra(g,history);
    else printf("] ");    
    if (!cl) printf("false");
    else wr_print_clause(g,cl);   
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
  if (tag==WR_HISTORY_TAG_RESOLVE || tag==WR_HISTORY_TAG_PROPAGATE) {
    if (tag==WR_HISTORY_TAG_RESOLVE) printf("\n %s: [mp, ",clns);
    else printf("\n %s: [fmp, ",clns);
    for(i=HISTORY_PARENT1_POS;i<len;i++) {
      cl1=wr_get_history_record_field(g,otp(db,history),i);
      tmp1=wg_get_assoc(db,(void *)cl1,*assoc);
      /*
      if (tmp1) {
        n1=wg_nth(db,tmp1,1);  
      } else {      
        show_history_error(g,"could not find parent in wr_print_flat_history");
        return NULL;
      }        
      h1=wr_get_history(g,otp(db,cl1));
      */
      //num=maxclnr-(int)(gint)(wg_nth(db,tmp1,2));
      num=(int)(gint)(wg_nth(db,tmp1,2));
      snprintf(namebuf1,19,"%d",num);
      if (i==1+HISTORY_DERIVATION_TAG_POS) {
        if (pos1) printf("%s.%d",namebuf1,pos1);
        else printf("%s",namebuf1);
      } else if (i==2+HISTORY_DERIVATION_TAG_POS) {
        if (pos2) printf("%s.%d",namebuf1,pos2);
        else printf("%s",namebuf1);
      } else {
       printf("%s",namebuf1);
      }    
      if (i<len-1) printf(", ");
    }
    if (g->print_history_extra) wr_print_history_extra(g,history);
    else printf("] ");
    if (!cl) printf("false");
    else wr_print_clause(g,cl);   
   
  } else if (tag==WR_HISTORY_TAG_FACTORIAL) {
    printf("\n %s: [merge, ",clns);
    for(i=HISTORY_PARENT1_POS;i<len;i++) {
      cl1=wr_get_history_record_field(g,otp(db,history),i);
      tmp1=wg_get_assoc(db,(void *)cl1,*assoc);
      /*
      if (tmp1) {
        n1=wg_nth(db,tmp1,1);  
      } else {      
        show_history_error(g,"could not find parent in wr_print_flat_history");
        return NULL;
      } 
      h1=wr_get_history(g,otp(db,cl1));
      */
      //num=maxclnr-(int)(gint)(wg_nth(db,tmp1,2));
      num=(int)(gint)(wg_nth(db,tmp1,2));
      snprintf(namebuf1,19,"%d",num);
      if (i==1+HISTORY_DERIVATION_TAG_POS) {
        printf("%s.%d.%d",namebuf1,pos1,pos2);       
      } else {
       printf("%s",namebuf1);
      }    
      if (i<len-1) printf(", ");
    }    
    if (g->print_history_extra) wr_print_history_extra(g,history);
    else printf("] ");  
    if (!cl) printf("false");
    else wr_print_clause(g,cl);  

   } else if (tag==WR_HISTORY_TAG_EQUALITY_REFLEXIVE) {
    printf("\n %s: [r=, ",clns);
    for(i=HISTORY_PARENT1_POS;i<len;i++) {
      cl1=wr_get_history_record_field(g,otp(db,history),i);
      tmp1=wg_get_assoc(db,(void *)cl1,*assoc);
      /*
      if (tmp1) {
        n1=wg_nth(db,tmp1,1);  
      } else {      
        show_history_error(g,"could not find parent in wr_print_flat_history");
        return NULL;
      } 
      h1=wr_get_history(g,otp(db,cl1));
      */
      //num=maxclnr-(int)(gint)(wg_nth(db,tmp1,2));
      num=(int)(gint)(wg_nth(db,tmp1,2));
      snprintf(namebuf1,19,"%d",num);
      if (i==1+HISTORY_DERIVATION_TAG_POS) {
        printf("%s.%d",namebuf1,pos1);       
      } else {
       printf("%s",namebuf1);
      }    
      if (i<len-1) printf(", ");
    }
    if (g->print_history_extra) wr_print_history_extra(g,history);
    else printf("] ");  
    if (!cl) printf("false");
    else wr_print_clause(g,cl);    

  } else if (tag==WR_HISTORY_TAG_PARA) {
    printf("\n %s: [=, ",clns);    
    leftflag=wr_get_history_extra(g,dechead);
    path=wr_get_history_record_field(g,otp(db,history),HISTORY_PATH_POS);   
    termpos=wr_decode_para_termpath_pos(g,wg_decode_int(db,path));    
    for(i=HISTORY_PARA_PARENT1_POS;i<len;i++) {
      cl1=wr_get_history_record_field(g,otp(db,history),i);
      tmp1=wg_get_assoc(db,(void *)cl1,*assoc);
      /*
      if (tmp1) {
        n1=wg_nth(db,tmp1,1);  
      } else {      
        show_history_error(g,"could not find parent in wr_print_flat_history");
        return NULL;
      }        
      h1=wr_get_history(g,otp(db,cl1));
      */
      //num=maxclnr-(int)(gint)(wg_nth(db,tmp1,2));
      /*
      repllitnr=wr_decode_para_termpath_litnr(g,replpath);
      */
      num=(int)(gint)(wg_nth(db,tmp1,2));
      snprintf(namebuf1,19,"%d",num);     
      if (i==2+HISTORY_DERIVATION_TAG_POS) {        
        if (pos1) {
          if (!leftflag) printf("%s.%d.R",namebuf1,pos1);
          else printf("%s.%d.L",namebuf1,pos1);
        } else {
          if (leftflag) printf("%s",namebuf1);
          else printf("%s.0.R",namebuf1);
        }  
      } else if (i==3+HISTORY_DERIVATION_TAG_POS) {
        if (pos2) printf("%s.%d.%d",namebuf1,pos2,termpos);
        else printf("%s.0.%d",namebuf1,termpos);
      } else {
       printf("%s",namebuf1);
      }    
      if (i<len-1) printf(", ");
    }
    if (g->print_history_extra) wr_print_history_extra(g,history);
    else printf("] ");  
    if (!cl) printf("false");
    else {
      wr_print_clause(g,cl);   
      //wg_print_record(db,cl);
    }  
  } else if (tag==WR_HISTORY_TAG_SIMPLIFY) {
    printf("\n %s: [simp, ",clns);    
    for(i=HISTORY_PARENT1_POS;i<len;i++) {
      cl1=wr_get_history_record_field(g,otp(db,history),i);
      tmp1=wg_get_assoc(db,(void *)cl1,*assoc);
      /*
      if (tmp1) {
        n1=wg_nth(db,tmp1,1);  
      } else {      
        show_history_error(g,"could not find parent in wr_print_flat_history");
        return NULL;
      }        
      h1=wr_get_history(g,otp(db,cl1));
      */
      //num=maxclnr-(int)(gint)(wg_nth(db,tmp1,2));
      num=(int)(gint)(wg_nth(db,tmp1,2));
      snprintf(namebuf1,19,"%d",num);
      printf("%s",namebuf1);          
      if (i<len-1) printf(", ");
    }
    if (g->print_history_extra) wr_print_history_extra(g,history);
    else printf("] ");  
    if (!cl) printf("false");
    else wr_print_clause(g,cl); 
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
  if (decprior==WR_HISTORY_GOAL_ROLENR) printf(",goal] ");
  else if (decprior==WR_HISTORY_ASSUMPTION_ROLENR) printf(",assumption] ");
  else if (decprior==WR_HISTORY_FROMGOALASSUMPTION_ROLENR) printf(",fromga] "); 
  else if (decprior==WR_HISTORY_FROMGOAL_ROLENR) printf(",fromgoal] ");
  else if (decprior==WR_HISTORY_FROMASSUMPTION_ROLENR) printf(",fromassumption] ");
  else if (decprior==WR_HISTORY_AXIOM_ROLENR) printf(",axiom] ");
  else if (decprior==WR_HISTORY_FROMAXIOM_ROLENR) printf(",fromaxiom] ");
  else if (decprior==WR_HISTORY_EXTAXIOM_ROLENR) printf(",extaxiom] ");
  else if (decprior) printf(", dp %d] ",(int)decprior);
  else printf("] ");
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
    printf("[] ");   
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
      printf("[%s",namestr);
    } else if (name && wg_get_encoded_type(db,name)==WG_INTTYPE) {
      printf("[%d",(int)(wg_decode_int(db,name)));
    } else {
      printf("[");
    }
    decprior=wr_decode_priority(g,priority);
    if (decprior==WR_HISTORY_GOAL_ROLENR) printf(",goal] ");
    else if (decprior==WR_HISTORY_ASSUMPTION_ROLENR) printf(",assumption] ");
    else if (decprior==WR_HISTORY_FROMGOALASSUMPTION_ROLENR) printf(",fromga] ");
    else if (decprior==WR_HISTORY_FROMASSUMPTION_ROLENR) printf(",fromassumption] ");
    else if (decprior==WR_HISTORY_AXIOM_ROLENR) printf(",axiom] ");
    else if (decprior==WR_HISTORY_FROMAXIOM_ROLENR) printf(",fromaxiom] ");
    else if (decprior==WR_HISTORY_EXTAXIOM_ROLENR) printf(",extaxiom] ");
    else if (decprior) printf(", dp %d] ",(int)decprior);

    //else if (decprior==1) printf(",goal] ");
    //else if (decprior==2) printf(",assumption] ");
    else printf("] ");     
    return;
  } else {
    // not an input clause
    printf("[] ");
  }
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
