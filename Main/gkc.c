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

 /** @file gkc.c
 *  GKC reasoner: the main command line tool
 */

/* ====== Includes =============== */

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#ifndef _WIN32
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#endif

#ifdef _WIN32
#include <conio.h> // for _getch
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#include "../config-w32.h"
#else
#include "../config.h"
#endif
#include "../Db/dballoc.h"
#include "../Db/dbmem.h"
#include "../Db/dbdata.h"
#include "../Db/dbdump.h"
#include "../Db/dblog.h"
//#include "../Db/dbquery.h"
#include "../Db/dbutil.h"
#include "../Db/dblock.h"
#include "../Db/dbjson.h"
#include "../Db/dbschema.h"
#ifdef USE_REASONER
#include "../Builtparser/dbparse.h"
#include "../Reasoner/rmain.h"
#include "../Reasoner/init.h"
#include "../Reasoner/analyze.h"
#include "../Reasoner/clstore.h"
#include "../Db/dbapi.h"
#include "../Parser/jsparse.h"
#endif

#ifndef _WIN32
#include <sys/wait.h> 
#include <unistd.h> 
#include <time.h>
#endif




/* ====== Private defs =========== */

/*
#ifdef _WIN32
#define sscanf sscanf_s  // XXX: This will break for string parameters 
#endif
*/

#define FLAGS_FORCE 0x1
#define FLAGS_LOGGING 0x2

#define DEFAULT_PARALLEL 4 // set to 1 for no parallel
#define DEFAULT_TPTP 0 // set to 1 for default tptp output
#define TPTP

//#define SHOWTIME

/* Helper macros for database lock management */

#define RLOCK(d,i) i = wg_start_read(d); \
    if(!i) { \
        fprintf(stderr, "Failed to get database lock\n"); \
        break; \
    }

#define WLOCK(d,i) i = wg_start_write(d); \
    if(!i) { \
        fprintf(stderr, "Failed to get database lock\n"); \
        break; \
    }

#define RULOCK(d,i) if(i) { \
        wg_end_read(d,i); \
        i = 0; \
    }
#define WULOCK(d,i)  if(i) { \
        wg_end_write(d,i); \
        i = 0; \
    }

/* ======= Private protos ================ */

int gkc_main(int argc, char **argv);
void sig_handler(int signum);
void usage(char *prog);
gint parse_shmsize(char *arg);
gint parse_flag(char *arg);
int parse_memmode(char *arg);
char** parse_cmdline(int argc, char **argv, char** cmdstr, int* mbnr, int* mbsize, 
          int* parallel, int* tptp, int* json, 
          char ***lstringsptr, char ***ljstringsptr, char ***lfilesptr, char **stratfile,
          int* convert, int* clausify, int* seconds, int* retcode, int* printlevel, int* printderived,
          char** strattext);          
void wg_set_kb_db(void* db, void* kb);
void segment_stats(void *db);
void wg_show_strhash(void* db);
void wg_show_database(void* db);
void gkc_show_cur_time(void);

void initial_printf(char* s);
void err_printf(int json, char* s);
void err_printf2(int json, char* s1, char* s2);

int wg_import_data_file(void *db, char* filename, int iskb, int* informat, int askpolarity, int* askinfo);
int wg_import_data_string(void *db, char* instr, int iskb, int* informat, int askpolarity, int* askinfo,
                          int jsflag);

int wg_seems_json_string(void* db, char* str);

#ifndef _WIN32
int gkc_ltb_main(int argc, char **argv);
#endif

void wr_make_batch_axiom_fullname(char* probname, char* probfullname, char* batchname, int formulation);
void wr_make_batch_prob_fullname(char* probname, char* probfullname, char* batchname);
void wr_output_batch_prob_failure(char* probname, char* outfullname, char* failure);

//#define SHOW_CONTENTS 1
//#undef SHOW_CONTENTS

/* ====== Emscripten specials ============== */
/*
#ifdef __EMSCRIPTEN__

#include <emscripten.h>


// start_timer(): call JS to set an async timer for 500ms
EM_JS(void, start_timer, (), {
  Module.timer = false;
  setTimeout(function() {
    Module.timer = true;
  }, 500);
});

// check_timer(): check if that timer occurred
EM_JS(int, check_timer, (), {
  return Module.timer;
});

#endif
*/
/* ====== Functions ============== */

/*
how to set 500 meg of shared memory:

su
echo 500000000  > /proc/sys/kernel/shmmax
*/

void* global_shmptr; // this is used by signal_handler of timer signal to cleanup

/** top level */

int main(int argc, char **argv) {
  int code;
  code=gkc_main(argc,argv);
  //printf("\n exit code %d\n",code);
  return code;
}

/** top level of the reasoner
*
*
*/

