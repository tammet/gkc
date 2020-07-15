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

// from gkc.c:

void segment_stats(void *db);
void wg_show_database(void* db);
  
/* ====== Functions ============== */


/* =====================================================

 

====================================================== */

int init_shared_database(void* db, char* guidefilename) {
  glb* g;
  db_memsegment_header* dbh = dbmemsegh(db);
  int tmp;
  char* guidebuf=NULL;
  cJSON *guide=NULL;
  //char* argvbuf[4];
  char* guidetext;
  int guideres=0;
  
#ifdef DEBUG
  wr_printf("\ninit_shared_database starts\n");
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
  wr_printf("\ng->db is %ld or %lx\n",
  (unsigned long int)(g->db),(unsigned long int)(g->db)); 

  wr_printf("db is %lx and g->db is %lx\n",(unsigned long int)db,(unsigned long int)(g->db));
  wr_printf("(g->db_offset) is %ld\n",(g->db_offset));
  wr_printf("\n dbcheck(db) gives %d \n", dbcheck(db));
  wr_printf("\n dbcheck(g->db) gives %d \n", dbcheck(g->db));
#endif   

  wr_glb_init_simple(g);  // fills in simple values (ints, strings etc)   
  //printf("\ndbmemsegh(db)->tptp %ld\n",dbmemsegh(db)->tptp);

  if (dbmemsegh(db)->tptp) {
    (g->print_tptp)=1;        
    (g->print_fof_conversion_proof)=1;
    (g->print_clauses_tptp)=1;
    (g->print_proof_tptp)=1;   
    (g->use_comp_arithmetic)=0;
  } else {
    (g->print_tptp)=0;        
    (g->print_fof_conversion_proof)=0; 
    (g->print_clauses_tptp)=0;
    (g->print_proof_tptp)=0;
    (g->use_comp_arithmetic)=1;
  }
  if (dbmemsegh(db)->json) {
    (g->print_json)=1; 
    (g->print_clauses_json)=1;
    if (g->print_tptp) {
      wr_errprint("do not set both tptp and json output parameters at the same time");    
      return -1;
    }
  }  

#ifdef DEBUG  
  wr_printf("\ninit_shared_database returned from wr_glb_init_simple\n");
#endif 

  if (guidefilename!=NULL) {
    //argvbuf[2]=guidefilename;
    wr_printf("\n");
    guide=wr_parse_guide_file(guidefilename,&guidebuf);
    if (guide==NULL) {
      if (guidebuf!=NULL) free(guidebuf);
      return -1;
    }
    guidetext=NULL;
    guideres=wr_parse_guide_section(g,guide,0,&guidetext);
    if (guideres<0) {
      // error in guide requiring stop      
      if (guidebuf!=NULL) free(guidebuf);
      if (guide!=NULL) cJSON_Delete(guide);      
    }
  }  
  //printf("\n(g->use_equality_strat) %d\n",(g->use_equality_strat));

  if (wr_glb_init_shared_complex(g)) {
    wr_errprint("cannot initialize complex part of shared db");
    return -1;
  }
#ifdef DEBUG  
  wr_printf("\ninit_shared_database returned from wr_glb_init_complex\n");
#endif 
  (g->inkb)=0; // temporarily pretend g is not inside the shared knowledge base 
  if (wr_glb_init_local_complex(g)) { // creates and fills in local tables, substructures, etc  
    wr_printf("\nerror: cannot init local complex datastructures\n");
    wr_glb_free_shared_complex(g);
    wr_glb_free_local_complex(g); 
    (g->inkb)=1;  // restore: g is inside the shared knowledge base  
    return -1;
  }
  (g->inkb)=1; // restore: g is inside the shared knowledge base 
  
#ifdef DEBUG  
  wg_show_db_memsegment_header(db);
  wr_printf("\nwg_show_db_memsegment_header ends\n");
  wg_show_strhash(db);
  wr_printf("\nwg_show_strhash ends\n");
#endif 
  tmp=wr_analyze_clause_list(g,db,db);
  if (!tmp) return -1;
  
  // temporarily set queryfocus-strat to disable equality ordering
  tmp=(g->queryfocus_strat);  
  (g->queryfocus_strat)=1;
  tmp=wr_init_db_clause_indexes(g,g->db);
  (g->queryfocus_strat)=tmp;

  //printf("\n (g->in_chain_clause_count) %d \n",(g->in_chain_clause_count));
  if (tmp<0) return -1;
  return 0;
}


