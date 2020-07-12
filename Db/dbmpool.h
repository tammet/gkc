/*
* $Id:  $
* $Version: $
*
* Copyright (c) Tanel Tammet 2004,2005,2006,2007,2008,2009
*
* Contact: tanel.tammet@gmail.com
*
* This file is part of WhiteDB
*
* WhiteDB is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* WhiteDB is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with WhiteDB.  If not, see <http://www.gnu.org/licenses/>.
*
*/

 /** @file dbmpool.h
 * Public headers for memory pool utilities.
 */

#ifndef DEFINED_DBMPOOL_H
#define DEFINED_DBMPOOL_H

#ifdef _WIN32
#include "../config-w32.h"
#else
#include "../config.h"
#endif

typedef ptrdiff_t wg_int;
typedef ptrdiff_t gint;

/* ====== data structures ======== */

/** located inside mpool_header: one single memory subarea header
*
*
*/

#define NROF_SUBAREAS 100           // size of subarea array

typedef struct _wg_mpoolsubarea_header {
  int size;           /** size of subarea in bytes */
  void* area_start;   /** pointer to the first byte of the subarea */
  void* area_end;     /** pointer to the first byte after the subarea */
} mpool_subarea_header;


/** memory pool management data
*  stored in the beginning of the first segment of mempool
*
*/

typedef struct {
  void* freeptr;     /** pointer to the next free location in the pool */
  int cur_subarea;   /** index of the currently used subarea in the subarea_table (starts with 0) */
  int nrof_subareas; /** full nr of rows in the subarea table */
  mpool_subarea_header subarea_table[NROF_SUBAREAS];    /** subarea information (mpool_subarea_header) table */
} mpool_header;

/* ==== Protos ==== */

mpool_header* wg_create_mpool(void* db, int bytes);             // call this to init pool with initial size bytes
void* wg_alloc_mpool(void* db, mpool_header* mpool, int bytes); // call each time you want to "malloc":
                                                        // automatically extends pool if no space left
void wg_free_mpool(void* db, mpool_header* mpool);              // remove the whole pool

int wg_ispair(void* db, void* ptr);
void* wg_mkpair(void* db, void* mpool, void* x, void* y);
void wg_replacepair_first(void* db, void* ptr, void* val);
void wg_replacepair_rest(void* db, void* ptr, void* val);
void* wg_mklist1(void* db, void* mpool, void* x1);
void* wg_mklist2(void* db, void* mpool, void* x1, void* x2);
void* wg_mklist3(void* db, void* mpool, void* x1, void* x2, void* x3);
void* wg_mklist4(void* db, void* mpool, void* x1, void* x2, void* x3, void* x4);
void* wg_appendlist(void* db, void* mpool, void* arg1, void* arg2);
void* wg_reverselist(void* db, void* mpool, void* arg1);
void* wg_inplace_reverselist(void* db, void* mpool, void* arg1);

void* wg_first(void* db, void* ptr);
void* wg_rest(void* db, void *ptr);
void* wg_nth(void* db, void *ptr, int n);

int wg_list_len(void* db, void *ptr);
int wg_listtreecount(void* db, void *ptr);

void* wg_list_memberuri(void* db, void* list, void* el);

void* wg_add_assoc(void* db, void* mpool, void* el1, void* el2, void* alist);
void* wg_get_assoc(void* db, void* el1, void* alist);

void* wg_get_keyval(void* db, void* key, void* keyval);
void* wg_get_keystrval(void* db, char* str, void* keyval);

int wg_isatom(void* db, void* ptr);
void* wg_mkatom(void* db, void* mpool, int type, char* str1, char* str2);

void* wg_mkatom_len(void* db, void* mpool, int type, char* str1, char* str2, int len1, int len2);
void* wg_mkatom_int(void* db, void* mpool, gint num);
void* wg_mkatom_int2(void* db, void* mpool, double num);
void* wg_mkatom_double(void* db, void* mpool, double num);

void* wg_mkrecatom(void* db, void* mpool, wg_int rec);
int wg_atomtype(void* db, void* ptr);
char* wg_atomstr1(void* db, void* ptr);
char* wg_atomstr2(void* db, void* ptr);
wg_int wg_atomrec(void* db, void* ptr);

int wg_islogtrue(void* db, void* ptr);
int wg_islogfalse(void* db, void* ptr);
int wg_islogneg(void* db, void* ptr);
int wg_islogand(void* db, void* ptr);
int wg_islogor(void* db, void* ptr);
int wg_islogimp(void* db, void* ptr);
int wg_islogeqv(void* db, void* ptr);
int wg_islogall(void* db, void* ptr);
int wg_islogexists(void* db, void* ptr);
int wg_islogat(void* db, void* ptr);
int wg_islogconnective(void* db, void* ptr);
int wg_islog_conn(void* db, void* ptr, char* str);
int wg_islog_uri(void* db, void* ptr, char* str);

void* wg_makelogtrue(void* db, void* mpool);
void* wg_makelogfalse(void* db, void* mpool);
void* wg_makelogneg(void* db, void* mpool);
void* wg_makelogand(void* db, void* mpool);
void* wg_makelogor(void* db, void* mpool);
void* wg_makelogimp(void* db, void* mpool);
void* wg_makelogeqv(void* db, void* mpool);
void* wg_makelogall(void* db, void* mpool);
void* wg_makelogexists(void* db, void* mpool);
void* wg_makeatomeq(void* db, void* mpool);
void* wg_makelogat(void* db, void* mpool);
void* wg_makelog_conn(void* db, void* mpool, char* str);

void* wg_mpool_copy(void* db, void* mpool, void* ptr);
void wg_mpool_print(void* db, void* ptr);


#endif /* DEFINED_DBMPOOL_H */