int gkc_main(int argc, char **argv) {

  char *shmname = NULL;
  char shmnamebuf[16];
  void *shmptr = NULL;
  void *shmptrlocal = NULL;
  int i; //, scan_to;
  gint shmsize=0, shmsize2=0;
  //wg_int rlock = 0;
  //wg_int wlock = 0;
  int mbnr=0, mbsize=0, parallel=DEFAULT_PARALLEL; // parsed from cmd line
  char* cmdstr=NULL; // parsed from cmd line
  char** cmdfiles; // parsed from cmd line
  int cmdfileslen; // computed below
  char** lstrings=NULL; // input strings (logic text) parsed from cmd line
  int strnr;
  char** ljstrings=NULL; // input js strings (logic text) parsed from cmd line
  int jstrnr;
  char** lfiles=NULL; // input filenames for logic text parsed from cmd line
  int filenr;
  char* stratfile=NULL;
  char* strattext=NULL;
  int tmp;
  int retcode=0;
  int informat=0; // 0 if not set, 1 if tptp fof
  int tptp=DEFAULT_TPTP; // 0 if not tptp format output
  int json=0;
  int convert=0;
  int clausify=0;
  int seconds=0; // 0 if no time limit, other value for time limit in seconds
  //int islocaldb=0; // lreasoner sets to 1 to avoid detaching db at the end
  int printlevel=10;
  int printderived=0;
  int askinfo=0;
  int askpolarity=1;
  clock_t allruns_start_clock;  
  char inputname[1000];

  allruns_start_clock=clock();
  global_shmptr=NULL;
  if (argc<2) {
    usage(argv[0]);
    return(0);
  }

#ifndef _WIN32  
#ifdef TPTP
  tmp=gkc_ltb_main(argc,argv);
  if (tmp==1) return(0);
#endif  
#endif
 
  inputname[0]=0;
  cmdfiles=parse_cmdline(argc,argv,&cmdstr,&mbnr,&mbsize,&parallel,
                         &tptp,&json,&lstrings,&ljstrings,&lfiles,&stratfile,
                         &convert,&clausify,&seconds,&retcode,&printlevel,&printderived,
                         &strattext);                         
  if (retcode) return retcode;
  if (tptp && json) {
    err_printf(json,"do not set both -tptp and -json output parameters at the same time");    
    return 0;
  }
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
  if (seconds) {
    signal(SIGALRM,sig_handler);
    alarm(seconds);
  } 
#endif  
#if defined(__EMSCRIPTEN__) || defined(_WIN32)
  parallel=0;
#endif  

  //printf("\n parsed cmdstr %s mbnr %d mbsize %d\n",cmdstr,mbnr,mbsize);
  cmdfileslen=0;
  for(i=0;i<argc;i++) {    
    //printf("\n cmdfiles i %d value %s",i,cmdfiles[i]);
    if (cmdfiles[i]!=NULL) cmdfileslen++;
    else break;
  }  
  
  if (cmdstr==NULL) {
    cmdstr="-prove";
  } else if (!(strncmp(cmdstr,"-help",10)) || 
      !(strncmp(cmdstr,"--help",10)) ) {
    usage(argv[0]);
    return(0);
  } else if (!(strncmp(cmdstr,"-version",10)) || 
             !(strncmp(cmdstr,"--version",10)) ) {
    wg_print_code_version();
    return(0); 
  }

  //printf("\ncmdstr %s cmdfileslen %d\n",cmdstr,cmdfileslen);

  if (mbsize==0) {
    // default
#ifdef __EMSCRIPTEN__    
    shmsize=(gint)100*(gint)1000000; // has to be small for wasm: 100 MB here   
#else    
#ifdef _WIN32
#ifdef _WIN64    
    shmsize = (gint)5000000000; // 5 gb default for large csr
    shmsize2 = (gint)1000000000; // local db (used when shared mem used) is smaller 
#else
    shmsize=(gint)1000*(gint)1000000; // has to be small for win32 
    shmsize2 =(gint)500*(gint)1000000; // local db (used when shared mem used) is smaller   
#endif    
#else
    // non-windows case
    shmsize = (gint)5000000000; // 5 gb default for large csr
    shmsize2 = (gint)1000000000; // local db (used when shared mem used) is smaller
  } else {  
#endif            
#endif    
    // given on cmd line  
    shmsize=(gint)mbsize*(gint)1000000; // mbsize given on cmdline is in megabytes
    shmsize2 = shmsize;
  }

  if (mbnr==0) {
    shmname="1000";
  } else {
    snprintf(shmnamebuf,15,"%d",mbnr);   
    shmname=shmnamebuf;
  }
   // -prove
  
  if(!(strncmp(cmdstr,"-prove",15))) {
    wg_int err=0;

    if (!lstrings && !ljstrings && !lfiles) {
      err_printf(json,"an input file or string as an argument is needed");
      return(1);
    }  
    shmptr=wg_attach_local_database(shmsize);
    if(!shmptr) {
      err_printf(json,"failed to attach local database");
      return(1);
    }
    global_shmptr=shmptr;    
    //islocaldb=1;   
    //err = wg_import_otter_file(shmptr,cmdfiles[1],0,&informat); 
    (dbmemsegh(shmptr)->max_forks)=parallel;
    (dbmemsegh(shmptr)->tptp)=tptp;
    (dbmemsegh(shmptr)->json)=json;
    (dbmemsegh(shmptr)->convert)=convert;
    (dbmemsegh(shmptr)->clausify)=clausify;        
    (dbmemsegh(shmptr)->printlevel)=printlevel;
    (dbmemsegh(shmptr)->printderived)=printderived;
    (dbmemsegh(shmptr)->max_seconds)=seconds;
    (dbmemsegh(shmptr)->allruns_start_clock)=allruns_start_clock;

    if (lfiles) {      
      for(filenr=0; lfiles[filenr] && !err; filenr++) {
        //printf("\nfilesnr %d files %s\n",filenr,lfiles[filenr]); 
        if (strlen(inputname)+strlen(lfiles[filenr])<900) {
          strcat(inputname,lfiles[filenr]);      
          strcat(inputname," ");
        }
        err=wg_import_data_file(shmptr,lfiles[filenr],0,&informat,askpolarity,&askinfo);
      }  
    }  
    if (lstrings) {      
      for(strnr=0; lstrings[strnr] && !err; strnr++) {
        //printf("\nstrnr %d str %s\n",strnr,lstrings[strnr]);
        if (strlen(inputname)<900) strcat(inputname,"input_text ");
        err = wg_import_data_string(shmptr,lstrings[strnr],0,&informat,askpolarity,&askinfo,0);
      }  
    } 
    if (ljstrings) {      
      for(jstrnr=0; ljstrings[jstrnr] && !err; jstrnr++) {
        //printf("\njstrnr %d jstr %s\n",jstrnr,ljstrings[jstrnr]);
        if (strlen(inputname)<900) strcat(inputname,"input_jstext ");
        err = wg_import_data_string(shmptr,ljstrings[jstrnr],0,&informat,askpolarity,&askinfo,1);
      }  
    }
          
    //printf("\nreturned from wg_import_otter_file\n");  
    if(!err) {
      //printf("Data read from %s.\n",cmdfiles[1]);
    } else if(err<-1) {
      err_printf(json,"error when reading file or text");     
#ifdef __EMSCRIPTEN__
      if (dbmemsegh(shmptr)->infrm_mpool) wg_free_mpool(shmptr, dbmemsegh(shmptr)->infrm_mpool);
      wg_delete_local_database(shmptr);
#endif          
      return(1);   
    } else {
      //err_printf(json,"error when reading file or text");     
#ifdef __EMSCRIPTEN__
      if (dbmemsegh(shmptr)->infrm_mpool) wg_free_mpool(shmptr, dbmemsegh(shmptr)->infrm_mpool);
      wg_delete_local_database(shmptr);
#endif 
      return(1); 
    }      
    if (convert && !clausify) {      
#ifdef __EMSCRIPTEN__
      if (dbmemsegh(shmptr)->infrm_mpool) wg_free_mpool(shmptr, dbmemsegh(shmptr)->infrm_mpool);
      wg_delete_local_database(shmptr);
#endif 
      return 0;
    } else if (clausify) {
      err = wg_run_converter(shmptr,inputname,stratfile,informat,NULL,NULL);
    } else {      
      err = wg_run_reasoner(shmptr,inputname,stratfile,informat,NULL,strattext);
      if (err) {
        if (printlevel>11) printf("\n\n");
        if (json) printf("\n{\"result\": \"proof not found\"}\n");
        else  {          
          printf("result: proof not found.\n");
        }  
      }  
    }         
#ifdef __EMSCRIPTEN__
    if (dbmemsegh(shmptr)->infrm_mpool) wg_free_mpool(shmptr, dbmemsegh(shmptr)->infrm_mpool);
    wg_delete_local_database(shmptr);
#endif    
    return(0);
  }  

  // -readkb

  if(!(strncmp(cmdstr,"-readkb",15))) {
    wg_int err;

#ifdef __EMSCRIPTEN__
    err_printf(json,"-readkb unavailable under wasm");
    return(1);
#endif    
    if (cmdfileslen<2) {
      err_printf(json,"-readkb needs a file as an argument");
      return(1);
    }   
#ifdef _WIN32      
    if (mbsize && mbsize<100) {
      err_printf(json,"-readkb needs at least 100 megabytes: change -mbsize argument");
      return(1);
    }        
#else
    if (mbsize && mbsize<1000) {
      err_printf(json,"-readkb needs at least 1000 megabytes: change -mbsize argument");
      return(1);
    }
#endif         
#ifdef SHOWTIME  
    printf("\nto -readkb\n");      
    gkc_show_cur_time();
#endif
    wg_delete_database(shmname);
#ifdef SHOWTIME      
    printf("\nprevious memory database deleted\n");
    gkc_show_cur_time();
#endif
    shmptr=wg_attach_database(shmname, shmsize);    
    if(!shmptr) {
      err_printf(json,"failed to attach to database");
      return(1);
    }
#ifdef SHOWTIME       
    printf("\nto wg_import_otter_file %s\n",cmdfiles[1]);
    gkc_show_cur_time();
#endif      
    //err = wg_import_otter_file(shmptr,cmdfiles[1],1,&informat);
    err = wg_import_data_file(shmptr,cmdfiles[1],0,&informat,askpolarity,&askinfo);
#ifdef SHOWTIME       
    printf("\nexited wg_import_otter_file\n");
    gkc_show_cur_time();
#endif      
    if(!err)
      printf("Data parsed into the shared memory db, starting to build indexes.");
    else if(err<-1)
      err_printf(json,"problem reading otter file, data may be partially"\
        " imported");
    else {
      err_printf(json,"reading failed");
      wg_delete_database(shmname);
      return(1);
    }
    //wg_show_database(shmptr);
#ifdef SHOWTIME 
    printf("\nstarting to init_shared_database\n");      
    gkc_show_cur_time();
#endif
    if (cmdfileslen==3) tmp=init_shared_database(shmptr,cmdfiles[2]);
    else tmp=init_shared_database(shmptr,NULL);
    if (tmp<0) {
      err_printf(json,"db creation failed");
      wg_delete_database(shmname);
      return(1);
    }  
    printf("\nDb ready in shared memory.\n");   
#ifdef SHOWTIME         
    gkc_show_cur_time();
#endif    
    //wr_show_stats(db_rglb(shmptr),1);
    //wr_show_database_details(NULL,shmptr,"shared db");
#ifdef _WIN32    
    //char c1,c2;
    printf ("Shared memory kb is available while this program is running.\n");
    printf ("Press any key to free shared memory kb and exit . . .\n");
    _getch();
#endif    
    return(0);
  }

  // -usekb

  if(!(strncmp(cmdstr,"-usekb",15))) {
    wg_int err;
    /*
    if (cmdfileslen<2) {
      err_printf(json,"-usekb needs a file as an argument");
      return(1);
    } 
    */     
#ifdef SHOWTIME 
    printf("\n-querykb starts with external shmname %s\n",shmname);
    gkc_show_cur_time();
#endif      
    shmptr=wg_attach_existing_database(shmname);
    if(!shmptr) {
      err_printf(json,"failed to attach to database");
      return(1);
    }
    if (tptp && !(clausify) && !(convert)) {
      printf("Using the existing shared memory kb %s.\n",shmname);
    }  
#ifdef SHOWTIME       
    printf("\ndb attached, showing attached shared memory db shmptr %ld\n",
      (unsigned long int)((gint)shmptr));
    gkc_show_cur_time();
    wr_show_database_details(NULL,shmptr,"shmptr");
#endif      
    // --- create a new temporary local db ---
    //shmsize2=100000000;     
#ifdef SHOWTIME      
    printf("\nto wg_attach_local_database_with_kb with shmptr %ld\n",
      (unsigned long int)((gint)shmptr));
    gkc_show_cur_time();
#endif      
    shmptrlocal=wg_attach_local_database_with_kb(shmsize2,(void*)shmptr);
#ifdef SHOWTIME
    printf("\nshmptrlocal is %lx and gint %ld\n",
      (unsigned long int)shmptrlocal,(gint)shmptrlocal);
    gkc_show_cur_time();
#endif      
    if(!shmptrlocal) {
      err_printf(json,"failed to attach local database");
      return(1);
    }        
    //islocaldb=1;
    err=0;
#ifdef SHOWTIME
    printf("\nqrun1 to wg_import_otter_file from argv[i+1] %s\n",cmdfiles[1]);
    gkc_show_cur_time();
#endif          
    //err = wg_import_otter_file(shmptrlocal,cmdfiles[1],0,&informat);
    (dbmemsegh(shmptrlocal)->max_forks)=parallel; 
    (dbmemsegh(shmptrlocal)->tptp)=tptp;
    (dbmemsegh(shmptrlocal)->json)=json;
    (dbmemsegh(shmptrlocal)->convert)=convert; 
    (dbmemsegh(shmptrlocal)->clausify)=clausify;
    (dbmemsegh(shmptrlocal)->printlevel)=printlevel;
    (dbmemsegh(shmptrlocal)->printderived)=printderived;
    (dbmemsegh(shmptrlocal)->max_seconds)=seconds;
    (dbmemsegh(shmptrlocal)->allruns_start_clock)=allruns_start_clock;
/*
    (dbmemsegh(shmptr)->max_forks)=parallel;
    (dbmemsegh(shmptr)->tptp)=tptp;
    (dbmemsegh(shmptr)->json)=json;
    (dbmemsegh(shmptr)->convert)=convert;
    (dbmemsegh(shmptr)->clausify)=clausify;
*/
    (dbmemsegh(shmptrlocal)->parse_skolem_nr)=(dbmemsegh(shmptr)->parse_skolem_nr)+1;
    (dbmemsegh(shmptrlocal)->parse_newpred_nr)=(dbmemsegh(shmptr)->parse_newpred_nr)+1;

    strcat(inputname,"kb "); 
    if (lfiles) {      
      for(filenr=0; lfiles[filenr] && !err; filenr++) {
        //printf("\nfilesnr %d files %s\n",filenr,lfiles[filenr]); 
        if (strlen(inputname)+strlen(lfiles[filenr])<900) {
          strcat(inputname,lfiles[filenr]);      
        }
        err=wg_import_data_file(shmptrlocal,lfiles[filenr],0,&informat,askpolarity,&askinfo);
      }  
    }  
    if (lstrings) {      
      for(strnr=0; lstrings[strnr] && !err; strnr++) {
        //printf("\nstrnr %d str %s\n",strnr,lstrings[strnr]);
        if (strlen(inputname)<900) strcat(inputname,"input_text ");
        err = wg_import_data_string(shmptrlocal,lstrings[strnr],0,&informat,askpolarity,&askinfo,0);
      }  
    } 
    if (ljstrings) {      
      for(jstrnr=0; ljstrings[jstrnr] && !err; jstrnr++) {
        //printf("\njstrnr %d jstr %s\n",jstrnr,ljstrings[jstrnr]);
        if (strlen(inputname)<900) strcat(inputname,"input_jstext ");
        err = wg_import_data_string(shmptrlocal,ljstrings[jstrnr],0,&informat,askpolarity,&askinfo,1);
      }  
    }
          
    //printf("\nreturned from wg_import_otter_file\n");  
    if(!err) {
      //printf("Data read from %s.\n",cmdfiles[1]);
    } else if(err<-1) {
      err_printf(json,"error when reading file or text");
#ifdef __EMSCRIPTEN__
      wg_delete_local_database(shmptrlocal);
#endif
      return(1);   
    } else {
      //err_printf(json,"error when reading file or text");
#ifdef __EMSCRIPTEN__
      wg_delete_local_database(shmptrlocal);
#endif      
      return(1); 
    }     
    // ---- local db created ------
#ifdef SHOWTIME
    printf("\nto call wg_run_reasoner\n");
    gkc_show_cur_time();
#endif   
    if (convert && !clausify) {     
      return 0; 
    } else if (clausify) {
      err_printf(json,"do not use -clausify together with -usekb");
      return 1;
      //err = wg_run_converter(shmptrlocal,inputname,stratfile,informat,NULL,NULL);
    } else {
      err = wg_run_reasoner(shmptrlocal,inputname,stratfile,informat,NULL,strattext);
      if (err) {
        if (printlevel>11) printf("\n\n");
        if (json) printf("\n{\"result\": \"proof not found\"}\n");        
        else  printf("result: proof not found.\n");
      }     
    }     
#ifdef SHOWTIME      
    printf("\nwg_run_reasoner returned\n");
    gkc_show_cur_time();
#endif       
#ifdef __EMSCRIPTEN__
    wg_delete_local_database(shmptrlocal);
#endif  
    return(0);  
  }

  // -writekb

  if (!(strncmp(cmdstr,"-writekb",15))) {
     wg_int err;
     int flags = 0;

    if (cmdfileslen<2) {
      err_printf(json,"-writekb needs a file as an argument");
      return(1);
    }  
    shmptr=wg_attach_existing_database(shmname);
    if(!shmptr) {
      err_printf(json,"failed to attach local database");
      return(1);
    }

    /* Locking is handled internally by the dbdump.c functions */
    if(flags & FLAGS_FORCE)
      err = wg_dump_internal(shmptr,cmdfiles[1], 0);
    else
      err = wg_dump(shmptr,cmdfiles[1]);

    if(err<-1) {
      err_printf(json,"cannot write to file, kb may have"\
        " become corrupt");
      return(1);  
    } else if(err) {
      err_printf(json,"writing failed");
      return(1);
    }
    return(0);
  }

  // -loadkb

  if (!(strncmp(cmdstr,"-loadkb",15))) {
    wg_int err, minsize, maxsize;
    int flags = 0;  

    if (cmdfileslen<2) {
      err_printf(json,"-loadkb needs a file as an argument");
      return(1);
    }  
    err = wg_check_dump(NULL, cmdfiles[1], &minsize, &maxsize);
    if(err) {
      err_printf2(json,"loading failed, problem with %s",cmdfiles[1]);
      return(1);
    }

    shmptr=wg_attach_memsegment(shmname, minsize, maxsize, 1,
      (flags & FLAGS_LOGGING), 0);
    if(!shmptr) {
      err_printf(json,"error: failed to attach local database");
      return(1);
    }

    // Locking is handled internally by the dbdump.c functions
    err = wg_import_dump(shmptr,cmdfiles[1]);
    if(!err)
      printf("Database imported.\n");
    else if(err<-1) {
      err_printf(json,"failed to load, db may have"\
        " become corrupt");
      return(1);  
    } else {
      err_printf(json,"loading failed");
      return(1);
    }
#ifdef _WIN32    
    printf ("Shared memory kb is available while this program is running.\n");
    printf ("Press any key to free shared memory kb and exit . . .\n");
    _getch();
#endif  
    //wr_show_database_details(NULL,shmptr,"shmptr");
    return(0);
  }

  if(!(strncmp(cmdstr,"-readwritekb",15))) {
    wg_int err;
    int flags = 0;

    if (cmdfileslen<3) {
      err_printf(json,"-readwritekb needs a data file and a dump filename as arguments");
      return(1);
    }   
#ifdef _WIN32      
    if (mbsize && mbsize<100) {
      err_printf(json,"-readwritekb needs at least 100 megabytes: change -mbsize argument");
      return(1);
    }        
#else
    if (mbsize && mbsize<1000) {
      err_printf(json,"-readwritekb needs at least 1000 megabytes: change -mbsize argument");
      return(1);
    }
#endif
#ifdef SHOWTIME  
    printf("\nto -readkb\n");      
    gkc_show_cur_time();
#endif
    wg_delete_database(shmname);
#ifdef SHOWTIME      
    printf("\nprevious memory database deleted\n");
    gkc_show_cur_time();
#endif
    shmptr=wg_attach_database(shmname, shmsize);
    if(!shmptr) {
      err_printf(json,"failed to attach to database");
      return(1);
    }
#ifdef SHOWTIME       
    printf("\nto wg_import_otter_file %s\n",cmdfiles[1]);
    gkc_show_cur_time();
#endif      
    //err = wg_import_otter_file(shmptr,cmdfiles[1],1,&informat);
    err = wg_import_data_file(shmptr,cmdfiles[1],0,&informat,askpolarity,&askinfo);
#ifdef SHOWTIME       
    printf("\nexited wg_import_otter_file\n");
    gkc_show_cur_time();
#endif      
    if(!err)
      printf("Data parsed into the shared memory db, starting to build indexes.");
    else if(err<-1)
      err_printf(json,"fatal error when reading otter file, data may be partially"\
        " imported");
    else {
      err_printf(json,"reading failed");
      wg_delete_database(shmname);
      return(1);
    }
    //wg_show_database(shmptr);
#ifdef SHOWTIME 
    printf("\nstarting to init_shared_database\n");      
    gkc_show_cur_time();
#endif
    if (cmdfileslen==4) tmp=init_shared_database(shmptr,cmdfiles[3]);
    else tmp=init_shared_database(shmptr,NULL);    
    if (tmp<0) {
      err_printf(json,"db creation failed");
      wg_delete_database(shmname);
      return(1);
    }  
    printf("\nDb ready in shared memory.\n");   
#ifdef SHOWTIME         
    gkc_show_cur_time();
#endif 

    /* Locking is handled internally by the dbdump.c functions */
    if(flags & FLAGS_FORCE)
      err = wg_dump_internal(shmptr,cmdfiles[2], 0);
    else
      err = wg_dump(shmptr,cmdfiles[2]);

    if(err<-1) {
      err_printf(json,"cannot write to file, kb may have"\
        " become corrupt");
      return(1);  
    } else if(err) {
      err_printf(json,"writing failed");
      return(1);
    }
    printf("Database written to file %s and remains present in memory.\n",cmdfiles[2]);
#ifdef _WIN32    
    //char c1,c2;
    printf ("Shared memory kb is available while this program is running.\n");
    printf ("Press any key to free shared memory kb and exit . . .\n");
    _getch();
#endif    
    return(0);
  }

   // -deletekb

  if (!(strncmp(cmdstr,"-deletekb",15))) {
    /* free shared memory */
#ifdef _WIN32      
    err_printf(json,"-deletekb is available on Linux and OSX only");
    return(0);
#endif      
    wg_delete_database(shmname);
    return(0);
  }
  return(0);
}