int wr_init_db_clause_indexes(glb* g, void* db) {
  void *rec;
  gint clmeta;
  gptr given_cl;
  gint given_cl_metablock[CLMETABLOCK_ELS];
  int rules_found=0;
  int facts_found=0;
  gint cell;
  gcell *cellptr;

#ifdef DEBUG
  wr_printf("\nwr_init_db_clause_indexes starts, g->inkb is %d\n",(g->inkb)); 
#endif  
  // next two need init_local_complex do be performed before:
  wr_clear_all_varbanks(g);
  wr_process_given_cl_setupsubst(g,g->given_termbuf,1,1);   
  (g->proof_found)=0;

  // loop over clauses
  cell=(dbmemsegh(db)->clauselist);
  while(cell) {         
    cellptr=(gcell *) offsettoptr(db, cell);
    rec=offsettoptr(db,cellptr->car);
    if (g->alloc_err) {
      wr_errprint("\nbuffer overflow while processing parsed clauses, terminating\n");            
      exit(0);
    }  
#ifdef DEBUG
    wr_printf("\nrule with len %ld x\n",wg_get_record_len(db,rec));
    wg_print_record(db,rec);
#endif
    clmeta=wr_calc_clause_meta(g,rec,given_cl_metablock);
#ifdef DEBUG
    wr_printf("\nafter wr_calc_clause_meta\n");
    wg_print_record(db,rec);
#endif
    wr_add_cl_to_unithash(g,rec,clmeta);     
#ifdef DEBUG
    wr_printf("\nafter wr_add_cl_to_unithash\n");
    wg_print_record(db,rec);     
    wr_printf("\npreparing to do wr_process_given_cl\n");
#endif    
    // start allocating from record area instead of g->build_buffer 
    (g->build_buffer)=NULL;
    given_cl=wr_process_given_cl(g,(gptr)rec, NULL);
    if (!given_cl) {
      wr_printf("\nwr_process_given_cl failed\n");
      return -1;
    }
    if ( ((gint)given_cl==ACONST_FALSE) || ((gint)given_cl==ACONST_TRUE) ||
          (given_cl==NULL) ) {
#ifdef DEBUG
      wr_printf("\nrule clause was simplified while adding to sos, original:\n");
      wr_print_clause(g,(gptr)rec);
#endif
      cell=cellptr->cdr; 
      continue;
    };
#ifdef DEBUG
    wr_printf("\nwr_process_given_cl gives given_cl\n");
    wg_print_record(db,given_cl);
#endif  
    if (wg_rec_is_rule_clause(db,rec)) {
      rules_found++;
#ifdef DEBUG      
      wr_printf("\nto do wr_sort_cl with given_cl\n");
#endif      
      wr_sort_cl(g,given_cl);    
#ifdef DEBUG      
      wr_printf("\nafter wr_sort_cl given_cl\n");  
#endif      
    } else {
      facts_found++;
    }  
     // calculate resolvability: (g->tmp_resolvability_vec)
    wr_calc_clause_resolvability(g,rec,1,0);
#ifdef DEBUG     
    wr_print_clause_resolvability(g,rec);  
    wr_print_clause_hardnesses(g,rec); 
#endif
    // start allocating from record area instead of g->build_buffer 
    given_cl=wr_add_given_cl_active_list(g,given_cl,given_cl_metablock,0,NULL,(g->tmp_resolvability_vec));
#ifdef DEBUG 
    wr_printf("\ngiven_cl after wr_add_given_cl_active_list\n");
    wg_print_record(db,given_cl);
#endif                
    cell=cellptr->cdr;
  }  
#ifdef DEBUG            
  wr_printf("\nrules_found %d facts_found %d \n",rules_found,facts_found); 
#endif   
  return rules_found+facts_found;
}

