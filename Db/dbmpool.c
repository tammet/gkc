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

 /** @file dbmpool.c
 *  Allocating data using a temporary memory pool.
 *
 */

/* ====== Includes =============== */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/shm.h>
#include <errno.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#include "../config-w32.h"
#else
#include "../config.h"
#endif
#include "dballoc.h"
#include "dbmem.h"
#include "dbapi.h"
#include "dbmpool.h"

#include "dbutil.h"

/* ====== Private headers and defs ======== */


#define MIN_FIRST_SUBAREA_SIZE 1024 // first free area minimum: if less asked, this given
#define ALIGNMENT_BYTES 4           // every val returned by wg_alloc_mpool is aligned to this

#define TYPEMASK 1    // memory pool convenience objects type mask for address
#define PAIRBITS 0    // memory pool convenience objects type bit for pairs (lists)
#define ATOMBITS 1    // memory pool convenience objects type bit for atoms (strings etc)

#define MAX_MPOOL_BYTES 3000000000
//#define EXIT_ON_ERROR


/* ======= Private protos ================ */

static int extend_mpool(void* db, mpool_header* mpool, int minbytes);
static int show_mpool_error(void* db, char* errmsg);
static int show_mpool_error_nr(void* db, char* errmsg, int nr);
static void wg_mpool_print_aux(void* db, void* ptr, int depth, int pflag);

#define WG_NO_ERRPRINT

/* ====== Functions for mpool creating/extending/allocating/destroying ============== */


/** create and initialise a new memory pool
*
* initial pool has at least origbytes of free space
* mpool is extended automatically later when space used up
* returns void* pointer to mpool if OK, NULL if failure
*
* does a single malloc (later extensions do further mallocs)
*/



mpool_header* wg_create_mpool(void* db, int origbytes) {
  //int bytes;
  mpool_header* mpool;
  mpool_header* mpoolh;
  //int puresize;
  void* nextptr;
  int i;

  void* subarea;
  /*
  if (origbytes<MIN_FIRST_SUBAREA_SIZE+ALIGNMENT_BYTES)
    bytes=sizeof(mpool_header)+MIN_FIRST_SUBAREA_SIZE+ALIGNMENT_BYTES;
  else
    bytes=sizeof(mpool_header)+origbytes+ALIGNMENT_BYTES;
  puresize=bytes-sizeof(mpool_header);
  */
  mpool=malloc(sizeof(mpool_header));
  if (mpool==NULL) {
    show_mpool_error_nr(db,
      " cannot create an mpool with size:  ",origbytes);
    return NULL;
  }
  mpoolh=mpool;
  subarea=malloc(origbytes);
  if (subarea==NULL) {
    show_mpool_error_nr(db,
      " cannot create mpool of size: ",origbytes);
    return NULL;
  }
  (mpoolh->freeptr)=subarea;
  (mpoolh->cur_subarea)=0;
  ((mpoolh->subarea_table)[mpoolh->cur_subarea]).size=origbytes;
  nextptr=subarea;
  // set correct alignment for nextptr
  i=((size_t)nextptr)%ALIGNMENT_BYTES;
  if (i!=0) nextptr=((char*)nextptr)+(ALIGNMENT_BYTES-i);
  // aligment now ok
  (mpoolh->freeptr)=nextptr;
  ((mpoolh->subarea_table)[mpoolh->cur_subarea]).area_start=subarea;
  ((mpoolh->subarea_table)[mpoolh->cur_subarea]).area_end=(void*)(((char*)subarea)+origbytes);
  return mpool;
}


/** extend an existing memory pool
*
* called automatically when mpool space used up
* does one malloc for a new subarea
*
*/