void sig_handler(int signum){
  fflush(stdout);
  printf("\n{\"result\": \"time limit, proof not found\"}\n");
#ifdef __EMSCRIPTEN__  
  glb* g;  
  if (global_shmptr) {
    if (dbmemsegh(global_shmptr)->infrm_mpool) 
       wg_free_mpool(global_shmptr, dbmemsegh(global_shmptr)->infrm_mpool);
    if (dbmemsegh(global_shmptr)->local_g) {
      // this is risky: may be in non-ok state
      //wr_glb_free_shared_complex((glb*)(dbmemsegh(global_shmptr)->local_g));
      g=(dbmemsegh(global_shmptr)->local_g);
      
      wr_vec_free(g,rotp(g,g->clbuilt)); 
      wr_vec_free(g,rotp(g,g->clactive));  
      wr_vec_free(g,rotp(g,g->clactivesubsume));  
      wr_vec_free(g,rotp(g,g->clpickstack));  
      wr_vec_free(g,rotp(g,g->clqueue));     
      //wr_vec_free(g,rotp(g,g->clweightqueue)); 
      wr_free_priorqueue(g,rotp(g,g->clpickpriorqueue));
      //wr_vec_free(g,rotp(g,g->clpickpriorqueue) );
      wr_free_clpick_queues(g,rotp(g,g->clpick_queues));
      //wr_vec_free(g,rotp(g,g->clpick_queues) );

      //wr_free_termhash(g,rotp(g,g->hash_pos_groundunits));
      wr_vec_free(g,rotp(g,g->hash_pos_groundunits));
      //wr_free_termhash(g,rotp(g,g->hash_neg_groundunits));
      wr_vec_free(g,rotp(g,g->hash_neg_groundunits) );

      //wr_free_termhash(g,rotp(g,g->hash_pos_active_groundunits));
      wr_vec_free(g,rotp(g,g->hash_pos_active_groundunits));
      //wr_free_termhash(g,rotp(g,g->hash_neg_active_groundunits));
      wr_vec_free(g,rotp(g,g->hash_neg_active_groundunits) );

      //wr_free_atomhash(g,rotp(g,g->hash_atom_occurrences));
      wr_vec_free(g,rotp(g,g->hash_atom_occurrences) ); 
      //wr_clterm_hashlist_free(g,rotp(g,g->hash_neg_atoms));
      wr_vec_free(g,rotp(g,g->hash_neg_atoms) );    
      //wr_clterm_hashlist_free(g,rotp(g,g->hash_pos_atoms)); 
      wr_vec_free(g,rotp(g,g->hash_pos_atoms) );
      //
      //if (g->use_strong_unit_cutoff) {    
      //  if (g->hash_neg_units) wr_clterm_hashlist_free(g,rotp(g,g->hash_neg_units));    
      //  if (g->hash_pos_units) wr_clterm_hashlist_free(g,rotp(g,g->hash_pos_units)); 
      //}
      wr_vec_free(g,rotp(g,g->hash_neg_units));
      wr_vec_free(g,rotp(g,g->hash_pos_units));
      //wr_clterm_hashlist_free(g,rotp(g,g->hash_units)); CP7
      //wr_vec_free(g,rotp(g,g->hash_units) );
      //wr_clterm_hashlist_free(g,rotp(g,g->hash_para_terms));
      wr_vec_free(g,rotp(g,g->hash_para_terms) );
      //wr_clterm_hashlist_free(g,rotp(g,g->hash_para_terms));
      wr_vec_free(g,rotp(g,g->hash_eq_terms) );
      //wr_clterm_hashlist_free(g,rotp(g,g->hash_para_terms));
      wr_vec_free(g,rotp(g,g->hash_rewrite_terms));     


      //wr_glb_free_local_complex((glb*)(dbmemsegh(global_shmptr)->local_g));

      wr_str_free(g,(g->filename));
      wr_vec_free(g,g->queue_termbuf);
      wr_vec_free(g,g->hyper_termbuf);
      wr_vec_free(g,g->active_termbuf);  
      wr_vec_free(g,g->cut_clvec); 
      wr_vec_free(g,g->rewrite_clvec);
      wr_vec_free(g,g->varstack); 
      wr_vec_free(g,g->xcountedvarlist);
      wr_vec_free(g,g->ycountedvarlist);
      wr_vec_free(g,g->varbanks);       
      wr_vec_free(g,g->given_termbuf);
      wr_vec_free(g,g->simplified_termbuf);
      wr_vec_free(g,g->derived_termbuf);        
      wr_vec_free(g,g->tmp_litinf_vec);  
      wr_vec_free(g,(g->tmp_hardnessinf_vec));
      wr_vec_free(g,(g->tmp_resolvability_vec));
      wr_vec_free(g,(g->tmp_sort_vec));
      //wr_vec_free(g,g->tmp_clinfo);
      //wr_vec_free(g,g->tmp_varinfo);
      wr_vec_free(g,g->hyper_queue);
      //wr_vec_free(g,g->answers);
      if (g->sine_k_values) sys_free(g->sine_k_values); // bytestring
      if (g->sine_uri_k_values) sys_free(g->sine_uri_k_values); // bytestring
      //if (g->backsubsume_values) sys_free(g->backsubsume_values);
       
      //wr_glb_free_shared_simple((glb*)(dbmemsegh(global_shmptr)->local_g)); 
      //wr_glb_free_local_simple((glb*)(dbmemsegh(global_shmptr)->local_g));

      sys_free((dbmemsegh(global_shmptr)->local_g)); // free whole spaces      
    }    
    wg_delete_local_database(global_shmptr); 
  }    
#endif  
  exit(0);
}


