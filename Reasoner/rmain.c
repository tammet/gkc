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

#ifdef __EMSCRIPTEN__
#include "../Builtparser/dbparse.h"
#else
#include "../Parser/dbparse.h"
#endif
#include <sys/wait.h> 
#include <unistd.h> 
//#include <sys/signal.h>
#include <signal.h>
#ifdef __linux__
#include <sys/prctl.h>
#else
#include <signal.h> 
#endif

#endif


#include "rincludes.h"  
#include "../cjson/cjson.h"
#include "../Db/dballoc.h"  // for ACONST_TRUE/FALSE
#include "../Db/dbmpool.h"

#ifdef __cplusplus
extern "C" {
#endif

  
/* ====== Private headers and defs ======== */

static void wr_set_no_printout(glb* g);
static void wr_set_tiny_printout(glb* g);
static void wr_set_tiny_extra_printout(glb* g);
static void wr_set_tiny_runs_printout(glb* g);
static void wr_set_low_printout(glb* g);
static void wr_set_normal_printout(glb* g);
static void wr_set_medium_printout(glb* g);

//static void wg_show_db_error(void* db);
//void wr_set_detailed_printout(glb* g);  

void show_cur_time(void);

int store_units_globally(void* db, glb* g);
void wr_store_offset_termhash(void* db, glb* g, gint* hasharr, int pos);
#define GLOBAL_UNITS

#define SHOW_SUBSUME_STATS
#define SHOW_MEM_STATS
#define SHOW_HASH_CUT_STATS

#define SUBSFLAG_CLAUSE_COUNT_LIMIT 10000 // no subs with active clauses if more active clauses
#define SINEORDER  

/* ======= Private protos ================ */

//#define DEBUG
//#undef DEBUG

//#define SHOWTIME
//#undef SHOWTIME

//#define PRINTERR

//#define SHOW_ADDED

//#define WEIGHT_PREFER_GOALS_ASSUMPTIONS 

void* wr_free_analyze_g(glb* g);

/* ====== Functions ============== */


#ifndef _WIN32
// set up signal handling
/*
volatile sig_atomic_t last_received_signal_pid = 0;

void sig_usr(int signo, siginfo_t *sinfo, void *context){
  if(signo == PROOF_FOUND_SIGNAL) {
    last_received_signal_pid=(long)sinfo->si_pid;
    printf("\nSignal %d from proc %ld caught!\n",PROOF_FOUND_SIGNAL,(long)sinfo->si_pid);    
  }
}
*/
#endif

int wg_run_reasoner(void *db, char* inputname, char* stratfile, int informat,  
                    char* outfilename, char* guidestr) {
  glb* g;    // our g (globals)
  glb* kb_g; // our copy of the g (globals) of the external shared db 
  glb* rglb=NULL; // ptr to g (globals) of the external shared db 
  void* kb_db;
  void* child_db;
  //void* tmp_db;
  void* local_db; // this will be set to the local db copy in all cases
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
  clock_t curclock;
  float total_seconds,total_dseconds;
  int forkpids[64];
  
  printf("\nwg_run_reasoner starts\n");
  //printf("\n guidestr %s\n",guidestr);
  if (!stratfile && (!guidestr || !strncmp(guidestr,"LTBSPECIAL",10))) {
    //guide=wr_parse_guide_file(argc,argv,&guidebuf);  
    givenguide=0;  
  } else if (stratfile) {
    guide=wr_parse_guide_file(stratfile,&guidebuf);
    givenguide=1;
  } else if (guidestr) {
    guide=wr_parse_guide_str(guidestr);  
    givenguide=1;
  }
  if (guide==NULL && givenguide) {
    if (guidebuf!=NULL) sys_free(guidebuf);
    return -1;
  }
  filename=inputname;
#ifdef SHOWTIME
  //wr_printf("Guide parsed.\n");
  //wr_printf("\ndb is %d \n",(int)((gint)db));
  //show_cur_time();
#endif
  // kb_db is set to parent db
  kb_db=db_get_kb_db(db);
  //printf("\nwg_run_reasoner starts now db is %lx and db->kb_db is %lx \n",(unsigned long int)db,(unsigned long int)(dbmemsegh(db)->kb_db));
  if (kb_db) {
    // separate child_db and kb_db
    // from now one var db will point to the parent, i.e. kb_db  
    //printf("\nseparate child_db and kb_db\n");  
#ifdef DEBUG
    wr_printf("\nseparate child_db and kb_db\n");
#endif      
    //printf("\ng_run_reasoner found separate child_db and kb_db\n");
    local_db=db;    
    //have_shared_kb=1;
    child_db=db;
    db=kb_db;    
    rglb=db_rglb(db); // this is the internal g of db
    //printf("\ng %ld rglb %ld\n",(gint)g,(gint)rglb);
    kb_g=sys_malloc(sizeof(glb)); // this is a new malloced g
    // copy rglb stuff to g
    memcpy(kb_g,rglb,sizeof(glb));
    // now kb_g should contain the same things as rglb
    // set the db ptr of kb_g to the shared db
    (kb_g->db)=kb_db; 
    //printf("\nwg_run_reasoner CP1 now db is %lx and kb_db is %lx \n",(unsigned long int)db,(unsigned long int)kb_db);
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
    local_db=db;    
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
  //printf("\nto call wr_analyze_clause_list\n");
  tmp=wr_analyze_clause_list(analyze_g,db,child_db);
  //printf("\nreturned from wr_analyze_clause_list\n");
  if (!givenguide) {
    if (guidestr && !strncmp(guidestr,"LTBSPECIAL",10)) {
      //(dbmemsegh(db)->max_forks)=2;
      guidebuf=make_auto_guide(analyze_g,kb_g,1);     
      guide=wr_parse_guide_str(guidebuf); 
      if (guide==NULL) {  return -1; } 
      //if (guidebuf!=NULL) wr_free(g,(void*)guidebuf);
      if (guidebuf!=NULL) { sys_free(guidebuf); guidebuf=NULL; }

    } else if (guidestr!=NULL) {      
      guide=wr_parse_guide_str(guidestr);       
      if (guide==NULL) {  return -1; } 
    } else {
      guidebuf=make_auto_guide(analyze_g,kb_g,0);     
      guide=wr_parse_guide_str(guidebuf);       
      if (guide==NULL) {  return -1; } 
      //if (guidebuf!=NULL) wr_free(g,(void*)guidebuf);
      if (guidebuf!=NULL) { sys_free(guidebuf); guidebuf=NULL; }
    }
  }
  
  int pid=1,stat;
  forkscreated=0;
  forkslive=0;  
#if defined(_WIN32) || defined(__EMSCRIPTEN__)
  maxforks=0;
  pid=1;
#else  
  /*
  struct sigaction sa;
  //sa.sa_handler = sig_usr;
  sigemptyset(&sa.sa_mask);
  sigaddset(&sa.sa_mask, PROOF_FOUND_SIGNAL);
  sa.sa_flags = SA_SIGINFO; // | SA_RESETHAND; 
  sa.sa_sigaction = sig_usr;
  */                        

  maxforks=(dbmemsegh(local_db)->max_forks); 
  //printf("\ndbmemsegh(db)->max_forks %ld dbmemsegh(local_db)->max_forks %ld\n",
  //      dbmemsegh(db)->max_forks, dbmemsegh(local_db)->max_forks); 
  if (maxforks>64) maxforks=64;
  if (maxforks==1) maxforks=0;
  setbuf(stdout, 0);
  for(forknr=0; forknr<maxforks; forknr++) {
    pid=fork();

    //printf("\nfork pid %d forknr %d\n",pid,forknr);
    //fflush(stdout);

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
      /*
      if(sigaction(PROOF_FOUND_SIGNAL, &sa, NULL) == -1) {
        printf("\nSignal %d checking error in parent\n",PROOF_FOUND_SIGNAL);
      } else {
        printf("\nSignal %d checking successful in parent",PROOF_FOUND_SIGNAL);
      } 
      */
    }       
  }
#endif 
 
 
  // forks have been created
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)  
  if (pid && forkscreated) {
    // only parent performs this loop
    while(forkslive) {      
      if (!givenguide && guidestr && !strncmp(guidestr,"LTBSPECIAL",10)) {
        //printf("\n(dbmemsegh(db)->min_strat_timeloop_nr) %ld\n",(dbmemsegh(db)->min_strat_timeloop_nr));
        //fflush(stdout);        
        if ((dbmemsegh(db)->min_strat_timeloop_nr)<3) {
          alarm(60);
        } else {  
          alarm(200);
        }          
      }       
      cpid=waitpid(-1,&stat, 0);

      //printf("\nwaitpid ended for cpid %d\n",cpid);
      //fflush(stdout);

      if (WIFEXITED(stat)) {

        //printf("\nWIFEXITED(stat) true for cpid %d\n",cpid);
        //fflush(stdout);

        err = WEXITSTATUS(stat);

        //printf("Child inside subprocess checking %d terminated with status err: %d\n", cpid, err); 
        //fflush(stdout);

        //printf("last_received_signal_pid: %d\n", last_received_signal_pid);
        forkslive--;  
        // remove cpid from pid array      
        for(i=0;i<forkscreated;i++) {          
          if (forkpids[i]==cpid) { //} && last_received_signal_pid!=cpid) {
            //printf("\ni %d removing cpid %d from forkpids[i] %d\n",i,cpid,forkpids[i]);
            forkpids[i]=0;
            break;
          }
        }
        if (err==0) {
          //printf("\nerr==0\n");
          // proof found, kill all children
          for(i=0;i<forkscreated;i++) {
            if (forkpids[i]) { //} && last_received_signal_pid!=forkpids[i]) {
              // kill pid
              //printf("\ni %d killing cpid forkpids[i] %d from forkpids\n",i,forkpids[i]);
              kill(forkpids[i], SIGKILL);
            }
          }  
          //break;
          if (analyze_g) analyze_g=wr_free_analyze_g(analyze_g);
          exit(0);
        }        
      }
      
      //printf("\nin forkslive loop\n");
      //fflush(stdout);

    } 

    //printf("\nout of forkslive loop\n");
    //fflush(stdout);

  }
#endif
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
    //printf("\niter %d pid %d maxforks %d forknr %d (iter div maxforks) %d\n",iter,pid,maxforks,forknr,(iter % maxforks));
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)     
    if (!pid && maxforks && ((iter % maxforks)!=forknr)) continue; 
