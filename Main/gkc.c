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
#include "../Db/dbapi.h"
#endif



/* ====== Private defs =========== */

/*
#ifdef _WIN32
#define sscanf sscanf_s  // XXX: This will break for string parameters 
#endif
*/

#define FLAGS_FORCE 0x1
#define FLAGS_LOGGING 0x2

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
void usage(char *prog);
gint parse_shmsize(char *arg);
gint parse_flag(char *arg);
int parse_memmode(char *arg);
char** parse_cmdline(int argc, char **argv, char** cmdstr, int* mbnr, int* mbsize, int* retcode);
void wg_set_kb_db(void* db, void* kb);
void segment_stats(void *db);
void wg_show_strhash(void* db);
void wg_show_database(void* db);
void gkc_show_cur_time(void);

void initial_printf(char* s);
void err_printf(char* s);
void err_printf2(char* s1, char* s2);

int gkc_ltb_main(int argc, char **argv);
void wr_make_batch_axiom_fullname(char* probname, char* probfullname, char* batchname);
void wr_make_batch_prob_fullname(char* probname, char* probfullname, char* batchname);
void wr_output_batch_prob_failure(char* probname, char* outfullname, char* failure);

//#define SHOW_CONTENTS 1
//#undef SHOW_CONTENTS

/* ====== Functions ============== */

/*
how to set 500 meg of shared memory:

su
echo 500000000  > /proc/sys/kernel/shmmax
*/


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
  int mbnr=0, mbsize=0; // parsed from cmd line
  char* cmdstr=NULL; // parsed from cmd line
  char** cmdfiles; // parsed from cmd line
  int cmdfileslen; // computed below
  int tmp;
  int retcode=0;
  int informat=0; // 0 if not set, 1 if tptp fof
  //int islocaldb=0; // lreasoner sets to 1 to avoid detaching db at the end

  if (argc<2) {
    usage(argv[0]);
    return(0);
  }
  
  tmp=gkc_ltb_main(argc,argv);
  if (tmp==1) return(0);

  cmdfiles=parse_cmdline(argc,argv,&cmdstr,&mbnr,&mbsize,&retcode); 
  if (retcode) return retcode;
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
#ifdef _WIN32
    shmsize=(gint)1000000;
#else
    shmsize = (gint)5000000000; // 5 mb default for large csr
