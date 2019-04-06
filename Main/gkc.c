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

gint parse_shmsize(char *arg);
gint parse_flag(char *arg);
int parse_memmode(char *arg);
char** parse_cmdline(int argc, char **argv, char** cmdstr, int* mbnr, int* mbsize);
void wg_set_kb_db(void* db, void* kb);
void segment_stats(void *db);
void wg_show_strhash(void* db);
void wg_show_database(void* db);
void gkc_show_cur_time(void);

//#define SHOW_CONTENTS 1
//#undef SHOW_CONTENTS

/* ====== Functions ============== */

/*
how to set 500 meg of shared memory:

su
echo 500000000  > /proc/sys/kernel/shmmax
*/

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

char** parse_cmdline(int argc, char **argv, char** cmdstr, int* mbnr, int* mbsize) {
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
            exit(1);       
          } else {
            //printf("\n parsed nr %d\n",nr);
            if (nr<1) {
              printf("Error: -mbnr argument should be at least 1, not %s\n",argv[i]);
              exit(1);
            }
            *mbnr=nr;
          }
        } else {
          printf("Error: missing parameter to keyword %s\n",arg);
          exit(1);
        } 
      } else if (!(strncmp(arg,"-mbsize",10))) {
        if ((i+1)<argc) {
          //printf("\n cmd %s param %s",arg,argv[i+1]);
          i++;
          if (!(sscanf(argv[i], "%d", &nr))) {
            printf("Error: keyword %s argument must be a number, not %s\n",
                    arg,argv[i]);
            exit(1);
          } else {
            //printf("\n parsed nr %d\n",nr);
            if (nr<10) {
              printf("Error: -mbsize argument should be at least 10 (megabytes), not %s\n",argv[i]);
              exit(1);
            }
            if (nr>=100000) {
              printf("Error: -mbsize argument means megabytes: %s is too big\n",argv[i]);
              exit(1);
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
        exit(1);
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


/** top level for the database command line tool
*
*
*/

int main(int argc, char **argv) {

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
  //int islocaldb=0; // lreasoner sets to 1 to avoid detaching db at the end

  if (argc<2) {
    usage(argv[0]);
    exit(0);
  }
  cmdfiles=parse_cmdline(argc,argv,&cmdstr,&mbnr,&mbsize);     
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
    exit(0);
  } else if (!(strncmp(cmdstr,"-version",10)) || 
             !(strncmp(cmdstr,"--version",10)) ) {
    wg_print_code_version();
    exit(0); 
  }

  //printf("\ncmdstr %s cmdfileslen %d\n",cmdstr,cmdfileslen);

  if (mbsize==0) {
    shmsize = 1000000000; // 1G
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
      printf("Error: -prove needs a file as an argument\n");
      exit(1);
    }  
    shmptr=wg_attach_local_database(shmsize);
    if(!shmptr) {
      fprintf(stderr, "Failed to attach local database.\n");
      exit(1);
    }
    //islocaldb=1;
    err = wg_import_otter_file(shmptr,cmdfiles[1],0);
    if(!err)
       printf("Data read from %s.\n",cmdfiles[1]);
    else if(err<-1)
      fprintf(stderr, "Fatal error when importing otter file, data may be partially"\
        " imported\n");
    else
      fprintf(stderr, "Import failed.\n");      
    //wg_show_database(shmptr);
    //printf("about to call wg_run_reasoner\n");
    err = wg_run_reasoner(shmptr,cmdfileslen,cmdfiles);
    //wg_show_database(shmptr);
    //if(!err);
      //printf("wg_run_reasoner finished ok.\n");
    //else
      //fprintf(stderr, "wg_run_reasoner finished with an error %d.\n",err);
    //break;

    exit(0);
  }  

  // -readkb

  if(!(strncmp(cmdstr,"-readkb",15))) {
    wg_int err;

    if (cmdfileslen<2) {
      printf("Error: -readkb needs a file as an argument\n");
      exit(1);
    }   
#ifdef _WIN32      
    if (mbsize && mbsize<100) {
      printf("Error:-readkb needs at least 100 megabytes: change -mbsize argument \n");
      exit(1);
    }        
#else
    if (mbsize && mbsize<1000) {
      printf("Error: -readkb needs at least 1000 megabytes: change -mbsize argument \n");
      exit(1);
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
      fprintf(stderr, "Failed to attach to database.\n");
      exit(1);
    }
#ifdef SHOWTIME       
    printf("\nto wg_import_otter_file %s\n",cmdfiles[1]);
    gkc_show_cur_time();
#endif      
    err = wg_import_otter_file(shmptr,cmdfiles[1],1);
#ifdef SHOWTIME       
    printf("\nexited wg_import_otter_file\n");
    gkc_show_cur_time();
#endif      
    if(!err)
      printf("Data parsed into the shared memory db, starting to build indexes.");
    else if(err<-1)
      fprintf(stderr, "Fatal error when reading otter file, data may be partially"\
        " imported\n");
    else {
      fprintf(stderr, "Reading failed.\n");
      wg_delete_database(shmname);
      exit(1);
    }
    //wg_show_database(shmptr);
#ifdef SHOWTIME 
    printf("\nstarting to init_shared_database\n");      
    gkc_show_cur_time();
#endif
    tmp=init_shared_database(shmptr);
    if (tmp<0) {
      printf("\nDb creation failed.\n");
      wg_delete_database(shmname);
      exit(1);
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
    exit(0);
  }

  // -provekb

  if(!(strncmp(cmdstr,"-provekb",15))) {
    wg_int err;

    if (cmdfileslen<2) {
      printf("Error: -provekb needs a file as an argument\n");
      exit(1);
    }      
#ifdef SHOWTIME 
    printf("\n-querykb starts with external shmname %s\n",shmname);
    gkc_show_cur_time();
#endif      
    shmptr=wg_attach_existing_database(shmname);
    if(!shmptr) {
      fprintf(stderr, "Failed to attach to database.\n");
      exit(1);
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
      fprintf(stderr, "Failed to attach local database.\n");
      exit(1);
    }        
    //islocaldb=1;
    err=0;
#ifdef SHOWTIME
    printf("\nqrun1 to wg_import_otter_file from argv[i+1] %s\n",cmdfiles[1]);
    gkc_show_cur_time();
#endif      
    err = wg_import_otter_file(shmptrlocal,cmdfiles[1],0);
    if(!err)
      printf("Data read from %s.\n",cmdfiles[1]);
    else if(err<-1)
      fprintf(stderr, "Fatal error when reading otter file, data may be partially"\
        " imported\n");
    else
      fprintf(stderr, "Import failed.\n");           
    
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
    exit(0);
    */
    err = wg_run_reasoner(shmptrlocal,cmdfileslen,cmdfiles);
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
    exit(0);  
  }

  // -writekb

  if (!(strncmp(cmdstr,"-writekb",15))) {
     wg_int err;
     int flags = 0;

    if (cmdfileslen<2) {
      printf("Error: -writekb needs a file as an argument.\n");
      exit(1);
    }  
    shmptr=wg_attach_existing_database(shmname);
    if(!shmptr) {
      printf("Error: failed to attach local database.\n");
      exit(1);
    }

    /* Locking is handled internally by the dbdump.c functions */
    if(flags & FLAGS_FORCE)
      err = wg_dump_internal(shmptr,cmdfiles[1], 0);
    else
      err = wg_dump(shmptr,cmdfiles[1]);

    if(err<-1) {
      printf("Error: cannot write to file, kb may have"\
        " become corrupt\n");
      exit(1);  
    } else if(err) {
      printf("Error: writing failed.\n");
      exit(1);
    }
    exit(0);
  }

  // -loadkb

  if (!(strncmp(cmdstr,"-loadkb",15))) {
    wg_int err, minsize, maxsize;
    int flags = 0;  

    if (cmdfileslen<2) {
      printf("Error: -loadkb needs a file as an argument.\n");
      exit(1);
    }  
    err = wg_check_dump(NULL, cmdfiles[1], &minsize, &maxsize);
    if(err) {
      printf("Error: loading failed, problem with %s.\n",cmdfiles[1]);
      exit(1);
    }

    shmptr=wg_attach_memsegment(shmname, minsize, maxsize, 1,
      (flags & FLAGS_LOGGING), 0);
    if(!shmptr) {
      printf("Error: failed to attach local database.\n");
      exit(1);
    }

    // Locking is handled internally by the dbdump.c functions
    err = wg_import_dump(shmptr,cmdfiles[1]);
    if(!err)
      printf("Database imported.\n");
    else if(err<-1) {
      printf("Error: failed to load, db may have"\
        " become corrupt.\n");
      exit(1);  
    } else {
      printf("Error: loading failed.\n");
      exit(1);
    }
#ifdef _WIN32    
    printf ("Shared memory kb is available while this program is running.\n");
    printf ("Press any key to free shared memory kb and exit . . .\n");
    _getch();
#endif  
    //wr_show_database_details(NULL,shmptr,"shmptr");
    exit(0);
  }

   if(!(strncmp(cmdstr,"-readwritekb",15))) {
    wg_int err;
    int flags = 0;

    if (cmdfileslen<3) {
      printf("Error: -readwritekb needs a data file and a dump filename as arguments\n");
      exit(1);
    }   
#ifdef _WIN32      
    if (mbsize && mbsize<100) {
      printf("Error: -readwritekb needs at least 100 megabytes: change -mbsize argument \n");
      exit(1);
    }        
#else
    if (mbsize && mbsize<1000) {
      printf("Error: -readwritekb needs at least 1000 megabytes: change -mbsize argument \n");
      exit(1);
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
      fprintf(stderr, "Failed to attach to database.\n");
      exit(1);
    }
#ifdef SHOWTIME       
    printf("\nto wg_import_otter_file %s\n",cmdfiles[1]);
    gkc_show_cur_time();
#endif      
    err = wg_import_otter_file(shmptr,cmdfiles[1],1);
#ifdef SHOWTIME       
    printf("\nexited wg_import_otter_file\n");
    gkc_show_cur_time();
#endif      
    if(!err)
      printf("Data parsed into the shared memory db, starting to build indexes.");
    else if(err<-1)
      fprintf(stderr, "Fatal error when reading otter file, data may be partially"\
        " imported\n");
    else {
      fprintf(stderr, "Reading failed.\n");
      wg_delete_database(shmname);
      exit(1);
    }
    //wg_show_database(shmptr);
#ifdef SHOWTIME 
    printf("\nstarting to init_shared_database\n");      
    gkc_show_cur_time();
#endif
    tmp=init_shared_database(shmptr);
    if (tmp<0) {
      printf("\nDb creation failed.\n");
      wg_delete_database(shmname);
      exit(1);
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
      printf("Error: cannot write to file, kb may have"\
        " become corrupt\n");
      exit(1);  
    } else if(err) {
      printf("Error: writing failed.\n");
      exit(1);
    }
    printf("Database written to file %s and remains present in memory.\n",cmdfiles[2]);
#ifdef _WIN32    
    //char c1,c2;
    printf ("Shared memory kb is available while this program is running.\n");
    printf ("Press any key to free shared memory kb and exit . . .\n");
    _getch();
#endif    
    exit(0);
  }

   // -deletekb

  if (!(strncmp(cmdstr,"-deletekb",15))) {
    /* free shared memory */
#ifdef _WIN32      
    printf("Error: -deletekb is available on Linux and OSX only\n");
    exit(0);
#endif      
    wg_delete_database(shmname);
    exit(0);
  }

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



#ifdef __cplusplus
}
#endif