#endif        
#ifdef DEBUG    
    //wr_printf("\n**** run %d starts\n",iter+1);    
#endif
    /*
    if (g->print_flag) {
      printf("\n**** run %d starts\n",iter+1);
      fflush(stdout);     
    }  
    */

    /*
    wg_run_reasoner_onestrat(
      db, pid, outfilename, iter, guide, givenguide, guidebuf, filename,
      kb_db, child_db, kb_g, rglb, analyze_g, &guideres);
    continue;  
    */

    curclock=clock();
    total_seconds = (float)(curclock - (dbmemsegh(local_db)->allruns_start_clock)) / CLOCKS_PER_SEC;
    total_dseconds = (float)(curclock - (dbmemsegh(local_db)->allruns_start_clock)) / 
                      ((float)CLOCKS_PER_SEC / 10.0);
    //printf("\ntotal_seconds %f max_seconds %d \n",total_seconds,(dbmemsegh(local_db)->max_seconds));
    if ((dbmemsegh(local_db)->max_seconds) && 
        (total_seconds>(dbmemsegh(local_db)->max_seconds))) {
      res=2;    
      break;
    }
    if ((dbmemsegh(local_db)->max_dseconds) && 
        (total_dseconds>(dbmemsegh(local_db)->max_dseconds))) {
      res=2;    
      break;
    }

    g=wr_glb_new_simple(db);
    (dbmemsegh(local_db)->local_g)=(glb*)g; // for cleaning in signal
    // init local_db
    (g->local_db)=local_db;
    // copy memsegh guiding stuff to g
    if (dbmemsegh(local_db)->tptp) {
      (g->print_tptp)=1;        
      (g->print_fof_conversion_proof)=1;
      (g->print_clauses_tptp)=1;
      (g->print_proof_tptp)=1;   
      (g->use_comp_arithmetic)=1;
    } else {
      (g->print_tptp)=0;        
      (g->print_fof_conversion_proof)=0; 
      (g->print_clauses_tptp)=0;
      (g->print_proof_tptp)=0;
      (g->use_comp_arithmetic)=1;
    }
    if (dbmemsegh(local_db)->json) {
      (g->print_json)=1; 
      (g->print_clauses_json)=1;
      if (g->print_tptp) {
        wr_errprint("do not set both tptp and json output parameters at the same time");    
        return -1;
      }
    }    

    (g->print_level_flag)=(dbmemsegh(local_db)->printlevel);
    (g->print_derived)=(dbmemsegh(local_db)->printderived);
    (g->max_seconds)=(dbmemsegh(local_db)->max_seconds);
    (g->max_dseconds)=(dbmemsegh(local_db)->max_dseconds);

    // copy analyze_g stats to g for easier handling
    wr_copy_sin_stats(analyze_g,g);    
    /*
    if (dbmemsegh(local_db)->printlevel) (g->print_level_flag)=(dbmemsegh(local_db)->printlevel);
    if (dbmemsegh(local_db)->printderived) (g->print_derived)=(dbmemsegh(local_db)->printderived);
    if (dbmemsegh(local_db)->max_seconds) (g->max_seconds)=(dbmemsegh(local_db)->max_seconds);
    if (dbmemsegh(local_db)->max_dseconds) (g->max_dseconds)=(dbmemsegh(local_db)->max_dseconds);
    */
    
    if (kb_g!=NULL) {      
      (g->kb_g)=kb_g; // points to the copy of globals of the external kb
    }  
    if (g==NULL) {
      wr_errprint("cannot allocate enough memory during reasoner initialization");
      if (guidebuf!=NULL) free(guidebuf);
      if (guide!=NULL) cJSON_Delete(guide);
      sys_free(g); 
      if (analyze_g) analyze_g=wr_free_analyze_g(analyze_g);
      return -1;
    }   
#ifdef DEBUG
    wr_printf("\nnow db is %ld and child_db is %ld \n",(gint)db,(gint)child_db);
