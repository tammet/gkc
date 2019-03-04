/*
* $Id:  $
* $Version: $
*
* Copyright (c) Tanel Tammet 2004,2005,2006,2007,2008,2009,2010
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

 /** @file dbotterprint.c
 *  Top level procedures for otterprinter
 *
 */

/* ====== Includes =============== */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "../Db/dbdata.h"
#include "../Db/dbmem.h"
#include "../Db/dballoc.h"
#include "../Db/dbdata.h"
#include "../Db/dbmpool.h"
#include "../Db/dbutil.h"

#include "../Reasoner/history.h"
#include "../Reasoner/clterm.h"

#include "dbotterprint.h"

               
/* ====== Private headers and defs ======== */

//#define DEBUG
#undef DEBUG

/* ======== Data ========================= */

/* ====== Private protos ======== */

/*
static gint show_print_error(void* db, char* errmsg);
static gint show_print_error_nr(void* db, char* errmsg, gint nr);
static gint show_print_error_str(void* db, char* errmsg, char* str);
*/

/* ====== Functions ============== */



void wr_print_clause(glb* g, gptr rec) {
  gint history;

  if (rec==NULL) return;
  //printf("len %d\n",wg_get_record_len(g->db, rec));  
  if (g->print_clause_history && g->print_flag) {
    history=wr_get_history(g,rec);
    wr_print_clause_name_history(g,history);
  }
  wr_print_clause_otter(g,rec,(g->print_clause_detaillevel));
  printf(" ");
  //wg_print_record(g->db,rec);
}  


void wr_print_halfbuilt_clause(glb* g, gptr rptr, int rpos) {
  int i,tmp;
  gint xatom, xatommeta;

  for(i=0; i < rpos; i++) { 
    tmp=i*LIT_WIDTH;
    xatom=rptr[tmp+LIT_ATOM_POS];
    xatommeta=rptr[tmp+LIT_META_POS];  
    if (xatommeta==0) {
      printf(" cut ");
    } else {
      if (wg_atom_meta_is_neg(db,xatommeta)) printf("-");
      wr_print_atom_otter(g,xatom,(g->print_clause_detaillevel));
      if (i+1 < rpos) printf(" | ");
    }  
  }
}

void wr_print_term(glb* g, gint rec) {
  if (rec==(gint)NULL || rec==WG_ILLEGAL) return;
  wr_print_term_otter(g,rec,(g->print_clause_detaillevel));
}  

void wr_print_record(glb* g, gptr rec) {
  wg_print_record(g->db,rec);
} 

/** Print whole db
 *
 */
 
void wr_print_db_otter(glb* g,int printlevel) { 
  void* db=g->db;
  void *rec;
  gint history;
  
  rec = wg_get_first_raw_record(db);
  while(rec) {
    if (wg_rec_is_rule_clause(db,rec)) {
      if (g->print_clause_history && g->print_flag) {
        history=wr_get_history(g,rec);
        wr_print_clause_name_history(g,history);
      }
      wr_print_rule_clause_otter(g, (gint *) rec,printlevel);
      printf("\n"); 
    } else if (wg_rec_is_fact_clause(db,rec)) {
      if (g->print_clause_history && g->print_flag) {
        history=wr_get_history(g,rec);
        wr_print_clause_name_history(g,history);
      }
      wr_print_fact_clause_otter(g, (gint *) rec,printlevel);
      printf("\n"); 
    }            
    rec = wg_get_next_raw_record(db,rec);    
  }
}


/** Print single clause (rule/fact record)
 *
 */

void wr_print_clause_otter(glb* g, gint* rec, int printlevel) {
  //printf("wg_print_clause_otter called with rec ");
  //wg_print_record(db,rec);
  if (rec==NULL) { printf("NULL"); return; }
  if (wg_rec_is_rule_clause(db,rec)) {
      //printf("ruleclause\n");
      wr_print_rule_clause_otter(g, (gint *) rec,printlevel);
      //printf("\n"); 
  } else if (wg_rec_is_fact_clause(db,rec)) {
      //printf("factclause\n");
      wr_print_fact_clause_otter(g, (gint *) rec,printlevel);
      //printf("\n"); 
  }     
  //printf("wg_print_clause_otter exiting\n");
}

/** Print single rule record
 *
 */