static int extend_mpool(void* db, mpool_header* mpool, int minbytes) {
  int cursize;
  gint bytes; // int bytes;
  void* subarea;
  mpool_header* mpoolh;
  int i;
  void* nextptr;

  mpoolh=mpool;
  cursize=((mpoolh->subarea_table)[(mpoolh->cur_subarea)]).size;
  bytes=cursize;
  for(i=0;i<100;i++) {
    bytes=bytes*2;
    if (bytes>=(minbytes+ALIGNMENT_BYTES)) break;
  }
#ifndef __EMSCRIPTEN__  
  if (bytes>MAX_MPOOL_BYTES) {
    show_mpool_error_nr(db,
      " limit blocks extending mpool size to ",bytes);
    return -1;
  }
#endif  
  subarea=malloc(bytes);
  if (subarea==NULL) {
    show_mpool_error_nr(db,
      " failed to extend mpool to size: ",bytes);
    return -1;
  }
  (mpoolh->freeptr)=subarea;
  (mpoolh->cur_subarea)++;
  ((mpoolh->subarea_table)[mpoolh->cur_subarea]).size=bytes;
  nextptr=subarea;
  // set correct alignment for nextptr
  i=((size_t)nextptr)%ALIGNMENT_BYTES;
  if (i!=0) nextptr=((char*)nextptr)+(ALIGNMENT_BYTES-i);
  // aligment now ok
  (mpoolh->freeptr)=nextptr;
  ((mpoolh->subarea_table)[mpoolh->cur_subarea]).area_start=subarea;
  ((mpoolh->subarea_table)[mpoolh->cur_subarea]).area_end=(void*)(((char*)subarea)+bytes);
  return 0;
}

/** free the whole memory pool
*
* frees all the malloced subareas and initial mpool
*
*/

void wg_free_mpool(void* db, mpool_header* mpool) {
  int i;
  mpool_header* mpoolh;

  mpoolh=mpool;
  if (!mpoolh) return;
  i=mpoolh->cur_subarea;
  for(;;i--) {
    //printf("\n i in loop %d",i);
    //printf("\n size %d",((mpoolh->subarea_table)[i]).size);
    if (((mpoolh->subarea_table)[i]).area_start) {
      free(((mpoolh->subarea_table)[i]).area_start);
    }  
    if (i<=0) break;
  }  
  free(mpoolh);
}

/** allocate bytes from a memory pool: analogous to malloc
*
* mpool is extended automatically if not enough free space present
* returns void* pointer to a memory block if OK, NULL if failure
*
*/

void* wg_alloc_mpool(void* db, mpool_header* mpool, int bytes) {
  void* curptr;
  void* nextptr;
  mpool_header* mpoolh;
  void* curend;
  int tmp;
  int i;

  if (bytes<=0) {
    show_mpool_error_nr(db,
      " trying to allocate too little from mpool: ",bytes);
    return NULL;
  }
  if (mpool==NULL) {
    show_mpool_error(db," mpool passed to wg_alloc_mpool is NULL ");
    return NULL;
  }
  mpoolh=mpool; 
  nextptr=(void*)(((char*)(mpoolh->freeptr))+bytes);
  curend=((mpoolh->subarea_table)[(mpoolh->cur_subarea)]).area_end;
  if (nextptr>curend) {
    //printf("\nmust extend with %d\n",bytes);    
    tmp=extend_mpool(db,mpool,bytes);
    if (tmp!=0) {
      show_mpool_error_nr(db," cannot extend mpool size by: ",bytes);
      return NULL;
    }
    nextptr=((char*)(mpoolh->freeptr))+bytes;
  }
  curptr=mpoolh->freeptr;
  // set correct alignment for nextptr
  i=((size_t)nextptr)%ALIGNMENT_BYTES;
  if (i!=0) nextptr=((char*)nextptr)+(ALIGNMENT_BYTES-i);
  // alignment now ok
  mpoolh->freeptr=nextptr;
  return curptr;
}



/* ====== Convenience functions for using data allocated from mpool ================= */

/*

Core object types are pairs and atoms plus 0 (NULL).

Lists are formed by pairs of gints. Each pair starts at address with two last bits 0.
The first element of the pair points to the contents of the cell, the second to rest.

Atoms may contain strings, ints etc etc. Each atom starts at address with last bit 1.

The first byte of the atom indicates its type. The following bytes are content, always
encoded as a 0-terminated string or TWO consequent 0-terminated strings.

The atom type byte contains dbapi.h values:

STRING, CONVERSION TO BE DETERMINED LATER: 0
#define WG_NULLTYPE 1
#define WG_RECORDTYPE 2
#define WG_INTTYPE 3
#define WG_DOUBLETYPE 4
#define WG_STRTYPE 5
#define WG_XMLLITERALTYPE 6
#define WG_URITYPE 7
#define WG_BLOBTYPE 8
#define WG_CHARTYPE 9
#define WG_FIXPOINTTYPE 10
#define WG_DATETYPE 11
#define WG_TIMETYPE 12
#define WG_ANONCONSTTYPE 13
#define WG_VARTYPE 14
#define WG_ILLEGAL 0xff

Atom types 5-8 (strings, xmlliterals, uris, blobs) contain TWO
consequent strings, first the main, terminating 0, then the
second (lang, namespace etc) and the terminating 0. Two terminating
0-s after the first indicates the missing second string (NULL).

Other types are simply terminated by two 0-s.

*/


