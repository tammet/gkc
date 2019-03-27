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


#define DEBUG
//#undef DEBUG  


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
  (g->db_offset)=(gint)(((char*)g)-((char*)db));
  (g->inkb)=1; // g is inside the shared knowledge base 

#ifdef DEBUG
  printf("\ng->db is %ld or %lx\n",
  (unsigned long int)(g->db),(unsigned long int)(g->db)); 

  printf("db is %lx and g->db is %lx\n",(unsigned long int)db,(unsigned long int)(g->db));
  printf("(g->db_offset) is %ld\n",(g->db_offset));
  printf("\n dbcheck(db) gives %d \n", dbcheck(db));
  printf("\n dbcheck(g->db) gives %d \n", dbcheck(g->db));
#endif   

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
  printf("\nwr_init_db_clause_indexes ended\n");

  printf("\n* hash_neg_groundunits at offset %ld and ptr %lx\n",
    (gint)(g->hash_neg_groundunits),(unsigned long int)rotp(g,g->hash_neg_groundunits));  
  wr_print_termhash(g,rotp(g,g->hash_neg_groundunits));
  printf("\n* hash_pos_groundunits at offset %ld and ptr %lx\n",
    (gint)(g->hash_pos_groundunits),(unsigned long int)rotp(g,g->hash_pos_groundunits));
  wr_print_termhash(g,rotp(g,g->hash_pos_groundunits));

  printf("\n* hash_neg_atoms:\n");      
  wr_clterm_hashlist_print(g,rotp(g,g->hash_neg_atoms));
  printf("\n* hash_pos_atoms:\n");      
  wr_clterm_hashlist_print(g,rotp(g,g->hash_pos_atoms));

  /*
  printf("\n* hash_neg_active_groundunits:\n"); 
  wr_print_termhash(g,rotp(g,g->hash_neg_active_groundunits));
  printf("\n* hash_pos_active_groundunits:\n"); 
  wr_print_termhash(g,rotp(g,g->hash_pos_active_groundunits));
  
  printf("\n* hash_para_terms after adding:");      
  wr_clterm_hashlist_print(g,rotp(g,g->hash_para_terms)); 
  printf("\nhash_eq_terms after adding:");      
  wr_clterm_hashlist_print(g,rotp(g,g->hash_eq_terms));
  */
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
  gptr active_cl;
  gint cell;
  gcell *cellptr;

  printf("\nwr_init_db_clause_indexes starts, g->inkb is %d\n",(g->inkb)); 
  // next two need init_local_complex do be performed before:
  wr_clear_all_varbanks(g);
  wr_process_given_cl_setupsubst(g,g->given_termbuf,1,1);   
  (g->proof_found)=0;
  
  // start allocating from record area instead of g->build_buffer
  // (g->build_buffer)=NULL;  // setting it here does not work: overwritten later

  // loop over initial clauses

  rec = wg_get_first_raw_record(db);
  while(rec) {     
    rlen=wg_get_record_len(db,rec);   
    if ((wg_rec_is_rule_clause(db,rec) || wg_rec_is_fact_clause(db,rec)) ) {
      printf("\ninitial rec with len %ld\n",rlen);
      wg_print_record(db,rec);      
      cell=alloc_listcell(db);
      cellptr = (gcell *) offsettoptr(db, cell);
      (cellptr->car) = ptrtooffset(db, rec);
      (cellptr->cdr) = (g->initial_cl_list);
      (g->initial_cl_list) = cell;           
    }
    rec = wg_get_next_raw_record(db,rec);    
  }
  
  /*
  cell=g->initial_cl_list;
  while(cell) {
    cellptr=(gcell *) offsettoptr(db, cell);
    printf("\nlisted rec \n");
    wg_print_record(db,offsettoptr(db,cellptr->car));
    printf("\nnext rec \n");
    cell=cellptr->cdr;  
  }
  */

  // next loop over collected clauses
 
  //rec = wg_get_first_raw_record(db);
  cell=g->initial_cl_list;
  while(cell) {        
    cellptr=(gcell *) offsettoptr(db, cell);
    rec=offsettoptr(db,cellptr->car);
    if (g->alloc_err) {
      wr_errprint("\nbuffer overflow while processing parsed clauses, terminating\n");            
      exit(0);
    }  
    rlen=wg_get_record_len(db,rec);   
    if (wg_rec_is_rule_clause(db,rec)) {
      rules_found++;

      printf("\nrule with len %ld x\n",rlen);
      wg_print_record(db,rec);

      clmeta=wr_calc_clause_meta(g,rec,given_cl_metablock);

      printf("\nafter wr_calc_clause_meta\n");
      wg_print_record(db,rec);

      wr_add_cl_to_unithash(g,rec,clmeta);     

      printf("\nafter wr_add_cl_to_unithash\n");
      wg_print_record(db,rec);
      
      wr_process_given_cl_setupsubst(g,g->active_termbuf,2,0);    
      (g->build_buffer)=NULL;
      active_cl=wr_build_calc_cl(g,rec);

      printf("\nactive_cl built by wr_build_calc_cl: \n");
      wg_print_record(db,active_cl);

      wr_process_given_cl_cleanupsubst(g); 
      if (active_cl==NULL) {
        printf("\nactive_cl created is NULL\n");
        //rec = wg_get_next_raw_record(db,rec); 
        cell=cellptr->cdr;
        continue;
        //return NULL; // could be memory err
      }  

      wr_cl_store_res_terms(g,active_cl);
      wr_cl_store_para_terms(g,active_cl);

      /*
      printf("\npreparing to do wr_process_given_cl\n");

      (g->build_buffer)=NULL;
      given_cl=wr_process_given_cl(g,(gptr)rec, NULL);
      if ( ((gint)given_cl==ACONST_FALSE) || ((gint)given_cl==ACONST_TRUE) ||
            (given_cl==NULL) ) {

        printf("\nrule clause was simplified while adding to sos, original:\n");
        wr_print_clause(g,(gptr)rec);

        cell=cellptr->cdr; 
        continue;
      };

      printf("\nwr_process_given_cl gives given_cl\n");
      wg_print_record(db,given_cl);
      */
      
      printf("\nto do wr_sort_cl with active_cl\n");
      //wr_sort_cl(g,given_cl);    
      wr_sort_cl(g,active_cl);

      printf("\nafter wr_sort_cl active_cl\n");
      wg_print_record(db,active_cl);
      
      /*
      //given_cl_as_active=wr_add_given_cl_active_list(g,given_cl,given_cl_metablock,0);
      given_cl_as_active=wr_add_given_cl_active_list(g,rec,given_cl_metablock,0);

      printf("\ngiven_cl_as_active\n");
      wg_print_record(db,given_cl_as_active);

      if (given_cl_as_active==NULL) continue;   
      */
      
    } else if (wg_rec_is_fact_clause(db,rec)) {
      facts_found++;

      printf("\nfact with len %ld\n",rlen);
      wg_print_record(db,rec);

      clmeta=wr_calc_clause_meta(g,rec,given_cl_metablock);

      printf("\nafter wr_calc_clause_meta\n");
      wg_print_record(db,rec);

      wr_add_cl_to_unithash(g,rec,clmeta);

      printf("\nafter wr_add_cl_to_unithash\n");
      wg_print_record(db,rec);

      wr_process_given_cl_setupsubst(g,g->active_termbuf,2,0);    
      (g->build_buffer)=NULL;
      active_cl=wr_build_calc_cl(g,rec);

      printf("\nactive_cl for fact clause built by wr_build_calc_cl: \n");
      wg_print_record(db,active_cl);

      wr_process_given_cl_cleanupsubst(g); 
      if (active_cl==NULL) {
        printf("\nactive_cl created is NULL\n");
        //rec = wg_get_next_raw_record(db,rec); 
        cell=cellptr->cdr;
        continue;
        //return NULL; // could be memory err
      }  

      wr_cl_store_res_terms(g,rec);
      wr_cl_store_para_terms(g,active_cl);
     
      /*
      given_cl=wr_process_given_cl(g,(gptr)rec);
      if ( ((gint)given_cl==ACONST_FALSE) || ((gint)given_cl==ACONST_TRUE) ||
            (given_cl==NULL) ) {
        
        printf("\nfact clause was simplified while processing, original:\n");
        wr_print_clause(g,(gptr)rec);

        rec = wg_get_next_raw_record(db,rec); 
        continue;
      }

      printf("\nwr_process_given_cl gives given_cl\n");
      wg_print_record(db,given_cl);

      //given_cl_as_active=wr_add_given_cl_active_list(g,given_cl,given_cl_metablock,1);
      given_cl_as_active=wr_add_given_cl_active_list(g,rec,given_cl_metablock,1);

      printf("\ngiven_cl_as_active\n");
      wg_print_record(db,given_cl_as_active);

      if (given_cl_as_active==NULL) continue; 
      */                   
    }               
    //rec = wg_get_next_raw_record(db,rec);    
    cell=cellptr->cdr;
  }