void wr_print_rule_clause_otter(glb* g, gint* rec,int printlevel) {
  void* db=g->db;
  gint meta, enc;
  int i, len;
  //char strbuf[256];
#if 0
/* XXX: FIXME */
#ifdef USE_CHILD_DB
  gint parent;
#endif
#endif

  if (rec==NULL) {printf("NULL\n"); return;}  
#if 0
/* XXX: FIXME */
#ifdef USE_CHILD_DB
  parent = wg_get_rec_base_offset(db, rec);
#endif
#endif
  //len = wg_get_record_len(db, rec);
  len = wg_count_clause_atoms(db, rec);
  //printf("[%d ",len);
  for(i=0; i<len; i++) {
    //printf(" #%d-%d ",i,((i-CLAUSE_EXTRAHEADERLEN)%2));
    //if (i<CLAUSE_EXTRAHEADERLEN) continue;
    //if (((i-CLAUSE_EXTRAHEADERLEN)%2)==0) continue;
    //if(i>(CLAUSE_EXTRAHEADERLEN+2)) printf(" | ");
    //if (i>1 && i+1<len) printf(" | ");
    //enc = wg_get_field(db, rec, i);
    
    if (i>0 && i<len) printf(" | ");
    meta=wg_get_rule_clause_atom_meta(db,rec,i);
    enc=wg_get_rule_clause_atom(db,rec,i);
    //printf("[i %d meta %d enc %d]",i,meta,enc);
    //enc=wg_get_field(db,rec,i);
#if 0
/* XXX: FIXME */
#ifdef USE_CHILD_DB
    if(parent)
      enc = wg_encode_parent_data(parent, enc);
#endif
#endif
    if (wg_atom_meta_is_neg(db,meta)) printf("-");
    if (wg_get_encoded_type(db, enc)==WG_RECORDTYPE) {   
      wr_print_atom_otter(g,enc,printlevel);
    } else {  
      wr_print_simpleterm_otter(g, enc,printlevel);
    }      
  }
  printf(".");
}

/** Print single fact record
 *
 */

void wr_print_fact_clause_otter(glb* g, gint* rec,int printlevel) {
  void* db=g->db;
  
  if (rec==NULL) { printf("NULL\n"); return; }
  wr_print_atom_otter(g,wg_encode_record(db,rec),printlevel);
  printf(".");
}


void wr_print_atom_otter(glb* g, gint rec, int printlevel) {
  void* db=g->db;
  gptr recptr;
  gint len, enc;
  int i;
  
  if (wg_get_encoded_type(db,rec)!=WG_RECORDTYPE) {
    wr_print_simpleterm_otter(g,rec,printlevel);
    return;
  }
#if 0
/* XXX: FIXME */
#ifdef USE_CHILD_DB
  gint parent;
  parent = wg_get_rec_base_offset(db, rec);
#endif
#endif
  recptr=wg_decode_record(db, rec);
  len = wg_get_record_len(db, recptr);
  //printf("[");
  for(i=0; i<len; i++) {
    if (i<(g->unify_firstuseterm)) continue;
    if(i>((g->unify_firstuseterm)+1)) printf(",");
    enc = wg_get_field(db, recptr, i);
#if 0
/* XXX: FIXME */
#ifdef USE_CHILD_DB
    if(parent)
      enc = wg_encode_parent_data(parent, enc);
#endif
#endif
    if (wg_get_encoded_type(db, enc)==WG_RECORDTYPE) {
      wr_print_term_otter(g,enc,printlevel);
    } else {  
      wr_print_simpleterm_otter(g, enc,printlevel);
    }       
    if (i==(g->unify_firstuseterm)) printf("(");
  }
  printf(")");
}


void wr_print_term_otter(glb* g, gint rec,int printlevel) {
  void* db=g->db;
  gptr recptr;
  gint len, enc;
  int i;

#ifdef DEBUG  
  printf("print_term called with enc %d and type %d \n",(int)rec,wg_get_encoded_type(db,rec)); 
#endif   
  if (wg_get_encoded_type(db,rec)!=WG_RECORDTYPE) {
    wr_print_simpleterm_otter(g,rec,printlevel);
    return;
  }
#if 0
/* XXX: FIXME */
#ifdef USE_CHILD_DB
  gint parent;
  parent = wg_get_rec_base_offset(db, rec);
#endif
#endif
  recptr=wg_decode_record(db, rec);
  len = wg_get_record_len(db, recptr);
  for(i=0; i<len; i++) {
    if (i<(g->unify_firstuseterm)) continue;
    if(i>((g->unify_firstuseterm)+1)) printf(",");
    enc = wg_get_field(db, recptr, i);
#if 0
/* XXX: FIXME */
#ifdef USE_CHILD_DB
    if(parent)
      enc = wg_encode_parent_data(parent, enc);
#endif
#endif
    if (wg_get_encoded_type(db, enc)==WG_RECORDTYPE) {
      wr_print_term_otter(g,enc,printlevel);
    } else {  
      wr_print_simpleterm_otter(g, enc,printlevel);
    }       
    if (i==(g->unify_firstuseterm)) printf("(");
  }
  printf(")");
}