// ------------- make pairs ----------------


int wg_ispair(void* db, void* ptr) {
  return (ptr!=NULL && ((((gint)ptr)&TYPEMASK)==PAIRBITS));
}

void* wg_mkpair(void* db, void* mpool, void* x, void* y) {
  void* ptr;

  ptr=wg_alloc_mpool(db,mpool,sizeof(gint)*2);
  if (ptr==NULL) {
    show_mpool_error(db,"cannot create a pair in mpool");
    return NULL;
  }
  *((gint*)ptr)=(gint)x;
  *((gint*)ptr+1)=(gint)y;
  return ptr;
}

void wg_replacepair_first(void* db, void* ptr, void* val) {
  *((gint*)ptr)=(gint)val;
}

void wg_replacepair_rest(void* db, void* ptr, void* val) {
  *((gint*)ptr+1)=(gint)val;
}

void* wg_mklist1(void* db, void* mpool, void* x1) {
  return  wg_mkpair(db,mpool,x1,NULL);
}

void* wg_mklist2(void* db, void* mpool, void* x1, void* x2) {
  void* tmp;

  tmp=wg_mkpair(db,mpool,x2,NULL);
  if (tmp==NULL) return NULL;
  return wg_mkpair(db,mpool,x1,tmp);
}

void* wg_mklist3(void* db, void* mpool, void* x1, void* x2, void* x3) {
  void* tmp;

  tmp=wg_mkpair(db,mpool,x3,NULL);
  if (tmp==NULL) return NULL;
  tmp=wg_mkpair(db,mpool,x2,tmp);
  if (tmp==NULL) return NULL;
  return wg_mkpair(db,mpool,x1,tmp);
}

void* wg_mklist4(void* db, void* mpool, void* x1, void* x2, void* x3, void* x4) {
  void* tmp;

  tmp=wg_mkpair(db,mpool,x4,NULL);
  if (tmp==NULL) return NULL;
  tmp=wg_mkpair(db,mpool,x3,tmp);
  if (tmp==NULL) return NULL;
  tmp=wg_mkpair(db,mpool,x2,tmp);
  if (tmp==NULL) return NULL;
  return wg_mkpair(db,mpool,x1,tmp);
}

void* wg_mklist5(void* db, void* mpool, void* x1, void* x2, void* x3, void* x4, void* x5) {
  void* tmp;

  tmp=wg_mkpair(db,mpool,x5,NULL);
  if (tmp==NULL) return NULL;
  tmp=wg_mkpair(db,mpool,x4,tmp);
  if (tmp==NULL) return NULL;
  tmp=wg_mkpair(db,mpool,x3,tmp);
  if (tmp==NULL) return NULL;
  tmp=wg_mkpair(db,mpool,x2,tmp);
  if (tmp==NULL) return NULL;
  return wg_mkpair(db,mpool,x1,tmp);
}

void* wg_appendlist(void* db, void* mpool, void* arg1, void* arg2) {
  void *res1, *res;

  //if (!arg2) return arg1;
  if (!arg1) return arg2;
  res1=wg_appendlist(db,mpool,wg_rest(db,arg1),arg2);
  res=wg_mkpair(db,mpool,wg_first(db,arg1),res1);
  if (!res) {
    show_mpool_error(db,"cannot create a pair in mpool");
    return NULL;
  }
  return res;  
}

void* wg_reverselist(void* db, void* mpool, void* arg1) {
  void *res=NULL;
  void* part;

  if (!arg1) return arg1;
  for (part=arg1; wg_ispair(db,part); part=wg_rest(db,part)) {
    res=wg_mkpair(db,mpool,wg_first(db,part),res);
  }
  return res;
}