#endif    
    (g->child_db)=child_db;
    (g->db)=db;
    (g->local_db)=local_db;
     
    (g->current_run_nr)=iter;
    (g->current_fork_nr)=forkslive;
    (g->print_level_flag)=(dbmemsegh(local_db)->printlevel);
    (g->print_derived)=(dbmemsegh(local_db)->printderived);
    (g->max_seconds)=(dbmemsegh(local_db)->max_seconds);
    (g->max_dseconds)=(dbmemsegh(local_db)->max_dseconds);
    if (outfilename) (g->outfilename)=outfilename;   

    if (iter==0) (g->allruns_start_clock)=clock();    
    guidetext=NULL;
    guideres=wr_parse_guide_section(g,guide,iter,&guidetext,-1); 
    //printf("\nguideres %d\n",guideres);
    if (dbmemsegh(local_db)->printlevel) (g->print_level_flag)=(dbmemsegh(local_db)->printlevel);
    if (dbmemsegh(local_db)->printderived) (g->print_derived)=(dbmemsegh(local_db)->printderived);

    /*
    if (g->print_flag) {
      printf("\n**** run %d starts\n",iter+1);
      fflush(stdout);     
    }  
    */
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
      if (analyze_g) analyze_g=wr_free_analyze_g(analyze_g);
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
    else if ((g->print_level_flag)<=11) wr_set_tiny_extra_printout(g);
    else if ((g->print_level_flag)<=12) wr_set_tiny_runs_printout(g);
    else if ((g->print_level_flag)<=15) wr_set_low_printout(g);
    else if ((g->print_level_flag)<=20) wr_set_normal_printout(g);
    else if ((g->print_level_flag)<=30) wr_set_medium_printout(g);
    else if ((g->print_level_flag)<=50) wr_set_detailed_printout(g);    
    else wr_set_detailed_plus_printout(g);

    if ((g->print_flag) && (g->print_runs)) {      
      if (guidetext!=NULL) {   
        if (maxforks) {
          wr_printf("\n**** run %d fork %d starts with strategy\n",iter+1,forkslive); 
        } else {
          wr_printf("\n**** run %d starts with strategy\n",iter+1);   
        }
        wr_printf("%s\n",guidetext);
        free(guidetext);
      } else {       
        if (maxforks) {
          wr_printf("\n**** run %d fork %d starts with strategy\n",iter+1,forkslive); 
        } else {
          wr_printf("\n**** run %d starts with strategy\n",iter+1);   
        }
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
      if (analyze_g) analyze_g=wr_free_analyze_g(analyze_g);
      return -1; 
    }   
    if (filename) {
      strncpy((g->filename),filename,MAX_FILENAME_LEN);
    } else {
      strcpy((g->filename),"input_text");
    }  
    if (outfilename) (g->outfilename)=outfilename;  
#ifdef SHOWTIME    
    wr_printf("\nto call wr_init_active_passive_lists_from_all\n");
    show_cur_time();
#endif

    if (db!=child_db) {
      printf("\nshared database used\n");
      (g->print_fof_conversion_proof)=0;
      (g->store_fof_skolem_steps)=0;      
      (g->sine_strat)=0; 
      (g->sine_strat_used)=0;
    } else {
      //printf("\nshared database not used\n");
    }
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
        //printf("\n**** no queryfocus, starting to read from the external shared mem kb\n");
        clause_count+=wr_init_active_passive_lists_from_one(g,db,db);   
        CP0    
        (g->kb_g)=NULL;        
        CP1
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
      
      if (!(g->in_poseq_clause_count) && !(rglb->in_poseq_clause_count) &&
          !(g->in_negeq_clause_count) && !(rglb->in_negeq_clause_count)) (g->use_equality)=0;
      else (g->use_equality)=(g->use_equality_strat);
      
      // if two db-s, this will take the clauses from the local db:
#ifdef DEBUG 
      wr_printf("\n**** starting to read from the local db kb\n");
#endif      
      //printf("\n**** queryfocus, starting to read from the local db kb\n");
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
      
      //if (!(g->in_poseq_clause_count)) (g->use_equality)=0;
      if (!(g->in_poseq_clause_count) &&  !(g->in_negeq_clause_count)) (g->use_equality)=0;
      else (g->use_equality)=(g->use_equality_strat);
      
      /// ----
#ifdef DEBUG       
      wr_printf("\n**** starting to read from the single local db\n");      
#endif    
      //printf("\nto call wr_init_active_passive_lists_from_one\n");
      //printf("\n**** starting to read from the single local db\n");      
      clause_count=wr_init_active_passive_lists_from_one(g,db,db);  
      //printf("\nreturned from wr_init_active_passive_lists_from_one with clause_count %d\n",clause_count);    
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
      if (analyze_g) analyze_g=wr_free_analyze_g(analyze_g);
      break;
    }
    // ok, clauses found and clause lists initialized
    //(g->kb_g)=NULL;
    //printf("\n(g->use_equality) %d (g->use_equality_strat) %d\n",(g->use_equality),(g->use_equality_strat));
    //wr_show_in_stats(g);
    //wr_print_clpick_queues(g,(g->clpick_queues));    
    //if ((g->print_flag) && (g->print_runs)) wr_print_strat_flags(g);



   

    if (!(g->proof_found) || !(wr_enough_answers(g))) {   
      if (guideres==10 && (g->guidetext)) {
        printf("\nseqstrat\n");
        int seqnr=0, seqguideres;
        //char *out;
        char* guidetext1;
        guidetext1=malloc(10000);
        while (!(g->proof_found) || !(wr_enough_answers(g))) {
          //out=cJSON_PrintUnformatted(guide);         

          seqguideres=wr_parse_guide_section(g,guide,iter,&guidetext1,seqnr); 
          if (seqguideres<0) break;
          //seqguideres=0;
          printf("seqnr %d seqguideres %d strat %s\n",seqnr,seqguideres,guidetext1);
          if (seqnr>0) (g->forward_subsume_derived)=0;
          res=wr_genloop(g);          
          printf("\ngenloop ended with res %d\n",res);
          //print_datastructs(g);
          if (res>0 && !(res==1 && wr_have_answers(g))) {
            tmp=wr_glb_init_seq_shared_complex(g); // creates and fills in shared tables, substructures, etc: 0.03 secs
            //CP0
            if (tmp) {
              wr_errprint("cannot init shared complex datastructures");
              if (guidebuf!=NULL) free(guidebuf);
              if (guide!=NULL) cJSON_Delete(guide); 
              wr_glb_free_shared_complex(g);
              sys_free(g);
              return -1; 
            }  
            //CP1
            tmp=wr_glb_init_seq_local_complex(g); // creates and fills in local tables, substructures, etc: fast
            if (tmp) {
              wr_errprint("cannot init local complex datastructures");
              if (guidebuf!=NULL) free(guidebuf);
              if (guide!=NULL) cJSON_Delete(guide); 
              wr_glb_free_shared_complex(g);
              wr_glb_free_local_complex(g);
              sys_free(g);
              if (analyze_g) analyze_g=wr_free_analyze_g(analyze_g);
              return -1; 
            }   
            //CP2          
            if (res==1) {
              clause_count=wr_init_active_passive_lists_from_one(g,db,db);  
              //CP3
              //print_datastructs(g);
              //CP4           
            }
            if (seqnr>0) {
              wr_activate_from_termhash(g,rotp(g,g->hash_neg_groundunits));
              wr_activate_from_termhash(g,rotp(g,g->hash_pos_groundunits));
            }  
            seqnr++;     
          } else {
            break;
          }       
        }
        //exit(0);
      } else {  
      //printf("\nwg_run_reasoner about to call wr_genloop now db is %lx and db->kb_db is %lx and g->db is %lx\n",
      //  (unsigned long int)db,(unsigned long int)(dbmemsegh(db)->kb_db),(unsigned long int)(g->db));   
        res=wr_genloop(g);
      }       
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
        if ((g->print_level_flag)>11) {  
          if (maxforks) {        
            wr_printf("\nfork %d: search finished without proof.\n",g->current_fork_nr); 
          } else {
            wr_printf("\nsearch finished without proof.\n");
          }  
          printf("\nPROOF NOT FOUND\n");
          /*
          printf("\nclactive(g):\n");
          wr_show_clactive(g);

          printf("\n(g->kb_g) %ld\n",(gint)(g->kb_g));
          if (r_kb_g(g)) {
            printf("\n** clactivesubsume data from external:\n"); 
            wr_show_clactivesubsume(r_kb_g(g));

          
            printf("\n*** r_kb_g(g): %ld ***\n",(gint)r_kb_g(g));

            printf("\n db hash_pos_active_groundunits\n");
            wr_print_termhash(g,rotp(g,(r_kb_g(g))->hash_pos_active_groundunits));           
            printf("\n db hash_pos_groundunits\n");      
            wr_print_termhash(g,rotp(g,(r_kb_g(g))->hash_pos_groundunits)); 
            printf("\n db hash_neg_active_groundunits\n");           
            wr_print_termhash(g,rotp(g,(r_kb_g(g))->hash_neg_active_groundunits));           
            printf("\n db hash_neg_groundunits\n");      
            wr_print_termhash(g,rotp(g,(r_kb_g(g))->hash_neg_groundunits)); 
            printf("\n--- printing done ---\n");


            printf("\n*** local units ***\n");

            printf("\n db hash_pos_active_groundunits\n");
            wr_print_termhash(g,rotp(g,g->hash_pos_active_groundunits));           
            printf("\n db hash_pos_groundunits\n");      
            wr_print_termhash(g,rotp(g,g->hash_pos_groundunits)); 
            printf("\n db hash_neg_active_groundunits\n");           
            wr_print_termhash(g,rotp(g,g->hash_neg_active_groundunits));           
            printf("\n db hash_neg_groundunits\n");      
            wr_print_termhash(g,rotp(g,g->hash_neg_groundunits)); 
            printf("\n--- printing done ---\n");
          }
          */ 

          //printf("\natomhash(rotp(g,(g->hash_atom_occurrences))):\n");
          //wr_print_atomhash(g,rotp(g,(g->hash_atom_occurrences)));
          //wr_print_atomhash(g, rotp(g,(g->hash_neg_groundunits)));
          //printf("\natomhash(rotp(g,(g->hash_pos_groundunits))):\n");
          //wr_print_atomhash(g, rotp(g,(g->hash_pos_groundunits)));
          //printf("\natomhash(rotp(g,(r_kb_g(g))->hash_pos_active_groundunits)):\n");
          //wr_print_atomhash(g,rotp(g,(r_kb_g(g))->hash_pos_active_groundunits));
        } else {
          /*
          if (maxforks) {   
            wr_printf("\n\nfork %d: search finished without proof.\n",g->current_fork_nr);          
          } else {
            wr_printf("\n\nsearch finished without proof.\n");
          } 
          */ 
        }
#ifdef GLOBAL_UNITS
        store_units_globally(db,g);
        /*
        void* glbdb;
        glbdb=db_get_kb_db(db);
        //wr_show_database_details(NULL,shmptr,"shmptr");
        wr_show_database_details(NULL,glbdb,"shmptr");
        */
#endif          
      }  
    } else if (res==2 && (g->print_runs)) {
      if (maxforks) {
        wr_printf("\n\nfork %d: search terminated without proof.\n",g->current_fork_nr);         
      } else {
        wr_printf("\n\nsearch terminated without proof.\n");    
      }             
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
        if (guidebuf!=NULL) free(guidebuf);
        if (guide!=NULL) cJSON_Delete(guide); 
        if (outfilename) {
          if (local_db) (dbmemsegh(local_db)->local_g)=NULL;
          wr_glb_free(g);
          return(0);
        } else {          
#ifdef __EMSCRIPTEN__
          if (local_db) (dbmemsegh(local_db)->local_g)=NULL;          
          wr_glb_free(g);                   
          if (analyze_g) analyze_g=wr_free_analyze_g(analyze_g);  
#endif          
          return(0);
        }  
      }