/** Print a single, encoded value or a subrecord
 *  
 */
void wr_print_simpleterm_otter(glb* g, gint enc,int printlevel) {
  void* db=g->db;
  
  int intdata;
  char *strdata, *exdata;
  double doubledata;
  char strbuf[80];

#ifdef DEBUG  
  printf("simpleterm called with enc %d and type %d \n",(int)enc,wg_get_encoded_type(db,enc)); 
#endif  
  switch(wg_get_encoded_type(db, enc)) {
    case WG_NULLTYPE:
      printf("NULL");
      break;
    //case WG_RECORDTYPE:
    //  ptrdata = (gint) wg_decode_record(db, enc);
    //  wg_print_subrecord_otter(db,(gint*)ptrdata);
    //  break;    
    case WG_INTTYPE:
      intdata = wg_decode_int(db, enc);
      printf("%d", intdata);
      break;
    case WG_DOUBLETYPE:
      doubledata = wg_decode_double(db, enc);
      printf("%f", doubledata);
      break;
    case WG_STRTYPE:
      strdata = wg_decode_str(db, enc);
      printf("\"%s\"", strdata);
      break;
    case WG_URITYPE:
      strdata = wg_decode_uri(db, enc);
      exdata = wg_decode_uri_prefix(db, enc);
      if (exdata==NULL)
        printf("%s", strdata);
      else
        printf("%s:%s", exdata, strdata);
      break;      
    case WG_XMLLITERALTYPE:
      strdata = wg_decode_xmlliteral(db, enc);
      exdata = wg_decode_xmlliteral_xsdtype(db, enc);
      printf("\"<xsdtype %s>%s\"", exdata, strdata);
      break;
    case WG_CHARTYPE:
      intdata = wg_decode_char(db, enc);
      printf("%c", (char) intdata);
      break;
    case WG_DATETYPE:
      intdata = wg_decode_date(db, enc);
      wg_strf_iso_datetime(db,intdata,0,strbuf);
      strbuf[10]=0;
      printf("<raw date %d>%s", intdata,strbuf);
      break;
    case WG_TIMETYPE:
      intdata = wg_decode_time(db, enc);
      wg_strf_iso_datetime(db,1,intdata,strbuf);        
      printf("<raw time %d>%s",intdata,strbuf+11);
      break;
    case WG_VARTYPE:
      intdata = wg_decode_var(db, enc);
      wg_nice_print_var(db,intdata);
      //printf("?%d", intdata);
      break;
    case WG_ANONCONSTTYPE:
      strdata = wg_decode_anonconst(db, enc);
      printf("!%s", strdata);
      break; 
    default:
      printf("<unsupported type>");
      break;
  }
}

void wg_nice_print_var(void* db, gint i) {
  if      (i==1000) printf("X1");
  else if (i==1001) printf("Y1");
  else if (i==1002) printf("Z1");
  else if (i==1003) printf("U1");
  else if (i==1004) printf("V1");
  else if (i==1005) printf("W1");

  else if (i==2000) printf("X");
  else if (i==2001) printf("Y");
  else if (i==2002) printf("Z");
  else if (i==2003) printf("U");
  else if (i==2004) printf("V");
  else if (i==2005) printf("W");

  else if (i==3000) printf("X3");
  else if (i==3001) printf("Y3");
  else if (i==3002) printf("Z3");
  else if (i==3003) printf("U3");
  else if (i==3004) printf("V3");
  else if (i==3005) printf("W3");

  else printf("?%d", i);

}



/* ------------ errors ---------------- */
/*

static gint show_print_error(void* db, char* errmsg) {
  printf("wg otterprint error: %s\n",errmsg);
  return -1;
}


static gint show_print_error_nr(void* db, char* errmsg, gint nr) {
  printf("wg parser error: %s %d\n", errmsg, (int) nr);
  return -1;
}

static gint show_print_error_str(void* db, char* errmsg, char* str) {
  printf("wg parser error: %s %s\n",errmsg,str);
  return -1;
}
*/