void* wg_inplace_reverselist(void* db, void* mpool, void* arg1) {
  //void *res=NULL;
  void *head;
  void *prevNode, *curNode;

  if (!arg1) return arg1;
  head=arg1;
  prevNode=arg1;
  curNode=wg_rest(db,head);
  head=wg_rest(db,head);
  //prevNode->next = NULL;
  *((gint*)prevNode+1)=(gint)NULL;  
  while(head != NULL) {    
    //head = head->next;
    head=wg_rest(db,head);
    //curNode->next = prevNode;
    *((gint*)curNode+1)=(gint)prevNode;   
    prevNode=curNode;
    curNode=head;
  }
  //*((gint*)arg1)=prevNode;  
  return prevNode;
}

// ------------- get pair parts ----------------

void* wg_first(void* db, void* ptr) {
  return (void*)(*((gint*)ptr));
}

void* wg_rest(void* db, void *ptr) {
  return (void*)(*((gint*)ptr+1));
}

void* wg_nth(void* db, void *ptr, int n) {
  void* part=ptr;
  int i;

  for(i=0; i<n; i++) {
    if (!wg_ispair(db,part)) return NULL; // terminate with NULL if not enough els
    part=wg_rest(db,part);
  }
  if (!wg_ispair(db,part)) return NULL;  
  return wg_first(db,part);
}

int wg_list_len(void* db, void *ptr) {
  void* part;
  int nr;

  for(part=ptr,nr=0; wg_ispair(db,part); part=wg_rest(db,part),nr++);
  return nr;
}

int wg_listtreecount(void* db, void *ptr) {
  if (wg_ispair(db,ptr))
    return wg_listtreecount(db,wg_first(db,ptr)) + wg_listtreecount(db,wg_rest(db,ptr));
  else
    return 1;
}


void* wg_list_memberuri(void* db, void* list, void* el) {
  void* listel;
  char* elstr;

  if (list==NULL) return NULL;
  elstr=wg_atomstr1(db,el);
  for (; list!=NULL; list=wg_rest(db,list)) {
    listel=wg_first(db,list);
    if (!wg_isatom(db,listel)) continue;
    if (wg_atomtype(db,listel)!=WG_URITYPE) continue;
    if (!strcmp(elstr,wg_atomstr1(db,listel))) return list;
  }
  return NULL;
}  

void* wg_list_memberuri_in_sublist(void* db, void* list, void* el) {
  void *listel, *tmp;
  char* elstr;

  if (list==NULL) return NULL;
  elstr=wg_atomstr1(db,el);
  for (; list!=NULL; list=wg_rest(db,list)) {
    listel=wg_first(db,list);
    if (wg_isatom(db,listel)) {
      if (wg_atomtype(db,listel)!=WG_URITYPE) continue;
      if (!strcmp(elstr,wg_atomstr1(db,listel))) return list;
    } else if (wg_ispair(db,listel)) {
      tmp=wg_list_memberuri(db,listel,el);
      if (tmp) return list;
    }
  }
  return NULL;
}  


// ----------- assoc -------------------


void* wg_add_assoc(void* db, void* mpool, void* el1, void* el2, void* alist) {
  void* tmp;

  tmp=wg_mkpair(db,mpool,el1,el2);
  if (tmp==NULL) return NULL;
  return wg_mkpair(db,mpool,tmp,alist);
}

void* wg_get_assoc(void* db, void* el1, void* alist) {
  void *part, *tmp;

  if (!alist) return NULL;
  for(part=alist; wg_ispair(db,part); part=wg_rest(db,part)) {
    tmp=wg_first(db,part);    

    if (wg_ispair(db,tmp)) {
      if (el1==wg_first(db,tmp)) return tmp;
    }
  }
  return NULL;
}


// ----------- json struct -------------------


void* wg_get_keyval(void* db, void* key, void* keyval) {
  void *str;

  if (!keyval || !key) return NULL;
  str=wg_atomstr1(db,key);
  return wg_get_keystrval(db,str,keyval);
}

