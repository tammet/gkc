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

 /** @file rmain.c
 *  Reasoner top level: initialisation and startup.
 *
 */

/* ====== Includes =============== */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../Builtparser/dbparse.h"
#else
#include "../Parser/dbparse.h"
#endif

#include "rincludes.h"  
#include "../cjson/cjson.h"
#include "../Db/dballoc.h"  // for ACONST_TRUE/FALSE

#ifdef __cplusplus
extern "C" {
#endif

  
/* ====== Private headers and defs ======== */

static void wr_set_no_printout(glb* g);
static void wr_set_low_printout(glb* g);
static void wr_set_normal_printout(glb* g);
static void wr_set_medium_printout(glb* g);
static void wr_set_detailed_printout(glb* g);  

#define SHOW_SUBSUME_STATS
#define SHOW_MEM_STATS
#define SHOW_HASH_CUT_STATS
  
/* ======= Private protos ================ */

//#define DEBUG
#undef DEBUG

/* ====== Functions ============== */


  
int wg_run_reasoner(void *db, int argc, char **argv) {
  glb* g;
  int res=1;
  int default_print_level=10;
  int iter,guideres=0,tmp;
  char* guidebuf=NULL;
  cJSON *guide=NULL;
  int clause_count=0;

  /*
    dprintf("\nargc %d\n",argc);
    for(i=0;i<argc;i++) {
      dprintf("arg %d is %s\n",i,argv[i]);
    }
  */

#ifdef CHECK
  //printf("\ndb CHECK macro is on\n");
#else
 //printf("\ndb CHECK macro is off\n");
#endif
  guide=wr_parse_guide_file(argc,argv,&guidebuf);
  if (guide==NULL) {
    if (guidebuf!=NULL) free(guidebuf);
    return -1;
  }

  dprintf("Guide parsed.\n");
  for(iter=0; 1; iter++) {    

    dprintf("\n**** run %d starts\n",iter+1);    

    g=wr_glb_new_simple(db);
    if (g==NULL) {
      wr_errprint("cannot allocate enough memory during reasoner initialization");
      if (guidebuf!=NULL) free(guidebuf);
      if (guide!=NULL) cJSON_Delete(guide);
      sys_free(g); 
      return -1;
    }   

    (g->current_run_nr)=iter;
    if (iter==0) (g->allruns_start_clock)=clock();  
    guideres=wr_parse_guide_section(g,guide,iter);
    if (guideres<0) {
      // error in guide requiring stop      
      if (guidebuf!=NULL) free(guidebuf);
      if (guide!=NULL) cJSON_Delete(guide);
      sys_free(g);
      return -1;
    }

    if (!(g->print_flag)) (g->print_level_flag)=0;
    if ((g->print_level_flag)<0) (g->print_level_flag)=default_print_level;
    if ((g->print_level_flag)==0) wr_set_no_printout(g);
    else if ((g->print_level_flag)<=10) wr_set_low_printout(g);
    else if ((g->print_level_flag)<=20) wr_set_normal_printout(g);
    else if ((g->print_level_flag)<=30) wr_set_medium_printout(g);
    //else if ((g->print_level_flag)<=40) wr_set_detailed_printout(g);
    else wr_set_detailed_printout(g);
    (g->cl_keep_weightlimit)=(g->cl_maxkeep_weightlimit);
    (g->cl_keep_sizelimit)=(g->cl_maxkeep_sizelimit);
    (g->cl_keep_depthlimit)=(g->cl_maxkeep_depthlimit);
    (g->cl_keep_lengthlimit)=(g->cl_maxkeep_lengthlimit);    

    //clock_t t1=clock();
    tmp=wr_glb_init_shared_complex(g); // creates and fills in shared tables, substructures, etc: 0.03 secs
    //clock_t t2=clock();
    //printf("wr_glb_init_shared_complex seconds: %d %d %f\n",
    //  t1,t2,(float)(t2 -t1) / (float)CLOCKS_PER_SEC);
      
    if (tmp) {
      wr_errprint("cannot init shared complex datastructures");
      if (guidebuf!=NULL) free(guidebuf);
      if (guide!=NULL) cJSON_Delete(guide); 
      wr_glb_free_shared_complex(g);
      sys_free(g);
      return -1; 
    }  
    tmp=wr_glb_init_local_complex(g); // creates and fills in local tables, substructures, etc: fast
    if (tmp) {
      wr_errprint("cannot init local complex datastructures");
      if (guidebuf!=NULL) free(guidebuf);
      if (guide!=NULL) cJSON_Delete(guide); 
      wr_glb_free_shared_complex(g);
      wr_glb_free_local_complex(g);
      sys_free(g);
      return -1; 
    } 

    clause_count=wr_init_active_passive_lists_from_all(g);
    if (clause_count<0) {
      // error
      printf("\nError initializing clause lists.\n");
      wr_glb_free(g);
      break;
    } else if (!clause_count) {
      // no clauses found
      printf("\nNo clauses found.\n");
      wr_glb_free(g);
      break;
    }
    // ok, clauses found and clause lists initialized
    res=wr_genloop(g);
    if (g->print_flag) { 
      if (res==0) {
        printf("\nProof found.\n"); 
        wr_show_history(g,g->proof_history);
      } else if (res==1) {
        printf("\nSearch finished without proof, result code %d.\n",res); 
      } else if (res==2) {
        printf("\nSearch terminated without proof.\n");        
      } else if (res==-1) {
        printf("\nSearch cancelled: memory overflow.\n");
      } else if (res<0) {
        printf("\nSearch cancelled, error code %d.\n",res);
      }      
      wr_show_stats(g);
    }  
    if (res==0) {
      // proof found 
      wr_glb_free(g);
      break;
    }
    // no proof found here
    wr_glb_free(g); 
    if ((iter+1)>=guideres) break;      
  }

  if (guidebuf!=NULL) free(guidebuf);
  if (guide!=NULL) cJSON_Delete(guide);      
  return res;  
} 

/*
glb* wg_init_reasoner(void *db, int argc, char **argv) {
  glb* g;
    
  dprintf("init starts\n");
  g=wr_glb_new_full(db);
  if (g==NULL) {
    printf("Error: cannot allocate enough memory during reasoner initialization\n");
    return NULL;
  }
  dprintf("glb made\n");
  dprintf("cycling over clauses to make active passive lists\n"); 
  wr_init_active_passive_lists_std(g);
  //wr_init_active_passive_lists_factactive(g);   
  //wr_init_active_passive_lists_ruleactive(g);
  dprintf("active passive lists made\n");  
  return g;  
}  
*/

int wg_import_otter_file(void *db, char* filename) {  
  glb* g;
  int res;

  //dprintf("wg_import_otterfile starts\n"); 
  g=wr_glb_new_simple(db); // no complex values given to glb elements 
  if (g==NULL) return 1; 
  (g->parser_print_level)=0;
  (g->print_initial_parser_result)=0;
  (g->print_generic_parser_result)=1;
  res=wr_import_otter_file(g,filename,NULL,NULL,0);
  if (res==1) {  
    if (1) { //} ((g->print_flag) &&  (g->print_level_flag)) {
      printf("No clauses generated by parser.\n");  
    }
  } else {
    //wg_mpool_print(db,pres2);
    if (0) { //((g->print_flag) && (g->print_generic_parser_result)>0) {    
      printf("\nGeneric parser result:\n");
      //wr_print_db_otter(g,(g->print_clause_detaillevel));
    }  
  } 
  sys_free(g); // no complex values given to glb elements
  //dprintf("wg_import_otterfile ends with res\n",res); 
  return res;  
}

int wg_import_prolog_file(void *db, char* filename) {  
  glb* g;
  int res;

  //dprintf("wg_import_prologfile starts\n"); 
  g=wr_glb_new_simple(db); // no complex values given to glb elements 
  if (g==NULL) return 1; 
  res=wr_import_prolog_file(g,filename,NULL,NULL);
  sys_free(g); // no complex values given to glb elements
  //dprintf("wg_import_prologfile ends with res %d\n",res); 
  return res;  
}

int wr_init_active_passive_lists_from_all(glb* g) {
  void* db=g->db;
  void* kb_db;

  kb_db=db_get_kb_db(db);
  wr_init_active_passive_lists_from_one(g,db,kb_db);
  if (kb_db!=NULL) {
    printf("\n external kb found, using\n");
    wr_init_active_passive_lists_from_one(g,kb_db,kb_db);
  } else {
    //printf("\n external kb NOT found\n");
  } 
}  

int wr_init_active_passive_lists_from_one(glb* g, void* db, void* kb_db) {
  void *rec;
  //void* db=(g->db);
  //int i;
  gint clmeta;
  gptr given_cl_as_active, given_cl;
  gint given_cl_metablock[CLMETABLOCK_ELS];
  int rules_found=0;
  int facts_found=0;
  gint weight;

  (g->proof_found)=0;

  //printf("\n!!! to wr_init_active_passive_lists_std on db %d\n",(int)db);

  //for(i=0;i<10;i++) printf(" %d ",(int)((rotp(g,g->clqueue))[i])); printf("\n");
  rec = wg_get_first_raw_record(db);

  wr_clear_all_varbanks(g); 
  wr_process_given_cl_setupsubst(g,g->given_termbuf,1,1); 
  /*
  while(rec) {     
    printf("\n next rec from db: ");
    wg_print_record(db,rec);
    printf("\n");
    if (kb_db) wg_print_record(kb_db,rec);
    printf("\n");

    rec = wg_get_next_raw_record(db,rec);    
  }
  return;
  */
  while(rec) {     
    //printf("\n next rec from db: ");
    //wg_print_record(db,rec);
    //printf("\n");
    if (g->alloc_err) {
      wr_errprint("\nbuffer overflow, terminating\n");
      wr_show_stats(g);      
      exit(0);
    }     

    if (wg_rec_is_rule_clause(db,rec)) {
      rules_found++;
      clmeta=wr_calc_clause_meta(g,rec,given_cl_metablock);
      wr_add_cl_to_unithash(g,rec,clmeta);
#ifdef DEBUG      
      wr_print_rule_clause_otter(g, (gint *) rec,(g->print_clause_detaillevel));
      printf("\n"); 

      printf("\nraw clause: ");
      wg_print_record(db,rec);
      printf("\n");
      wr_print_clause(g,rec);
      printf("\n");
      
      printf("\nhash_neg_groundunits\n");
      wr_print_termhash(g,rotp(g,g->hash_neg_groundunits));
      printf("\nhash_pos_groundunits\n");
      wr_print_termhash(g,rotp(g,g->hash_pos_groundunits));
#endif       

      if (g->queryfocus_strat && wr_initial_select_active_cl(g,(gptr)rec)) {
        given_cl=wr_process_given_cl(g,(gptr)rec);
        if ( ((gint)given_cl==ACONST_FALSE) || ((gint)given_cl==ACONST_TRUE) ||
             (given_cl==NULL) ) {
          /*     
          dp("\n rule clause was simplified while adding to sos, original:\n");
          wr_print_clause(g,(gptr)rec);
          */
          rec = wg_get_next_raw_record(db,rec); 
          continue;
        };
        wr_sort_cl(g,given_cl);
        //given_cl_as_active=wr_add_given_cl_active_list(g,(gptr)rec);
        given_cl_as_active=wr_add_given_cl_active_list(g,given_cl,given_cl_metablock,0);
        if (given_cl_as_active==NULL) {
          //if (g->alloc_err) return -1;
          continue; 
        }    
      } else {
        //wr_push_clqueue_cl(g,rec);
        /*
        if (wr_cl_is_goal(g, rec)) weight=1;
        else if (wr_cl_is_assumption(g, rec)) weight=2;
        else weight=-1;
        */
        weight=-1;
        wr_push_cl_clpick_queues(g,(g->clpick_queues),rec,weight); // -1 means we do not know weight
      } 

    } else if (wg_rec_is_fact_clause(db,rec)) {
      facts_found++;
      clmeta=wr_calc_clause_meta(g,rec,given_cl_metablock);
      wr_add_cl_to_unithash(g,rec,clmeta);
#ifdef DEBUG      
      wr_print_fact_clause_otter(g, (gint *) rec,(g->print_clause_detaillevel));
      printf("\n"); 

      printf("\nraw clause: ");
      wg_print_record(db,rec);
      printf("\n");
      wr_print_clause(g,rec);
      printf("\n");
     
      printf("\nhash_neg_groundunits\n");
      wr_print_termhash(g,rotp(g,g->hash_neg_groundunits));
      printf("\nhash_pos_groundunits\n");
      wr_print_termhash(g,rotp(g,g->hash_pos_groundunits));
#endif      
      
      if (g->queryfocus_strat && wr_initial_select_active_cl(g,(gptr)rec)) {
        given_cl=wr_process_given_cl(g,(gptr)rec);
        if ( ((gint)given_cl==ACONST_FALSE) || ((gint)given_cl==ACONST_TRUE) ||
             (given_cl==NULL) ) {
          /*     
          dp("\n fact clause was simplified while adding to sos, original:\n");
          wr_print_clause(g,(gptr)rec);
          */
          rec = wg_get_next_raw_record(db,rec); 
          continue;
        }
        given_cl_as_active=wr_add_given_cl_active_list(g,given_cl,given_cl_metablock,1);
        if (given_cl_as_active==NULL) {
          if (g->alloc_err) return -1;
          //continue; 
        }    
      } else {
        //wr_push_clqueue_cl(g,rec);
        //wr_push_clqueue_cl(g,rec);
        /*
        if (wr_cl_is_goal(g, rec)) weight=1;
        else if (wr_cl_is_assumption(g, rec)) weight=2;
        else weight=-1;
        */
        weight=-1;
        wr_push_cl_clpick_queues(g,(g->clpick_queues),rec,weight); // -1 means we do not know weight
      }
      
    }  else {
#ifdef DEBUG            
      printf("\nrec is not a rule nor a fact: "); 
      wg_print_record(db,rec);
      printf("\n");
#endif  
    }             
    //for(i=0;i<10;i++) printf(" %d ",(int)((rotp(g,g->clqueue))[i])); printf("\n");
    rec = wg_get_next_raw_record(db,rec);    
  }
  return rules_found+facts_found;
}


/*
int wr_init_active_passive_lists_factactive(glb* g) {
  void *rec;
  void* db=(g->db);
  //int i;
  
  (g->proof_found)=0;
  //for(i=0;i<10;i++) printf(" %d ",(int)((rotp(g,g->clqueue))[i])); printf("\n");
  rec = wg_get_first_raw_record(db);
  while(rec) {
    if (wg_rec_is_rule_clause(db,rec)) {
#ifdef DEBUG       
      wr_print_rule_clause_otter(g, (gint *) rec);
      printf("\n"); 
#endif      
      //wr_push_clqueue_cl(g,rec);      
      wr_push_cl_clpick_queues(g,(g->clpick_queues),rec,-1); // -1 means we do not know weight      
    } else if (wg_rec_is_fact_clause(db,rec)) {
#ifdef DEBUG       
      wr_print_fact_clause_otter(g, (gint *) rec);
      printf("\n"); 
#endif      
      wr_push_clactive_cl(g,rec);
    }               
    //for(i=0;i<10;i++) printf(" %d ",(int)((rotp(g,g->clqueue))[i])); printf("\n");
    rec = wg_get_next_raw_record(db,rec);    
  }
  return 0;
}
*/

/*
int wr_init_active_passive_lists_ruleactive(glb* g) {
  void *rec;
  void* db=(g->db);
  //int i;
  
  (g->proof_found)=0;
  //for(i=0;i<10;i++) printf(" %d ",(int)((rotp(g,g->clqueue))[i])); printf("\n");
  rec = wg_get_first_raw_record(db);
  while(rec) {
    if (wg_rec_is_rule_clause(db,rec)) {
#ifdef DEBUG       
      wr_print_rule_clause_otter(g, (gint *) rec);
      printf("\n"); 
#endif      
      wr_push_clactive_cl(g,rec);      
    } else if (wg_rec_is_fact_clause(db,rec)) {
#ifdef DEBUG       
      wr_print_fact_clause_otter(g, (gint *) rec);
      printf("\n"); 
#endif      
      //wr_push_clqueue_cl(g,rec);
      wr_push_cl_clpick_queues(g,(g->clpick_queues),rec,-1); // -1 means we do not know weight
    }               
    //for(i=0;i<10;i++) printf(" %d ",(int)((rotp(g,g->clqueue))[i])); printf("\n");
    rec = wg_get_next_raw_record(db,rec);    
  }
  return 0;
}
*/


/* ------------------------

  Printout settings 
  
 -------------------------- */

static void wr_set_no_printout(glb* g) {
  (g->print_flag)=0;
  
  (g->parser_print_level)=0;
  (g->print_initial_parser_result)=0;
  (g->print_generic_parser_result)=0;
  
  (g->print_initial_active_list)=0;
  (g->print_initial_passive_list)=0;
  
  (g->print_given_interval_trace)=0;
  (g->print_initial_given_cl)=0;
  (g->print_final_given_cl)=0;
  (g->print_active_cl)=0;
  (g->print_partial_derived_cl)=0;
  (g->print_derived_cl)=0;
  (g->print_derived_subsumed_cl)=0;
  (g->print_derived_precut_cl)=0;

  (g->print_clause_detaillevel)=0;
  (g->print_stats)=0;
  
}

static void wr_set_low_printout(glb* g) {
  (g->print_flag)=1;
  
  (g->parser_print_level)=0;
  (g->print_initial_parser_result)=0;
  (g->print_generic_parser_result)=0;
  
  (g->print_initial_active_list)=0;
  (g->print_initial_passive_list)=0;
  
  (g->print_given_interval_trace)=1;
  (g->print_initial_given_cl)=0;
  (g->print_final_given_cl)=0;
  (g->print_active_cl)=0;
  (g->print_partial_derived_cl)=0;
  (g->print_derived_cl)=0;
  (g->print_derived_subsumed_cl)=0;
  (g->print_derived_precut_cl)=0;
  
  (g->print_clause_detaillevel)=1;
  (g->print_stats)=1;
  
}

static void wr_set_normal_printout(glb* g) {
  (g->print_flag)=1;
  
  (g->parser_print_level)=0;
  (g->print_initial_parser_result)=0;
  (g->print_generic_parser_result)=0;
  
  (g->print_initial_active_list)=0;
  (g->print_initial_passive_list)=0;
  
  (g->print_given_interval_trace)=0;
  (g->print_initial_given_cl)=0;
  (g->print_final_given_cl)=1;
  (g->print_active_cl)=0;
  (g->print_partial_derived_cl)=0;
  (g->print_derived_cl)=0;
  (g->print_derived_subsumed_cl)=0;
  (g->print_derived_precut_cl)=0;
  
  (g->print_clause_detaillevel)=1;
  (g->print_stats)=1;
  
}  

static void wr_set_medium_printout(glb* g) {
  (g->print_flag)=1;
  
  (g->parser_print_level)=0;
  (g->print_initial_parser_result)=0;
  (g->print_generic_parser_result)=0;
  
  (g->print_initial_active_list)=1;
  (g->print_initial_passive_list)=1;
  
  (g->print_given_interval_trace)=0;
  (g->print_initial_given_cl)=1;
  (g->print_final_given_cl)=1;
  (g->print_active_cl)=0;
  (g->print_partial_derived_cl)=0;
  (g->print_derived_cl)=1;
  (g->print_derived_subsumed_cl)=0;
  (g->print_derived_precut_cl)=0;
  
  (g->print_clause_detaillevel)=1;
  (g->print_stats)=1;
  
}

static void wr_set_detailed_printout(glb* g) {
  (g->print_flag)=1;
  
  (g->parser_print_level)=1;
  (g->print_initial_parser_result)=1;
  (g->print_generic_parser_result)=1;
  
  (g->print_initial_active_list)=1;
  (g->print_initial_passive_list)=1;
  
  (g->print_initial_given_cl)=1;
  (g->print_final_given_cl)=1;
  (g->print_active_cl)=1;
  (g->print_partial_derived_cl)=1;
  (g->print_derived_cl)=1;
  (g->print_derived_subsumed_cl)=1;
  (g->print_derived_precut_cl)=1;
  
  (g->print_clause_detaillevel)=1;
  (g->print_stats)=1;
  
}

/* -----------------------------------------------

   Some funs for statistics 
   ----------------------------------------------- */

void wr_show_stats(glb* g) {
  
  if (!(g->print_stats)) return;
  
  printf("\nstatistics:\n");
  printf("----------------------------------\n");
  printf("this run seconds: %f\n",
    (float)(clock() - (g->run_start_clock)) / (float)CLOCKS_PER_SEC);
  printf("total seconds: %f\n",
    (float)(clock() - (g->allruns_start_clock)) / (float)CLOCKS_PER_SEC);
  printf("stat_given_used: %d\n",g->stat_given_used);
  printf("stat_given_candidates: %d\n",g->stat_given_candidates); 
  //printf("stat_derived_cl: %d\n",g->stat_derived_cl);
  printf("stat_binres_derived_cl: %d\n",g->stat_binres_derived_cl);
  printf("stat_propagated_derived_cl: %d\n",g->stat_propagated_derived_cl);
  printf("stat_factor_derived_cl: %d\n",g->stat_factor_derived_cl);
  printf("stat_para_derived_cl: %d\n",g->stat_para_derived_cl);
  printf("stat_tautologies_discarded: %d\n",g->stat_tautologies_discarded);
  printf("stat_forward_subsumed: %d\n",g->stat_forward_subsumed);   
  printf("stat_derived_cut: %d\n",g->stat_derived_cut);  
  printf("stat_weight_discarded_building: %d\n",g->stat_weight_discarded_building);
  printf("stat_weight_discarded_cl: %d\n",g->stat_weight_discarded_cl);
  printf("stat_internlimit_discarded_cl: %d\n",g->stat_internlimit_discarded_cl);
  printf("stat_simplified:  %d simplified %d derived %d given\n",
         g->stat_simplified, g->stat_simplified_derived, g->stat_simplified_given);  
  printf("stat_kept_cl: %d\n",g->stat_kept_cl);       
  printf("stat_built_cl: %d\n",g->stat_built_cl); 
  printf("stat_hyperres_partial_cl: %d\n",g->stat_hyperres_partial_cl);
  printf("stat_backward_subsumed: %d\n",g->stat_backward_subsumed);  
  printf("stat_propagated_subsumed: %d\n",g->stat_propagated_subsumed);
  
#ifdef SHOW_SUBSUME_STATS  
  printf("stat_clsubs_attempted:           %15d\n",g->stat_clsubs_attempted);  
  printf("stat_clsubs_fact_groundunit_found: %13d\n",g->stat_clsubs_fact_groundunit_found);
  printf("stat_clsubs_rule_groundunit_found: %13d\n",g->stat_clsubs_rule_groundunit_found);
  printf("stat_clsubs_top_meta_attempted:    %15ld\n",g->stat_clsubs_top_meta_attempted);
  printf("stat_clsubs_top_meta_failed:       %15ld\n",g->stat_clsubs_top_meta_failed);

  printf("stat_clsubs_top_meta_nonpref_attempted:    %15ld\n",g->stat_clsubs_top_meta_nonpref_attempted);
  printf("stat_clsubs_top_meta_nonpref_succeeded:    %15ld\n",g->stat_clsubs_top_meta_nonpref_succeeded);
  printf("stat_clsubs_top_meta_pref_attempted:       %15ld\n",g->stat_clsubs_top_meta_pref_attempted);
  printf("stat_clsubs_top_meta_pref_succeeded:       %15ld\n",g->stat_clsubs_top_meta_pref_succeeded);
  printf("stat_clsubs_top_meta_pref1_succeeded:       %15ld\n",g->stat_clsubs_top_meta_pref1_succeeded);
  printf("stat_clsubs_top_meta_pref2_succeeded:       %15ld\n",g->stat_clsubs_top_meta_pref2_succeeded);
  printf("stat_clsubs_top_meta_pref3_succeeded:       %15ld\n",g->stat_clsubs_top_meta_pref3_succeeded);

  printf("stat_clsubs_meta_attempted:      %15d\n",g->stat_clsubs_meta_attempted);
  printf("stat_clsubs_meta_failed:         %15d\n",g->stat_clsubs_meta_failed);
  printf("stat_clsubs_predsymbs_attempted: %15d\n",g->stat_clsubs_predsymbs_attempted);
  printf("stat_clsubs_unit_attempted:      %15d\n",g->stat_clsubs_unit_attempted);
  printf("stat_clsubs_full_attempted:      %15d\n",g->stat_clsubs_full_attempted);
  printf("stat_forwardsubs_attempted:      %15d\n",g->stat_forwardsubs_attempted);
#endif 
#ifdef SHOW_HASH_CUT_STATS  
  printf("stat_lit_hash_added:        %15d\n",g->stat_lit_hash_added);
  printf("stat_lit_hash_computed:     %15d\n",g->stat_lit_hash_computed);  
  printf("stat_lit_hash_match_found:  %15d\n",g->stat_lit_hash_match_found);
  printf("stat_lit_hash_match_miss:   %15d\n",g->stat_lit_hash_match_miss); 
  printf("stat_lit_hash_cut_ok:       %15d\n",g->stat_lit_hash_cut_ok); 
  printf("stat_lit_hash_subsume_ok:   %15d\n",g->stat_lit_hash_subsume_ok); 
#endif
#ifdef SHOW_MEM_STATS
  
  //if ((g->clbuilt)!=(gint)NULL) 
  //  printf("clbuilt els %d used %d\n",
  //         (rotp(g,g->clbuilt))[0],(rotp(g,g->clbuilt))[1]-1);
  if ((g->clqueue)!=(gint)NULL) 
    printf("clqueue els %d used %d\n",
           (int)((rotp(g,g->clqueue))[0]),(int)((rotp(g,g->clqueue))[1]-1));
  if ((g->clactive)!=(gint)NULL) 
    printf("clactive els %d used %d\n",
           (int)((rotp(g,g->clactive))[0]),(int)((rotp(g,g->clactive))[1]-1));
  if ((g->clactivesubsume)!=(gint)NULL) 
    printf("clactivesubsume els %d used %d\n",
           (int)((rotp(g,g->clactivesubsume))[0]),(int)((rotp(g,g->clactivesubsume))[1]-1));           
  //if ((g->clweightqueue)!=(gint)NULL) 
  //  printf("clweightqueue els %d used %d\n",((gptr)(g->clweightqueue))[0],((gptr)(g->clweightqueue))[1]-1);
  
  if ((g->queue_termbuf)!=NULL) 
    printf("queue_termbuf els %d used %d\n",(int)((g->queue_termbuf)[0]),(int)((g->queue_termbuf)[1]-1));
  if ((g->active_termbuf)!=NULL) 
    printf("active_termbuf els %d used %d\n",(int)((g->active_termbuf)[0]),(int)((g->active_termbuf)[1]-1));
  if ((g->varstack)!=NULL) 
    printf("varstack els %d last used %d\n",(int)((g->varstack)[0]),(int)((g->varstack)[1]-1));
  if ((g->given_termbuf)!=NULL) 
    printf("given_termbuf els %d last used %d\n",(int)((g->given_termbuf)[0]),(int)((g->given_termbuf)[1]-1));
  if ((g->simplified_termbuf)!=NULL) 
    printf("simplified_termbuf els %d last used %d\n",(int)((g->simplified_termbuf)[0]),(int)((g->simplified_termbuf)[1]-1));  
  if ((g->derived_termbuf)!=NULL) 
    printf("derived_termbuf els %d last used %d\n",(int)((g->derived_termbuf)[0]),(int)((g->derived_termbuf)[1]-1));

  printf("wr_mallocs: %d\n",(g->stat_wr_mallocs));
  printf("wr_reallocs: %d\n",(g->stat_wr_reallocs));
  printf("wr_frees: %d\n",(g->stat_wr_frees));
  printf("wr_malloc_bytes: %d\n",(g->stat_wr_malloc_bytes));
  printf("wr_realloc_bytes: %d\n",(g->stat_wr_realloc_bytes));

  printf("tmp1: %d\n",(g->tmp1));
  printf("tmp2: %d\n",(g->tmp2));
  printf("tmp3: %d\n",(g->tmp3));
  printf("tmp4: %d\n",(g->tmp4));
  printf("tmp5: %d\n",(g->tmp5));

#endif  
  printf("----------------------------------\n");
}  



#ifdef __cplusplus
}
#endif