/** usage: display command line help.
*
*/

void usage(char *prog) {
  printf("\ngkc is a first order logic reasoner for large knowledge bases:\n"\
         "\n"\
         "basic proof search with an automatic strategy:\n"\
         "  gkc <logic file_1> ...<logic file_N>\n"\
         "    read all logic files and then search for proof, where \n"\
         "    *.js* file suffix indicates json format\n"\
         "    and you can optionally input logic directly from command line:\n"\
         "    ... -text 'logic text' ... -jstext 'logic text in json' \n"\
         "\n"\
         "options for proof search with a user-determined strategy:\n"\
         "  -strategy <strategy file>\n"\
         "     use the json <strategy file> to determine proof search strategy and options\n"\
         "  -strategytext 'strategy text in json' \n"\
         "     alternatively input strategy text directly from command line\n"\
         "\n"\
         "options and parameters for using the shared memory database of axioms:\n"\
         "  -usekb\n"\
         "     use the axioms in the shared memory database in addition to other input\n"\
         "  -mbnr <shared memory database nr>\n"\
         "     if omitted, number 1000 is used\n"\
         "  gkc -readkb <logic file>\n"\
         "     parse and load a logic file into the shared memory database\n"\
         "  gkc -writekb <dump file>\n"\
         "     write the present shared memory database to a file for faster loading later\n"\
         "  gkc -loadkb <dump file>\n"\
         "     load a shared memory database from a file\n"\
         "  gkc -readwritekb <filename> <dump file>\n"\
         "     parse and load a file into shared memory database and write this to a file\n"\
         "  gkc -deletekb\n"\
         "     delete the present shared memory database (not necessary for reading a new one)\n"\
         "\n"\
         "options with numeric parameters:\n"\
         "  -seconds <n>\n"\
         "     use <n> as an upper limit of running time in seconds; default unrestricted\n"\
         "  -parallel <nr of parallel processes to run>\n"\
         "     UNIX only; if omitted, 4 worker processes and 1 parent used\n"\
         "  -mbsize <megabytes to allocate initially>\n"\
         "     if omitted, 5000 megabytes assumed for UNIX, 1000 for 32-bit Windows\n"\
         "  -print <nr>\n"\
         "     indicate the amount of output: 10 is default, bigger numbers give more,\n"\
         "     useful values are 1,10,11,12,13,14,20,30,40,50,51\n"\
         "\n"\
         "options without parameters:\n"\
         "  -tptp\n"\
         "     if present, use tptp output format, otherwise a simple output format\n"\
         "  -json\n"\
         "     if present, use json output format, otherwise a simple output format\n"\
         "  -convert\n"\
         "     if present, convert input format only, otherwise search for proof;\n"\
         "     use along with -tptp or -json options \n"\
         "  -clausify\n"\
         "     if present, convert to clauses only, otherwise search for proof\n"\
         "  -derived\n"\
         "     print out all the derived clauses\n"\
         "  -version\n"\
         "     show gkc version\n"\
         "  -help\n"\
         "     show this help text;\n"\
         "     see https://github.com/tammet/gkc for details and examples\n");
}