void* wg_get_keystrval(void* db, char* str, void* keyval) {
  void *part, *tmp, *el;

  if (!keyval || !str) return NULL; 
  for(part=wg_rest(db,keyval); wg_ispair(db,part); part=wg_rest(db,part)) {
    el=wg_first(db,part);
    tmp=wg_rest(db,part);
    if (!strcmp(wg_atomstr1(db,el),str)) {      
      if (wg_ispair(db,tmp)) {
        return wg_first(db,tmp);
      } else {
        return NULL;
      }
    }
    if (wg_ispair(db,tmp)) {
      part=tmp;
    } else {
      return NULL;
    }
  }
  return NULL;
}

void* wg_get_keystrplace(void* db, char* str, void* keyval) {
  void *part, *tmp, *el;

  if (!keyval || !str) return NULL; 
  for(part=wg_rest(db,keyval); wg_ispair(db,part); part=wg_rest(db,part)) {
    el=wg_first(db,part);
    tmp=wg_rest(db,part);
    if (wg_isatom(db,el) && wg_atomstr1(db,el) && !strcmp(wg_atomstr1(db,el),str)) {      
      if (wg_ispair(db,tmp)) {
        return part;
      } else {
        return NULL;
      }
    }
    if (wg_ispair(db,tmp)) {
      part=tmp;
    } else {
      return NULL;
    }
  }
  return NULL;
}


// ------------ atoms ------------------


int wg_isatom(void* db, void* ptr) {
  return (ptr!=NULL && ((((gint)ptr)&TYPEMASK)==ATOMBITS));

}

void* wg_mkatom(void* db, void* mpool, int type, char* str1, char* str2) {
  char* ptr;
  char* curptr;
  int size=2;
  
  if (str1!=NULL) size=size+strlen(str1);
  size++;
  if (str2!=NULL) size=size+strlen(str2);
  size++;
  ptr=(char*)(wg_alloc_mpool(db,mpool,size));
  if (ptr==NULL) {
    show_mpool_error(db,"cannot create an atom in mpool");
    return NULL;
  }
  ptr++; // shift one pos right to set address last byte 1
  curptr=ptr;
  *curptr=(char)type;
  curptr++;
  if (str1!=NULL) {
    while((*curptr++ = *str1++));
  } else {
    *curptr=(char)0;
    curptr++;
  }
  if (str2!=NULL) {    
    while((*curptr++ = *str2++));
  } else {
    *curptr=(char)0;
    curptr++;
  } 
  return ptr; 
}


void* wg_mkatom_len(void* db, void* mpool, int type, char* str1, char* str2, int len1, int len2) {
  char* ptr;
  char* curptr;
  int i,size=2;

  //printf("\nstr1 %s %s len %d %d\n",str1,str2,len1,len2);
  if (str1!=NULL) size=size+len1;
  size++;
  if (str2!=NULL) size=size+len2;
  size++;
  ptr=(char*)(wg_alloc_mpool(db,mpool,size));
  if (ptr==NULL) {
    show_mpool_error(db,"cannot create an atom in mpool");
    return NULL;
  }
  ptr++; // shift one pos right to set address last byte 1
  curptr=ptr;
  *curptr=(char)type;
  curptr++;
  if (str1!=NULL) {
    for(i=0;i<len1;i++) {
      if(!(*curptr++ = *str1++)) break;           
    }    
  } else {
    *curptr=(char)0;
    curptr++;
  }
  if (str2!=NULL) {
    for(i=0;i<len2;i++) {
      if(!(*curptr++ = *str2++)) break;    
    }     
  } else {
    *curptr=(char)0;
    curptr++;
  }
  return ptr;
}


void* wg_mkatom_int(void* db, void* mpool, gint num) {
  char* ptr;
  char* curptr;
  int size=2;
  char buf[40];
  int len1;
  char* str1;

  //printf("\nint %ld\n",num);
  len1=snprintf(buf,38,"%ld",num)+1;
  str1=buf;
  size=size+len1;
  size++;  
  size++;
  ptr=(char*)(wg_alloc_mpool(db,mpool,size));
  if (ptr==NULL) {
    show_mpool_error(db,"cannot create an atom in mpool");
    return NULL;
  }
  ptr++; // shift one pos right to set address last byte 1
  curptr=ptr;
  (*curptr)=(char)WG_INTTYPE;
  curptr++;
  while((*curptr++ = *str1++));
  (*curptr)=(char)0;
  curptr++;  
  return ptr;
}


