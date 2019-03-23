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

 /** @file init.c
 * Initialization of reasoning-specific data structures before search
 */

/* ====== Includes =============== */


#ifdef __cplusplus
extern "C" {
#endif

#include "rincludes.h"  
#include "../Test/dbtest.h" 
 

/* ====== Private defs =========== */


//#define DEBUG
#undef DEBUG  


/* ====== Private headers ======== */

int wr_init_db_clause_indexes(glb* g, void* db);  
  
/* ====== Functions ============== */


/* =====================================================

 

====================================================== */

int init_shared_database(void* db) {
  glb* g;
  db_memsegment_header* dbh = dbmemsegh(db);

#ifdef DEBUG
  printf("\ninit_shared_database starts\n");
#endif    
  //g=sys_malloc(sizeof(glb)); // allocate space
  g=wg_rawalloc(db,sizeof(glb));
  //g=malloc(sizeof(glb));

  if (g==NULL) {
    wr_errprint("cannot allocate enough memory during reasoner structures initialization");    
    return -1;
  } 
  // store database pointer to glb structure created
  (dbh->rglb)=ptrtooffset(db,g);
  // store pointer to whole shared db into the glb structure 
  (g->db)=db;       
  (g->kb_db)=db;
  (g->child_db)=NULL;
  (g->inkb)=1; // g is inside the shared knowledge base 

  wr_glb_init_simple(g);  // fills in simple values (ints, strings etc)   
#ifdef DEBUG  
  printf("\ninit_shared_database returned from wr_glb_init_simple\n");
#endif 
  if (wr_glb_init_shared_complex(g)) {
    wr_errprint("cannot initialize complex part of shared db");
    return -1;
  }
#ifdef DEBUG  
  printf("\ninit_shared_database returned from wr_glb_init_complex\n");
#endif 
  (g->inkb)=0; // temporarily pretend g is not inside the shared knowledge base 
  if (wr_glb_init_local_complex(g)) { // creates and fills in local tables, substructures, etc  
    printf("\nerror: cannot init local complex datastructures\n");
    wr_glb_free_shared_complex(g);
    wr_glb_free_local_complex(g); 
    (g->inkb)=1;  // restore: g is inside the shared knowledge base  
    return -1;
  }
  (g->inkb)=1; // restore: g is inside the shared knowledge base 

  
#ifdef DEBUG  
  wg_show_db_memsegment_header(db);
  printf("\nwg_show_db_memsegment_header ends\n");
  wg_show_strhash(db);
  printf("\nwg_show_strhash ends\n");
#endif 
  wr_init_db_clause_indexes(g,db);
#ifdef DEBUG
  printf("\ninit_shared_database ends\n");
#endif   
  return 0;
}


int wr_init_db_clause_indexes(glb* g, void* db) {
  void *rec;
  gint clmeta;
  gptr given_cl_as_active, given_cl;
  gint given_cl_metablock[CLMETABLOCK_ELS];
  int rules_found=0;
  int facts_found=0;
  //gint weight;
  gint rlen;

  // next two need init_local_complex do be performed before:
  wr_clear_all_varbanks(g);
  wr_process_given_cl_setupsubst(g,g->given_termbuf,1,1);   
  (g->proof_found)=0;

  rec = wg_get_first_raw_record(db);
  while(rec) {     
    rlen=wg_get_record_len(db,rec);   

    if ((wg_rec_is_rule_clause(db,rec) || wg_rec_is_fact_clause(db,rec)) ) {
      printf("\nrec with len %ld\n",rlen);
      wg_print_record(db,rec);
      clmeta=wr_calc_clause_meta(g,rec,given_cl_metablock);
      wr_add_cl_to_unithash(g,rec,clmeta);
    }


    if ((wg_rec_is_rule_clause(db,rec) || wg_rec_is_fact_clause(db,rec)) ) {
      printf("\nrec with len %ld\n",rlen);
      wg_print_record(db,rec);
      clmeta=wr_calc_clause_meta(g,rec,given_cl_metablock);
      wr_add_cl_to_unithash(g,rec,clmeta);
    }
    rec = wg_get_next_raw_record(db,rec);    
  }
  printf("\nhash_neg_groundunits\n");
  wr_print_termhash(g,rotp(g,g->hash_neg_groundunits));
  printf("\nhash_pos_groundunits\n");
  wr_print_termhash(g,rotp(g,g->hash_pos_groundunits));
  return 0;
  
 
  while(rec) {        
    if (g->alloc_err) {
      wr_errprint("\nbuffer overflow while processing parsed clauses, terminating\n");            
      exit(0);
    }     
    if (wg_rec_is_rule_clause(db,rec)) {
      rules_found++;
      clmeta=wr_calc_clause_meta(g,rec,given_cl_metablock);
      wr_add_cl_to_unithash(g,rec,clmeta);         
      given_cl=wr_process_given_cl(g,(gptr)rec);
      if ( ((gint)given_cl==ACONST_FALSE) || ((gint)given_cl==ACONST_TRUE) ||
            (given_cl==NULL) ) {

        printf("\nrule clause was simplified while adding to sos, original:\n");
        wr_print_clause(g,(gptr)rec);

        rec = wg_get_next_raw_record(db,rec); 
        continue;
      };
      wr_sort_cl(g,given_cl);      
      given_cl_as_active=wr_add_given_cl_active_list(g,given_cl,given_cl_metablock,0);
      if (given_cl_as_active==NULL) continue;       
    } else if (wg_rec_is_fact_clause(db,rec)) {
      facts_found++;
      clmeta=wr_calc_clause_meta(g,rec,given_cl_metablock);
      wr_add_cl_to_unithash(g,rec,clmeta);
      given_cl=wr_process_given_cl(g,(gptr)rec);
      if ( ((gint)given_cl==ACONST_FALSE) || ((gint)given_cl==ACONST_TRUE) ||
            (given_cl==NULL) ) {
        
        printf("\nfact clause was simplified while processing, original:\n");
        wr_print_clause(g,(gptr)rec);

        rec = wg_get_next_raw_record(db,rec); 
        continue;
      }
      given_cl_as_active=wr_add_given_cl_active_list(g,given_cl,given_cl_metablock,1);
      if (given_cl_as_active==NULL) continue;                    
    }               
    rec = wg_get_next_raw_record(db,rec);    
  }
#ifdef DEBUG            
  printf("\nrules_found %d facts_found %d \n",rules_found,facts_found); 
#endif   
  return rules_found+facts_found;
}


#ifdef __cplusplus
}
#endif