/** Handle the user-supplied database size (or pick a reasonable
*   substitute). Parses up to 32-bit values, but the user may
*   append up to 'G' for larger bases on 64-bit systems.
*/
gint parse_shmsize(char *arg) {
  char *trailing = NULL;
  long maxv = LONG_MAX, mult = 1, val = strtol(arg, &trailing, 10);

  if((val == LONG_MAX || val == LONG_MIN) && errno==ERANGE) {
    fprintf(stderr, "Numeric value out of range (try k, M, G?)\n");
  } else if(trailing) {
    switch(trailing[0]) {
      case 'k':
      case 'K':
        mult = 1000;
        break;
      case 'm':
      case 'M':
        mult = 1000000;
        break;
      case 'g':
      case 'G':
        mult = 1000000000;
        break;
      default:
        break;
    }
  }

#ifndef HAVE_64BIT_GINT
  maxv /= mult;
#endif
  if(val > maxv) {
    fprintf(stderr, "Requested segment size not supported (using %ld)\n",
      mult * maxv);
    val = maxv;
  }

  return (gint) val * (gint) mult;
}

/** Handle a command-line flag
*
*/
gint parse_flag(char *arg) {
  while(arg[0] == '-')
    arg++;
  switch(arg[0]) {
    case 'f':
      return FLAGS_FORCE;
    case 'l':
      return FLAGS_LOGGING;
    default:
      fprintf(stderr, "Unrecognized option: `%c'\n", arg[0]);
      break;
  }
  return 0;
}

/** Parse the mode bits given in octal (textual representation)
 *
 */
int parse_memmode(char *arg) {
  char *trailing = NULL;
  long parsed = strtol(arg, &trailing, 8);
  if(errno == 0 && (!trailing || strlen(trailing) == 0) &&
      parsed <= 0777 && parsed > 0) {
    return (int) parsed;
  }
  return 0;
}

/** Parse command line args prior to detecting actual command
 *  
 *  cmdstr: command string
 *  mbnr: memory base number
 *  mbsize: size number to allocate
 * 
 */