void* wg_mkatom_double(void* db, void* mpool, double num) {
  char* ptr;
  char* curptr;
  int size=2;
  char buf[40];
  int len1;
  char* str1;

  //printf("\ndouble2c %lf\n",num);  
  len1=snprintf(buf,38,"%f",num)+1;
  str1=buf;
  size=size+len1;
  size++;  
  size++;
  ptr=(char*)(wg_alloc_mpool(db,mpool,size));
  if (ptr==NULL) {
    show_mpool_error(db,"cannot create an atom in mpool");
    return NULL;
  }
  ptr++; // shift one pos right to set address last byte 1
  curptr=ptr;
  (*curptr)=(char)WG_DOUBLETYPE;
  curptr++;
  while((*curptr++ = *str1++));
  (*curptr)=(char)0;
  curptr++;  
  return ptr;  
}


void* wg_mkrecatom(void* db, void* mpool, gint rec) {
  char* ptr;
  char* curptr;
  int type=WG_RECORDTYPE;
  int size=sizeof(gint)+1;

  ptr=(char*)(wg_alloc_mpool(db,mpool,size));
  if (ptr==NULL) {
    show_mpool_error(db,"cannot create an atom in mpool");
    return NULL;
  }
  ptr++; // shift one pos right to set address last byte 1
  curptr=ptr;
  *curptr=(char)type;
  curptr++;
  *((gint*)curptr)=rec;

  return ptr;
}

int wg_atomtype(void* db, void* ptr) {
  if (ptr==NULL) return 0;
  else return (gint)*((char*)ptr);
}


char* wg_atomstr1(void* db, void* ptr) {
  if (ptr==NULL) return NULL;
  if (*(((char*)ptr)+1)==(char)0) return NULL;
  else return ((char*)ptr)+1;
}

char* wg_atomstr2(void* db, void* ptr) {
  if (ptr==NULL) return NULL;
  //printf("\nwg_atomstr2 called with str1 %s and strlen((char*)((char*)ptr+1)) %d\n",
  //  ((char*)ptr+1),strlen((char*)((char*)ptr+1)));
  ptr=(char*)ptr+strlen((char*)((char*)ptr+1))+2;
  //printf("\nstr2 pos1 %s pos2 %s\n",(char*)ptr,((char*)ptr)+1);
  if (*((char*)ptr)==(char)0) return NULL;
  else return ((char*)ptr);
}

gint wg_atomrec(void* db, void* ptr) {
  if (ptr==NULL) return 0;
  return *((gint*)(((char*)ptr)+1));
}

// --  special logical connectives and constants --


int wg_islogtrue(void* db, void* ptr) {
  return wg_islog_conn(db,ptr,"$true");
}

int wg_islogfalse(void* db, void* ptr) {
  return wg_islog_conn(db,ptr,"$false");
}

int wg_islogneg(void* db, void* ptr) {
  return wg_islog_conn(db,ptr,"not");
}

int wg_islogand(void* db, void* ptr) {
  return wg_islog_conn(db,ptr,"and");
}

int wg_islogor(void* db, void* ptr) {
  return wg_islog_conn(db,ptr,"or");
}

int wg_islogimp(void* db, void* ptr) {
  return wg_islog_conn(db,ptr,"imp");
}

int wg_islogeqv(void* db, void* ptr) {
  return wg_islog_conn(db,ptr,"eqv");
}

int wg_islogall(void* db, void* ptr) {
  return wg_islog_conn(db,ptr,"all");
}

int wg_islogexists(void* db, void* ptr) {
  return wg_islog_conn(db,ptr,"exists");
}

int wg_islogat(void* db, void* ptr) {
  return wg_islog_conn(db,ptr,"@");
}

/*
int wg_islogterm(void* db, void* ptr) {
  void* fun;

  if (wg_isatom(db,ptr)) return 1;
  fun=wg_first(db,ptr);
  if (!wg_isatom(db,fun)) return 
  if ()
   || wg_atomtype(db,ptr)!=WG_ANONCONSTTYPE) return 0; 
  return 1;
}
*/

