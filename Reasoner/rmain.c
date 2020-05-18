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
#include <sys/wait.h> 
#include <unistd.h> 
#ifdef __linux__
#include <sys/prctl.h>
#endif
#endif

#include "rincludes.h"  
#include "../cjson/cjson.h"
#include "../Db/dballoc.h"  // for ACONST_TRUE/FALSE

#ifdef __cplusplus
extern "C" {
#endif

  
/* ====== Private headers and defs ======== */

static void wr_set_no_printout(glb* g);
static void wr_set_tiny_printout(glb* g);
static void wr_set_low_printout(glb* g);
static void wr_set_normal_printout(glb* g);
static void wr_set_medium_printout(glb* g);

static void wg_show_db_error(void* db);
//void wr_set_detailed_printout(glb* g);  

void show_cur_time(void);

#define SHOW_SUBSUME_STATS
#define SHOW_MEM_STATS
#define SHOW_HASH_CUT_STATS

#define SUBSFLAG_CLAUSE_COUNT_LIMIT 10000 // no subs with active clauses if more active clauses
  
/* ======= Private protos ================ */

//#define DEBUG
//#undef DEBUG

//#define SHOWTIME
#undef SHOWTIME

//#define PRINTERR

//#define SHOW_ADDED
//#define WEIGHT_PREFER_GOALS_ASSUMPTIONS

/* ====== Functions ============== */


  
int wg_run_reasoner(void *db, int argc, char **argv, int informat, char* outfilename, char* guidestr) {
  glb* g;    // our g (globals)
  glb* kb_g; // our copy of the g (globals) of the external shared db 
  glb* rglb=NULL; // ptr to g (globals) of the external shared db 
  void* kb_db;
  void* child_db;
  //void* tmp_db;
  int res=1;
  int default_print_level=10;
  int iter,guideres=0,tmp,propres;
  char* guidebuf=NULL;
  cJSON *guide=NULL;
  int clause_count=0;
  //int have_shared_kb=0; // set to 1 if external shared db present
  char* guidetext;
  int exit_on_proof=1; // set to 0 to clean memory and not call exit at end
  int givenguide=0;
  glb* analyze_g;
  char* filename=NULL;
  int forkslive=0, forkscreated=0, maxforks=2, forknr, err, cpid, i;
  int forkpids[64];

  if (argc<3) {
    //guide=wr_parse_guide_file(argc,argv,&guidebuf);  
    givenguide=0;  
  } else {
    guide=wr_parse_guide_file(argc,argv,&guidebuf);
    givenguide=1;
  }  
  if (guide==NULL && givenguide) {
    if (guidebuf!=NULL) free(guidebuf);
    return -1;
  }
  filename=argv[1];
#ifdef SHOWTIME
  wr_printf("Guide parsed.\n");
  wr_printf("\ndb is %d \n",(int)((gint)db));
  show_cur_time();
#endif
  // kb_db is set to parent db
  kb_db=db_get_kb_db(db);
  if (kb_db) {
    // separate child_db and kb_db
    // from now one var db will point to the parent, i.e. kb_db
#ifdef DEBUG
    wr_printf("\nseparate child_db and kb_db\n");
#endif    
    //have_shared_kb=1;
    child_db=db;
    db=kb_db;    

    rglb=db_rglb(db); // this is the internal g of db
    kb_g=sys_malloc(sizeof(glb)); // this is a new malloced g
    // copy rglb stuff to g
    memcpy(kb_g,rglb,sizeof(glb));
    // now kb_g should contain the same things as rglb
    // set the db ptr of kb_g to the shared db
    (kb_g->db)=kb_db; 
#ifdef DEBUG       
    wr_printf("\n** input stats for kb_g ****** \n");
    show_cur_time();
    wr_show_in_stats(kb_g);
    wr_printf("\n");
#endif   
  } else {

    // just one single db 
#ifdef DEBUG
    printf("\njust one single db \n");
#endif 
    //have_shared_kb=0;
    child_db=db;
    kb_db=db;
    kb_g=NULL;
  }
  analyze_g=wr_glb_new_simple(db);
  if (kb_g!=NULL) {
    (analyze_g->kb_g)=kb_g; // points to the copy of globals of the external kb
  }
  (analyze_g->child_db)=child_db;
  (analyze_g->db)=db; 
  (analyze_g->varbanks)=wr_vec_new(analyze_g,NROF_VARBANKS*NROF_VARSINBANK);
  (analyze_g->varstack)=wr_cvec_new(analyze_g,NROF_VARBANKS*NROF_VARSINBANK); 
  (analyze_g->varstack)[1]=2; // first free elem  
  (analyze_g->in_has_fof)=informat;
  if (outfilename) (analyze_g->outfilename)=outfilename;
  tmp=wr_analyze_clause_list(analyze_g,db,child_db);

  if (!givenguide) {
    if (guidestr!=NULL) {      
      guide=wr_parse_guide_str(guidestr); 
      if (guide==NULL) {  return -1; } 
    } else {
      guidebuf=make_auto_guide(analyze_g,kb_g);     
      guide=wr_parse_guide_str(guidebuf); 
      if (guide==NULL) {  return -1; } 
      //if (guidebuf!=NULL) wr_free(g,(void*)guidebuf);
      if (guidebuf!=NULL) { sys_free(guidebuf); guidebuf=NULL; }
    }
  }
  
    
  int pid=1,stat;
  forkscreated=0;
  forkslive=0;  

  // NB! TODO: find maxforks before wr_parse_guide_section is run

  maxforks=4; //(analyze_g->max_forks); // TESTING
  if (maxforks>64) maxforks=64;
  for(forknr=0; forknr<maxforks; forknr++) {
    pid=fork();
    if (pid<0) {
      // fork fails
      printf("\nerror: fork nr %d fails with pid %d\n",forknr,pid);         
      fflush(stdout);
      break;
    } else if (pid==0) {
      // child
      //printf("\nafter fork nr %d with pid 0 (child)\n",forknr);
#ifdef __linux__      
      // make child to terminate if parent terminates
      tmp = prctl(PR_SET_PDEATHSIG, SIGTERM);
      if (tmp == -1) { printf("\nparent died for fork %d, exiting\n", forknr); exit(1); }
      // test in case the original parent exited just
      // before the prctl() call
      if (getppid() == 1) exit(1);
#endif      
      break;
    } else {
      // parent
      forkpids[forkscreated]=pid;     
      forkscreated++;
      forkslive++;
    }
  }
 
  // forks have been created
  if (pid && forkscreated) {
    // only parent performs this loop
    while(forkslive) {
      cpid=waitpid(-1,&stat, 0);
      //printf("\nwaitpid ended for cpid %d\n",cpid);
      if (WIFEXITED(stat)) {
        //printf("\nWIFEXITED(stat) true for cpid %d\n",cpid);
        err = WEXITSTATUS(stat);
        //printf("Child %d terminated with status: %d\n", cpid, err); 
        forkslive--;  
        // remove cpid from pid array      
        for(i=0;i<forkscreated;i++) {
          if (forkpids[i]==cpid) {
            forkpids[i]=0;
            break;
          }
        }
        if (err==0) {
          // proof found, kill all children
          for(i=0;i<forkscreated;i++) {
            if (forkpids[i]) {
              // kill pid
              kill(forkpids[i], SIGKILL);
            }
          }  
          break;
        }        
      }
    }  
  }


  for(iter=0; 1; iter++) { 
    // check if this process needs to run this iter
    // two forks case:
    // fork 0: 0, 2, 4, 6, ...
    // fork 1: 1, 3, 5, 7, ...
    // three forks case:
    // fork 0: 0, 3, 6, 9, ...
    // fork 1: 1, 4, 7, 10, ... 
    // fork 2: 2, 5, 8, 11, ...

    // parent should not do this loop at all if there are forks
    if (pid && forkscreated) break;
    
    // child should only take some iters and pass others:
    if (!pid && maxforks && ((iter % maxforks)!=forknr)) continue;
        
#ifdef DEBUG    
    wr_printf("\n**** run %d starts\n",iter+1);    
#endif
    fflush(stdout);     
   
    /*
    wg_run_reasoner_onestrat(
      db, pid, outfilename, iter, guide, givenguide, guidebuf, filename,
      kb_db, child_db, kb_g, rglb, analyze_g, &guideres);
    continue;  
    */

    g=wr_glb_new_simple(db);
    // copy analyze_g stats to g for easier handling
    wr_copy_sin_stats(analyze_g,g);    

    if (kb_g!=NULL) {
      (g->kb_g)=kb_g; // points to the copy of globals of the external kb
    }  
    if (g==NULL) {
      wr_errprint("cannot allocate enough memory during reasoner initialization");
      if (guidebuf!=NULL) free(guidebuf);
      if (guide!=NULL) cJSON_Delete(guide);
      sys_free(g); 
      return -1;
    }   
#ifdef DEBUG
    wr_printf("\nnow db is %ld and child_db is %ld \n",(gint)db,(gint)child_db);
#endif    
    (g->child_db)=child_db;
    (g->db)=db;

    (g->current_run_nr)=iter;
    (g->current_fork_nr)=forkslive;
    if (iter==0) (g->allruns_start_clock)=clock();  
    guidetext=NULL;
    guideres=wr_parse_guide_section(g,guide,iter,&guidetext);  

    // (g->cl_maxkeep_depthlimit)=10;  // TESTING
    // (g->res_shortarglen_limit)=3; // TESTING
    // (g->posunitpara_strat)=1; // TESTING
    // (g->use_strong_unit_cutoff)=1; // TESTING
    // (g->pick_given_queue_ratio)=100; // TESTING
    // (g->cl_maxkeep_sizelimit)=30; // TESTING

    if (guideres<0) {
      // error in guide requiring stop      
      if (guidebuf!=NULL) free(guidebuf);
      if (guide!=NULL) cJSON_Delete(guide);
      (g->guidetext)[0]=0;
      sys_free(g);
      return -1;
    } else if (guidetext) {
      tmp=strlen(guidetext);
      if (tmp>=MAX_GUIDETEXT_LEN) tmp=MAX_GUIDETEXT_LEN-2;
      memcpy(g->guidetext,guidetext,tmp+1);
    }
   
    if (!(g->print_flag)) (g->print_level_flag)=0;
    if ((g->print_level_flag)<0) (g->print_level_flag)=default_print_level;
    if ((g->print_level_flag)==0) wr_set_no_printout(g);
    else if ((g->print_level_flag)<=10) wr_set_tiny_printout(g);
    else if ((g->print_level_flag)<=15) wr_set_low_printout(g);
    else if ((g->print_level_flag)<=20) wr_set_normal_printout(g);
    else if ((g->print_level_flag)<=30) wr_set_medium_printout(g);
    else if ((g->print_level_flag)<=50) wr_set_detailed_printout(g);    
    else wr_set_detailed_plus_printout(g);

    if ((g->print_flag) && (g->print_runs)) {      
      if (guidetext!=NULL) {   
        wr_printf("\n**** run %d fork %d starts with strategy\n",iter+1,forkslive); 
        wr_printf("%s\n",guidetext);
        free(guidetext);
      } else {
        wr_printf("\n**** run %d fork %d starts\n",iter+1, forkslive);
      }          
    }  

    (g->cl_keep_weightlimit)=(g->cl_maxkeep_weightlimit);
    (g->cl_keep_sizelimit)=(g->cl_maxkeep_sizelimit);
    (g->cl_keep_depthlimit)=(g->cl_maxkeep_depthlimit);
    (g->cl_keep_lengthlimit)=(g->cl_maxkeep_lengthlimit);   
    tmp=wr_glb_init_shared_complex(g); // creates and fills in shared tables, substructures, etc: 0.03 secs
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
    strncpy((g->filename),filename,MAX_FILENAME_LEN);
    if (outfilename) (g->outfilename)=outfilename;    
#ifdef SHOWTIME    
    wr_printf("\nto call wr_init_active_passive_lists_from_all\n");
    show_cur_time();
#endif    
    // if two db-s, this will take the clauses from the shared db     
    clause_count=0;
    if (db!=child_db) {
      // two separate db-s
#ifdef DEBUG
      wr_printf("\n *** separate (local) child kb found, using\n");
#endif      
      // analyze local clauses; have to temporarily replace g->db to child
      /*
      tmp_db=g->db;
      g->db=child_db;
      */
      //tmp=wr_analyze_clause_list2(g,tmp_db,(dbmemsegh(g->child_db))->clauselist);

      //tmp=wr_analyze_clause_list(g,db,child_db);
      //
      //if (!givenguide) {
      //  make_auto_guide(g);
      //  guide=wr_parse_guide_str(buf);
      //}  

      //g->db=tmp_db;

      // wr_show_in_stats(g);  // show local stats
      // wr_show_in_stats(rglb); // show global stats
#ifdef DEBUG
      wr_printf("\n** input stats for local g ****** \n");
      show_cur_time();
      wr_show_in_stats(g);
      wr_printf("\n"); 
      //exit(0);
#endif
      if (!(g->queryfocus_strat)) {
        // for non-queryfocus_strat read the clauses from the 
        // external shared mem db and do not use the
        // indexes present there
        //printf("\n(g->initial_cl_list) is %ld\n",(gint)((r_kb_g(g))->initial_cl_list));
#ifdef DEBUG 
        wr_printf("\n**** starting to read from the external shared mem kb\n");
#endif              
        clause_count+=wr_init_active_passive_lists_from_one(g,db,db);        
        (g->kb_g)=NULL;        
      } else {
        // queryfocus case

        //CP0
        //printf("\n (g->in_goal_count %d\n",(g->in_goal_count));

        // if no goals, set negative-or-ans-into passive initialization strat
        if (!(g->in_goal_count)) {
          (g->queryfocusneg_strat)=1;
        } else {
          (g->queryfocusneg_strat)=0;
        }
       /// ----
      }
      // if no equality, do not initialize indexes and do not try to use
      
      if (!(g->in_poseq_clause_count) && !(rglb->in_poseq_clause_count)) (g->use_equality)=0;
      else (g->use_equality)=(g->use_equality_strat);
      
      // if two db-s, this will take the clauses from the local db:
#ifdef DEBUG 
      wr_printf("\n**** starting to read from the local db kb\n");
#endif      
      clause_count+=wr_init_active_passive_lists_from_one(g,db,child_db);      
    } else {
      // one single db      
#ifdef DEBUG 
        wr_printf("\n external kb NOT found\n");      
        wr_printf("\n** starting to calc input stats for g ****** \n");
        show_cur_time();
#endif          
        //tmp=wr_analyze_clause_list(g,db,db);
        //if (!tmp) return -1;
#ifdef DEBUG
        wr_printf("\n** input stats for g ****** \n");
        show_cur_time();
        wr_show_in_stats(g);
        wr_printf("\n");   
#endif           

      // if no goals, set negative-or-ans-into passive initialization strat
      if (!(g->in_goal_count)) {
        (g->queryfocusneg_strat)=1;
      } else {
        (g->queryfocusneg_strat)=0;
      }
      // if no equality, do not initialize indexes and do not try to use
      
      if (!(g->in_poseq_clause_count)) (g->use_equality)=0;
      else (g->use_equality)=(g->use_equality_strat);
      
      /// ----
#ifdef DEBUG       
      wr_printf("\n**** starting to read from the single local db\n");      
#endif     
      clause_count=wr_init_active_passive_lists_from_one(g,db,db);      
    } 

#ifdef SHOWTIME     
    wr_printf("\nreturned from wr_init_active_passive_lists_from_all\n");
    show_cur_time();
#endif    
    if (clause_count<0) {
      // error
      wr_printf("\nError initializing clause lists.\n");
      wr_glb_free(g);
      break;
    } else if (!clause_count) {
      // no clauses found
      wr_printf("\nNo clauses found.\n");
      wr_glb_free(g);
      break;
    }
    // ok, clauses found and clause lists initialized
    //(g->kb_g)=NULL;
    //printf("\n(g->use_equality) %d (g->use_equality_strat) %d\n",(g->use_equality),(g->use_equality_strat));
     
    //wr_show_in_stats(g);

    //wr_print_clpick_queues(g,(g->clpick_queues));
    
    //if ((g->print_flag) && (g->print_runs)) wr_print_strat_flags(g);

    if (!(g->proof_found) || !(wr_enough_answers(g))) {
      res=wr_genloop(g);
      //printf("\ngenloop ended\n");
      //printf("\n prop_hash_clauses:\n");  
      //wr_print_prop_clausehash(g,rotp(g,g->prop_hash_clauses));  

      if (res>0 && !(res==1 && wr_have_answers(g)) &&
          ((g->instgen_strat) || (g->propgen_strat))) {
        propres=wr_prop_solve_current(g);
        if (propres==2) {
          res=0;
        } 
      }      
    } else {
      res=0;
    }
    if (res>=0 && !(g->parse_errflag) && !(g->alloc_err) && db && (dbmemsegh(db)->errflag)) {
      wg_show_db_error(db);
      res=0-2;
    }

    //printf("\nwr_genloop exited, showing database details\n");
    //wr_show_database_details(g,NULL,"local g");
    //printf("\n res is %d\n",res);
    if (res==0) {
      //wr_printf("\n\nProof found.\n"); 
      wr_show_result(g,g->proof_history);
    } else if (res==1) {
      if (wr_have_answers(g)) {
        wr_show_result(g,g->proof_history);
      } else {
        if ((g->print_level_flag)>5) {
          wr_printf("\n\nfork %d: search finished without proof, result code %d.\n",g->current_fork_nr,res); 
        }  
      }  
    } else if (res==2 && (g->print_runs)) {
      wr_printf("\n\nfork %d: search terminated without proof.\n",g->current_fork_nr);        
    } else if (res==-1) {
#ifdef PRINTERR       
      wr_printf("\n\nfork %d: search cancelled: memory overflow.\n",g->current_fork_nr);
#endif      
    } else if (res<0) {
#ifdef PRINTERR       
      wr_printf("\n\nfork %d: search cancelled, error code %d.\n",g->current_fork_nr,res);
#endif      
    }      
    //if (g->print_flag) wr_show_stats(g,1);

    if (res==0) {
      // proof found 
      if (g->print_flag) wr_show_stats(g,1);
      if (exit_on_proof) {
#ifdef SHOWTIME       
        wr_printf("\nexiting\n");
        show_cur_time();  
#endif              
        if (outfilename) {
          wr_glb_free(g);
          return(0);
        } else {
          return(0);
        }  
      }
#ifdef SHOWTIME       
      wr_printf("\nto call wr_glb_free\n");
      show_cur_time();  
#endif            
      wr_glb_free(g);
#ifdef SHOWTIME       
      wr_printf("\nwr_glb_free returns\n");
      show_cur_time(); 
#endif      
      break;
    }
    // no proof found here
    wr_glb_free(g); 
    if ((iter+1)>=guideres) break;      
  }

  if (guidebuf!=NULL) free(guidebuf);
  if (guide!=NULL) cJSON_Delete(guide);  
#ifdef SHOWTIME       
  wr_printf("\nto return from rmain\n");
  show_cur_time(); 
#endif  
  return res;  
} 




/*  
int wg_run_reasoner_onestrat(
  void* db, int pid, char* outfilename, int iter, cJSON *guide, int givenguide, char *guidebuf, char* filename,
  void* kb_db, void *child_db, glb *kb_g, glb *rglb, glb* analyze_g, int* guideres)  {

  glb* g;
  int res=1;
  int default_print_level=10;
  int tmp,propres;
  int clause_count=0;
  char* guidetext;
  int exit_on_proof=1; // set to 0 to clean memory and not call exit at end


    g=wr_glb_new_simple(db);
    // copy analyze_g stats to g for easier handling
    wr_copy_sin_stats(analyze_g,g);    

    if (kb_g!=NULL) {
      (g->kb_g)=kb_g; // points to the copy of globals of the external kb
    }  
    if (g==NULL) {
      wr_errprint("cannot allocate enough memory during reasoner initialization");
      if (guidebuf!=NULL) free(guidebuf);
      if (guide!=NULL) cJSON_Delete(guide);
      sys_free(g); 
      return -1;
    }   
#ifdef DEBUG
    wr_printf("\nnow db is %ld and child_db is %ld \n",(gint)db,(gint)child_db);
#endif    
    (g->child_db)=child_db;
    (g->db)=db;

    (g->current_run_nr)=iter;
    if (iter==0) (g->allruns_start_clock)=clock();  
    guidetext=NULL;
    *guideres=wr_parse_guide_section(g,guide,iter,&guidetext);
    if (*guideres<0) {
      // error in guide requiring stop      
      if (guidebuf!=NULL) free(guidebuf);
      if (guide!=NULL) cJSON_Delete(guide);
      sys_free(g);
      return -1;
    }
   
    if (!(g->print_flag)) (g->print_level_flag)=0;
    if ((g->print_level_flag)<0) (g->print_level_flag)=default_print_level;
    if ((g->print_level_flag)==0) wr_set_no_printout(g);
    else if ((g->print_level_flag)<=10) wr_set_tiny_printout(g);
    else if ((g->print_level_flag)<=15) wr_set_low_printout(g);
    else if ((g->print_level_flag)<=20) wr_set_normal_printout(g);
    else if ((g->print_level_flag)<=30) wr_set_medium_printout(g);
    else if ((g->print_level_flag)<=50) wr_set_detailed_printout(g);    
    else wr_set_detailed_plus_printout(g);

    if ((g->print_flag) && (g->print_runs)) {      
      if (guidetext!=NULL) {   
        wr_printf("\n**** run %d pid %d starts with strategy\n",iter+1,pid); 
        wr_printf("%s\n",guidetext);
        free(guidetext);
      } else {
        wr_printf("\n**** run %d pid %d starts\n",iter+1,pid);
      }          
    }  

    (g->cl_keep_weightlimit)=(g->cl_maxkeep_weightlimit);
    (g->cl_keep_sizelimit)=(g->cl_maxkeep_sizelimit);
    (g->cl_keep_depthlimit)=(g->cl_maxkeep_depthlimit);
    (g->cl_keep_lengthlimit)=(g->cl_maxkeep_lengthlimit);   
    tmp=wr_glb_init_shared_complex(g); // creates and fills in shared tables, substructures, etc: 0.03 secs
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
    strncpy((g->filename),filename,MAX_FILENAME_LEN);
    if (outfilename) (g->outfilename)=outfilename;    
#ifdef SHOWTIME    
    wr_printf("\nto call wr_init_active_passive_lists_from_all\n");
    show_cur_time();
#endif    
    // if two db-s, this will take the clauses from the shared db     
    clause_count=0;
    if (db!=child_db) {
      // two separate db-s
#ifdef DEBUG
      wr_printf("\n *** separate (local) child kb found, using\n");
#endif      
      // analyze local clauses; have to temporarily replace g->db to child
     
      //tmp_db=g->db;
      //g->db=child_db;
      
      //tmp=wr_analyze_clause_list2(g,tmp_db,(dbmemsegh(g->child_db))->clauselist);

      //tmp=wr_analyze_clause_list(g,db,child_db);
      //
      //if (!givenguide) {
      //  make_auto_guide(g);
      //  guide=wr_parse_guide_str(buf);
      //}  

      //g->db=tmp_db;

      // wr_show_in_stats(g);  // show local stats
      // wr_show_in_stats(rglb); // show global stats
#ifdef DEBUG
      wr_printf("\n** input stats for local g ****** \n");
      show_cur_time();
      wr_show_in_stats(g);
      wr_printf("\n"); 
      //exit(0);
#endif
      if (!(g->queryfocus_strat)) {
        // for non-queryfocus_strat read the clauses from the 
        // external shared mem db and do not use the
        // indexes present there
        //printf("\n(g->initial_cl_list) is %ld\n",(gint)((r_kb_g(g))->initial_cl_list));
#ifdef DEBUG 
        wr_printf("\n**** starting to read from the external shared mem kb\n");
#endif              
        clause_count+=wr_init_active_passive_lists_from_one(g,db,db);        
        (g->kb_g)=NULL;        
      } else {
        // queryfocus case

        //CP0
        //printf("\n (g->in_goal_count %d\n",(g->in_goal_count));

        // if no goals, set negative-or-ans-into passive initialization strat
        if (!(g->in_goal_count)) {
          (g->queryfocusneg_strat)=1;
        } else {
          (g->queryfocusneg_strat)=0;
        }
       /// ----
      }
      // if no equality, do not initialize indexes and do not try to use
      
      if (!(g->in_poseq_clause_count) && !(rglb->in_poseq_clause_count)) (g->use_equality)=0;
      else (g->use_equality)=(g->use_equality_strat);
      
      // if two db-s, this will take the clauses from the local db:
#ifdef DEBUG 
      wr_printf("\n**** starting to read from the local db kb\n");
#endif      
      clause_count+=wr_init_active_passive_lists_from_one(g,db,child_db);      
    } else {
      // one single db      
#ifdef DEBUG 
        wr_printf("\n external kb NOT found\n");      
        wr_printf("\n** starting to calc input stats for g ****** \n");
        show_cur_time();
#endif          
        //tmp=wr_analyze_clause_list(g,db,db);
        //if (!tmp) return -1;
#ifdef DEBUG
        wr_printf("\n** input stats for g ****** \n");
        show_cur_time();
        wr_show_in_stats(g);
        wr_printf("\n");   
#endif           

      // if no goals, set negative-or-ans-into passive initialization strat
      if (!(g->in_goal_count)) {
        (g->queryfocusneg_strat)=1;
      } else {
        (g->queryfocusneg_strat)=0;
      }
      // if no equality, do not initialize indexes and do not try to use
      
      if (!(g->in_poseq_clause_count)) (g->use_equality)=0;
      else (g->use_equality)=(g->use_equality_strat);
      
      /// ----
#ifdef DEBUG       
      wr_printf("\n**** starting to read from the single local db\n");      
#endif     
      clause_count=wr_init_active_passive_lists_from_one(g,db,db);      
    } 

#ifdef SHOWTIME     
    wr_printf("\nreturned from wr_init_active_passive_lists_from_all\n");
    show_cur_time();
#endif    
    if (clause_count<0) {
      // error
      wr_printf("\nError initializing clause lists.\n");
      wr_glb_free(g);
      return 10;
    } else if (!clause_count) {
      // no clauses found
      wr_printf("\nNo clauses found.\n");
      wr_glb_free(g);
      return 10;
    }
    // ok, clauses found and clause lists initialized
    //(g->kb_g)=NULL;
    //printf("\n(g->use_equality) %d (g->use_equality_strat) %d\n",(g->use_equality),(g->use_equality_strat));
     
    //wr_show_in_stats(g);

    //wr_print_clpick_queues(g,(g->clpick_queues));
    
    //if ((g->print_flag) && (g->print_runs)) wr_print_strat_flags(g);

    if (!(g->proof_found) || !(wr_enough_answers(g))) {
      res=wr_genloop(g);
      //printf("\ngenloop ended\n");
      //printf("\n prop_hash_clauses:\n");  
      //wr_print_prop_clausehash(g,rotp(g,g->prop_hash_clauses));  

      if (res>0 && !(res==1 && wr_have_answers(g)) &&
          ((g->instgen_strat) || (g->propgen_strat))) {
        propres=wr_prop_solve_current(g);
        if (propres==2) {
          res=0;
        } 
      }      
    } else {
      res=0;
    }
    if (res>=0 && !(g->parse_errflag) && !(g->alloc_err) && db && (dbmemsegh(db)->errflag)) {
      wg_show_db_error(db);
      res=0-2;
    }

    //printf("\nwr_genloop exited, showing database details\n");
    //wr_show_database_details(g,NULL,"local g");
    //printf("\n res is %d\n",res);
    if (res==0) {
      //wr_printf("\n\nProof found.\n"); 
      wr_show_result(g,g->proof_history);
    } else if (res==1) {
      if (wr_have_answers(g)) {
        wr_show_result(g,g->proof_history);
      } else {
        if ((g->print_level_flag)>5) {
          wr_printf("\n\nSearch finished without proof, result code %d.\n",res); 
        }  
      }  
    } else if (res==2 && (g->print_runs)) {
      wr_printf("\n\nSearch terminated without proof.\n");        
    } else if (res==-1) {
#ifdef PRINTERR       
      wr_printf("\n\nSearch cancelled: memory overflow.\n");
#endif      
    } else if (res<0) {
#ifdef PRINTERR       
      wr_printf("\n\nSearch cancelled, error code %d.\n",res);
#endif      
    }      
    //if (g->print_flag) wr_show_stats(g,1);

    if (res==0) {
      // proof found 
      if (g->print_flag) wr_show_stats(g,1);
      if (exit_on_proof) {
#ifdef SHOWTIME       
        wr_printf("\nexiting\n");
        show_cur_time();  
#endif              
        if (outfilename) {
          wr_glb_free(g);
          return(0);
        } else {
          wr_glb_free(g); // ???? added while splitting fun
          return(0);
        }  
      }
#ifdef SHOWTIME       
      wr_printf("\nto call wr_glb_free\n");
      show_cur_time();  
#endif            
      wr_glb_free(g);
#ifdef SHOWTIME       
      wr_printf("\nwr_glb_free returns\n");
      show_cur_time(); 
#endif      
      return 20;
    }
  return res;  
} 
*/

int wg_import_otter_file(void *db, char* filename, int iskb, int* informat) {  
  glb* g;
  int res;

  //db_printf("wg_import_otterfile starts for file %s\n",filename); 

  g=wr_glb_new_simple(db); // no complex values given to glb elements 
  if (g==NULL) return 1;   
  (g->parser_print_level)=0;
  (g->print_initial_parser_result)=0;
  (g->print_generic_parser_result)=1;
  res=wr_import_otter_file(g,filename,NULL,NULL,iskb);
  if (g->parse_errmsg) {
    printf("%s\n",(g->parse_errmsg));
    sys_free(g->parse_errmsg);
    //res=1;
  } else if (dbmemsegh(db)->errflag) {
    wg_show_db_error(db);
    res=1;
  }
  /*
  if (res==1) {  
    if (0) { //} ((g->print_flag) &&  (g->print_level_flag)) {
      db_printf("No clauses generated by parser.\n");  
    }
  } else {
    //wg_mpool_print(db,pres2);
    if (0) { //((g->print_flag) && (g->print_generic_parser_result)>0) {    
      db_printf("\nGeneric parser result:\n");
      //wr_print_db_otter(g,(g->print_clause_detaillevel));
    }  
  } 
  */
  if (g->in_has_fof) *informat=1;
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


int wr_init_active_passive_lists_from_one(glb* g, void* db, void* child_db) {
  void *rec=NULL;
  //void* db=(g->db);
  int i=0,j;
  gint clmeta;
  gptr given_cl_as_active, given_cl;
  gint given_cl_metablock[CLMETABLOCK_ELS];
  int rules_found=0;
  int facts_found=0;
  gint weight;
  gint cell=0;
  gcell *cellptr=NULL;
  // for reversing order:
  gcell *cellptr2; 
  gint cell2,lastcell;
  int n=0;
  int size,depth,length,tmp,subsflag;
  int vecflag=0; // set to 1 if clauses sorted into vector

  //printf("\n ** wr_init_active_passive_lists_from_one called ** \n");
  (g->proof_found)=0;
  wr_clear_all_varbanks(g); 
  wr_process_given_cl_setupsubst(g,g->given_termbuf,1,1); 

  //wr_show_in_summed_stats(g);
  //printf("\ng->in_clause_count %d g->sin_clause_count %d\n",
  //       g->in_clause_count,g->sin_clause_count);

  // Reverse the order
    
  if (!(g->queryfocus_strat)) {
    //printf("\nstarting to build a clause vector:\n");
    vecflag=1;
    cell2=(dbmemsegh(child_db)->clauselist);
    lastcell=0;
    n=0;
    while(cell2) {
      cellptr2=(gcell *) offsettoptr(child_db, cell2);
      rec=offsettoptr(child_db,cellptr2->car); 
      //printf("\nn: %d ",n);
      //wr_print_clause(g,rec); 
      j=(n*2)+1;  
      weight=wr_calc_clause_weight(g,rec,&size,&depth,&length); 
#ifdef WEIGHT_PREFER_GOALS_ASSUMPTIONS      
      if (wr_cl_is_goal(g, rec)) weight=weight*1;
      else if (wr_cl_is_assumption(g, rec)) weight=weight*1000;
      else weight=weight*10000;
#else
      if (g->cl_pick_queue_strategy) {
        if (wr_cl_is_goal(g, rec)) weight=weight*1;
        else if (wr_cl_is_assumption(g, rec)) weight=weight*1000;
        else weight=weight*10000;
      }
#endif      
      (g->tmp_sort_vec)=wr_vec_store(g,g->tmp_sort_vec,j,(gint)weight);
      (g->tmp_sort_vec)=wr_vec_store(g,g->tmp_sort_vec,j+1,(gint)rec);
      cell2=cellptr2->cdr;         
      n++;
    }       
    // sort    
    if (!(g->reverse_clauselist_strat)) {
      wr_qsort_metaclvec(g,(g->tmp_sort_vec),0,n-1);
    }      
  } else {
    lastcell=(dbmemsegh(child_db)->clauselist);
  }  
  
  if (!vecflag) cell=lastcell;
  else if (g->reverse_clauselist_strat) i=n-1;
  else i=0;

  while(1) { 
    // take rec
    if (vecflag) {
      if (g->reverse_clauselist_strat) {
        if (i<0) break;
      } else {
        if (i>=n) break;
      }  
      rec=(gptr)((g->tmp_sort_vec)[(i*2)+2]);
    } else {      
      if (!cell) break;
      cellptr=(gcell *) offsettoptr(child_db, cell);
      rec=offsettoptr(child_db,cellptr->car);
    } 
    if (g->alloc_err) {
      wr_errprint("\nbuffer overflow, terminating\n");
      //wr_show_stats(g,1);      
      //exit(0);
      return 0;
    }     
    
    //wr_print_clause(g,rec);
    //printf("\n");
#ifdef DEBUG   
    wr_printf("\n next rec from db: ");
    wg_print_record(db,rec);
    wr_printf("\n");    
#endif
     
    if (wg_rec_is_rule_clause(db,rec)) {
      rules_found++;
      clmeta=wr_calc_clause_meta(g,rec,given_cl_metablock);
      wr_add_cl_to_unithash(g,rec,clmeta);

#ifdef DEBUG      
      wr_printf("\n+++++++ nrec is a rule  "); 
      wr_print_rule_clause_otter(g, (gint *) rec,(g->print_clause_detaillevel));
      wr_printf("\n"); 

      wr_printf("\nraw clause: ");
      wg_print_record(db,rec);
      wr_printf("\n");
      wr_print_clause(g,rec);
      wr_printf("\n");
      
      wr_printf("\nhash_neg_groundunits\n");
      wr_print_termhash(g,rotp(g,g->hash_neg_groundunits));
      wr_printf("\nhash_pos_groundunits\n");
      wr_print_termhash(g,rotp(g,g->hash_pos_groundunits));
#endif       

      if (g->queryfocus_strat && wr_initial_select_active_cl(g,(gptr)rec)) {   
        // calculate resolvability: (g->tmp_resolvability_vec)
        wr_calc_clause_resolvability(g,rec,1,0);      
        given_cl=wr_process_given_cl(g,(gptr)rec, g->given_termbuf);
        if ( ((gint)given_cl==ACONST_FALSE) || ((gint)given_cl==ACONST_TRUE) ||
             (given_cl==NULL) ) {          
          cell=cellptr->cdr;   
          continue;
        };
        wr_sort_cl(g,given_cl);      
        if ((g->sin_clause_count) > SUBSFLAG_CLAUSE_COUNT_LIMIT) subsflag=0;
        else subsflag=1;
        given_cl_as_active=wr_add_given_cl_active_list
                             (g,given_cl,given_cl_metablock,subsflag,
                              g->active_termbuf,(g->tmp_resolvability_vec));                           
        if (given_cl_as_active==NULL) {
          //if (g->alloc_err) return -1;
          cell=cellptr->cdr;
          continue; 
        }    
        if (g->use_strong_unit_cutoff) wr_cl_store_res_units(g,given_cl_as_active);
      } else {       
        if (0) { //(!(g->cl_pick_queue_strategy)) {
          weight=wr_calc_clause_weight(g,rec,&size,&depth,&length);
        } else {  
          if (wr_cl_is_goal(g, rec)) weight=1;
          else if (wr_cl_is_assumption(g, rec)) weight=2;
          //else weight=-1;
          //else weight=-1; //wr_calc_clause_weight(g,rec,&size,&depth,&length);
          else weight=wr_calc_clause_weight(g,rec,&size,&depth,&length);
        }
        //weight=-1;
#ifdef SHOW_ADDED
        wr_printf("\nadding rulecl used weight %ld real weight %ld assumptionflag %d goalflag %d\n",
          weight,
          wr_calc_clause_weight(g,rec,&size,&depth,&length),
          wr_cl_is_assumption(g,rec),
          wr_cl_is_goal(g,rec));
        wr_print_clause(g,rec);          
#endif
        if (g->use_strong_unit_cutoff) wr_cl_store_res_units(g,rec);
        wr_push_cl_clpick_queues(g,(g->clpick_queues),rec,weight); // -1 means we do not know weight
        tmp=wr_cl_create_propinst(g,rec);
        if (tmp==2) {
          return rules_found+facts_found;   
        }
      } 

    } else if (wg_rec_is_fact_clause(db,rec)) {
      facts_found++;
      clmeta=wr_calc_clause_meta(g,rec,given_cl_metablock);
      wr_add_cl_to_unithash(g,rec,clmeta);
#ifdef DEBUG
      wr_printf("\n+++++++ nrec is a fact  ");
      wr_print_fact_clause_otter(g, (gint *) rec,(g->print_clause_detaillevel));
      wr_printf("\n"); 

      wr_printf("\nraw clause: ");
      wg_print_record(db,rec);
      wr_printf("\n");
      wr_print_clause(g,rec);
      wr_printf("\n");
     
      wr_printf("\nhash_neg_groundunits\n");
      wr_print_termhash(g,rotp(g,g->hash_neg_groundunits));
      wr_printf("\nhash_pos_groundunits\n");
      wr_print_termhash(g,rotp(g,g->hash_pos_groundunits));
#endif      
      if (g->queryfocus_strat && wr_initial_select_active_cl(g,(gptr)rec)) {
        wr_calc_clause_resolvability(g,rec,1,0);
        given_cl=wr_process_given_cl(g,(gptr)rec,g->given_termbuf);
        if ( ((gint)given_cl==ACONST_FALSE) || ((gint)given_cl==ACONST_TRUE) ||
             (given_cl==NULL) ) {
          cell=cellptr->cdr;    
          continue;
        }
        subsflag=1;
        given_cl_as_active=wr_add_given_cl_active_list
                            (g,given_cl,given_cl_metablock,subsflag,
                            g->active_termbuf,(g->tmp_resolvability_vec));                        
        if (given_cl_as_active==NULL) {
          if (g->alloc_err) return -1;
        }    
        if (g->use_strong_unit_cutoff) wr_cl_store_res_units(g,given_cl_as_active);
      } else {
        if (0) { // (!(g->cl_pick_queue_strategy)) {
          weight=wr_calc_clause_weight(g,rec,&size,&depth,&length);
        } else {  
          if (wr_cl_is_goal(g, rec)) weight=1;
          else if (wr_cl_is_assumption(g, rec)) weight=2;
          //else weight=-1;
          //else weight=-1; //wr_calc_clause_weight(g,rec,&size,&depth,&length);
          //else weight=-1; //wr_calc_clause_weight(g,rec,&size,&depth,&length);
          else weight=wr_calc_clause_weight(g,rec,&size,&depth,&length);
        }        
        //weight=-1;
#ifdef SHOW_ADDED
        wr_printf("\nadding factcl used weight %ld real weight %ld assumptionflag %d goalflag %d\n",
          weight,
          wr_calc_clause_weight(g,rec,&size,&depth,&length),
          wr_cl_is_assumption(g,rec),
          wr_cl_is_goal(g,rec));
        wr_print_clause(g,rec);          
#endif        
        if (g->use_strong_unit_cutoff) wr_cl_store_res_units(g,rec);
        wr_push_cl_clpick_queues(g,(g->clpick_queues),rec,weight); // -1 means we do not know weight
        tmp=wr_cl_create_propinst(g,rec);
        if (tmp==2) {
          return rules_found+facts_found;   
        }
      }
      
    }  else {
#ifdef DEBUG            
      wr_printf("\nrec is not a rule nor a fact: "); 
      wg_print_record(db,rec);
      wr_printf("\n");
#endif  
    }
    // take next element
    if (vecflag) {
      if (g->reverse_clauselist_strat) i--;
      else i++;
    } else {      
      cell=cellptr->cdr;
    }        
  }
#ifdef DEBUG            
  wr_printf("\nrules_found %d facts_found %d \n",rules_found,facts_found); 
#endif   
  return rules_found+facts_found;
}



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
  (g->print_litterm_selection)=0;
  (g->print_partial_derived_cl)=0;
  (g->print_derived_cl)=0;
  (g->print_derived_subsumed_cl)=0;
  (g->print_derived_precut_cl)=0;

  (g->print_clause_detaillevel)=0;
  (g->print_runs)=0;
  (g->print_stats)=0;
  (g->print_datastructs)=0;
  
}

static void wr_set_tiny_printout(glb* g) {
  (g->print_flag)=1;
  
  (g->parser_print_level)=0;
  (g->print_initial_parser_result)=0;
  (g->print_generic_parser_result)=0;
  
  (g->print_initial_active_list)=0;
  (g->print_initial_passive_list)=0;
  
  (g->print_given_interval_trace)=0;
  (g->print_initial_given_cl)=0;
  (g->print_final_given_cl)=0;
  (g->print_active_cl)=0;
  (g->print_litterm_selection)=0;
  (g->print_partial_derived_cl)=0;
  (g->print_derived_cl)=0;
  (g->print_derived_subsumed_cl)=0;
  (g->print_derived_precut_cl)=0;
  
  (g->print_clause_detaillevel)=1;
  (g->print_runs)=0;
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
  (g->print_litterm_selection)=0;
  (g->print_partial_derived_cl)=0;
  (g->print_derived_cl)=0;
  (g->print_derived_subsumed_cl)=0;
  (g->print_derived_precut_cl)=0;
  
  (g->print_clause_detaillevel)=1;
  (g->print_runs)=1;
  (g->print_stats)=1;
  (g->print_datastructs)=0;
  
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
  (g->print_litterm_selection)=0;
  (g->print_partial_derived_cl)=0;
  (g->print_derived_cl)=0;
  (g->print_derived_subsumed_cl)=0;
  (g->print_derived_precut_cl)=0;
  
  (g->print_clause_detaillevel)=1;
  (g->print_runs)=1;
  (g->print_stats)=1;
  (g->print_datastructs)=0;
  
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
  (g->print_litterm_selection)=0;
  (g->print_partial_derived_cl)=0;
  (g->print_derived_cl)=1;
  (g->print_derived_subsumed_cl)=0;
  (g->print_derived_precut_cl)=0;
  
  (g->print_clause_detaillevel)=1;
  (g->print_runs)=1;
  (g->print_stats)=1;
  (g->print_datastructs)=0;
  
}

void wr_set_detailed_printout(glb* g) {
  (g->print_flag)=1;
  
  (g->parser_print_level)=1;
  (g->print_initial_parser_result)=1;
  (g->print_generic_parser_result)=1;
  
  (g->print_initial_active_list)=1;
  (g->print_initial_passive_list)=1;
  
  (g->print_initial_given_cl)=1;
  (g->print_final_given_cl)=1;
  (g->print_active_cl)=1;
  (g->print_litterm_selection)=0;
  (g->print_partial_derived_cl)=1;
  (g->print_derived_cl)=1;
  (g->print_derived_subsumed_cl)=1;
  (g->print_derived_precut_cl)=1;
  
  (g->print_clause_detaillevel)=1;
  (g->print_runs)=1;
  (g->print_stats)=1;
  (g->print_datastructs)=0;
  
}

void wr_set_detailed_plus_printout(glb* g) {
  (g->print_flag)=1;
  
  (g->parser_print_level)=1;
  (g->print_initial_parser_result)=1;
  (g->print_generic_parser_result)=1;
  
  (g->print_initial_active_list)=1;
  (g->print_initial_passive_list)=1;
  
  (g->print_initial_given_cl)=1;
  (g->print_final_given_cl)=1;
  (g->print_active_cl)=1;
  (g->print_litterm_selection)=1;
  (g->print_partial_derived_cl)=1;
  (g->print_derived_cl)=1;
  (g->print_derived_subsumed_cl)=1;
  (g->print_derived_precut_cl)=1;
  
  (g->print_clause_detaillevel)=1;
  (g->print_runs)=1;
  (g->print_stats)=1;  
  (g->print_datastructs)=1;
}



/* -----------------------------------------------

   Some funs for statistics 
   ----------------------------------------------- */

void wr_show_stats(glb* g, int show_local_complex) {
  
  if (!(g->print_stats)) return;
  
  wr_printf("\nrun %d fork %d statistics:\n",(g->current_run_nr)+1,g->current_fork_nr);
  wr_printf("----------------------------------\n");
  wr_printf("this run seconds: %f\n",
    (float)(clock() - (g->run_start_clock)) / (float)CLOCKS_PER_SEC);
  //wr_printf("total seconds: %f\n",
  //  (float)(clock() - (g->allruns_start_clock)) / (float)CLOCKS_PER_SEC);
  wr_printf("stat_given_used: %d\n",g->stat_given_used);
  wr_printf("stat_given_candidates:   %d\n",g->stat_given_candidates); 
  wr_printf("stat_given_candidates_h: %d\n",g->stat_given_candidates_hyper); 
  //printf("stat_derived_cl: %d\n",g->stat_derived_cl);
  wr_printf("stat_binres_derived_cl:   %d\n",g->stat_binres_derived_cl);
  wr_printf("stat_binres_derived_cl_h: %d\n",g->stat_derived_partial_hyper_cl);
  //wr_printf("stat_instgen_derived_cl:   %d\n",g->stat_instgen_derived_cl);
  //wr_printf("stat_prop_inst_derived_cl:   %d\n",g->stat_prop_inst_derived_cl);
  //wr_printf("stat_propagated_derived_cl: %d\n",g->stat_propagated_derived_cl);
  wr_printf("stat_factor_derived_cl: %d\n",g->stat_factor_derived_cl);
  wr_printf("stat_para_derived_cl: %d\n",g->stat_para_derived_cl);
  wr_printf("stat_tautologies_discarded: %d\n",g->stat_tautologies_discarded);
  wr_printf("stat_forward_subsumed: %d\n",g->stat_forward_subsumed);   
  wr_printf("stat_derived_cut: %d\n",g->stat_derived_cut);  
  wr_printf("stat_derived_rewritten: %d\n",g->stat_derived_rewritten); 
  wr_printf("stat_weight_discarded_building: %d\n",g->stat_weight_discarded_building);
  wr_printf("stat_weight_discarded_cl: %d\n",g->stat_weight_discarded_cl);
  wr_printf("stat_internlimit_discarded_cl: %d\n",g->stat_internlimit_discarded_cl);
  wr_printf("stat_simplified:  %d simplified %d derived %d given\n",
         g->stat_simplified, g->stat_simplified_derived, g->stat_simplified_given);  
  wr_printf("stat_kept_cl: %d\n",g->stat_kept_cl);       
  wr_printf("stat_built_cl: %d\n",g->stat_built_cl); 
  wr_printf("stat_hyperres_partial_cl: %d\n",g->stat_hyperres_partial_cl);
  wr_printf("stat_made_rewriters: %d\n", g->stat_made_rewriters);
  //wr_printf("stat_backward_subsumed: %d\n",g->stat_backward_subsumed);  
  //wr_printf("stat_propagated_subsumed: %d\n",g->stat_propagated_subsumed);
  
#ifdef SHOW_SUBSUME_STATS  
  wr_printf("stat_clsubs_attempted:           %15d\n",g->stat_clsubs_attempted);  
  wr_printf("stat_clsubs_fact_groundunit_found: %13d\n",g->stat_clsubs_fact_groundunit_found);
  wr_printf("stat_clsubs_rule_groundunit_found: %13d\n",g->stat_clsubs_rule_groundunit_found);
  wr_printf("stat_clsubs_top_meta_attempted:    %15ld\n",g->stat_clsubs_top_meta_attempted);
  wr_printf("stat_clsubs_top_meta_failed:       %15ld\n",g->stat_clsubs_top_meta_failed);

  wr_printf("stat_clsubs_top_meta_nonpref_attempted:     %15ld\n",g->stat_clsubs_top_meta_nonpref_attempted);
  wr_printf("stat_clsubs_top_meta_nonpref_succeeded:     %15ld\n",g->stat_clsubs_top_meta_nonpref_succeeded);
  wr_printf("stat_clsubs_top_meta_pref_attempted:        %15ld\n",g->stat_clsubs_top_meta_pref_attempted);  
  wr_printf("stat_clsubs_top_meta_pref1_succeeded:       %15ld\n",g->stat_clsubs_top_meta_pref1_succeeded);
  wr_printf("stat_clsubs_top_meta_pref2_succeeded:       %15ld\n",g->stat_clsubs_top_meta_pref2_succeeded);
  wr_printf("stat_clsubs_top_meta_pref3_succeeded:       %15ld\n",g->stat_clsubs_top_meta_pref3_succeeded);
  wr_printf("stat_clsubs_top_meta_pref_succeeded:        %15ld\n",g->stat_clsubs_top_meta_pref_succeeded);

  wr_printf("stat_clsubs_meta_attempted:      %15d\n",g->stat_clsubs_meta_attempted);
  wr_printf("stat_clsubs_meta_failed:         %15d\n",g->stat_clsubs_meta_failed);
  wr_printf("stat_clsubs_predsymbs_attempted: %15d\n",g->stat_clsubs_predsymbs_attempted);
  wr_printf("stat_clsubs_unit_attempted:      %15d\n",g->stat_clsubs_unit_attempted);
  wr_printf("stat_clsubs_full_attempted:      %15d\n",g->stat_clsubs_full_attempted);
  wr_printf("stat_forwardsubs_attempted:      %15d\n",g->stat_forwardsubs_attempted);
#endif 
#ifdef SHOW_HASH_CUT_STATS  
  wr_printf("stat_lit_hash_added:        %15d\n",g->stat_lit_hash_added);
  wr_printf("stat_lit_hash_computed:     %15d\n",g->stat_lit_hash_computed);  
  wr_printf("stat_lit_hash_match_found:  %15d\n",g->stat_lit_hash_match_found);
  wr_printf("stat_lit_hash_match_miss:   %15d\n",g->stat_lit_hash_match_miss); 
  wr_printf("stat_lit_hash_cut_ok:       %15d\n",g->stat_lit_hash_cut_ok); 
  wr_printf("stat_lit_strong_cut_ok:     %15d\n",g->stat_lit_strong_cut_ok); 
  wr_printf("stat_lit_hash_subsume_ok:   %15d\n",g->stat_lit_hash_subsume_ok); 
#endif
#ifdef SHOW_MEM_STATS
  
  if (show_local_complex) {
    //if ((g->clbuilt)!=(gint)NULL) 
    //  wr_printf("clbuilt els %d used %d\n",
    //         (rotp(g,g->clbuilt))[0],(rotp(g,g->clbuilt))[1]-1);
    if ((g->clqueue)!=(gint)NULL) 
      wr_printf("clqueue els %d used %d\n",
            (int)((rotp(g,g->clqueue))[0]),(int)((rotp(g,g->clqueue))[1]-1));
    if ((g->clactive)!=(gint)NULL) 
      wr_printf("clactive els %d used %d\n",
            (int)((rotp(g,g->clactive))[0]),(int)((rotp(g,g->clactive))[1]-1));
    if ((g->clactivesubsume)!=(gint)NULL) 
      wr_printf("clactivesubsume els %d used %d\n",
            (int)((rotp(g,g->clactivesubsume))[0]),(int)((rotp(g,g->clactivesubsume))[1]-1));           
    //if ((g->clweightqueue)!=(gint)NULL) 
    //  wr_printf("clweightqueue els %d used %d\n",((gptr)(g->clweightqueue))[0],((gptr)(g->clweightqueue))[1]-1);
    
    if ((g->queue_termbuf)!=NULL) 
      wr_printf("queue_termbuf els %d used %d\n",(int)((g->queue_termbuf)[0]),(int)((g->queue_termbuf)[1]-1));
    if ((g->hyper_termbuf)!=NULL) 
      wr_printf("hyper_termbuf els %d used %d\n",(int)((g->hyper_termbuf)[0]),(int)((g->hyper_termbuf)[1]-1));  
    if ((g->active_termbuf)!=NULL) 
      wr_printf("active_termbuf els %d used %d\n",(int)((g->active_termbuf)[0]),(int)((g->active_termbuf)[1]-1));
    if ((g->varstack)!=NULL) 
      wr_printf("varstack els %d last used %d\n",(int)((g->varstack)[0]),(int)((g->varstack)[1]-1));
    if ((g->given_termbuf)!=NULL) 
      wr_printf("given_termbuf els %d last used %d\n",(int)((g->given_termbuf)[0]),(int)((g->given_termbuf)[1]-1));
    if ((g->simplified_termbuf)!=NULL) 
      wr_printf("simplified_termbuf els %d last used %d\n",(int)((g->simplified_termbuf)[0]),(int)((g->simplified_termbuf)[1]-1));  
    if ((g->derived_termbuf)!=NULL) 
      wr_printf("derived_termbuf els %d last used %d\n",(int)((g->derived_termbuf)[0]),(int)((g->derived_termbuf)[1]-1));
  }

  wr_printf("wr_mallocs: %d\n",(g->stat_wr_mallocs));
  wr_printf("wr_callocs: %d\n",(g->stat_wr_callocs));
  wr_printf("wr_reallocs: %d\n",(g->stat_wr_reallocs));
  wr_printf("wr_frees: %d\n",(g->stat_wr_frees));
  wr_printf("wr_malloc_bytes: %ld\n",(g->stat_wr_malloc_bytes));
  wr_printf("wr_calloc_bytes: %ld\n",(g->stat_wr_calloc_bytes));
  wr_printf("wr_realloc_bytes: %ld\n",(g->stat_wr_realloc_bytes));
  wr_printf("wr_realloc_freebytes: %ld\n",(g->stat_wr_realloc_freebytes));
  /*
  wr_printf("tmp1: %d\n",(g->tmp1));
  wr_printf("tmp2: %d\n",(g->tmp2));
  wr_printf("tmp3: %d\n",(g->tmp3));
  wr_printf("tmp4: %d\n",(g->tmp4));
  wr_printf("tmp5: %d\n",(g->tmp5));
  */ 
#endif  
  wr_printf("----------------------------------\n");

  if (g->print_datastructs) print_datastructs(g);
}  

void show_cur_time(void)  {
  time_t rawtime;
  clock_t curclock;
  struct tm * timeinfo;
  float run_seconds;

  time (&rawtime);
  timeinfo = localtime ( &rawtime );
  curclock=clock();
  run_seconds = (float)(curclock) / CLOCKS_PER_SEC;
  printf("run_seconds %f time %s\n", run_seconds,asctime (timeinfo));
}

void print_datastructs(glb* g) {
  //wr_print_priorqueue(g, clpick_queues)
  wr_print_active_clauses(g);
  wr_print_clpick_queues(g, rotp(g,g->clpick_queues));

}

void wr_print_active_clauses(glb* g)  {
  gptr actptr, hasharrpos, hasharrneg, hashvec;
  int dbused;
  gint iactive, iactivelimit;

  wr_printf("\n===== active clauses as used for subsumption =====  \n");
  for(dbused=0; dbused<2; dbused++) {
    if (dbused==0) {
      actptr=rotp(g,g->clactivesubsume); 
      hasharrpos=rotp(g,g->hash_pos_active_groundunits);
      hasharrneg=rotp(g,g->hash_neg_active_groundunits);  
      wr_printf("\n-- in local db: -- \n");         
    } else {       
      if (r_kb_g(g)) {        
        actptr=rotp(g,(r_kb_g(g))->clactivesubsume); 
        hasharrpos=rotp(g,(r_kb_g(g))->hash_pos_active_groundunits);
        hasharrneg=rotp(g,(r_kb_g(g))->hash_neg_active_groundunits);
        wr_printf("\n -- in shared db: -- \n");         
      }  
      else break;      
    }
    // ground units:
    if (1) {
      wr_printf("\npos ground units\n"); 
      wr_print_offset_termhash(g,hasharrpos);
    }
    if (1) {
      wr_printf("\nneg ground units\n"); 
      wr_print_offset_termhash(g,hasharrneg);
    }
    // non-ground-units:
    wr_printf("\nnot ground-units:\n\n");  
    iactivelimit=CVEC_NEXT(actptr);
    for(iactive=CVEC_START; iactive<iactivelimit; iactive+=CLMETABLOCK_ELS) {
      wr_print_clause(g,rotp(g,(actptr[iactive+CLMETABLOCK_CL_POS])));
      wr_printf("\n");
    }  
  }
  wr_printf("\n\n===== end of active clauses as used for subsumption  =====  \n\n");

  wr_printf("\n===== rewriters =====  \n");

  for(dbused=0; dbused<2; dbused++) {            
    if (dbused==0) {         
      hashvec=rotp(g,g->hash_rewrite_terms);           
      wr_printf("\n-- in local db: -- \n"); 
    } else {    
      if (!(r_kb_g(g))) continue;             
      hashvec=rotp(g,r_kb_g(g)->hash_rewrite_terms);       
      wr_printf("\n -- in shared db: -- \n");                   
    }
    wr_clterm_hashlist_print_para(g,hashvec);
  }     

  wr_printf("\n\n===== end of rewriters  =====  \n\n");

  wr_printf("\n===== active clauses in a resolvable literal hash  =====  \n");

  for(dbused=0; dbused<2; dbused++) {
    if (dbused==0) {
      hasharrpos=rotp(g,g->hash_pos_atoms);
      hasharrneg=rotp(g,g->hash_neg_atoms);
      wr_printf("\n-- in local db: -- \n");    
    } else {
      if (r_kb_g(g)) {
        hasharrpos=rotp(g,r_kb_g(g)->hash_pos_atoms);
        hasharrneg=rotp(g,r_kb_g(g)->hash_neg_atoms); 
        wr_printf("\n -- in shared db: -- \n");      
      } else {
        // no external database
        break;
      }       
    }
    wr_printf("pos usable literals :\n"); 
    wr_clterm_hashlist_print(g,hasharrpos); 
    //wr_clterm_hashdata_print(g,hasharrpos);
    wr_printf("\n\nneg usable literals :\n"); 
    wr_clterm_hashlist_print(g,hasharrneg); 
    //wr_clterm_hashdata_print(g,hasharrneg);
    
  }

  wr_printf("\n\n===== end of active clauses in a resolvable literal hash  =====  \n");


}

static void wg_show_db_error(void* db) {
  int err;
  char* msg;
  //char* errmsg;
  
  err=dbmemsegh(db)->errflag;
  if (!err) return;
  if (err==DB_MEMORY_ALLOC_ERROR) msg="DB_MEMORY_ALLOC_ERROR";
  else if (err==DB_MEMORY_ALLOC_ERROR2) msg="DB_MEMORY_ALLOC_ERROR2";
  else if (err==DB_IO_ERROR) msg="DB_IO_ERROR";
  else if (err==DB_IO_ERROR2) msg="DB_IO_ERROR2";
  else if (err==DB_LOG_ERROR) msg="DB_LOG_ERROR";
  else if (err==DB_MPOOL_ERROR) msg="DB_MPOOL_ERROR";
  else if (err==DB_MPOOL_ERROR2) msg="B_MPOOL_ERROR2";
  else if (err==DB_DATA_ERROR) msg="B_DATA_ERROR";
  else if (err==DB_DATA_ERROR2) msg="DB_DATA_ERROR2";
  else if (err==DB_DATA_ERROR3) msg="DB_DATA_ERROR3";
  else if (err==DB_DATA_ERROR4) msg="DB_DATA_ERROR4";
  else msg="unknown db error";
  printf("\n{\"error\": \"uncaught db library error: %s\"}\n",msg);
}

#ifdef __cplusplus
}
#endif