#ifdef SHOWTIME       
      wr_printf("\nto call wr_glb_free\n");
      show_cur_time();  
#endif      
      if (local_db) (dbmemsegh(local_db)->local_g)=NULL;          
      wr_glb_free(g);                   
      if (analyze_g) analyze_g=wr_free_analyze_g(analyze_g);            
#ifdef SHOWTIME       
      wr_printf("\nwr_glb_free returns\n");
      show_cur_time(); 
#endif      
      break;
    }
    // no proof found here
    // wr_show_stats(g,1);
    if (local_db) (dbmemsegh(local_db)->local_g)=NULL;
    wr_glb_free(g);        
    if ((iter+1)>=guideres) break;      
  }
  if (guidebuf!=NULL) free(guidebuf);
  if (guide!=NULL) cJSON_Delete(guide);
  if (analyze_g) analyze_g=wr_free_analyze_g(analyze_g); 
#ifdef SHOWTIME       
  wr_printf("\nto return from rmain\n");
  show_cur_time(); 
#endif  
  return res;  
} 

void* wr_free_analyze_g(glb* g) {
  if (g) {
    if (g->varbanks) wr_vec_free(g,g->varbanks);
    if (g->varstack) wr_vec_free(g,g->varstack);
    sys_free(g);
  }  
  return NULL;
}

int wg_run_converter(void *db, char* inputname, char* stratfile, int informat, 
                     char* outfilename, char* guidestr) {
  glb* g;    // our g (globals)
  glb* kb_g; // our copy of the g (globals) of the external shared db 
  glb* rglb=NULL; // ptr to g (globals) of the external shared db 
  void* kb_db;
  void* child_db;   
  int tmp;  
  int clause_count=0; 
  char* guidebuf=NULL;
  cJSON *guide=NULL;
  //char* guidetext;
  //int givenguide=0;
  //glb* analyze_g;
  char* filename=NULL;  

  /*
  guide=NULL;  
  guidetext=NULL;
  givenguide=0;
  */
  filename=inputname;
  if (!filename) {
    wr_errprint("input is not given");
    return -1;
  }  
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
  /*
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
  //printf("\nto call wr_analyze_clause_list\n");
  //tmp=wr_analyze_clause_list(analyze_g,db,child_db);
  //printf("\nreturned from wr_analyze_clause_list\n");
  if (!givenguide) {
    guide=NULL;  
  }    
  */

  g=wr_glb_new_simple(db);
  // copy analyze_g stats to g for easier handling
  //wr_copy_sin_stats(analyze_g,g);     
 
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
  if (outfilename) (g->outfilename)=outfilename;
  (g->allruns_start_clock)=clock();  
 
  wr_set_tiny_printout(g);
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
  if (filename)
    strncpy((g->filename),filename,MAX_FILENAME_LEN);
  else
    strncpy((g->filename),"input_text",MAX_FILENAME_LEN); 
  if (outfilename) (g->outfilename)=outfilename;    
  (g->print_fof_conversion_proof)=0;
  (g->store_fof_skolem_steps)=0;   
  // if two db-s, this will take the clauses from the shared db     
  clause_count=0;
  if (db!=child_db) {
    // two separate db-s
#ifdef DEBUG
    wr_printf("\n *** separate (local) child kb found, using\n");
#endif         
    clause_count=wr_print_all_clauses(g,child_db);    
  } else {
    // one single db      
#ifdef DEBUG 
      wr_printf("\n external kb NOT found\n");      
#endif                
    clause_count=wr_print_all_clauses(g,db);      
  }    
  if (clause_count<0) {
    // error
    wr_errprint("error initializing clause lists");     
  } else if (!clause_count) {
    // no clauses found
    wr_errprint("no clauses found");     
  }
  // final cleanup
  wr_glb_free(g); 
  if (guidebuf!=NULL) free(guidebuf);
  if (guide!=NULL) cJSON_Delete(guide);  
  return 0;  
} 



int wg_import_otter_file(void *db, char* filename, int iskb, int* informat) {  
  glb* g;
  int res;

  //db_printf("wg_import_otterfile starts for file %s\n",filename); 

  g=wr_glb_new_simple(db); // no complex values given to glb elements 
  if (g==NULL) return 1;   
  (g->parser_print_level)=0;
  (g->print_initial_parser_result)=0;
  (g->print_generic_parser_result)=1;
  (dbmemsegh(db)->infrm_mpool)=wg_create_mpool(db,PARSER_MEMPOOL_SIZE); 
  res=wr_import_otter_file(g,filename,NULL,NULL,iskb);
  if (g->parse_errmsg) {
    printf("%s\n",(g->parse_errmsg));
    sys_free(g->parse_errmsg);
    //res=1;
  } else if (dbmemsegh(db)->errflag) {
    wg_show_db_error(db);
    res=1;
  }
  if (g->in_has_fof) *informat=1;
  sys_free(g); // no complex values given to glb elements
  //dprintf("wg_import_otterfile ends with res\n",res); 
  return res;  
}