int wg_islogconnective(void* db, void* ptr) {
  char* str;

  if (!wg_isatom(db,ptr) || wg_atomtype(db,ptr)!=WG_ANONCONSTTYPE) return 0; 
  str=wg_atomstr1(db,ptr);
  if (!strcmp("not",str)) return 1;
  if (!strcmp("and",str)) return 1;
  if (!strcmp("or",str)) return 1;
  if (!strcmp("imp",str)) return 1;
  if (!strcmp("eqv",str)) return 1;
  if (!strcmp("all",str)) return 1;
  if (!strcmp("exists",str)) return 1;
  //if (!strcmp("@",str)) return 1;
  return 0;  
}

int wg_islog_conn(void* db, void* ptr, char* str) {
  if (!ptr || !wg_isatom(db,ptr) || wg_atomtype(db,ptr)!=WG_ANONCONSTTYPE) return 0;
  if (strcmp(wg_atomstr1(db,ptr),str)) return 0;
  return 1;
}

int wg_islog_uri(void* db, void* ptr, char* str) {
  if (!ptr || !wg_isatom(db,ptr) || wg_atomtype(db,ptr)!=WG_URITYPE) return 0;
  if (strcmp(wg_atomstr1(db,ptr),str)) return 0;
  if (wg_atomstr2(db,ptr)!=NULL) return 0;
  return 1;
}


void* wg_makelogtrue(void* db, void* mpool) {
  return wg_makelog_conn(db,mpool,"$true");
}

void* wg_makelogfalse(void* db, void* mpool) {
  return wg_makelog_conn(db,mpool,"$false");
}

void* wg_makelogneg(void* db, void* mpool) {
  return wg_makelog_conn(db,mpool,"not");
}

void* wg_makelogand(void* db, void* mpool) {
  return wg_makelog_conn(db,mpool,"and");
}

void* wg_makelogor(void* db, void* mpool) {
  return wg_makelog_conn(db,mpool,"or");
}

void* wg_makelogimp(void* db, void* mpool) {
  return wg_makelog_conn(db,mpool,"imp");
}

void* wg_makelogeqv(void* db, void* mpool) {
  return wg_makelog_conn(db,mpool,"eqv");
}

void* wg_makelogall(void* db, void* mpool) {
  return wg_makelog_conn(db,mpool,"all");
}

void* wg_makelogexists(void* db, void* mpool) {
  return wg_makelog_conn(db,mpool,"exists");
}

void* wg_makelogat(void* db, void* mpool) {
  return wg_makelog_conn(db,mpool,"@");
}

void* wg_makeatomeq(void* db, void* mpool) {
  return wg_makelog_conn(db,mpool,"=");
}

void* wg_makelog_conn(void* db, void* mpool, char* str) {
  void* res;
  
  res=wg_mkatom(db,mpool,WG_ANONCONSTTYPE,str,NULL);
  return res;
}

// ---------- copying ---------

void* wg_mpool_copy(void* db, void* mpool, void* ptr) {
  if (!wg_ispair(db,ptr)) return ptr;
  return wg_mkpair(db,mpool,
                   wg_mpool_copy(db,mpool,wg_first(db,ptr)),
                   wg_mpool_copy(db,mpool,wg_rest(db,ptr)) );   
}  



// ------------ printing ------------------

void wg_mpool_print(void* db, void* ptr) {
  wg_mpool_print_aux(db,ptr,0,1);
}