char** parse_cmdline(int argc, char **argv, char** cmdstr, int* mbnr, int* mbsize, 
          int* parallel, int* tptp, int* json, 
          char ***lstringsptr, char ***ljstringsptr, char ***lfilesptr, char **stratfile,
          int* convert, int* clausify, int* seconds, int* retcode, int* printlevel, int* printderived,
          char** strattext) {
  int i,alen,nargc;
  char* arg;
  char c;
  char **nargv;
  int nr;
  char **lstrings;
  char **ljstrings;
  char **lfiles;
  int maxlstrings=10, maxljstrings=10, maxlfiles=10;
  int lstringscount=0, ljstringscount=0, lfilescount=0;
  
  // init nargv: an array of non-command args, i.e. filenames
  nargc=0;
  nargv=(char**)malloc(sizeof(char*)*argc);
  for(i=0;i<argc;i++) {
    nargv[i]=NULL;
  }
  lstrings=(char**)malloc(sizeof(char*)*maxlstrings);
  ljstrings=(char**)malloc(sizeof(char*)*maxlstrings);
  lfiles=(char**)malloc(sizeof(char*)*maxlfiles);

  // loop over all command line elements
  for(i=0; i<argc; i++) {
    //printf("\n i %d",i);
    arg=argv[i];
    alen=strlen(arg);
    //printf("\n i %d arg %s len %d",i,arg,alen);
    if (alen<1) continue;
    c=arg[0];
    if (c=='-') {
      // keyword
      if (!(strncmp(arg,"-mbnr",10))) {
        if ((i+1)<argc) {
          //printf("\n cmd %s param %s",arg,argv[i+1]);
          i++;
          if (!(sscanf(argv[i], "%d", &nr))) {
            printf("Error: keyword %s argument must be a number, not %s\n",
                   arg,argv[i]);
            *retcode=1;       
            return NULL;
          } else {
            //printf("\n parsed nr %d\n",nr);
            if (nr<1) {
              printf("Error: -mbnr argument should be at least 1, not %s\n",argv[i]);
              *retcode=1;       
              return NULL;
            }
            *mbnr=nr;
          }
        } else {
          printf("Error: missing parameter to keyword %s\n",arg);
          *retcode=1;       
          return NULL;
        } 
      } else if (!(strncmp(arg,"-mbsize",10))) {
        if ((i+1)<argc) {
          //printf("\n cmd %s param %s",arg,argv[i+1]);
          i++;
          if (!(sscanf(argv[i], "%d", &nr))) {
            printf("Error: keyword %s argument must be a number, not %s\n",
                    arg,argv[i]);
            *retcode=1;       
            return NULL;
          } else {
            //printf("\n parsed nr %d\n",nr);
            if (nr<10) {
              printf("Error: -mbsize argument should be at least 10 (megabytes), not %s\n",argv[i]);
              *retcode=1;       
            return NULL;
            }
            if (nr>=100000) {
              printf("Error: -mbsize argument means megabytes: %s is too big\n",argv[i]);
              *retcode=1;       
              return NULL;
            }
            *mbsize=nr;
          }
        } else {
          printf("Error: missing parameter to keyword %s\n",arg);
          exit(1);
        }
      } else if (!(strncmp(arg,"-parallel",10))) {
        if ((i+1)<argc) {
          //printf("\n cmd %s param %s",arg,argv[i+1]);
          i++;
          if (!(sscanf(argv[i], "%d", &nr))) {
            printf("Error: keyword %s argument must be a number, not %s\n",
                    arg,argv[i]);
            *retcode=1;       
            return NULL;
          } else {
            //printf("\n parsed nr %d\n",nr);
            if (nr<0) {
              printf("Error: -parallel argument is negative\n");
              *retcode=1;       
            return NULL;
            }
            if (nr>=10000) {
              printf("Error: -parallel argument: %s is too big\n",argv[i]);
              *retcode=1;       
              return NULL;
            }
            *parallel=nr;
          }          
        } else {
          printf("Error: missing parameter to keyword %s\n",arg);
          exit(1);
        }             
      } else if (!(strncmp(arg,"-seconds",9))) {
        if ((i+1)<argc) {
          //printf("\n cmd %s param %s",arg,argv[i+1]);
          i++;
          if (!(sscanf(argv[i], "%d", &nr))) {
            printf("Error: keyword %s argument must be a number, not %s\n",
                    arg,argv[i]);
            *retcode=1;       
            return NULL;
          } else {
            //printf("\n parsed nr %d\n",nr);
            if (nr<0) {
              printf("Error: -seconds argument is negative\n");
              *retcode=1;       
            return NULL;
            }            
            *seconds=nr;
          }  
        } else {
          printf("Error: missing parameter to keyword %s\n",arg);
          exit(1);
        }   
      } else if (!(strncmp(arg,"-print",6))) {
        if ((i+1)<argc) {
          //printf("\n cmd %s param %s",arg,argv[i+1]);
          i++;
          if (!(sscanf(argv[i], "%d", &nr))) {
            printf("Error: keyword %s argument must be a number, not %s\n",
                    arg,argv[i]);
            *retcode=1;       
            return NULL;
          } else {
            //printf("\n parsed nr %d\n",nr);
            if (nr<0) {
              printf("Error: -print argument is negative\n");
              *retcode=1;       
            return NULL;
            }            
            *printlevel=nr;
          }  
        } else {
          printf("Error: missing parameter to keyword %s\n",arg);
          exit(1);
        }   
      } else if (!(strncmp(arg,"-derived",8))) {
        *printderived=1;    
      } else if (!(strncmp(arg,"-tptp",5))) {
        *tptp=1;       
      } else if (!(strncmp(arg,"-json",5))) {
        *json=1;       
      } else if (!(strncmp(arg,"-convert",8))) {
        *convert=1;               
      } else if (!(strncmp(arg,"-clausify",9))) {
        *clausify=1;  
      } else if (!(strncmp(arg,"-prove",10)) || 
                !(strncmp(arg,"-readkb",10)) ||
                !(strncmp(arg,"-usekb",10)) || 
                !(strncmp(arg,"-deletekb",10)) ||
                !(strncmp(arg,"-writekb",10)) ||
                !(strncmp(arg,"-loadkb",10)) ||
                !(strncmp(arg,"-readwritekb",15)) ||  
                !(strncmp(arg,"-help",10)) || 
                !(strncmp(arg,"--help",10)) || 
                !(strncmp(arg,"-version",10)) || 
                !(strncmp(arg,"--version",10)) ) {
        //printf("\n cmd %s",arg);
        *cmdstr=argv[i];
      } else if (!(strncmp(arg,"-text",5))) {
        if ((i+1)<argc) {
          //printf("\n cmd %s param %s",arg,argv[i+1]);          
          if ((lstringscount+1)<maxlstrings) {
            lstrings[lstringscount]=argv[i+1];           
            lstringscount++;
            i++;
          } else {
            printf("Error: too many %s keywords given\n",arg);
            exit(1);
          }          
        } else {
          printf("Error: missing parameter to keyword %s\n",arg);
          exit(1);
        }      
      } else if (!(strncmp(arg,"-jstext",5))) {
        if ((i+1)<argc) {
          //printf("\n cmd %s param %s",arg,argv[i+1]);          
          if ((ljstringscount+1)<maxljstrings) {
            ljstrings[ljstringscount]=argv[i+1];           
            ljstringscount++;
            i++;
          } else {
            printf("Error: too many %s keywords given\n",arg);
            exit(1);
          }          
        } else {
          printf("Error: missing parameter to keyword %s\n",arg);
          exit(1);
        }    
       } else if (!(strncmp(arg,"-strategytext",13))) {        
        if ((i+1)<argc) {
          *strattext=argv[i+1];
          i++;          
        } else {
          printf("Error: missing parameter to keyword %s\n",arg);
          exit(1);
        }    
      } else if (!(strncmp(arg,"-strategy",9))) {
        if ((i+1)<argc) {
          //printf("\n cmd %s param %s",arg,argv[i+1]);          
          *stratfile=argv[i+1];          
          i++;                 
        } else {
          printf("Error: missing parameter to keyword %s\n",arg);
          exit(1);
        }          
      } else {
        printf("Error: unknown keyword %s \n",arg);
        usage(argv[0]);
        *retcode=1;       
        return NULL;
      }   
    } else {
      // norm value
      nargv[nargc]=argv[i];
      //printf("\n nargc %d nargv[nargc] %s",nargc,nargv[nargc]);
      nargc++;     
      if (i>0) {
        if ((lfilescount+1)<maxlfiles) {
          lfiles[lfilescount]=argv[i];           
          lfilescount++;        
        } else {
          printf("Error: too many input files given\n");
          exit(1);
        }
      }          
    }    
  }
  if (lstringscount) {   
    lstrings[lstringscount]=NULL;
    *lstringsptr=lstrings;
  } else {  
    *lstringsptr=NULL;
    free(lstrings);
  }  
  if (ljstringscount) {       
    ljstrings[ljstringscount]=NULL;
    *ljstringsptr=ljstrings;
  } else {  
    *ljstringsptr=NULL;
    free(ljstrings);
  }
  if (lfilescount) {   
    lfiles[lfilescount]=NULL;
    *lfilesptr=lfiles;
  } else {  
    *lfilesptr=NULL;
    free(lfiles);
  } 
  //printf("\nexiting\n");
  return nargv;
}



/** Print information about the memory database.
 */