int wg_import_otter_string(void *db, char* instr, int iskb, int* informat) {  
  glb* g;
  int res;

  //db_printf("wg_import_otter_strings starts for string %s\n",instr); 

  g=wr_glb_new_simple(db); // no complex values given to glb elements 
  if (g==NULL) return 1;   
  (g->parser_print_level)=0;
  (g->print_initial_parser_result)=0;
  (g->print_generic_parser_result)=1;
  (dbmemsegh(db)->infrm_mpool)=wg_create_mpool(db,PARSER_MEMPOOL_SIZE); 
  res=wr_import_otter_file(g,NULL,instr,NULL,iskb);
  if (g->parse_errmsg) {
    printf("%s\n",(g->parse_errmsg));
    sys_free(g->parse_errmsg);
    //res=1;
  } else if (dbmemsegh(db)->errflag) {
    wg_show_db_error(db);
    res=1;
  } 
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
  //printf("\n ** wr_init_active_passive_lists_from_one called ** with db %lx child_db %lx \n",(unsigned long int)db,(unsigned long int)child_db);
  (g->proof_found)=0;
  wr_clear_all_varbanks(g); 
  wr_process_given_cl_setupsubst(g,g->given_termbuf,1,1); 

  //wr_show_in_summed_stats(g);
  //printf("\ng->in_clause_count %d g->sin_clause_count %d\n",
  //       g->in_clause_count,g->sin_clause_count);

  // setup local clid start: this makes each proof search start 
  // a new sequence of clids after input clids (these are shared)
  
  (g->local_clid_next)=(g->shared_clid_next);

  // perform sine analysis
  //printf("\n(g->sin_clause_count)%d\n",(g->sin_clause_count));
  if (!(g->sine_order_strat)) {
    //if ((g->sin_clause_count) < 1000) {
    //  (g->sine_order_strat)=1;
    //}
    // NB! june 2020
    if (((g->sin_clause_count) < 10000) && ((g->sin_clause_count) > 100) &&
        ((g->sin_goal_count)*2 < (g->sin_clause_count)) ) {
      (g->sine_order_strat)=1;
    }
  }  
  //printf("\n (g->sine_strat): %d  (g->sine_order_strat) %d\n",(g->sine_strat), (g->sine_order_strat));

  if (g->sine_strat) {
    (g->sine_strat_used)=1;
    wr_analyze_sine(g,db,child_db); // this may turn off use_sine_strat
  } else {
#ifdef SINEORDER    
    if (g->sine_order_strat) {
      wr_analyze_sine(g,db,child_db); // this may turn off use_sine_strat // !!! EXPERIMENT 6
    }  
#endif    
    (g->sine_strat_used)=0;
  }  

  // Reverse the order
#ifdef SINEORDER  
  int sine_k;  
#endif  

  if (!(g->queryfocus_strat) || ((g->sin_clause_count) < 100000)) {
    //printf("\nstarting to build a clause vector:\n");
    vecflag=1;
    cell2=(dbmemsegh(child_db)->clauselist);
    lastcell=0;
    n=0;
    while(cell2) {
      cellptr2=(gcell *) offsettoptr(child_db, cell2);
      rec=offsettoptr(child_db,cellptr2->car); 

      if (1 || wr_get_cl_sine_k(g,rec)) {

        //printf("\nn: %d ",n);
        //wr_print_clause(g,rec); 

        j=(n*2)+1;  
        weight=wr_calc_clause_weight(g,rec,&size,&depth,&length); 
  #ifdef WEIGHT_PREFER_GOALS_ASSUMPTIONS      
        if (wr_cl_is_goal(g, rec)) weight=weight*1;
        else if (wr_cl_is_assumption(g, rec)) weight=weight*1000;
        //else if (wr_cl_is_extaxiom(g, rec)) weight=weight*20000;
        else weight=weight*10000;
  #else
  #ifdef SINEORDER 
        if (g->sine_order_strat) {
          sine_k=wr_get_cl_sine_k(g,rec); 
          //if (sine_k==121) sine_k=4;        
          //else if (!sine_k) sine_k=200; 
          //if (sine_k) sine_k=1;
          //else sine_k=10;            
          if (!sine_k) sine_k=200;    
        }  
  #endif         
        if (g->cl_pick_queue_strategy) {

          //wr_print_clause(g,rec);
          //printf(" weight %d sine_k %d\n",weight,sine_k);
  #ifdef SINEORDER         
          if (g->sine_order_strat) {
            weight=weight+(sine_k*10);          
          }  
  #endif        
          /*     
          if (wr_cl_is_goal(g, rec)) weight=weight*1;
          else if (wr_cl_is_assumption(g, rec)) weight=weight*100;
          else weight=weight*10000;                   
          */
          if (wr_cl_is_goal(g, rec)) weight=weight*1;
          else if (wr_cl_is_assumption(g, rec)) weight=weight*100;
          //else if (wr_cl_is_extaxiom(g, rec)) weight=weight*20000;
          else weight=weight*10000;     

          //printf(" total weight %d\n",weight);

        } else {

          //wr_print_clause(g,rec);
          //printf(" weight %d sine_k %d\n",weight,sine_k);

    #ifdef SINEORDER       
          if (g->sine_order_strat) {
            weight=weight+(sine_k*10*10);
          }  
    #endif    
             
          if ((g->sin_goal_count)*10 < (g->sin_clause_count)) {
            if (wr_cl_is_goal(g, rec)) weight=weight*1;
            else if (wr_cl_is_assumption(g, rec)) weight=weight*1;
            //else if (wr_cl_is_extaxiom(g, rec)) weight=weight*4;
            else weight=weight*2;
          }
          
          //printf(" total weight %d\n",weight);

        }
  #endif      
        (g->tmp_sort_vec)=wr_vec_store(g,g->tmp_sort_vec,j,(gint)weight);
        (g->tmp_sort_vec)=wr_vec_store(g,g->tmp_sort_vec,j+1,(gint)rec);
      }  
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
    
    if ((g->sine_strat_used) && (g->sine_k_values) && (g->sine_k_bytes) 
        && !wr_get_cl_sine_k(g,rec)) {
      // take next element
      //printf("\ndropped clause by sine ");
      //wr_print_clause(g,rec);
      //printf("\n");
      goto LOOPEND;
      if (vecflag) {
        if (g->reverse_clauselist_strat) i--;
        else i++;
      } else {      
        cell=cellptr->cdr;
      }   
      continue;
    }      
    /*
    printf("\ni %d\n",i);
      wr_print_clause(g,rec);
    printf("weight %ld sine k %d\n",
      (gint)((g->tmp_sort_vec)[(i*2)+1]), 
      wr_get_cl_sine_k(g,rec));
    */
#ifdef DEBUG   
    wr_printf("\n next rec from db: ");
    wg_print_record(db,rec);
    wr_printf("\n");    
#endif
     
    if (wg_rec_is_rule_clause(db,rec)) {
      rules_found++;      
      clmeta=wr_calc_clause_meta(g,rec,given_cl_metablock);
      if (wr_tautology_cl(g,rec)) {
          goto LOOPEND;
      }
      wr_add_cl_to_unithash(g,rec,clmeta);
      wr_add_cl_to_doublehash(g,rec);

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
      if ((g->res_arglen_limit)==1) {
        if (wr_count_cl_nonans_atoms(g,rec) > 1) {
          goto LOOPEND;
          if (vecflag) {
            if (g->reverse_clauselist_strat) i--;
            else i++;
          } else {      
            cell=cellptr->cdr;
          }   
        }
      }

      if (g->queryfocus_strat && wr_initial_select_active_cl(g,(gptr)rec)) {   
        // calculate resolvability: (g->tmp_resolvability_vec)       
        wr_calc_clause_resolvability(g,rec,1,0);      
        given_cl=wr_process_given_cl(g,(gptr)rec, g->given_termbuf);
        if ( ((gint)given_cl==ACONST_FALSE) || ((gint)given_cl==ACONST_TRUE) ||
             (given_cl==NULL) ) {  
          goto LOOPEND;             
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
          goto LOOPEND;
          cell=cellptr->cdr;
          continue; 
        }    
        if (g->use_strong_unit_cutoff) wr_cl_store_res_units(g,given_cl_as_active);
      } else {       
        if (g->goalweight_normal_strat) { //(!(g->cl_pick_queue_strategy)) {
          weight=wr_calc_clause_weight(g,rec,&size,&depth,&length);
          //if (wr_cl_is_goal(g, rec) && weight>2) weight=weight-2;
          //else if (wr_cl_is_assumption(g, rec) && weight>3) weight=weight-1;
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
          goto LOOPEND;     
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
        if (g->goalweight_normal_strat) { // (!(g->cl_pick_queue_strategy)) {
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
LOOPEND:    
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


void wr_activate_from_termhash(glb* g, gint* hasharr) {
  int i,j;
  cvec bucket;
  int weight;
  int size,depth,length;
  //gint resmeta;
  gptr clause;
  double avg;
  //gint cl_metablock[CLMETABLOCK_ELS];
 
  //printf("\nhashvec len %ld ptr %lx and els:\n",hasharr[0],(unsigned long int)hasharr);  
  //printf("\nhashvec len %ld els:\n",hasharr[0]);  
  for(i=1;i<hasharr[0];i++) {    
    if (hasharr[i]) {
      bucket=rotp(g,(hasharr[i]));
      //printf("\nhashslot i %d node %ld size %ld next free %ld\n",
      //        i,hasharr[i],bucket[0],bucket[1]);
      //printf("\nhashslot i %d size %ld next free %ld\n",i,bucket[0],bucket[1]);        
      if (1) {
        for(j=2;j<bucket[0] && j<bucket[1]; j=j+2) {
          /*
          wr_printf("term ");
          wr_print_term(g,bucket[j]);
          //printf(" path %d in cl ",0);
          //CP1
          //printf("\nj %d bucket[j+1] %ld \n",j,bucket[j+1]);
          //CP2          
          wr_printf(" in clause ");
          wr_print_clause(g,rotp(g,bucket[j+1]));          
          wr_printf(" as rec ");
          wg_print_record(g->db,rotp(g,bucket[j+1]));
          wr_printf("\n");
          */
          clause=rotp(g,bucket[j+1]);
          weight=wr_calc_clause_weight(g,clause,&size,&depth,&length);
          ++(g->stat_kept_cl);
          avg=(g->avg_kept_weight); 
          avg+=(weight-avg)/((g->stat_kept_cl)+1);
          //resmeta=wr_calc_clause_meta(g,clause,cl_metablock);
          //wr_add_cl_to_unithash(g,clause,resmeta);
          if (g->use_strong_unit_cutoff) wr_cl_store_res_units(g,clause);
          wr_push_cl_clpick_queues(g,(g->clpick_queues),clause,weight); 
        }
      }  
    }
  }
}

/*
 weight=wr_calc_clause_weight(g,res,&size,&depth,&length);
    avg=(g->avg_kept_weight);  
    if (!wr_derived_weight_check(g,avg,weight,size,depth,length,0,0)) {
      (g->stat_weight_discarded_cl)++;
      CVEC_NEXT(g->build_buffer)=initial_queue_termbuf_next; // initial next-to-take restored
      if (g->print_derived_cl) wr_printf("\nw discarded overweight");
      return;
    }
    ++(g->stat_kept_cl);
    avg+=(weight-avg)/((g->stat_kept_cl)+1);
    (g->avg_kept_weight)=avg;
    resmeta=wr_calc_clause_meta(g,res,cl_metablock);
    wr_add_cl_to_unithash(g,res,resmeta);
    if (g->use_strong_unit_cutoff) wr_cl_store_res_units(g,res);
    wr_push_cl_clpick_queues(g,(g->clpick_queues),res,weight);   
    tmp=wr_cl_create_propinst(g,res);    
    if (tmp==2) {  return;  }
*/

int wr_print_all_clauses(glb* g, void* child_db) {
  void* db=child_db;
  void *rec=NULL; 
  gcell *cellptr2; 
  gint cell2;
  int i,n;  
  char namebuf[1000];  
  //char orderbuf[80];
  char* buf;
  int blen, bpos;
  gint history, name;
  int objflag, nameflag, clnr;
  gptr historyptr;
  char* namestr;
  char* priorstr;
  gint priority,decprior,enctype;
  
  cell2=(dbmemsegh(child_db)->clauselist);
  n=0;
  while(cell2) {
    cellptr2=(gcell *) offsettoptr(child_db, cell2);   
    if (cellptr2->car) {
      (g->tmp_sort_vec)=wr_vec_store(g,g->tmp_sort_vec,n+1,(gint)(cellptr2->car)); 
    }  
    /*
    rec=offsettoptr(child_db,cellptr2->car);
    wr_print_clause(g,rec);
    printf("\n");
    */
    cell2=cellptr2->cdr;      
    n++;
  } 
  if (!n) return 0;  
   // create buf for printing
  //namebuf[0]=0;
  blen=1000;
  bpos=0;
  buf=wr_malloc(g,blen);
  if (buf==NULL) {   
    return -1;
  }   
  if (g->print_json) {
    printf("[");
  }
  clnr=0;
  for(i=n; i; i--) {   
    clnr++;
    cell2=(g->tmp_sort_vec)[i];   
    rec=offsettoptr(child_db,cell2);

    bpos=0;  
    if (!wr_str_guarantee_space(g,&buf,&blen,bpos+100)) {
      if (buf) wr_free(g,buf);
      return -1;
    }
    
    history=wr_get_history(g,rec);
    //len=wg_get_record_len(db,otp(db,history));
    historyptr=otp(db,history);    
    name = wr_get_history_record_field(g,historyptr,HISTORY_NAME_POS);
    priority = wr_get_history_record_field(db,historyptr,HISTORY_PRIORITY_POS);
    decprior=wr_decode_priority(g,priority);
     
    if (decprior==WR_HISTORY_AXIOM_ROLENR) priorstr="axiom";
    else if (decprior==WR_HISTORY_ASSUMPTION_ROLENR) priorstr="assumption";   
    else if (decprior==WR_HISTORY_GOAL_ROLENR) priorstr="negated_conjecture"; 
    else priorstr="axiom";

    nameflag=0; // if this is 1, original name is used
    namestr=NULL;
    enctype=0;
    if (name) {
      // check for json only: original name or not
      enctype=wg_get_encoded_type(db,name);
      if (enctype==WG_URITYPE || enctype==WG_STRTYPE) {       
        namestr=wg_decode_str(db,name);
        if (namestr[0] && strncmp(namestr,"cl_",3)) {
          // name is NOT cl_, i.e. it is originally present: invent a cl_-prefixed name            
          nameflag=1;          
        }                 
      }     
    }
    if (!namestr || !namestr[0]) {
      sprintf(namebuf,"cl_%d",clnr);
      namestr=namebuf;
    }  
    //printf("\nnamebuf %s namestr %s nameflag %d enctype %ld\n",namebuf,namestr,nameflag,enctype);
    objflag=0;
    if (g->print_json) {
      if ((nameflag && namestr[0]) && (decprior && decprior!=WR_HISTORY_AXIOM_ROLENR)) {        
        if (wg_contains_dquote(namestr)) {
          bpos+=snprintf(buf+bpos,blen-bpos,"\n{\"@name\":");
          bpos+=wg_print_dquoted(&buf,blen,bpos,namestr,0,0);
          bpos+=snprintf(buf+bpos,blen-bpos,",\"@role\":\"%s\",\"@logic\":",priorstr);
        } else {
          bpos+=snprintf(buf+bpos,blen-bpos,"\n{\"@name\":\"%s\",\"@role\":\"%s\",\"@logic\":",
                        namestr,priorstr);
        }  
        objflag=1;
        //bpos+=snprintf(buf+bpos,blen-bpos,"\n{\"name\":\"%s\",\"role\":\"%s\",\"logic\":",namestr,priorstr);
        //objflag=1;
      } else if (nameflag && namestr[0]) {
         if (wg_contains_dquote(namestr)) {
          bpos+=snprintf(buf+bpos,blen-bpos,"\n{\"@name\":");
          bpos+=wg_print_dquoted(&buf,blen,bpos,namestr,0,0);
          bpos+=snprintf(buf+bpos,blen-bpos,",\"@logic\":");
        } else {
          bpos+=snprintf(buf+bpos,blen-bpos,"\n{\"@name\":\"%s\",\"@logic\":",namestr);
        }  
        objflag=1;
        //bpos+=snprintf(buf+bpos,blen-bpos,"\n{\"name\":\"%s\",\"logic\":",namestr);
        //objflag=1;
      } else if (decprior && decprior!=WR_HISTORY_AXIOM_ROLENR) {
        bpos+=snprintf(buf+bpos,blen-bpos,"\n{\"@role\":\"%s\",\"@logic\":",priorstr);
        objflag=1;
      }
      if (!wr_str_guarantee_space(g,&buf,&blen,bpos+100)) {
        if (buf) wr_free(g,buf);
        return -1;        
      }     
      if (!objflag) {        
        bpos+=snprintf(buf+bpos,blen-bpos,"\n");
      }       
      bpos=wr_strprint_clause(g,rec,&buf,&blen,bpos);  
      if (bpos<0) return -1;       
      if (!wr_str_guarantee_space(g,&buf,&blen,bpos+100)) {
        if (buf) wr_free(g,buf);
        return -1;        
      }
      if (objflag) {        
        bpos+=snprintf(buf+bpos,blen-bpos,"}");
      } else {
        //bpos+=snprintf(buf+bpos,blen-bpos,"\n");
      }
      if (i>1) {
        bpos+=snprintf(buf+bpos,blen-bpos,",");
      }
    } else if (g->print_proof_tptp) {
      if (clnr!=1) {
        bpos+=snprintf(buf+bpos,blen-bpos,"\n");
      } 
      if (nameflag && wg_should_quote(namestr)) {
        bpos+=snprintf(buf+bpos,blen-bpos,"cnf('%s',%s,",namestr,priorstr);
      } else {
        bpos+=snprintf(buf+bpos,blen-bpos,"cnf(%s,%s,",namestr,priorstr);
      }
      bpos=wr_strprint_clause(g,rec,&buf,&blen,bpos);
      if (bpos<0) return -1;
      if (!wr_str_guarantee_space(g,&buf,&blen,bpos+100)) {
        if (buf) wr_free(g,buf);
        return -1;
      }
      bpos+=snprintf(buf+bpos,blen-bpos,").");   
    } else {      
      bpos=wr_strprint_clause(g,rec,&buf,&blen,bpos);
      if (bpos<0) return -1;
      bpos+=snprintf(buf+bpos,blen-bpos,"\n");  
    }  
    printf("%s",buf);
  }
  if (g->print_json) {
    printf("\n]\n");
  } else if (g->print_proof_tptp) {
    printf("\n");
  }
  return n;       
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
  (g->print_history_extra)=0; 
  
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
  (g->print_history_extra)=0;
  
}

static void wr_set_tiny_extra_printout(glb* g) {
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
  (g->print_history_extra)=1;
  
}

static void wr_set_tiny_runs_printout(glb* g) {
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
  (g->print_runs)=1;
  (g->print_stats)=0;
  (g->print_history_extra)=1;
  
}

static void wr_set_low_printout(glb* g) {
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
  (g->print_runs)=1;
  (g->print_stats)=1;
  (g->print_datastructs)=0;
  (g->print_history_extra)=1;
  
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
  (g->print_history_extra)=1;
  
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
  (g->print_history_extra)=1;
  
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
  (g->print_history_extra)=1;
  
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
  (g->print_history_extra)=1;
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
  wr_printf("total seconds: %f\n",
   (float)(clock() - (g->allruns_start_clock)) / (float)CLOCKS_PER_SEC);
  wr_printf("stat_given_used: %d\n",g->stat_given_used);
  wr_printf("stat_given_used_at_endgame: %d\n",g->stat_given_used_at_endgame);
  wr_printf("stat_given_candidates:   %d\n",g->stat_given_candidates); 
  wr_printf("stat_given_candidates_at_endgame: %d\n",g->stat_given_candidates_at_endgame);
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
  wr_printf("stat_backward_subsumed: %d\n",g->stat_backward_subsumed);  
  wr_printf("stat_propagated_subsumed: %d\n",g->stat_propagated_subsumed);
  
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
  fflush(stdout);
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

    wr_printf("\n\npos doubles :\n"); 
    wr_print_offset_termhash(g,rotp(g,g->hash_pos_grounddoubles)); 
    //wr_clterm_hashdata_print(g,hasharrpos);
    wr_printf("\n\nneg doubles :\n"); 
    wr_print_offset_termhash(g,rotp(g,g->hash_neg_grounddoubles)); 
    //wr_clterm_hashdata_print(g,hasharrneg);
    
  }

  wr_printf("\n\n===== end of active clauses in a resolvable literal hash  =====  \n");


}
/*
void wg_show_db_error(void* db) {
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
*/

#ifdef GLOBAL_UNITS

int store_units_globally(void* db, glb* g) {
  printf("\nstore_units_globally called\n");

  gptr actptr, hasharrpos, hasharrneg; //, hashvec;
  int dbused;
  gint iactive, iactivelimit;
  glb* shared_g;
  void* global_db;

  //printf("\n db %ld g %ld r_kb_g(g) %ld db_rglb(db) %ld\n",(gint)db,(gint)g,(gint)(r_kb_g(g)),(gint)(db_rglb(db)));
  //shared_g=r_kb_g(g);
  //shared_g=db_rglb(db);
  shared_g=(g->kb_g);
  //global_db=shared_g->kb_db;
  global_db=shared_g->db;
  
  //wr_show_database_details(g,db, "local"); // works ok: if first arg given, will use this (local)

  //wr_show_database_details(NULL,db, "global"); // works ok: if first arg NULL, will try to use global

  //wr_show_database_details(db_rglb(db),db, "global"); // segfaults!

  //wr_show_database_details(g,NULL, "local?"); // works, shows local

  // g->kb_db is external db, g->kb_g is g inside this external db
  //printf("\ng->inkb %ld g->kb_g %ld, g->kb_db %ld g->local_db %ld\n",
  //      (gint)(g->inkb),(gint)(g->kb_g),(gint)(g->kb_db),(gint)(g->local_db));
   // somewhy (g->kb_db) is 0 !!
  //glb* intern_g;
  //intern_g=(g->kb_g);

  //printf("\nshared_g->inkb %ld shared_g->kb_g %ld, shared_g->kb_db %ld shared_g->local_db %ld\n",
  //      (gint)(shared_g->inkb),(gint)(shared_g->kb_g),(gint)(shared_g->kb_db),(gint)(shared_g->local_db));

  // db 140522599059456 g 30152464 r_kb_g(g) 30067504 db_rglb(db) 140522599194176
  //        g->inkb 0        g->kb_g 30067504,        g->kb_db 0                      g->local_db 140521599057936
  // shared_g->inkb 1 shared_g->kb_g 0,        shared_g->kb_db 140058015969280 shared_g->local_db 0

  
 
  //wr_show_database_details(g->kb_g,g->kb_db, "global?"); // works, shows global

  //wr_show_database_details(shared_g,shared_g->kb_db, "global???"); // works, shows global
  wr_show_database_details_shared(shared_g,NULL, "global???"); // works, shows global, uses shared_g->db internally


  // g->
  // void* db;             /**< mem database used as a base for offset: either kb_db or child_db here */
  // void* kb_db;          /**< shared-mem knowledge base */
  // void* child_db;       /**< local child db: if kb_db present, then child_db->kb_db is set to this */
  // gint db_offset;       /**< offset of the db ptr */
  // gint inkb;            /**< 1 if g struct is inside shared kb, 0 otherwise */
  // void* kb_g;            /**< if external shared mem kb_db is present, then the g ptr inside kb_db, else NULL */ 
  // void* local_db;        /** this will be set to local db in any case in rmain.c */  
  // char* filename;       /**< input filename */
  // char* outfilename;       /**< output filename */
  

  //exit(0);

  wr_printf("\n===== active clauses as used for subsumption =====  \n");
  for(dbused=0; dbused<2; dbused++) {
    if (dbused==0) {
      // local db
      actptr=rotp(g,g->clactivesubsume); 
      hasharrpos=rotp(g,g->hash_pos_active_groundunits);
      hasharrneg=rotp(g,g->hash_neg_active_groundunits);  
      //wr_printf("\n-- in local db: -- \n");         
    } else {     
      // global db   
      /*
      if (r_kb_g(g)) {        
        actptr=rotp(g,(r_kb_g(g))->clactivesubsume); 
        hasharrpos=rotp(g,(r_kb_g(g))->hash_pos_active_groundunits);
        hasharrneg=rotp(g,(r_kb_g(g))->hash_neg_active_groundunits);
        wr_printf("\n -- in shared db: -- \n");         
      } 
      */ 
      //else break;      
    }
    printf("\n---storing----\n");
    // ground units:
    if (!dbused && r_kb_g(g)) {
      //wr_printf("\npos ground units hasharrpos %ld\n",(gint)hasharrpos); 
      wr_store_offset_termhash(db,g,hasharrpos,1); //rotp(g,(r_kb_g(g))->hash_pos_active_groundunits));
      //wr_store_offset_termhash(g,rotp(g,(r_kb_g(g))->hash_pos_active_groundunits),1);
    }
    if (!dbused && r_kb_g(g)) {
      //wr_printf("\nneg ground units hasharrneg %ld\n",(gint)hasharrneg); 
      wr_store_offset_termhash(db,g,hasharrneg,0);//rotp(g,(r_kb_g(g))->hash_neg_active_groundunits));
    }

    //wr_printf("\nshared neg ground units\n"); 
    //printf("\ndb %ld, shared_g %ld, shared_g->shared_hash_neg_groundunits %ld \n",
    //      (gint)db,(gint)shared_g,(gint)(shared_g->shared_hash_neg_groundunits));
      //wr_print_offset_termhash(g,hasharrpos);


    //wr_show_database_details(g,db, "local");
    
    //wr_show_database_details(db_rglb(db_get_kb_db(db)),db_get_kb_db(db), "global");
    wr_show_database_details_shared(shared_g,global_db, "global+++");

    exit(0);
    /*
    printf("\n\n** hash_neg_groundunits\n");
    wr_print_termhash(g,rotp(g,g->hash_neg_groundunits));
    printf("\n** hash_pos_groundunits\n");
    wr_print_termhash(g,rotp(g,g->hash_pos_groundunits));  
    printf("\n");

    wr_print_termhash(g,hasharrneg);  
    wr_print_termhash(g,hasharrpos);
    wr_print_termhash(shared_g,rotp(g,shared_g->hash_pos_groundunits));
    */
    //wr_print_termhash(g,(r_kb_g(g))->hash_neg_active_groundunits);
     //wr_print_termhash(shared_g,shared_g->shared_hash_neg_groundunits); 
     //wr_print_termhash(shared_g,rotp(shared_g,shared_g->shared_hash_neg_groundunits));

    continue;

    printf("\n---printing----\n");
    // ground units:
    if (1) {
      wr_printf("\npos ground units\n"); 
      //wr_print_offset_termhash(g,hasharrpos);
      wr_print_termhash(g,hasharrpos);
    }
    if (1) {
      wr_printf("\nneg ground units\n"); 
      //wr_print_offset_termhash(g,hasharrneg);
      wr_print_termhash(g,hasharrneg);
    }
    if (1) {
      wr_printf("\nshared neg ground units\n"); 
      printf("\ndb %ld, shared_g %ld, shared_g->shared_hash_neg_groundunits %ld \n",
          (gint)db,(gint)shared_g,(gint)(shared_g->shared_hash_neg_groundunits));
      //wr_print_offset_termhash(g,hasharrpos);
      //wr_print_termhash(g,hasharrpos);
      wr_print_termhash(shared_g,rotp(shared_g,shared_g->shared_hash_neg_groundunits));
      //wr_print_termhash(shared_g,shared_g->shared_hash_neg_groundunits);
    }    
    if (1) {
      wr_printf("\nshared pos ground units\n"); 
      //wr_print_offset_termhash(g,hasharrpos);
      //wr_print_termhash(g,hasharrpos);
      wr_print_termhash(shared_g,rotp(shared_g,shared_g->shared_hash_pos_groundunits));
    } 
    // non-ground-units:
    wr_printf("\nactive:\n\n");  
    iactivelimit=CVEC_NEXT(actptr);
    for(iactive=CVEC_START; iactive<iactivelimit; iactive+=CLMETABLOCK_ELS) {
      wr_print_clause(g,rotp(g,(actptr[iactive+CLMETABLOCK_CL_POS])));
      wr_printf("\n");
    }  
  }
  exit(0);

  wr_printf("\n\n===== end of active clauses as used for subsumption  =====  \n\n");

  if (r_kb_g(g)) {        
    actptr=rotp(g,(r_kb_g(g))->clactivesubsume); 
    hasharrpos=rotp(g,(r_kb_g(g))->hash_pos_active_groundunits);
    hasharrneg=rotp(g,(r_kb_g(g))->hash_neg_active_groundunits);
    wr_printf("\n -- in shared db: -- \n");         
  } 

  return 0;
}


void wr_store_offset_termhash(void* db, glb* g, gint* hasharr, int pos) {
  int i,j;
  //gint* tohasharr;
  cvec bucket;
  gint resmeta;
  //gint hash;
  gptr cl;
  gint cl_metablock[CLMETABLOCK_ELS];
  glb* shared_g;
  //void* global_db;

  shared_g=(g->kb_g);
  //global_db=shared_g->db;

  //printf("\nwr_store_offset_termhash called \n");
  /*
  if (pos) {
    tohasharr=rotp(g,(shared_g->hash_pos_active_groundunits));
  } else {
    tohasharr=rotp(g,(shared_g->hash_neg_active_groundunits));
  }
  */
  //printf("\n g %ld shared_g %ld tohasharr %ld (shared_g->hash_neg_active_groundunits %ld",
  //(gint)g,(gint)shared_g,(gint)tohasharr,(gint)(shared_g->hash_neg_active_groundunits));
  //printf("\nhashvec len %ld ptr %lx and els:\n",hasharr[0],(unsigned long int)hasharr);  
  //wr_printf("\nhashvec len %ld els:\n",hasharr[0]);  
  for(i=1;i<hasharr[0];i++) {    
    if (hasharr[i]) {
      bucket=rotp(g,(hasharr[i]));      
      //wr_printf("\nhashslot i %d size %ld next free %ld\n",i,bucket[0],bucket[1]);        
      if (1) {
        for(j=2;j<bucket[0] && j<bucket[1]; j=j+2) {
          /*
          wr_printf("term ");
          wr_print_term(g,bucket[j]);
          //printf(" path %d in cl ",0);
          //CP1
          //printf("\nj %d bucket[j+1] %ld \n",j,bucket[j+1]);
          //CP2          
          wr_printf(" in clause ");
          wr_print_clause(g,rotp(g,bucket[j+1]));          
          wr_printf(" as rec ");
          wg_print_record(db,rotp(g,bucket[j+1]));
          wr_printf("\n");
          // -- now storing to global ---
          printf("\nabout to store\n");
          */          
          cl=rotp(g,bucket[j+1]);
          resmeta=wr_calc_clause_meta(shared_g,cl,cl_metablock);
          wr_add_cl_to_shared_unithash(g,shared_g,cl,resmeta); 
        }
      }  
    }
  }
}


#endif

#ifdef __cplusplus
}
#endif