/*

  passedg: g block to be shown directly, instead of the following db
  if passedg is NULL, pass the following db instead

  db: ptr to shared db memory which contains the internal gb block
  desc: description str of db, for debug output only

*/

void wr_show_database_details(glb* passedg,void* db, char* desc) {
  glb* rglb;
  glb* g;
  db_memsegment_header* dbh;
  void *rec;
  gint rlen;
  void *gdbptr;
  gint cell;
  gcell *cellptr;

  db_printf("\n*** wr_show_database_details for desc %s\n",desc);
  
  if (passedg==NULL) {
    printf("\ndirect g block passedg is NULL, hence using g inside db \n");
 
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

  } else {
    printf("\ndirect g block passedg is present, hence using passedg and not db or g inside db \n");
    g=passedg;
    db=g->db;
  }

  // show modified g contents

  printf("(g->db_offset) is %ld\n",(g->db_offset));
  printf("(g->db) is ptr %lx and int %ld\n",(unsigned long int)(g->db),(gint)(g->db));
  printf("(g->kb_db) is ptr %lx and int %ld\n",(unsigned long int)(g->kb_db),(gint)(g->kb_db));
  printf("(g->child_db) is ptr %lx and int %ld\n",(unsigned long int)(g->child_db),(gint)(g->child_db));
  printf("(g->inkb) is %ld\n",(g->inkb));

  // show g stats

  wr_show_stats(g,0); 

  // show g strings

  //wg_show_strhash(db);
 
  // show records in db   
  
  printf("\n\n** clauses\n");

  cell=(dbmemsegh(db)->clauselist);
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
    cell=cellptr->cdr;
  }
  
  // show groundunits hash

  printf("\n\n** hash_neg_groundunits\n");
  wr_print_termhash(g,rotp(g,g->hash_neg_groundunits));
  printf("\n** hash_pos_groundunits\n");
  wr_print_termhash(g,rotp(g,g->hash_pos_groundunits));  
  printf("\n");
   
  // show neg and pos atoms

  printf("\n** hash_neg_atoms:\n");      
  wr_clterm_hashlist_print(g, rotp(g,g->hash_neg_atoms));
  printf("\n** hash_pos_atoms:\n");      
  wr_clterm_hashlist_print(g, rotp(g,g->hash_pos_atoms));
  printf("\n");
  
  // show groundunits hash for given clause subsumption
  
  printf("\n** hash_neg_active_groundunits:\n"); 
  wr_print_termhash(g,rotp(g,g->hash_neg_active_groundunits));
  printf("\n** hash_pos_active_groundunits:\n"); 
  wr_print_termhash(g,rotp(g,g->hash_pos_active_groundunits));
  
  wr_printf("\n** clactivesubsume:\n"); 
  wr_show_clactivesubsume(g);

  wr_printf("\n** hash_para_terms:\n");      
  wr_clterm_hashlist_print_para(g,rotp(g,g->hash_para_terms)); 
  
  wr_printf("\n** hash_eq_terms:");      
  wr_clterm_hashlist_print_para(g,rotp(g,g->hash_eq_terms));  
  
}



void wr_show_database_headers(void* db) {
  db_memsegment_header* dbh;

  db_printf("\n*** wr_show_database_headers for ptr %lx as int %ld\n",
      (unsigned long int)db,(gint)db);
  db_printf("\ndbcheck(db) gives %d \n", dbcheck(db));
  dbh=dbmemsegh(db);
  db_printf("\ndbcheckh(dbh) gives %d \n", dbcheck(dbh));
  
  db_printf("\ndb->kb_db is ptr %lx and int %ld\n",(unsigned long int)(dbh->kb_db),(gint)(dbh->kb_db));
  db_printf("\ndb->rglb is ptr %lx and int %ld\n",(unsigned long int)(dbh->rglb),(gint)(dbh->rglb));

  wg_show_database(db);
}

#ifdef __cplusplus
}
#endif