void segment_stats(void *db) {
  char buf1[200], buf2[40];
#ifndef _WIN32
  //struct passwd *pwd;
  //struct group *grp;
#endif
  db_memsegment_header *dbh = dbmemsegh(db);

  printf("database key: %d\n", (int) dbh->key);
  printf("database version: ");
  wg_print_header_version(dbh, 0);
  wg_pretty_print_memsize(dbh->size, buf1, 40);
  wg_pretty_print_memsize(dbh->size - dbh->free, buf2, 40);
  printf("free space: %s (of %s)\n", buf2, buf1);
#ifndef _WIN32
  /*
  pwd = getpwuid(wg_memowner(db));
  if(pwd) {
    printf("owner: %s\n", pwd->pw_name);
  }
  grp = getgrgid(wg_memgroup(db));
  if(grp) {
    printf("group: %s\n", grp->gr_name);
  }
  printf("permissions: %o\n", wg_memmode(db));
  */
#endif
#ifdef USE_DBLOG
  if(dbh->logging.active) {
    wg_journal_filename(db, buf1, 200);
    printf("logging is active, journal file: %s\n", buf1);
  } else {
    printf("logging is not active\n");
  }
#endif
  printf("database has ");
  switch(dbh->index_control_area_header.number_of_indexes) {
    case 0:
      printf("no indexes\n");
      break;
    case 1:
      printf("1 index\n");
      break;
    default:
      printf("%d indexes\n",
        (int) dbh->index_control_area_header.number_of_indexes);
      break;
  }
}

// ============ showing database contents for development ============

void wg_show_database(void* db) {
  printf("\nwg_show_database called\n");

  segment_stats(db);
  //wg_show_strhash(db);

  printf("\nwg_show_database exited\n");
}


void wg_show_strhash(void* db) {
  db_memsegment_header* dbh = dbmemsegh(db);
  gint i;
  gint hashchain;
  // lasthashchain;
  gint type;

  printf("\nshowing strhash table and buckets\n");
  printf("-----------------------------------\n");
  printf("configured strhash size %d (%% of db size)\n",STRHASH_SIZE);
  printf("size %d\n", (int) (dbh->strhash_area_header).size);
  printf("offset %d\n", (int) (dbh->strhash_area_header).offset);
  printf("arraystart %d\n", (int) (dbh->strhash_area_header).arraystart);
  printf("arraylength %d\n", (int) (dbh->strhash_area_header).arraylength);
  printf("nonempty hash buckets:\n");
  for(i=0;i<(dbh->strhash_area_header).arraylength;i++) {
    hashchain=dbfetch(db,(dbh->strhash_area_header).arraystart+(sizeof(gint)*i));
    //lasthashchain=hashchain;   
    if (hashchain!=0) {
      printf("%d: contains %d encoded offset to chain\n",
        (int) i, (int) hashchain);
      for(;hashchain!=0;
          hashchain=dbfetch(db,decode_longstr_offset(hashchain)+LONGSTR_HASHCHAIN_POS*sizeof(gint))) {
#ifdef SHOW_CONTENTS             
          printf("hashchain %ld decode_longstr_offset(hashchain) %ld fulladr %ld contents %ld\n",
                 hashchain,
                 decode_longstr_offset(hashchain),
                 (decode_longstr_offset(hashchain)+LONGSTR_HASHCHAIN_POS*sizeof(gint)),
                 dbfetch(db,decode_longstr_offset(hashchain)+LONGSTR_HASHCHAIN_POS*sizeof(gint)));
#endif          
          type=wg_get_encoded_type(db,hashchain);
          printf("  ");
#ifdef SHOW_CONTENTS                    
          //wg_debug_print_value(db,hashchain);
          printf("\n");
          printf("  type %s",wg_get_type_name(db,type));
#endif          
          if (type==WG_BLOBTYPE) {
            //printf(" len %d\n",wg_decode_str_len(db,hashchain));
          } else if (type==WG_STRTYPE || type==WG_XMLLITERALTYPE ||
                     type==WG_URITYPE || type== WG_ANONCONSTTYPE) {
          } else {
            printf("ERROR: wrong type in strhash bucket\n");
            exit(0);
          }
          //lasthashchain=hashchain;
      }
    }
  }
}