static void wg_mpool_print_aux(void* db, void* ptr, int depth, int pflag) {
  int type;
  char* p;
  int count;
  int ppflag=1;
  int i;
  void *curptr;

  //printf("\nwg_mpool_print_aux called ptr %ld depth %d pflag %d\n",(gint)ptr,depth,pflag);
  if (ptr==NULL) {
    printf("()");
  } else if (wg_isatom(db,ptr)) {
    type=wg_atomtype(db,ptr);
    switch (type) {
      case 0: printf("_:"); break;
      case WG_NULLTYPE: printf("n:"); break;
      case WG_RECORDTYPE: 
        printf("r:"); 
        wg_print_record(db, ((gint*)(offsettoptr((db),(wg_atomrec(db,ptr))))));
        printf(" ");
        break;
      case WG_INTTYPE: printf("i:"); break;
      case WG_DOUBLETYPE: printf("d:"); break;
      case WG_STRTYPE: printf("s:"); break;
      case WG_XMLLITERALTYPE: printf("x:"); break;
      case WG_URITYPE: printf("u:"); break;
      case WG_BLOBTYPE: printf("b:"); break;
      case WG_CHARTYPE: printf("c:"); break;
      case WG_FIXPOINTTYPE: printf("f:"); break;
      case WG_DATETYPE: printf("date:"); break;
      case WG_TIMETYPE: printf("time:"); break;
      case WG_ANONCONSTTYPE: printf("a:"); break;
      case WG_VARTYPE: printf("?:"); break;
      default: printf("!:");
    }    
    p=wg_atomstr1(db,ptr);
    /*
    if (type==WG_ANONCONSTTYPE || type==WG_URITYPE) {
      for(i=0; *p!='\0'; i++, p++) {
        printf(" i%d%c ",i,*p);
      }
      printf(" t1:%d at %d",(int)*p,(int)p);
      p++;
      printf(" t2:%d at %d",(int)*p,(int)p);
    }  
    p=wg_atomstr2(db,ptr);
    printf(" compp:%d at %d",100,(int)p);
    p=wg_atomstr1(db,ptr); 
    */
    if (p!=NULL) {
      if (strchr(p,' ')!=NULL || strchr(p,'\n')!=NULL || strchr(p,'\t')!=NULL) {
        printf("\"%s\"",p);
      } else {
        printf("%s",p);
      }
    } else {
      printf("\"\"");
    }
    p=wg_atomstr2(db,ptr);
    if (p!=NULL) {
      if (strchr(p,' ')!=NULL || strchr(p,'\n')!=NULL || strchr(p,'\t')!=NULL) {
        printf("^^\"%s\"",p);
      } else {
        printf("^^%s",p);
      }
    }
  } else {
    //if (pflag && wg_listtreecount(db,ptr)>10) ppflag=1;
    printf ("(");
    for(curptr=ptr, count=0;curptr!=NULL && !wg_isatom(db,curptr);curptr=wg_rest(db,curptr), count++) {
      if (count>0) {
        if (ppflag) {
          printf("\n");
          for(i=0;i<depth;i++) printf(" ");
        }
        printf(" ");
      }
      // check pointer
      if (wg_mpool_bad_ptr(db,curptr)) {      
        printf("*first_wrongptr*");
        break;
      }
      // end check pointer
      wg_mpool_print_aux(db,wg_first(db,curptr),depth+1,0);
    }
    if (wg_isatom(db,curptr)) {
      printf(" . ");
      wg_mpool_print_aux(db,curptr,depth+1,ppflag);
    }
    printf (")");
    if (ppflag) printf("\n");
  }
}



// ------------- ints ---------------------



// ------------- floats --------------------





/* ============== error handling ==================== */

int wg_mpool_bad_ptr(void* db, void* ptr) {
  gint tmp;

  tmp=(gint)((ptrdiff_t)((char*)ptr-(char*)db));
  if (tmp<0) tmp=0-tmp;
  //printf("\ntmp %ld\n",tmp);
  if (tmp>(dbmemsegh(db)->size)) {
    return 1;
  }
  return 0;
}


/** called with err msg when an mpool allocation error occurs
*
*  may print or log an error
*  does not do any jumps etc
*/

static int show_mpool_error(void* db, char* errmsg) {
  //printf("db memory pool allocation error: %s\n",errmsg);
#ifdef WG_NO_ERRPRINT
#else
  fprintf(stderr,"db memory pool allocation error: %s\n",errmsg);
#endif
#ifdef EXIT_ON_ERROR
  exit(1);
#endif
  dbmemsegh(db)->errflag=DB_MPOOL_ERROR;
  return -1;
}

/** called with err msg and err nr when an mpool allocation error occurs
*
*  may print or log an error
*  does not do any jumps etc
*/

static int show_mpool_error_nr(void* db, char* errmsg, int nr) {
#ifdef WG_NO_ERRPRINT
#else
  fprintf(stderr,"db memory pool allocation error: %s %d\n",errmsg,nr);
#endif
#ifdef EXIT_ON_ERROR
  exit(1);
#endif
  dbmemsegh(db)->errflag=DB_MPOOL_ERROR2;
  return -1;
}

#ifdef __cplusplus
}
#endif
