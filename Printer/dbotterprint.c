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
#include "../Reasoner/clmeta.h"

#include "dbotterprint.h"

               
/* ====== Private headers and defs ======== */

//#define DEBUG
#undef DEBUG

//#define PRINT_TERMMETA

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
  /* 
  int tmp, len=80;
  char *buf;
  buf=malloc(len);  
  tmp=wr_strprint_clause_otter(g,rec,(g->print_clause_detaillevel),&buf,&len,0);
  if (tmp<0 || buf==NULL) {
    printf("printing error for clause \n");
    wr_print_clause_otter(g,rec,(g->print_clause_detaillevel));
  } else {
    printf("%s %d %d",buf,tmp,len);
  }
  if (buf!=NULL) free(buf);
  */

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
  if (rec==NULL) { printf("false"); return; }
  if (wg_rec_is_rule_clause(db,rec)) {
      //printf("ruleclause\n");
      wr_print_rule_clause_otter(g, (gint *) rec,printlevel);
      //printf("\n"); 
  } else if (wg_rec_is_fact_clause(db,rec)) {
      //printf("factclause\n");
      wr_print_fact_clause_otter(g, (gint *) rec,printlevel);
      //printf("\n"); 
  } else if (wg_rec_is_prop_clause(db,rec)) {
      //printf("propclause\n");
      wr_print_prop_clause_otter(g, (gint *) rec,printlevel);
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

  if (rec==NULL) {printf("NULL\n"); return;}  
  len = wg_count_clause_atoms(db, rec);
  for(i=0; i<len; i++) {
    if (i>0 && i<len) printf(" | ");
    meta=wg_get_rule_clause_atom_meta(db,rec,i);
    enc=wg_get_rule_clause_atom(db,rec,i);
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

/** Print single prop record
 *
 */

void wr_print_prop_clause_otter(glb* g, gint* rec,int printlevel) {
  void* db=g->db;
  gint var;
  //gint meta;
  int i, len;

  UNUSEDVAR(db);
  if (rec==NULL) {printf("NULL\n"); return;}  
  len = wg_count_prop_clause_atoms(db, rec);
  for(i=0; i<len; i++) {
    if (i>0 && i<len) printf(" ");
    //meta=wg_get_rule_clause_atom_meta(db,rec,i);
    var=wg_get_prop_clause_atom(db,rec,i);
    printf("%ld",var); 
  }
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
  recptr=wg_decode_record(db, rec);
  len = wg_get_record_len(db, recptr);
  for(i=0; i<len; i++) {
#ifdef PRINT_TERMMETA    
    if ((i==(g->unify_firstuseterm)-1) && (wg_get_field(db,recptr,i)!=0) &&
        !isdatarec(wg_get_field(db,recptr,i)) ) {
      printf("%d.%d",
               (int)((wg_decode_int(db,wg_get_field(db, recptr, i)) & TERMMETA_GROUND_MASK) ? 1 : 0),
               (int)(wg_decode_int(db,wg_get_field(db, recptr, i)) & TERMMETA_SIZE_MASK)
      );
    }
#endif    
    if (i<(g->unify_firstuseterm)) continue;
    if(i>((g->unify_firstuseterm)+1)) printf(",");
    enc = wg_get_field(db, recptr, i);
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
  recptr=wg_decode_record(db, rec);
  len = wg_get_record_len(db, recptr);
  for(i=0; i<len; i++) {
#ifdef PRINT_TERMMETA    
    if ((i==(g->unify_firstuseterm)-1) && (wg_get_field(db,recptr,i)!=0) &&
        !isdatarec(wg_get_field(db,recptr,i))) {
      printf("%d.%d",
               (int)((wg_decode_int(db,wg_get_field(db, recptr, i)) & TERMMETA_GROUND_MASK) ? 1 : 0),
               (int)(wg_decode_int(db,wg_get_field(db, recptr, i)) & TERMMETA_SIZE_MASK)
      );
    }
#endif
    if (i<(g->unify_firstuseterm)) continue;
    if(i>((g->unify_firstuseterm)+1)) printf(",");
    enc = wg_get_field(db, recptr, i);    
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
      if (exdata==NULL) {
        printf("%s", strdata);        
      } else
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

  else printf("?%d", (int)i);

}


/* ============= print clause to a string ============ */


int wr_strprint_clause(glb* g, gptr rec, char** buf, int *len, int pos) {
  //gint history;

  if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;  
  if (rec==NULL) {
    if (g->print_clauses_json) {
      pos+=snprintf((*buf)+pos,(*len)-pos,"[]");
    } else if (g->print_json) {
      pos+=snprintf((*buf)+pos,(*len)-pos,"\"false\"");
    } else {
      pos+=snprintf((*buf)+pos,(*len)-pos,"false");
    }
    return pos;
  }  
  //printf("len %d\n",wg_get_record_len(g->db, rec)); 
  /* 
  if (g->print_clause_history && g->print_flag) {
    history=wr_get_history(g,rec);
    wr_print_clause_name_history(g,history);
  }
  */
  /* 
  int tmp, len=80;
  char *buf;
  buf=malloc(len);  
  tmp=wr_strprint_clause_otter(g,rec,(g->print_clause_detaillevel),&buf,&len,0);
  if (tmp<0 || buf==NULL) {
    printf("printing error for clause \n");
    wr_print_clause_otter(g,rec,(g->print_clause_detaillevel));
  } else {
    printf("%s %d %d",buf,tmp,len);
  }
  if (buf!=NULL) free(buf);
  */
  if ((g->print_json) && !(g->print_clauses_json)) {
    pos+=snprintf((*buf)+pos,(*len)-pos,"\"");
    pos=wr_strprint_clause_otter(g,rec,(g->print_clause_detaillevel), buf, len, pos);
    pos+=snprintf((*buf)+pos,(*len)-pos,"\"");
  } else {
    pos=wr_strprint_clause_otter(g,rec,(g->print_clause_detaillevel), buf, len, pos);
  }  

  //printf(" ");
  //wg_print_record(g->db,rec);
  return pos;
} 

/** Print single clause (rule/fact record)
 * 
 * return next-to-write position in buf or -1 if error
 */

int wr_strprint_clause_otter(glb* g, gptr rec, int printlevel, char** buf, int *len, int pos) {
  if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;  
  if (rec==NULL) { pos+=sprintf((*buf)+pos,"NULL"); return pos; }
  if (wg_rec_is_rule_clause(db,rec)) {
    pos=wr_strprint_rule_clause_otter(g,(gptr)rec,printlevel,buf,len,pos);
  } else if (wg_rec_is_fact_clause(db,rec)) {
    pos=wr_strprint_fact_clause_otter(g,(gptr)rec,printlevel,buf,len,pos);
  } else if (wg_rec_is_prop_clause(db,rec)) {
    pos=wr_strprint_prop_clause_otter(g,(gptr)rec,printlevel,buf,len,pos);
  }    
  return pos;
}

/** Print single rule record
 *
 * return next-to-write position in buf or -1 if error
 * 
 */

int wr_strprint_rule_clause_otter(glb* g, gint* rec,int printlevel, char** buf, int *len, int pos) {
  void* db=g->db;
  gint meta, enc;
  int i, clen, isneg;
  
  if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;
  if (rec==NULL) {    
    if (g->print_clauses_json) {    
      pos+=snprintf((*buf)+pos,(*len)-pos,"[]"); 
      return pos;       
    } else {    
      pos+=snprintf((*buf)+pos,(*len)-pos,"NULL"); 
      return pos;
    }
  }
  clen = wg_count_clause_atoms(db, rec);
  if (g->print_clauses_json) {
    pos+=snprintf((*buf)+pos,(*len)-pos,"[");
  } 
  for(i=0; i<clen; i++) {
    if (i>0 && i<clen) {
      if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;
      if (g->print_clauses_json) {
        pos+=snprintf((*buf)+pos,(*len)-pos,", ");          
      } else {
        pos+=snprintf((*buf)+pos,(*len)-pos," | ");
      }       
    }  
    meta=wg_get_rule_clause_atom_meta(db,rec,i);
    enc=wg_get_rule_clause_atom(db,rec,i);
    if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;
    isneg=0;
    if (g->print_clauses_json) {
      if (wg_atom_meta_is_neg(db,meta)) isneg=1;  
    } else {
      if (wg_atom_meta_is_neg(db,meta)) pos+=snprintf((*buf)+pos,(*len)-pos,"-");
    }    
    if (wg_get_encoded_type(db, enc)==WG_RECORDTYPE) {   
      pos=wr_strprint_atom_otter(g,enc,printlevel,buf,len,pos,isneg);
      if (pos<0) return pos;
    } else {  
      pos=wr_strprint_simpleterm_otter(g,enc,printlevel,buf,len,pos,0);
      if (pos<0) return pos;
    }      
  }
  if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;
  if (g->print_clauses_json) {
    pos+=snprintf((*buf)+pos,(*len)-pos,"]");
  } else {
    pos+=snprintf((*buf)+pos,(*len)-pos,".");
  } 
  return pos;
}

/** Print single fact record
 *
 */

int wr_strprint_fact_clause_otter(glb* g, gint* rec, int printlevel, char** buf, int *len, int pos) {
  void* db=g->db;
    
  
  if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;
  if (rec==NULL) {    
    if (g->print_clauses_json) {    
      pos+=snprintf((*buf)+pos,(*len)-pos,"[]"); 
      return pos;       
    } else {    
      pos+=snprintf((*buf)+pos,(*len)-pos,"NULL"); 
      return pos;
    }
  }
  if (g->print_clauses_json) {
    pos+=snprintf((*buf)+pos,(*len)-pos,"[");
    pos=wr_strprint_atom_otter(g,wg_encode_record(db,rec),printlevel,buf,len,pos,0);
    if (pos<0) return pos;
    if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;
    pos+=snprintf((*buf)+pos,(*len)-pos,"]");
  } else {
    pos=wr_strprint_atom_otter(g,wg_encode_record(db,rec),printlevel,buf,len,pos,0);
    if (pos<0) return pos;
    if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;
    pos+=sprintf((*buf)+pos,".");
  }      
  return pos;
}

int wr_strprint_prop_clause_otter(glb* g, gint* rec,int printlevel, char** buf, int *len, int pos) {
  void* db=g->db;
  gint var;
  //gint meta;
  int i, clen;

  UNUSEDVAR(db);
  if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;
  if (rec==NULL) {    
    if (g->print_clauses_json) {    
      pos+=snprintf((*buf)+pos,(*len)-pos,"[]"); 
      return pos;       
    } else {    
      pos+=snprintf((*buf)+pos,(*len)-pos,"NULL"); 
      return pos;
    }
  } 
  clen = wg_count_prop_clause_atoms(db, rec);
  for(i=0; i<clen; i++) {
    if (!wr_str_guarantee_space(g,buf,len,pos+20)) return -1;
    if (i>0 && i<clen) {      
      if (g->print_clauses_json) {    
        pos+=snprintf((*buf)+pos,(*len)-pos,",");              
      } else {    
        pos+=snprintf((*buf)+pos,(*len)-pos," ");       
      }
    }  
    //meta=wg_get_rule_clause_atom_meta(db,rec,i);
    var=wg_get_prop_clause_atom(db,rec,i);    
    if (g->print_clauses_json) {    
      pos+=snprintf((*buf)+pos,(*len)-pos,"%ld",var);              
    } else {    
      pos+=snprintf((*buf)+pos,(*len)-pos,"%ld",var);       
    }
  }
  if (g->print_clauses_json) {    
    pos+=snprintf((*buf)+pos,(*len)-pos,"]");            
  } else {    
    pos+=snprintf((*buf)+pos,(*len)-pos,".");    
  }
  return pos;
}


int wr_strprint_atom_otter(glb* g, gint rec, int printlevel,char** buf, int *len, int pos, int isneg) {
  void* db=g->db;
  gptr recptr;
  gint clen, enc;
  int i;
  
  if (wg_get_encoded_type(db,rec)!=WG_RECORDTYPE) {
    if (isneg) {
      return wr_strprint_simpleterm_otter(g,rec,printlevel,buf,len,pos,1); 
    } else {
      return wr_strprint_simpleterm_otter(g,rec,printlevel,buf,len,pos,0); 
    }       
  }
  recptr=wg_decode_record(db, rec);
  clen = wg_get_record_len(db, recptr);
  if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;
  if (g->print_clauses_json) { 
    pos+=snprintf((*buf)+pos,(*len)-pos,"[");
  }  
  for(i=0; i<clen; i++) {  
    if (i<(g->unify_firstuseterm)) continue;
    if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;
    if(i>((g->unify_firstuseterm)+1)) pos+=snprintf((*buf)+pos,(*len)-pos,",");
    enc = wg_get_field(db, recptr, i);
    if (wg_get_encoded_type(db, enc)==WG_RECORDTYPE) {
      pos=wr_strprint_term_otter(g,enc,printlevel,buf,len,pos);
      if (pos<0) return pos;
    } else {  
      if (isneg && (i<=((g->unify_firstuseterm)))) {
        pos=wr_strprint_simpleterm_otter(g, enc,printlevel,buf,len,pos,1);
      } else {
        pos=wr_strprint_simpleterm_otter(g, enc,printlevel,buf,len,pos,0);
      }      
      if (pos<0) return pos;
    }       
    if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;
    if (g->print_clauses_json) {
      if (i==(g->unify_firstuseterm))  pos+=snprintf((*buf)+pos,(*len)-pos,",");
    } else {
      if (i==(g->unify_firstuseterm))  pos+=snprintf((*buf)+pos,(*len)-pos,"(");
    }     
  }
  if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;
  if (g->print_clauses_json) { 
    pos+=snprintf((*buf)+pos,(*len)-pos,"]");
  } else {
    pos+=snprintf((*buf)+pos,(*len)-pos,")");
  }  
  return pos;
}


int wr_strprint_term_otter(glb* g, gint rec,int printlevel, char** buf, int *len, int pos) {
  void* db=g->db;
  gptr recptr;
  gint  enc;
  int i, clen;

#ifdef DEBUG  
  printf("print_term called with enc %d and type %d \n",(int)rec,wg_get_encoded_type(db,rec)); 
#endif   
  if (wg_get_encoded_type(db,rec)!=WG_RECORDTYPE) {
    pos=wr_strprint_simpleterm_otter(g,rec,printlevel,buf,len,pos,0);
    return pos;
  }
  recptr=wg_decode_record(db, rec);
  clen = wg_get_record_len(db, recptr);
  if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;
  if (g->print_clauses_json) { 
    pos+=snprintf((*buf)+pos,(*len)-pos,"[");
  }  
  for(i=0; i<clen; i++) {
    if (i<(g->unify_firstuseterm)) continue;
    if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;
    if(i>((g->unify_firstuseterm)+1)) pos+=snprintf((*buf)+pos,(*len)-pos,",");
    enc = wg_get_field(db, recptr, i);    
    if (wg_get_encoded_type(db, enc)==WG_RECORDTYPE) {
      pos=wr_strprint_term_otter(g,enc,printlevel,buf,len,pos);
      if (pos<0) return pos;
    } else {  
      pos=wr_strprint_simpleterm_otter(g, enc,printlevel,buf,len,pos,0);
      if (pos<0) return pos;
    }           
    if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;
    if (g->print_clauses_json) {
      if (i==(g->unify_firstuseterm))  pos+=snprintf((*buf)+pos,(*len)-pos,",");
    } else {
      if (i==(g->unify_firstuseterm))  pos+=snprintf((*buf)+pos,(*len)-pos,"(");
    }  
  } 
  if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;
  if (g->print_clauses_json) { 
    pos+=snprintf((*buf)+pos,(*len)-pos,"]");
  } else {
    pos+=snprintf((*buf)+pos,(*len)-pos,")");
  }  
  return pos;
}


/** Print a single, encoded value or a subrecord
 *  
 */
int wr_strprint_simpleterm_otter(glb* g, gint enc,int printlevel, char** buf, int *len, int pos, int isneg) {
  void* db=g->db;
  
  int intdata;
  char *strdata, *exdata;
  double doubledata;
  char strbuf[80];
  int len1=0,len2=0;

#ifdef DEBUG  
  printf("simpleterm called with enc %d and type %d \n",(int)enc,wg_get_encoded_type(db,enc)); 
#endif  
  strbuf[0]=(char)0;
  if (!wr_str_guarantee_space(g,buf,len,pos+50)) return -1;
  switch(wg_get_encoded_type(db, enc)) {
    case WG_NULLTYPE:
      return pos+snprintf((*buf)+pos,(*len)-pos,"NULL");      
    //case WG_RECORDTYPE:
    //  ptrdata = (gint) wg_decode_record(db, enc);
    //  wg_print_subrecord_otter(db,(gint*)ptrdata);
    //  break;    
    case WG_INTTYPE:      
      intdata = wg_decode_int(db, enc);     
      return pos+snprintf((*buf)+pos,(*len)-pos,"%d", intdata);
    case WG_DOUBLETYPE:
      doubledata = wg_decode_double(db, enc);
      return pos+snprintf((*buf)+pos,(*len)-pos,"%f", doubledata);
    case WG_STRTYPE:
      strdata = wg_decode_str(db, enc);
      if (g->print_clauses_json) {
        if (isneg) {
          return pos+snprintf((*buf)+pos,(*len)-pos,"\"-:%s\"", strdata);
        } else {
          return pos+snprintf((*buf)+pos,(*len)-pos,"\":%s\"", strdata);
        }  
      } else {
        return pos+snprintf((*buf)+pos,(*len)-pos,":%s", strdata);
      }      
    case WG_URITYPE:
      strdata = wg_decode_uri(db, enc);
      exdata = wg_decode_uri_prefix(db, enc);
      if (strdata) len1=strlen(strdata);
      if (exdata) len2=strlen(exdata);
      if (!wr_str_guarantee_space(g,buf,len,pos+len1+len2+50)) return -1;
      if (exdata==NULL) {
        if (g->print_clauses_json) {
          if (isneg) {
            return pos+snprintf((*buf)+pos,(*len)-pos,"\"-%s\"", strdata);
          } else {
            return pos+snprintf((*buf)+pos,(*len)-pos,"\"%s\"", strdata);
          }          
        } else {          
          if (isneg) {
            return pos+snprintf((*buf)+pos,(*len)-pos,"-%s", strdata);
          } else {
            return pos+snprintf((*buf)+pos,(*len)-pos,"%s", strdata);
            //return pos+snprintf((*buf)+pos,(*len)-pos,"%s%ld", strdata,wg_decode_uri_id(db,enc));
          }           
        }        
      } else {
        if (g->print_clauses_json) {
          if (isneg) {
            return pos+snprintf((*buf)+pos,(*len)-pos,"\"-%s:%s\"", exdata, strdata);
          } else {
            return pos+snprintf((*buf)+pos,(*len)-pos,"\"%s:%s\"", exdata, strdata);
          }          
        } else {
          if (isneg) {
            return pos+snprintf((*buf)+pos,(*len)-pos,"-%s:%s", exdata, strdata);
          } else {
            return pos+snprintf((*buf)+pos,(*len)-pos,"%s:%s", exdata, strdata);
          }           
        }               
      }  
    case WG_XMLLITERALTYPE:
      strdata = wg_decode_xmlliteral(db, enc);
      exdata = wg_decode_xmlliteral_xsdtype(db, enc);
      if (strdata) len1=strlen(strdata);
      if (exdata) len2=strlen(exdata);
      if (!wr_str_guarantee_space(g,buf,len,pos+len1+len2+50)) return -1;
      if (g->print_clauses_json) {
        if (isneg) {
          return pos+snprintf((*buf)+pos,(*len)-pos,"\"-x:%s:%s\"", exdata, strdata);
        } else {
          return pos+snprintf((*buf)+pos,(*len)-pos,"\"x:%s:%s\"", exdata, strdata);
        }        
      } else {
        if (isneg) {
          return pos+snprintf((*buf)+pos,(*len)-pos,"-x:%s:%s", exdata, strdata);
        } else {
          return pos+snprintf((*buf)+pos,(*len)-pos,"x:%s:%s", exdata, strdata);
        }        
      }          
    case WG_CHARTYPE:
      intdata = wg_decode_char(db, enc);    
      if (g->print_clauses_json) {
        if (isneg) {
          return pos+snprintf((*buf)+pos,(*len)-pos,"\"-%c\"", (char) intdata);
        } else {
          return pos+snprintf((*buf)+pos,(*len)-pos,"\"%c\"", (char) intdata);
        }        
      } else {
        if (isneg) {
          return pos+snprintf((*buf)+pos,(*len)-pos,"-%c", (char) intdata);
        } else {
          return pos+snprintf((*buf)+pos,(*len)-pos,"%c", (char) intdata);
        }        
      }       
    case WG_DATETYPE:
      intdata = wg_decode_date(db, enc);
      wg_strf_iso_datetime(db,intdata,0,strbuf);
      strbuf[10]=0;
      if (!wr_str_guarantee_space(g,buf,len,pos+50)) return -1;
      if (g->print_clauses_json) {
        if (isneg) {
          return pos+snprintf((*buf)+pos,(*len)-pos,"\"-d:%s\"",strbuf);
        } else {
          return pos+snprintf((*buf)+pos,(*len)-pos,"\"d:%s\"",strbuf);
        }        
      } else {
        if (isneg) {
          return pos+snprintf((*buf)+pos,(*len)-pos,"-d:%s",strbuf);
        } else {
          return pos+snprintf((*buf)+pos,(*len)-pos,"d:%s",strbuf);
        }         
      }      
    case WG_TIMETYPE:
      intdata = wg_decode_time(db, enc);
      wg_strf_iso_datetime(db,1,intdata,strbuf);  
      if (g->print_clauses_json) {
        if (isneg) {
          return pos+snprintf((*buf)+pos,(*len)-pos,"\"-t:%s\"",strbuf);
        } else {
          return pos+snprintf((*buf)+pos,(*len)-pos,"\"t:%s\"",strbuf);
        }        
      } else {
        if (isneg) {
          return pos+snprintf((*buf)+pos,(*len)-pos,"-t:%s",strbuf);
        } else {
          return pos+snprintf((*buf)+pos,(*len)-pos,"t:%s",strbuf);
        }        
      }             
    case WG_VARTYPE:
      intdata = wg_decode_var(db, enc);
      return wg_nice_strprint_var(g,intdata,buf,len,pos);
    case WG_ANONCONSTTYPE:
      strdata = wg_decode_anonconst(db, enc);
      if (strdata) len1=strlen(strdata);
      if (!wr_str_guarantee_space(g,buf,len,pos+len1+50)) return -1;      
      if (g->print_clauses_json) {
        if (isneg) {
          return pos+snprintf((*buf)+pos,(*len)-pos,"\"-%s\"",strbuf);
        } else {
          return pos+snprintf((*buf)+pos,(*len)-pos,"\"%s\"",strbuf);
        }        
      } else {
        if (isneg) {
          return pos+snprintf((*buf)+pos,(*len)-pos,"-!%s",strbuf);
        } else {
          return pos+snprintf((*buf)+pos,(*len)-pos,"!%s",strbuf);
        }       
      } 
    default:
      if (g->print_clauses_json) {
        return pos+snprintf((*buf)+pos,(*len)-pos,"\"$unknown_type\"");
      } else {
        return pos+snprintf((*buf)+pos,(*len)-pos,"<unsupported type>");
      }
      
  }
}

int wg_nice_strprint_var(glb* g, gint i, char** buf, int *len, int pos) {
  char strbuf[80];

  strbuf[0]=(char)0;
  if (!wr_str_guarantee_space(g,buf,len,pos+20)) return -1;

  if      (i==1000) strcpy(strbuf,"X1");
  else if (i==1001) strcpy(strbuf,"Y1");
  else if (i==1002) strcpy(strbuf,"Z1");
  else if (i==1003) strcpy(strbuf,"U1");
  else if (i==1004) strcpy(strbuf,"V1");
  else if (i==1005) strcpy(strbuf,"W1");

  else if (i==2000) strcpy(strbuf,"X");
  else if (i==2001) strcpy(strbuf,"Y");
  else if (i==2002) strcpy(strbuf,"Z");
  else if (i==2003) strcpy(strbuf,"U");
  else if (i==2004) strcpy(strbuf,"V");
  else if (i==2005) strcpy(strbuf,"W");

  else if (i==3000) strcpy(strbuf,"X3");
  else if (i==3001) strcpy(strbuf,"Y3");
  else if (i==3002) strcpy(strbuf,"Z3");
  else if (i==3003) strcpy(strbuf,"U3");
  else if (i==3004) strcpy(strbuf,"V3");
  else if (i==3005) strcpy(strbuf,"W3");

  else {
    if (g->print_clauses_json) {
      snprintf(strbuf,70,"%d", (int)i);
    } else {
      snprintf(strbuf,70,"?%d", (int)i);
    }
  }  
  if (g->print_clauses_json) {
    return pos+snprintf((*buf)+pos,(*len)-pos,"\"?%s\"",strbuf);
  } else {
    return pos+snprintf((*buf)+pos,(*len)-pos,"%s",strbuf);
  }  
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