#endif        
    shmsize2 = shmsize;
  } else {
    shmsize=(gint)mbsize*(gint)1000000; // mbsize given on cmdline is in megabytes
    shmsize2 = shmsize;
  }

  if (mbnr==0) {
    shmname="1000";
  } else {
    snprintf(shmnamebuf,15,"%d",mbnr);   
    shmname=shmnamebuf;
  }

  //printf("\nmbsize %d shmsize %ld shmsize2 %ld shmname %s\n",
  //  mbsize,shmsize,shmsize2,shmname);

   // -prove
  
  if(!(strncmp(cmdstr,"-prove",15))) {
    wg_int err;

    if (cmdfileslen<2) {
      err_printf("-prove needs a file as an argument");
      return(1);
    }  
    shmptr=wg_attach_local_database(shmsize);
    if(!shmptr) {
      err_printf("failed to attach local database");
      return(1);
    }
    //islocaldb=1;
    err = wg_import_otter_file(shmptr,cmdfiles[1],0,&informat);   
    if(!err) {
      //printf("Data read from %s.\n",cmdfiles[1]);
    } else if(err<-1) {
      err_printf("fatal error when reading otter file, data may be partially"\
        " imported");
      return(1);   
    } else {
      //err_printf("import failed");           
      return(1); 
    }  


    //wg_show_database(shmptr);
    //printf("about to call wg_run_reasoner\n");
    err = wg_run_reasoner(shmptr,cmdfileslen,cmdfiles,informat,NULL,NULL);
    //wg_show_database(shmptr);
    //if(!err);
      //printf("wg_run_reasoner finished ok.\n");
    //else
      //fprintf(stderr, "wg_run_reasoner finished with an error %d.\n",err);
    //break;

    return(0);
  }  

  // -readkb

  if(!(strncmp(cmdstr,"-readkb",15))) {
    wg_int err;

    if (cmdfileslen<2) {
      err_printf("-readkb needs a file as an argument");
      return(1);
    }   
#ifdef _WIN32      
    if (mbsize && mbsize<100) {
      err_printf("-readkb needs at least 100 megabytes: change -mbsize argument");
      return(1);
    }        
#else
    if (mbsize && mbsize<1000) {
      err_printf("-readkb needs at least 1000 megabytes: change -mbsize argument");
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
      err_printf("failed to attach to database");
      return(1);
    }
#ifdef SHOWTIME       
    printf("\nto wg_import_otter_file %s\n",cmdfiles[1]);
    gkc_show_cur_time();
#endif      
    err = wg_import_otter_file(shmptr,cmdfiles[1],1,&informat);
#ifdef SHOWTIME       
    printf("\nexited wg_import_otter_file\n");
    gkc_show_cur_time();
#endif      
    if(!err)
      printf("Data parsed into the shared memory db, starting to build indexes.");
    else if(err<-1)
      err_printf("problem reading otter file, data may be partially"\
        " imported");
    else {
      err_printf("reading failed");
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
      err_printf("db creation failed");
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

  // -provekb

  if(!(strncmp(cmdstr,"-provekb",15))) {
    wg_int err;

    if (cmdfileslen<2) {
      err_printf("-provekb needs a file as an argument");
      return(1);
    }      
#ifdef SHOWTIME 
    printf("\n-querykb starts with external shmname %s\n",shmname);
    gkc_show_cur_time();
#endif      
    shmptr=wg_attach_existing_database(shmname);
    if(!shmptr) {
      err_printf("failed to attach to database");
      return(1);
    }
    printf("Using the existing shared memory kb %s.\n",shmname);
#ifdef SHOWTIME       
    printf("\ndb attached, showing attached shared memory db shmptr %ld\n",
      (unsigned long int)((gint)shmptr));
    gkc_show_cur_time();
    wr_show_database_details(NULL,shmptr,"shmptr");
#endif      
    // --- create a new temporary local db ---
    shmsize2=100000000;     
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
      err_printf("failed to attach local database");
      return(1);
    }        
    //islocaldb=1;
    err=0;
#ifdef SHOWTIME
    printf("\nqrun1 to wg_import_otter_file from argv[i+1] %s\n",cmdfiles[1]);
    gkc_show_cur_time();
#endif      
    err = wg_import_otter_file(shmptrlocal,cmdfiles[1],0,&informat);
    if(!err) {
      //printf("Data read from %s.\n",cmdfiles[1]);
    } else if(err<-1) {
      err_printf("fatal error when reading otter file, data may be partially"\
        " imported");
      return(1);   
    } else {
      //err_printf("import failed");           
      return(1); 
    }  
    
    // ---- local db created ------
#ifdef SHOWTIME
    printf("\nto call wg_run_reasoner\n");
    gkc_show_cur_time();
#endif      
    /*
    glb* xg=wr_glb_new_simple(shmptrlocal);
    wr_glb_init_local_complex(xg); 
    tmp=wr_analyze_clause_list(xg,(dbmemsegh(shmptrlocal))->clauselist);
    wr_show_in_stats(xg);
    return(0);
    */
    err = wg_run_reasoner(shmptrlocal,cmdfileslen,cmdfiles,informat,NULL,NULL);
    //if(!err);
      //printf("wg_run_reasoner finished ok.\n");
    //else
      //fprintf(stderr, "wg_run_reasoner finished with an error %d.\n",err);
    //break;
#ifdef SHOWTIME      
    printf("\nwg_run_reasoner returned\n");
    gkc_show_cur_time();
#endif
    //printf("\nshowing shared memory db\n"); 
    //wr_show_database_details(NULL,shmptr,"shmptr");
    //printf("\n-querykb exits\n");
    return(0);  
  }

  // -writekb

  if (!(strncmp(cmdstr,"-writekb",15))) {
     wg_int err;
     int flags = 0;

    if (cmdfileslen<2) {
      err_printf("-writekb needs a file as an argument");
      return(1);
    }  
    shmptr=wg_attach_existing_database(shmname);
    if(!shmptr) {
      err_printf("failed to attach local database");
      return(1);
    }

    /* Locking is handled internally by the dbdump.c functions */
    if(flags & FLAGS_FORCE)
      err = wg_dump_internal(shmptr,cmdfiles[1], 0);
    else
      err = wg_dump(shmptr,cmdfiles[1]);

    if(err<-1) {
      err_printf("cannot write to file, kb may have"\
        " become corrupt");
      return(1);  
    } else if(err) {
      err_printf("writing failed");
      return(1);
    }
    return(0);
  }

  // -loadkb

  if (!(strncmp(cmdstr,"-loadkb",15))) {
    wg_int err, minsize, maxsize;
    int flags = 0;  

    if (cmdfileslen<2) {
      err_printf("-loadkb needs a file as an argument");
      return(1);
    }  
    err = wg_check_dump(NULL, cmdfiles[1], &minsize, &maxsize);
    if(err) {
      err_printf2("loading failed, problem with %s",cmdfiles[1]);
      return(1);
    }

    shmptr=wg_attach_memsegment(shmname, minsize, maxsize, 1,
      (flags & FLAGS_LOGGING), 0);
    if(!shmptr) {
      err_printf("error: failed to attach local database");
      return(1);
    }

    // Locking is handled internally by the dbdump.c functions
    err = wg_import_dump(shmptr,cmdfiles[1]);
    if(!err)
      printf("Database imported.\n");
    else if(err<-1) {
      err_printf("failed to load, db may have"\
        " become corrupt");
      return(1);  
    } else {
      err_printf("loading failed");
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
      err_printf("-readwritekb needs a data file and a dump filename as arguments");
      return(1);
    }   
#ifdef _WIN32      
    if (mbsize && mbsize<100) {
      err_printf("-readwritekb needs at least 100 megabytes: change -mbsize argument");
      return(1);
    }        
#else
    if (mbsize && mbsize<1000) {
      err_printf("-readwritekb needs at least 1000 megabytes: change -mbsize argument");
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
      err_printf("failed to attach to database");
      return(1);
    }
#ifdef SHOWTIME       
    printf("\nto wg_import_otter_file %s\n",cmdfiles[1]);
    gkc_show_cur_time();
#endif      
    err = wg_import_otter_file(shmptr,cmdfiles[1],1,&informat);
#ifdef SHOWTIME       
    printf("\nexited wg_import_otter_file\n");
    gkc_show_cur_time();
#endif      
    if(!err)
      printf("Data parsed into the shared memory db, starting to build indexes.");
    else if(err<-1)
      err_printf("fatal error when reading otter file, data may be partially"\
        " imported");
    else {
      err_printf("reading failed");
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
      err_printf("db creation failed");
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
      err_printf("cannot write to file, kb may have"\
        " become corrupt");
      return(1);  
    } else if(err) {
      err_printf("writing failed");
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
    err_printf("-deletekb is available on Linux and OSX only");
    return(0);
#endif      
    wg_delete_database(shmname);
    return(0);
  }
  return(0);
}



/** usage: display command line help.
*
*/

void usage(char *prog) {
  printf("gkc is a reasoner for large knowledge bases.\n\n"\
         "usage:\n"\
         "basic proof search with a default strategy:\n"\
         "  gkc <problem file>\n"\
         "proof search with a strategy selection file:\n"\
         "  gkc -prove <problem file> <strategy file>\n"\
         "parse and load a file into shared memory database:\n"\
         "  gkc -readkb <axioms file>\n"\
         "proof search using the shared memory database as prepared additional axioms:\n"\
         "  gkc -provekb <problem file> <strategy file>\n"\
         "write the present shared memory database to a file for fast loading:\n"\
         "  gkc -writekb <dump file>\n"\
         "load a shared memory database from a file:\n"\
         "  gkc -loadkb <dump file>\n"\
         "parse and load a file into shared memory database and write this to a file:\n"\
         "  gkc -readwritekb <filename> <dump file>\n"\
         "delete the present shared memory database (not necessary for reading a new one):\n"\
         "  gkc -deletekb\n"\
         "show gkc version:\n"\
         "  gkc -version\n"\
         "\n"\
         "where:\n"\
         "  <problem file> should be in TPTP FOF or CNF syntax or Otter CNF syntax\n"\
         "  <strategy file> is an optional json file: see neg.txt, runs.txt in Examples\n"\
         "  <axioms file> is like a <problem file> \n"\
         "  <dump file> stores the parsed and prepared database for fast loading \n"\
         "\n"\
         "additional optional parameters:\n"\
         "  -mbsize <megabytes to allocate>\n"\
         "   if omitted, 1000 megabytes assumed\n"\
         "  -mbnr <shared memory database nr>\n"\
         "   if omitted, 1000 used\n");
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

char** parse_cmdline(int argc, char **argv, char** cmdstr, int* mbnr, int* mbsize, int* retcode) {
  int i,alen,nargc;
  char* arg;
  char c;
  char **nargv;
  int nr;
  
  // init nargv: an array of non-command args, i.e. filenames
  nargc=0;
  nargv=(char**)malloc(sizeof(char*)*argc);
  for(i=0;i<argc;i++) {
    nargv[i]=NULL;
  }
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
      } else if (!(strncmp(arg,"-prove",10)) || 
                !(strncmp(arg,"-readkb",10)) ||
                !(strncmp(arg,"-provekb",10)) || 
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
    }    
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
  //gint offset;
  //gint refc;
  //int encoffset;

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

void err_printf(char* s) {
  printf("{\"error\": \"%s\"}\n",s);
}

void err_printf2(char* s1, char* s2) {
  printf("{\"error\": \"%s %s\"}\n",s1,s2);
}

/*

============= TPTP LTB handling ==============

*/


int gkc_ltb_main(int argc, char **argv) {

  //char *shmname = NULL;
  //char shmnamebuf[16];
  void *shmptr = NULL;
  void *shmptrlocal = NULL;
  int i,j,len,seppos; //, scan_to;
  gint shmsize=0, shmsize2=100000000;
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
  char* rows[100000];  
  char probname[1000];
  char outname[1000];
  char probfullname[1000];
  char outfullname[1000];
  //char* sep;
  char* cmdfiles[2];
  int err,isproblemrow,isincluderow;
  char* strats[10];
  //int stratscount;
  char* stratstr;
  int iteration;
  //int cp;
  

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
  // show batch
  //printf("\nread batchfile ok, rowcount %d\n",rowcount);
  /*
  for(i=0;i<rowcount;i++) {
    printf("%d %s",i,rows[i]);
  }
  printf("\n");
  */
  // create a new local database for includes

  shmsize = (gint)5000000000;      
  //wg_delete_database("1000");
  //shmptr=wg_attach_database("1000", shmsize);  
  shmptr=wg_attach_local_database(shmsize);
  if(!shmptr) {
    err_printf("failed to attach to database");
    exit(0);
  }    

  // read includes
   
  isincluderow=0;
  for(i=0;i<rowcount;i++) {
    rowstr=rows[i];
    if (!rowstr) continue;    
    if (!strncmp("% SZS start BatchIncludes",rowstr, strlen("% SZS start BatchIncludes"))) {
      isincluderow=1;  
      continue;
    }
    if (!strncmp("% SZS end BatchIncludes",rowstr, strlen("% SZS end BatchIncludes"))) {
      break;
    }
    if (isincluderow && strlen(rowstr)>1) {
      printf("%s",rowstr);
      // -- st         
      wr_make_batch_axiom_fullname(rowstr,probfullname,batchfilename);
      //printf("\nprobname |%s| probfullname |%s|\n",
      //  rowstr,probfullname);
      err = wg_import_otter_file(shmptr,probfullname,1,&informat);    
      if(!err)
        printf("Data parsed into the memory db.\n");
      else if(err<-1)
        err_printf("problem reading otter file, data may be partially"\
          " imported");
      else {
        err_printf("reading failed");
        wg_delete_local_database(shmptr);
        return(1);
      }
      // -- ed
    }  
  }
  tmp=init_shared_database(shmptr,NULL);
  //shmptr=wg_attach_existing_database("1000");
  //wr_show_database_details(NULL,shmptr,"shmptr");
  if (tmp<0) {
    err_printf("db creation failed");   
    return(1);
  }  
  printf("Db ready.\n");
  //wr_show_database_details(NULL,shmptr,"shmptr");


  // includes are read, proceed to strategy and problems

  // strategy
  
  make_ltb_guide(NULL,strats,10);

  // problems

  //printf("\nproblems:\n");

  for(iteration=0;iteration<4;iteration++) {
    // next run over all the problems
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
      //printf("problem %d %s",i,rowstr);
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
      *outfullname=0;
      strcat(outfullname,outfoldername); 
      strcat(outfullname,"/");
      strcat(outfullname,outname);   
      wr_make_batch_prob_fullname(probname,probfullname,batchfilename);
      //printf("\nprobname |%s| outname |%s| probfullname |%s| outfullname |%s|\n",
      //    probname,outname,probfullname,outfullname);
      printf("%% SZS status Started for %s\n",probfullname);  
      //shmptr=NULL;
      shmsize2=100000000;        
            
  #ifdef SHOWTIME      
      printf("\nto wg_attach_local_database_with_kb with shmptr %ld\n",
        (unsigned long int)((gint)shmptr));
      gkc_show_cur_time();
  #endif      
      //shmptrlocal=wg_attach_local_database_with_kb(shmsize2,(void*)shmptr);
      //shmptrlocal=wg_attach_local_database(shmsize2);
      shmptrlocal=wg_attach_local_database_with_kb(shmsize2,(void*)shmptr);
      if(!shmptrlocal) {
        //err_printf("failed to attach local database");
        wr_output_batch_prob_failure(probfullname,outfullname,"Error");       
        continue;
      }
      err = wg_import_otter_file(shmptrlocal,probfullname,0,&informat);    
      if(!err) {
        //printf("Data read from %s.\n",cmdfiles[1]);
      } else if(err<-1) {  
        wr_output_batch_prob_failure(probfullname,outfullname,"Error");     
        continue;
      } else {
        wr_output_batch_prob_failure(probfullname,outfullname,"Error");     
        continue; 
      }     
      //wg_show_database(shmptrlocal);
      //wr_show_database_details(NULL,shmptrlocal,"shmptrlocal");
      //printf("about to call wg_run_reasoner\n");
      cmdfiles[1]=probfullname;
      stratstr=strats[iteration];
      //printf("\nto use strategy \n%s\n",stratstr);
      err = wg_run_reasoner(shmptrlocal,2,cmdfiles,informat,outfullname,stratstr);
      //err = wg_run_reasoner(shmptrlocal,2,cmdfiles,informat,NULL);
      //wg_show_database(shmptr);
      if(!err) {
        //printf("wg_run_reasoner finished ok.\n");
        printf("%% SZS status Theorem for %s\n",probfullname);
        rows[i]=NULL;
        wg_delete_local_database(shmptrlocal);
        printf("%% SZS status Ended for %s\n",probfullname);
        continue;
      } else {
        wg_delete_local_database(shmptrlocal);
        wr_output_batch_prob_failure(probfullname,outfullname,"Timeout");     
        continue;
      }              
    } // iteration over all problems ended
  }  // iteration over all strategies ended
  return(1);  
}

void wr_make_batch_axiom_fullname(char* probname, char* probfullname, char* batchname) {
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
    if(probname[i]!='*') {
      probfullname[j++]=probname[i];
      continue;
    }  
    probfullname[j++]='+';
    probfullname[j++]='1';   
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
    if(probname[i]!='*') {
      probfullname[j++]=probname[i];
      continue;
    }  
    probfullname[j++]='+';
    probfullname[j++]='1';
  }
  probfullname[j]=0;
}