void gkc_show_cur_time(void)  {
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

void initial_printf(char* s) {
  printf("{\"error\": \"%s\"}",s);
}

void err_printf(int json,char* s) {
  if(json)
    printf("{\"error\": \"%s\"}\n",s);
  else
    printf("error: %s\n",s);  
}

void err_printf2(int json,char* s1, char* s2) {
  if(json)
    printf("{\"error\": \"%s %s\"}\n",s1,s2);
  else
    printf("error: %s %s\n",s1,s2); 
}


/* =========== data file reading top level ========= */

int wg_import_data_file(void *db, char* filename, int iskb, int* informat, int askpolarity, int* askinfo) {
  int err;

  if (wg_is_jsfile(db,filename)) {   
    if (wg_is_js_streaming_file(db,filename)) {
      err=wg_import_js_file(db,filename,iskb,informat,askpolarity,askinfo,1);
    } else {
      err=wg_import_js_file(db,filename,iskb,informat,askpolarity,askinfo,0);
    }  
  } else {
    err=wg_import_otter_file(db,filename,iskb,informat);
  }
  return err;
}

int wg_import_data_string(void *db, char* instr, int iskb, int* informat, int askpolarity, int* askinfo,
                           int jsflag) {
  int err;
  if (jsflag) 
    err=wg_import_js_string(db,instr,iskb,informat,askpolarity,askinfo,0);   
  else 
    err=wg_import_otter_string(db,instr,iskb,informat);  
  return err;
}

/*

============= TPTP LTB handling ==============

*/

#ifndef _WIN32 
#ifdef TPTP

int gkc_ltb_main(int argc, char **argv) {

  //char *shmname = NULL;
  //char shmnamebuf[16];
  void *shmptr = NULL;
  void *shmptrlocal = NULL;
  int i,j,len,seppos; //, scan_to;
  gint shmsize=0;
  //wg_int rlock = 0;
  //wg_int wlock = 0;
  //int mbnr=0, mbsize=0; // parsed from cmd line
  //char* cmdstr=NULL; // parsed from cmd line
  //char** cmdfiles; // parsed from cmd line
  //int cmdfileslen; // computed below
  int tmp;
  //int retcode=0;
  int informat=0; // 0 if not set, 1 if tptp fof
  //int islocaldb=0; // lreasoner sets to 1 to avoid detaching db at the end
  char* batchfilename;
  char* outfoldername;
  FILE *batchfile;
  char batchfilerow[1000];
  int row, tmp1, tmp2, found, rowcount;
  char* rowptr;
  char* rowstr;
  char** rows; //[100000];  
  char probname[1000];
  char outname[1000];
  char probfullname[1000];
  char probfullname_copy[1000];
  char outfullname[1000];
  //char* sep;
  char* cmdfiles[2];
  int err,isproblemrow;
  int myerr;  
  int askinfo=0;
  int askpolarity=1;
  char* inputname="LTB";

  if (argc<3) {
    return(0);
  }
  batchfilename=argv[1];
  outfoldername=argv[2];
  if (!batchfilename || !outfoldername) return 0;
  // test if right type of batchfilename
  batchfile=fopen(batchfilename, "r");
  if (!batchfile) return 0;
  found=0;
  for(row=0; row<10; row++) {
    rowptr=fgets(batchfilerow,900,batchfile);
    if (rowptr) {
      tmp1=strncmp("division.category LTB",batchfilerow, strlen("division.category LTB"));
      tmp2=strncmp("% SZS start BatchConfiguration",batchfilerow, strlen("% SZS start BatchConfiguration"));
      if (tmp1==0 || tmp2==0) {
        found=1;
        break;
      }
    }
  }
  fclose(batchfile);
  if (!found) return 0;
  // ok, tested that batchfile is ok
  setbuf(stdout, NULL);
  rows=malloc(sizeof(char*)*100000);
  batchfile=fopen(batchfilename, "r");
  for(rowcount=0; rowcount<100000; rowcount++) {
    rowptr=fgets(batchfilerow,900,batchfile);
    if (rowptr==NULL) {
      break;
    } else {
      tmp=strlen(rowptr);
      if (tmp) {
        rowstr=(char*)(malloc(tmp+1));
        strcpy(rowstr,rowptr);        
      } else {
        rowstr=NULL;
      }      
      rows[rowcount]=rowstr;
    }  
  }  
  fclose(batchfile);
  fflush(stdout);  
  // next run over all the problems

  isproblemrow=0;
  int bigloopi, biglooptime=0;
  for(bigloopi=0;bigloopi<4;bigloopi++) {
    if (biglooptime==0) biglooptime=1; 
    else biglooptime=biglooptime*5;
    printf("\n** big loop %d starts with time %d \n",bigloopi,biglooptime);   
    isproblemrow=0;
    for(i=0;i<rowcount;i++) {
      rowstr=rows[i];
      if (!rowstr) continue;    
      if (!strncmp("% SZS start BatchProblems",rowstr, strlen("% SZS start BatchProblems"))) {
        isproblemrow=1;  
        continue;
      }
      if (!strncmp("% SZS end BatchProblems",rowstr, strlen("% SZS end BatchProblems"))) {
        break;
      }
      if (!isproblemrow || strlen(rowstr)<2) continue;      

      //printf("\nproblem %d %s\n",i,rowstr);

      len=strlen(rowstr);
      if (len>900) continue;
      seppos=0;
      for(j=0;j<len;j++) {
        if (rowstr[j]==' ' && !seppos) {
          seppos=j;
          //break;
        }        
        if (rowstr[j]=='\r') rowstr[j]=0; 
        if (rowstr[j]=='\n') rowstr[j]=0;      
      }      
      len=strlen(rowstr);

      if (!seppos) continue;
      memcpy(probname,rowstr,seppos);
      probname[seppos]=0;
      memcpy(outname,rowstr+seppos+1,(len-seppos)-1);
      outname[(len-seppos)-1]=0;
      int k;
      char* outnameptr=outname;
      for(k=0;k<(len-seppos);k++) {
        if (outnameptr[k]==' ') outnameptr++;
        else break;
      }
      *outfullname=0;
      strcat(outfullname,outfoldername); 
      strcat(outfullname,"/");
      strcat(outfullname,outnameptr); 

      
      
      int version, globber_found=0;
      for(version=4;version<6;version++) { // <6
        probfullname[200]=0;       

        int co;
        for(co=0;co<900;co++) probfullname[co]=0;  
        wr_make_batch_prob_fullname(probname,probfullname,batchfilename);

        // replace * with +4 or +5
        int ci;
        for(co=0;co<900;co++) probfullname_copy[co]=0;
          for(ci=0,co=0;ci<strlen(probfullname);ci++,co++) {
          if (probfullname[ci]=='*') {
            globber_found=1;
            probfullname_copy[co]='+';
            if (version==4) probfullname_copy[co+1]='4';
            else probfullname_copy[co+1]='5';
            co++;
          } else {
            probfullname_copy[co]=probfullname[ci];
          }
        }          
        cmdfiles[1]=probfullname_copy;

        printf("%% SZS status Started for %s\n",probfullname_copy); 
        fflush(stdout); 

        int pid,stat;    
        pid=fork();            
        if (pid<0) {
          // fork fails
          printf("\nerror: fork fails\n");         
          fflush(stdout);
          wr_output_batch_prob_failure(probfullname,outfullname,"Error");
          continue;
        } else if (pid==0) {
          // child

          shmptr=wg_attach_local_database(shmsize);
          if(!shmptr) {
            err_printf(0,"failed to attach local database");
            exit(1);
          }
          (dbmemsegh(shmptr)->min_strat_timeloop_nr)=bigloopi; 
          (dbmemsegh(shmptr)->max_strat_timeloop_nr)=bigloopi; 
          //printf("\nto call wg_import_otter_file\n");
          //err = wg_import_otter_file(shmptr,cmdfiles[1],0,&informat); 
          err = wg_import_data_file(shmptr,cmdfiles[1],0,&informat,askpolarity,&askinfo);
          //printf("\nreturned from wg_import_otter_file\n");  
          if(!err) {
          } else if(err<-1) {
            wr_output_batch_prob_failure(cmdfiles[1],outfullname,"could not parse problem file"); 
            exit(1);
          } 
          (dbmemsegh(shmptr)->max_forks)=8;   
          //printf("\nchild to call wg_run_reasoner\n");      
          myerr = wg_run_reasoner(shmptr,inputname,NULL,informat,outfullname,"LTBSPECIAL");                             
          //printf("\nchild myerr %d \n",myerr);
          fflush(stdout);
          if(!myerr) {            
            exit(0);
          } else {                       
            wg_delete_local_database(shmptrlocal);
          }  
          fflush(stdout);                     
          exit(1);              
        } else {
          // parent
          err=0;
          //printf("\nwaiting for pid %d\n",pid);
          //pid_t cpid = waitpid(pid, &stat, 0); 
          waitpid(-1, &stat, 0);
          //printf("\nreceived for pid %d stat %d \n",pid,stat);
          if (WIFEXITED(stat)) {
            err = WEXITSTATUS(stat);
            //printf("Child %d terminated with status: %d\n", pid,err); 
            if (!err) {
              printf("%% SZS status Theorem for %s\n",probfullname_copy); 
              printf("%% SZS status Ended for %s\n",probfullname_copy);       
              fflush(stdout);              
            } else {
              wr_output_batch_prob_failure(probfullname_copy,outfullname,"Timeout");
              //printf("%% SZS status Timeout for %s\n",probfullname_copy); 
              //printf("%% SZS status Ended for %s version %d\n",probfullname_copy,version);       
              fflush(stdout);
            }
          }  else {
            wr_output_batch_prob_failure(probfullname_copy,outfullname,"Error"); 
          }       
          //wait();
          if (!err) {
            rows[i]=NULL;
            break;
            //printf("\nproof found for row i %d!\n",i);
          } else if (err==1) {
            //wr_output_batch_prob_failure(probfullname_copy,outfullname,"Error");
          }
          //fflush(stdout);
          if (!globber_found) break;   
        }
      }  
    }  
  }        
  //free(rows);
  return(1);  
}

#endif
#endif


void wr_make_batch_axiom_fullname(char* probname, char* probfullname, char* batchname, int formulation) {
  int i,j,len,tmp;
  char *lastslash = NULL;
  char *p;

  j=0;
  lastslash=strrchr(batchname,'/');
  if (lastslash!=NULL) {
    for(p=batchname; p<=lastslash; p++) {
      probfullname[j++] = *p;      
    }    
  }
  len=strlen(probname);
  tmp=0;
  for(i=0;i<len;i++) {
    if(probname[i]=='\'') {
      if (tmp==1) break;
      tmp=1;
      continue;
    }
    if (!tmp) continue;
    if (probname[i]=='\r') continue;
    if (probname[i]=='\n') continue;
    probfullname[j++]=probname[i];
  }
  probfullname[j]=0;
}

void wr_make_batch_prob_fullname(char* probname, char* probfullname, char* batchname) {
  int i,j,len;
  char *lastslash = NULL;
  char *p;

  j=0;
  lastslash=strrchr(batchname,'/');
  if (lastslash!=NULL) {
    for(p=batchname; p<=lastslash; p++) {
      probfullname[j++] = *p;      
    }    
  } 
  len=strlen(probname);
  for(i=0;i<len;i++) {
    if (probname[i]=='\r') continue;
    if (probname[i]=='\n') continue;
    probfullname[j++]=probname[i];  
  }
  probfullname[j]=0;
}

void wr_output_batch_prob_failure(char* probname, char* outfullname, char* failure) {
  FILE* outfile;
  int exists=0;

  printf("%% SZS status %s for %s\n",failure,probname);
  printf("%% SZS status Ended for %s\n",probname);
#ifdef _WIN32
  DWORD dwAttrib = GetFileAttributes(outfullname);
  exists=(dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
#else  
  if(access(outfullname,F_OK) != -1) exists=1;
#endif    
  if (exists) {
    // file exists
  } else {
    // file doesn't exist
    outfile=fopen(outfullname, "w");
    if (!outfile) {
      printf("{\"error\": \"cannot open outfile to print failure %s\"}\n",outfullname);               
    } else {
      fprintf(outfile,"%% SZS status %s for %s\n",failure,probname);   
      fclose(outfile);
    }  
  }  
}  

#ifdef __cplusplus
}
#endif