#ifdef DEBUG            
  printf("\nrules_found %d facts_found %d \n",rules_found,facts_found); 
#endif   
  return rules_found+facts_found;
}



void wr_show_database_details(glb* passedg,void* db, char* desc) {
  glb* rglb;
  glb* g;
  db_memsegment_header* dbh;

  void *rec;
  gint clmeta;
  gptr given_cl_as_active, given_cl;
  gint given_cl_metablock[CLMETABLOCK_ELS];
  int rules_found=0;
  int facts_found=0;
  //gint weight;
  gint rlen;
  void *gdbptr;
  gint cell;
  gcell *cellptr;

  printf("\n*** wr_show_database_details for desc %s\n",desc);
  
  printf("\ndbcheck(db) gives %d \n", dbcheck(db));
  dbh=dbmemsegh(db);
  printf("\ndbcheckh(dbh) gives %d \n", dbcheck(dbh));
  printf("\ndb as ptr is %lx \n", (unsigned long int)db);


  rglb=db_rglb(db); // this is the internal g of db
  g=malloc(sizeof(glb)); // this is a new malloced g
  // copy rglb stuff to g
  memcpy(g,rglb,sizeof(glb));
  // now g should contain the same things as rglb

  printf("internal rglb ptr is %lx and int is %ld\n", (unsigned long int)rglb,(gint)rglb);
  printf("malloced    g ptr is %lx and int is %ld\n", (unsigned long int)g,(gint)g);

  // show rglb contents

  printf("(rglb->db_offset) is %ld\n",(rglb->db_offset));
  printf("(rglb->db) is ptr %lx and int %ld\n",(unsigned long int)(rglb->db),(gint)(rglb->db));
  printf("(rglb->kb_db) is ptr %lx and int %ld\n",(unsigned long int)(rglb->kb_db),(gint)(rglb->kb_db));
  printf("(rglb->child_db) is ptr %lx and int %ld\n",(unsigned long int)(rglb->child_db),(gint)(rglb->child_db));
  printf("(rglb->inkb) is %ld\n",(rglb->inkb));

  // compute, check and store correct db ptrs to g
  // offset was created as (g->db_offset)=(gint)(((char*)g)-((char*)db))
  gdbptr = (void *)(((char*)rglb)-(rglb->db_offset));
  printf("calculated db ptr from internal rglb as %lx and int %ld\n",(unsigned long int)gdbptr, (gint)gdbptr);
  printf("\ndbcheck(gdbptr) gives %d \n", dbcheck(gdbptr));
  (g->db) = gdbptr;
  (g->kb_db) = gdbptr;

  // show modified g contents

  printf("(g->db_offset) is %ld\n",(g->db_offset));
  printf("(g->db) is ptr %lx and int %ld\n",(unsigned long int)(g->db),(gint)(g->db));
  printf("(g->kb_db) is ptr %lx and int %ld\n",(unsigned long int)(g->kb_db),(gint)(g->kb_db));
  printf("(g->child_db) is ptr %lx and int %ld\n",(unsigned long int)(g->child_db),(gint)(g->child_db));
  printf("(g->inkb) is %ld\n",(g->inkb));

  // show g stats

  wr_show_stats(g,0);  
  
  // show records in db   
  
  //rec = wg_get_first_raw_record(db);
  cell=g->initial_cl_list;
  while(cell) {        
    cellptr=(gcell *) offsettoptr(db, cell);
    rec=offsettoptr(db,cellptr->car);    
    rlen=wg_get_record_len(db,rec);   

    if ((wg_rec_is_rule_clause(db,rec) || wg_rec_is_fact_clause(db,rec)) ) {
      printf("\nrec with rlen %ld rec as ptr %lx as int %ld \n",rlen,(unsigned long int)rec,(gint)rec);     
      wg_print_record(db,rec);
      printf("\n as clause \n");
      wr_print_clause(g,rec);
    }
    //rec = wg_get_next_raw_record(db,rec);    
    cell=cellptr->cdr;
  }
  
  // show groundunits hash

  printf("\n** show hash_neg_groundunits\n");
  wr_print_termhash(g,rotp(g,g->hash_neg_groundunits));
  printf("\n** show hash_pos_groundunits\n");
  wr_print_termhash(g,rotp(g,g->hash_pos_groundunits));  
  printf("\n");
   
  // show neg and pos atoms

  printf("\n** show hash_neg_atoms:\n");      
  wr_clterm_hashlist_print(g, rotp(g,g->hash_neg_atoms));
  printf("\n** show hash_pos_atoms:\n");      
  wr_clterm_hashlist_print(g, rotp(g,g->hash_pos_atoms));
  printf("\n");
  
  /*
  printf("\n* hash_neg_active_groundunits:\n"); 
  wr_print_termhash(g,
     (unsigned long int) ((gptr)(offsettoptr(db,(g->hash_neg_active_groundunits)))) );
     //rotp(g,g->hash_neg_active_groundunits));
  printf("\n* hash_pos_active_groundunits:\n"); 
  wr_print_termhash(g,
     (unsigned long int) ((gptr)(offsettoptr(db,(g->hash_pos_active_groundunits)))) );
     //rotp(g,g->hash_pos_active_groundunits));
  */
  
  
  printf("\n** show hash_para_terms:\n");      
  wr_clterm_hashlist_print_para(g,rotp(g,g->hash_para_terms)); 

  /*
  printf("\nhash_eq_terms after adding:");      
  wr_clterm_hashlist_print(g,rotp(g,g->hash_eq_terms));  
  */
}

#ifdef __cplusplus
}
#endif