/*

envfolder=getenv("TPTP");
#ifdef IDEBUG
  //printf("\npathatom is\n");
  //wg_mpool_print(db,pathatom); 
  printf("\nTPTP env var value is %s\n",envfolder);
  printf("\n$TPTP env var value is  %s\n",getenv("$TPTP"));
  //wg_mpool_print(db,envfolder);
  //printf("\nwg_atomtype(db,pathatom) %d WG_URITYPE %d\n",wg_atomtype(db,pathatom),WG_URITYPE);
  //printf("\n");
#endif  
  if (wg_atomtype(db,pathatom)!=WG_URITYPE) return 0;  
  str=wg_atomstr1(db,pathatom); 
  if (str && str[0]=='#') str=str+1;
  lastslash=strrchr(g->filename,'/');
  if (lastslash!=NULL) {
    bytes=strlen(str)+strlen(g->filename)+10;
    str2=wg_alloc_mpool(db,mpool,bytes);
    for(p1=(g->filename), p2=str2; p1<lastslash; p1++, p2++) {
      *p2=*p1;
    }
    *p2=(char)0;    
    strcat(str2,"/");
    strcat(str2,str);    
  } else {
    str2=str;
  }  

 */

void wr_output_batch_prob_failure(char* probname, char* outfullname, char* failure) {
  FILE* outfile;

  printf("%% SZS status %s for %s\n",failure,probname);
  printf("%% SZS status Ended for %s\n",probname);
  outfile=fopen(outfullname, "w");
  if (!outfile) {
    printf("{\"error\": \"cannot open outfile to print failure %s\"}\n",outfullname);               
  } else {
    fprintf(outfile,"%% SZS status %s for %s\n",failure,probname);   
    fclose(outfile);
  }  
}  

#ifdef __cplusplus
}
#endif
