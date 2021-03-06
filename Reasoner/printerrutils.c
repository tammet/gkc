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

 /** @file printerrutils.c
 *   printing and err handling utils.
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

#define PRINTERR

/* ======= Private protos ================ */


/* ====== Functions ============== */

  
/* =========== debug printing  ========================= */

  
  
/* ============ warnings =================== */

/** Guide-type warning not requiring immediate exit.
*
*/

void* wr_warn(glb* g, char* errstr) {
#ifdef PRINTERR  
  if (g->print_flag) printf("Warning: %s.\n",errstr);
#endif  
  return NULL;
}  

/** Guide-type warning not requiring immediate exit.
*
*/

void* wr_warn2(glb* g, char* errstr1, char* errstr2) {
#ifdef PRINTERR   
  if (g->print_flag) printf("Warning: %s %s.\n",errstr1,errstr2);  
#endif  
  return NULL;
}  



  
/* ======================= errors ================================= */

/** Error not requiring immediate exit; will exit up in the calling tree
*
*/

void* wr_errprint(char* errstr) {
  //printf("Error: %s.\n",errstr);
#ifdef PRINTERR   
  printf("{\"error\": \"%s\"}\n",errstr);
#endif  
  return NULL;
}  

/** Error not requiring immediate exit; will exit up in the calling tree
*
*/

void* wr_errprint2(char* errstr1, char* errstr2) {
  //printf("Error: %s %s.\n",errstr1,errstr2);
#ifdef PRINTERR   
  printf("{\"error\": \"%s %s\"}\n",errstr1,errstr2);
#endif  
  return NULL;
}

void* wr_errprint3(char* errstr1, char* errstr2, char* errstr3) {
  //printf("Error: %s %s.\n",errstr1,errstr2);
#ifdef PRINTERR   
  printf("{\"error\": \"%s %s %s\"}\n",errstr1,errstr2,errstr3);
#endif  
  return NULL;
}

/** Allocation error not requiring immediate exit.
*
*/

void* wr_alloc_err(glb* g, char* errstr) {
  if (g->print_flag) {
    //printf("Cannot allocate memory: %s.\n",errstr);
    wr_errprint2("cannot allocate memory",errstr);
  }  
  (g->alloc_err)=1;
  return NULL;
}  


/** Allocation error not requiring immediate exit.
*
*/

void* wr_alloc_err2(glb* g, char* errstr1, char* errstr2) {
  if (g->print_flag) {
    //printf("Cannot allocate memory: %s %s.\n",errstr1,errstr2);
    wr_errprint3("cannot allocate memory",errstr1,errstr2);
  }  
  (g->alloc_err)=1;
  return NULL;
}  


/** Allocation error not requiring immediate exit.
*
*/

void* wr_alloc_err2int(glb* g, char* errstr, int n) {
#ifdef PRINTERR   
  if (g->print_flag) printf("Cannot allocate memory: %s %d.\n",errstr,n);
#endif  
  (g->alloc_err)=1;
  return NULL;
}  



/** Hard system error requiring immediate exit.
*
*/

void wr_sys_exiterr(glb* g, char* errstr) {
  //printf("System error in wgdb reasoner, exiting: %s.\n",errstr);
  wr_errprint2("system error",errstr);
  exit(1);
}  


/** Hard system error requiring immediate exit.
*
*/

void wr_sys_exiterr2(glb* g, char* errstr1, char* errstr2) {
  //printf("System error in wgdb reasoner, exiting: %s %s.\n",errstr1,errstr2);
  wr_errprint3("system error",errstr1,errstr2);
  exit(1);
}  


/** Hard system error requiring immediate exit.
*
*/

void wr_sys_exiterr2int(glb* g, char* errstr, int n) {
#ifdef PRINTERR   
  printf("System error in wgdb reasoner, exiting: %s %d.\n",errstr,n);
#endif  
  exit(1);
}  

/** Hard io-type error requiring immediate exit.
*
*/

void wr_exiterr(glb* g, char* errstr1) {
  //printf("%s.\n",errstr1);
  wr_errprint(errstr1);
  exit(1);
} 

/** Hard io-type error requiring immediate exit.
*
*/

void wr_exiterr2(glb* g, char* errstr1, char* errstr2) {
  //printf("%s %s.\n",errstr1,errstr2);
  wr_errprint2(errstr1,errstr2);
  exit(1);
}  


/** Hard io-type error error requiring immediate exit.
*
*/

void wr_exiterr2int(glb* g, char* errstr, int n) {
#ifdef PRINTERR   
  printf("System error in wgdb reasoner, exiting: %s %d.\n",errstr,n);
#endif  
  exit(1);
}


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

#ifdef __cplusplus
}
#endif
