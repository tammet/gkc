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

#include "../Reasoner/build.h"

               
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
  if (g->print_clause_history && g->print_flag) {
    history=wr_get_history(g,rec);
    wr_print_clause_name_history(g,history);
  }
  wr_print_clause_otter(g,rec,(g->print_clause_detaillevel));
  printf(" ");  
}  


void wr_print_halfbuilt_clause(glb* g, gptr rptr, int rpos) {
  int i,tmp;
  gint xatom, xatommeta;

  for(i=0; i < rpos; i++) { 
    tmp=i*LIT_WIDTH;
    xatom=rptr[tmp+LIT_ATOM_POS];
    xatommeta=rptr[tmp+LIT_META_POS];  
    if (xatommeta==0) {
      printf(" (cut element) ");
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
  printf("simpleterm called with enc %d and type %d \n",(int)enc,wg_get_encoded_type(db,enc)); 
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

void wr_print_clause_via_buf(glb* g, gptr rec) {
  int bpos=0;
  int blen=190;
  char* buf;
  
  buf=wr_malloc(g,200);
  if (!buf) return;
  bpos=wr_strprint_clause(g,rec,&buf,&blen,bpos);
  if (bpos>0) {
    printf("%s\n",buf);
  }
  wr_free(g,buf);
}
 


int wr_strprint_clause(glb* g, gptr rec, char** buf, int *len, int pos) {
  //gint history;

  if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;  
  if (rec==NULL) {
    if (g->print_clauses_json) {
      pos+=snprintf((*buf)+pos,(*len)-pos,"false");
    } else if (g->print_json) {
      pos+=snprintf((*buf)+pos,(*len)-pos,"\"false\"");
    } else if (g->print_clauses_tptp) {    
      pos+=snprintf((*buf)+pos,(*len)-pos,"($false)"); 
      return pos;  
    } else {
      pos+=snprintf((*buf)+pos,(*len)-pos,"false");
    }
    return pos;
  }  
  if ((g->print_json) && !(g->print_clauses_json)) {
    pos+=snprintf((*buf)+pos,(*len)-pos,"\"");
    pos=wr_strprint_clause_otter(g,rec,(g->print_clause_detaillevel), buf, len, pos);
    pos+=snprintf((*buf)+pos,(*len)-pos,"\"");
  } else {
    pos=wr_strprint_clause_otter(g,rec,(g->print_clause_detaillevel), buf, len, pos);
  }  
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
  gint meta, enc, encx, ency;
  int i, clen, isneg;
  gptr tptr;
  
  if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;
  if (rec==NULL) {    
    if (g->print_clauses_json) {    
      pos+=snprintf((*buf)+pos,(*len)-pos,"false"); 
      return pos;
    } else if (g->print_clauses_tptp) {    
      pos+=snprintf((*buf)+pos,(*len)-pos,"$false"); 
      return pos;                   
    } else {    
      pos+=snprintf((*buf)+pos,(*len)-pos,"false"); 
      return pos;
    }
  }
  clen = wg_count_clause_atoms(db, rec);
  if (g->print_clauses_json) {
    pos+=snprintf((*buf)+pos,(*len)-pos,"[");
  } else if (g->print_clauses_tptp) {
    pos+=snprintf((*buf)+pos,(*len)-pos,"(");
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
    } else if (g->print_clauses_tptp) {
      if (wr_equality_atom(g,enc)) {
        tptr=rotp(g,enc);      
        encx=tptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
        ency=tptr[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];
        //pos+=snprintf((*buf)+pos,(*len)-pos,"("); 
        pos=wr_strprint_term_otter(g,encx,printlevel,buf,len,pos);
        if (pos<0) return pos;
        if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
        if (wg_atom_meta_is_neg(db,meta)) {
          pos+=snprintf((*buf)+pos,(*len)-pos," != ");
        } else {
          pos+=snprintf((*buf)+pos,(*len)-pos," = ");
        }          
        pos=wr_strprint_term_otter(g,ency,printlevel,buf,len,pos);
        if (pos<0) return pos;       
        if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
        //pos+=snprintf((*buf)+pos,(*len)-pos,")");
        continue;
      } else {
        if (wg_atom_meta_is_neg(db,meta)) pos+=snprintf((*buf)+pos,(*len)-pos,"~");        
      }      
    } else {
      if (wg_atom_meta_is_neg(db,meta)) pos+=snprintf((*buf)+pos,(*len)-pos,"-");
    }    
    if (wg_get_encoded_type(db, enc)==WG_RECORDTYPE) {   
      if (g->print_clauses_json) {
        pos=wr_strprint_atom_otter(g,enc,printlevel,buf,len,pos,isneg);
      } else {  
        pos=wr_strprint_term_otter(g,enc,printlevel,buf,len,pos);
      }  
      if (pos<0) return pos;
    } else {  
      pos=wr_strprint_simpleterm_otter(g,enc,printlevel,buf,len,pos,0);
      if (pos<0) return pos;
    }      
  }
  if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;
  if (g->print_clauses_json) {
    pos+=snprintf((*buf)+pos,(*len)-pos,"]");
  } else if (g->print_clauses_tptp) {    
    pos+=snprintf((*buf)+pos,(*len)-pos,")"); 
    return pos;  
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
  gint encx, ency;
  gptr tptr;  
  
  if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;
  if (rec==NULL) {    
    if (g->print_clauses_json) {    
      pos+=snprintf((*buf)+pos,(*len)-pos,"false"); 
      return pos;       
    } else if (g->print_clauses_tptp) {    
      pos+=snprintf((*buf)+pos,(*len)-pos,"$false"); 
      return pos;    
    } else {    
      pos+=snprintf((*buf)+pos,(*len)-pos,"false"); 
      return pos;
    }
  }
  if (g->print_clauses_json) {
    pos+=snprintf((*buf)+pos,(*len)-pos,"[");
    pos=wr_strprint_atom_otter(g,wg_encode_record(db,rec),printlevel,buf,len,pos,0);
    if (pos<0) return pos;
    if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;
    pos+=snprintf((*buf)+pos,(*len)-pos,"]");
  } else if (g->print_clauses_tptp) {
    //pos+=sprintf((*buf)+pos,"(");
    if (wr_equality_atom(g,wg_encode_record(db,rec))) {
      tptr=rec;            
      encx=tptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
      ency=tptr[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];
      pos+=snprintf((*buf)+pos,(*len)-pos,"("); 
      pos=wr_strprint_term_otter(g,encx,printlevel,buf,len,pos);
      if (pos<0) return pos;
      if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
      if (0) { //(wg_atom_meta_is_neg(db,meta)) {
        pos+=snprintf((*buf)+pos,(*len)-pos," != ");
      } else {
        pos+=snprintf((*buf)+pos,(*len)-pos," = ");
      }          
      pos=wr_strprint_term_otter(g,ency,printlevel,buf,len,pos);
      if (pos<0) return pos;       
      if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
      pos+=snprintf((*buf)+pos,(*len)-pos,")");
      return pos;
    } else {      
      pos+=sprintf((*buf)+pos,"(");
      pos=wr_strprint_atom_otter(g,wg_encode_record(db,rec),printlevel,buf,len,pos,0);
      if (pos<0) return pos;
      if (!wr_str_guarantee_space(g,buf,len,pos+10)) return -1;
      pos+=sprintf((*buf)+pos,")");
      return pos;    
    }  
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
      if ((g->print_clauses_tptp) && (i<=((g->unify_firstuseterm))) &&
          wg_get_encoded_type(db,enc)==WG_URITYPE &&
          wg_decode_uri(db,enc) && strlen(wg_decode_uri(db,enc))==1) {
        if (!strcmp(wg_decode_uri(db,enc),"<")) {         
          if (isneg) {
            pos+=snprintf((*buf)+pos,(*len)-pos,"~$less");           
          } else {
            pos+=snprintf((*buf)+pos,(*len)-pos,"$less");            
          }       
        } else {          
          if (isneg && (i<=((g->unify_firstuseterm)))) {
            pos=wr_strprint_simpleterm_otter(g, enc,printlevel,buf,len,pos,1);
          } else {
            pos=wr_strprint_simpleterm_otter(g, enc,printlevel,buf,len,pos,0);
          }
        }
      } else {
        if (isneg && (i<=((g->unify_firstuseterm)))) {
          pos=wr_strprint_simpleterm_otter(g, enc,printlevel,buf,len,pos,1);
        } else {
          pos=wr_strprint_simpleterm_otter(g, enc,printlevel,buf,len,pos,0);
        }
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
      if (//(g->print_clauses_tptp) && 
          wg_get_encoded_type(db,enc)==WG_URITYPE &&
          wg_decode_str(db,enc) && strlen(wg_decode_str(db,enc))==1) {
        if (!strcmp(wg_decode_str(db,enc),"+")) {
          pos+=snprintf((*buf)+pos,(*len)-pos,"$sum");
        } else if (!strcmp(wg_decode_str(db,enc),"*")) {
          pos+=snprintf((*buf)+pos,(*len)-pos,"$product");
        } else if (!strcmp(wg_decode_str(db,enc),"-")) {
          pos+=snprintf((*buf)+pos,(*len)-pos,"$difference");
        } else if (!strcmp(wg_decode_str(db,enc),"/")) {
          pos+=snprintf((*buf)+pos,(*len)-pos,"$quotient");
        } else if (!strcmp(wg_decode_str(db,enc),"<")) {
          pos+=snprintf((*buf)+pos,(*len)-pos,"$less");  
        } else {
          pos=wr_strprint_simpleterm_otter(g, enc,printlevel,buf,len,pos,0);
        }
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
  printf("simpleterm called with enc %d and type %d str %s\n",
    (int)enc,wg_get_encoded_type(db,enc),wg_decode_str(db, enc));
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
      if (!wr_str_guarantee_space(g,buf,len,pos+strlen(strdata)+50)) return -1;
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
      /*
      if (strchr(p,' ')!=NULL || strchr(p,'\n')!=NULL || strchr(p,'\t')!=NULL) {
        //printf("\"%s\"",p);
        pos+=snprintf((*buf)+pos,(*len)-pos,"\'%s\'",p);
      */

      exdata = wg_decode_uri_prefix(db, enc);
      if (strdata) len1=strlen(strdata);
      if (exdata) len2=strlen(exdata);
      if (!wr_str_guarantee_space(g,buf,len,pos+len1+len2+100)) return -1;
      if (exdata==NULL) {        
        if (g->print_clauses_json) {
          if (isneg) {          
            if (wg_contains_dquote(strdata)) {                        
              pos+=wg_print_dquoted(buf,*len,pos,strdata,1);                
              return pos;
            } else {    
              return pos+snprintf((*buf)+pos,(*len)-pos,"\"-%s\"", strdata);
            }  
          } else {
            if (wg_contains_dquote(strdata)) {                       
              pos+=wg_print_dquoted(buf,*len,pos,strdata,0);                          
              return pos;
            } else {  
              return pos+snprintf((*buf)+pos,(*len)-pos,"\"%s\"", strdata);
            }  
          }          
        } else {          
          if (isneg) {
            if (wg_contains_quote(strdata)) {    
              pos+=snprintf((*buf)+pos,(*len)-pos,"-");        
              pos+=wg_print_quoted(buf,*len,pos,strdata); 
              return pos;
            } else if (wg_should_quote(strdata)) {
              return pos+snprintf((*buf)+pos,(*len)-pos,"-'%s'", strdata);
            } else {
              return pos+snprintf((*buf)+pos,(*len)-pos,"-%s", strdata);
            }            
          } else {
            if (wg_contains_quote(strdata)) {           
              pos+=wg_print_quoted(buf,*len,pos,strdata); 
              return pos;
            } else if (wg_should_quote(strdata)) {
              return pos+snprintf((*buf)+pos,(*len)-pos,"'%s'", strdata);             
            } else {
              return pos+snprintf((*buf)+pos,(*len)-pos,"%s", strdata);             
            }             
            //return pos+snprintf((*buf)+pos,(*len)-pos,"%s%ld", strdata,wg_decode_uri_id(db,enc));
          }           
        }        
      } else {       
        if (g->print_clauses_json) {
          if (isneg) {
             if (wg_contains_dquote(strdata)) {                        
              pos+=wg_print_dquoted(buf,*len,pos,strdata,1);                
              return pos;
            } else {    
              return pos+snprintf((*buf)+pos,(*len)-pos,"\"-%s\"", strdata);
            }  
            //return pos+snprintf((*buf)+pos,(*len)-pos,"\"-%s:%s\"", exdata, strdata);
          } else {
            if (wg_contains_dquote(strdata)) {                       
              pos+=wg_print_dquoted(buf,*len,pos,strdata,0);                          
              return pos;
            } else {  
              return pos+snprintf((*buf)+pos,(*len)-pos,"\"%s\"", strdata);
            } 
            //return pos+snprintf((*buf)+pos,(*len)-pos,"\"%s:%s\"", exdata, strdata);
          }          
        } else {
          if (isneg) {
            pos+=snprintf((*buf)+pos,(*len)-pos,"-"); 
            pos+=wg_print_quoted2(buf,*len,pos,exdata,strdata);
            return pos; 
            //return pos+snprintf((*buf)+pos,(*len)-pos,"-'%s:%s'", exdata, strdata);
          } else {
            pos+=wg_print_quoted2(buf,*len,pos,exdata,strdata);
            return pos;
            //return pos+snprintf((*buf)+pos,(*len)-pos,"'%s:%s'", exdata, strdata);
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

int wg_should_quote(char* s) {
  char* p;
  char c;

  if (!s) return 0;
  if (*(s+1)==0) {
    c=*s;
    if (c=='=') return 0;
    if (c=='-') return 0;
    if (c=='+') return 0;
    if (c=='*') return 0;
    if (c=='/') return 0;
    if (c=='>') return 0;
    if (c=='<') return 0;
  } 
  if (*s=='=' && *(s+1)==0) return 0;
  for(p=s; *p!=0; p++) {
    c=*p;
    if (c=='$' || c=='_') continue;
    if (c<48) return 1;
    if (c>57 && c<64) return 1;
    if (c>90 && c<97) return 1;
    if (c>122) return 1;
  }
  return 0;
}

int wg_contains_quote(char* s) {
  if (!s) return 0;  
  for(; *s!=0; s++) {
    if ((*s)=='\'') return 1;   
  }
  return 0;
}

int wg_contains_dquote(char* s) {
  if (!s) return 0;  
  for(; *s!=0; s++) {
    if ((*s)=='"') return 1;   
  }
  return 0;
}

int wg_print_quoted(char** buf, int len, int pos, char* s) {
  char c;
  int i;

  //printf("\n len %d str %s\n",len,s);
  if (!s || !buf || !len || len<2) return 0;    
  *((*buf)+pos)='\'';
  for(i=1; *s!=0; s++, i++) {
    c=*s;
    if (c=='\'') {
      *((*buf)+pos+i)='\\';
      i++;
    }     
    *((*buf)+pos+i)=c;   
    if ((i+pos)>=len) return i;
  }  
  *((*buf)+pos+i)='\'';
  i++;
  //printf("\n i %d\n",i);
  return i;
}

int wg_print_quoted2(char** buf, int len, int pos, char* es, char* s) {
  char c;
  int i,j;
  char* origes=es;

  //printf("\n len %d str %s\n",len,s);
  if (!s || !buf || !len || len<2) return 0;    
  *((*buf)+pos)='\'';
  for(i=1; *es!=0; es++, i++) {
    c=*es;
    if (c=='\'') {
      *((*buf)+pos+i)='\\';
      i++;
    }     
    *((*buf)+pos+i)=c;   
    if ((i+pos)>=len) return i;
  }  
  
  if (origes && *origes) {
    *((*buf)+pos+i)=':';
    i++;
  }

  for(j=0; *s!=0; s++, j++) {
    c=*s;
    if (c=='\'') {
      *((*buf)+pos+i+j)='\\';
      j++;
    }     
    *((*buf)+pos+i+j)=c;   
    if ((i+j+pos)>=len) return i+j;
  }      
  
  *((*buf)+pos+i+j)='\'';
  //printf("\n i %d\n",i);
  return i+j+1;
}


int wg_print_dquoted(char** buf, int len, int pos, char* s, int negflag) {
  char c;
  int i;

  //printf("\n len %d str %s\n",len,s);
  if (!s || !buf || !len || len<2) return 0;    
  *((*buf)+pos)='"';
  if (negflag) {
    pos++;
    *((*buf)+pos)='-';
  }  
  for(i=1; *s!=0; s++, i++) {
    c=*s;
    if (c=='"') {
      *((*buf)+pos+i)='\\';
      i++;
    }     
    *((*buf)+pos+i)=c;   
    if ((i+pos)>=len) return i;
  }  
  *((*buf)+pos+i)='"';
  i++;
  //printf("\n i %d\n",i);
  if (negflag) return i+1;
  else return i;
}

/*
int wg_print_dquoted2(char** buf, int len, int pos, char* s, char* s2, int negflag) {
  char c;
  int i,j;

  //printf("\n len %d str %s\n",len,s);
  if (!s || !buf || !len || len<2) return 0;    
  *((*buf)+pos)='"';
  if (negflag) {
    pos++;
    *((*buf)+pos)='-';
  }  
  for(i=1; *s!=0; s++, i++) {
    c=*s;
    if (c=='"') {
      *((*buf)+pos+i)='\\';
      i++;
    }     
    *((*buf)+pos+i)=c;   
    if ((i+pos)>=len) return i;
  }  
  *((*buf)+pos+i)='"';
  i++;

  j=0;
  if (s2 && *s2) {
    for(j=1; *s2!=0; s2++, j++) {
      c=*s2;
      if (c=='"') {
        *((*buf)+pos+i+j)='\\';
        j++;
      }     
      *((*buf)+pos+i+j)=c;   
      if ((i+j+pos)>=len) return i+j;
    }  
    *((*buf)+pos+i+j)='"';
    j++;
  } 

  //printf("\n i %d\n",i);
  if (negflag) return i+j+1;
  else return i+j;
}
*/

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
    } else if (g->print_clauses_tptp) {
      snprintf(strbuf,70,"X%d", (int)i);       
    } else {
      snprintf(strbuf,70,"X%d", (int)i);
    }
  }  
  if (g->print_clauses_json) {
    return pos+snprintf((*buf)+pos,(*len)-pos,"\"?:%s\"",strbuf);
  } else {
    return pos+snprintf((*buf)+pos,(*len)-pos,"%s",strbuf);
  }  
}

/* ==================================== 


   TPTP printing

===================================== */


void wg_tptp_print(void* db, void* ptr) {
  char* buf;
  int blen, bpos;

  blen=1000;
  bpos=0;
  buf=sys_malloc(blen);
  if (buf==NULL) {   
    wg_printerr_tptp(db,"tptp printing start cannot alloc buffer");
    return;
  }
  *buf=0;  
  if (!wg_str_guarantee_space(db,&buf,&blen,bpos+100)) {
    wg_printerr_tptp(db,"tptp printing start cannot guarantee buffer space"); 
    if (buf) sys_free(buf);
    return;
  }
  //bpos+=snprintf(buf+bpos,blen-bpos,"buf start");
  printf("\nTPTP starts\n");
  wg_tptp_print_aux(db,ptr,0,1);
  printf("\nCP");
  bpos=wg_print_frm_tptp(db,ptr,&buf,&blen,bpos); 
  //bpos=wg_print_subfrm_tptp(db,ptr,0,0,0,&buf,&blen,bpos);
  printf("\npos %d \n",bpos);
  printf("buf: %s\n",buf); 
  if (buf) sys_free(buf);
  printf("\nTPTP ends\n");
}


void wg_tptp_print_aux(void* db, void* ptr, int depth, int pflag) {
  int type;
  char* p;
  //int count;
  //int ppflag=1;
  //int i;
  //void *curptr;

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
      if (wg_should_quote(p)) {
        printf("\"%s\"",p);
      } else {
        printf("%s",p);
      }
    } else {
      printf("\"\"");
    }
    p=wg_atomstr2(db,ptr);
    if (p!=NULL) {
      if (wg_should_quote(p)) {
        printf("^^\"%s\"",p);
      } else {
        printf("^^%s",p);
      }
    }
  } else {
    /*
    if (pflag && wg_listtreecount(db,ptr)>10) ppflag=1;
    printf ("(");
    for(curptr=ptr, count=0;curptr!=NULL && !wg_isatom(db,curptr);curptr=wg_rest(db,curptr), count++) {
      if (count>0) {
        if (ppflag) {
          printf("\n");
          for(i=0;i<depth;i++) printf(" ");
        }
        printf(" ");
      }
      wg_tptp_print_aux(db,wg_first(db,curptr),depth+1,0);
    }
    if (wg_isatom(db,curptr)) {
      printf(" . ");
      wg_tptp_print_aux(db,curptr,depth+1,ppflag);
    }
    printf (")");
    if (ppflag) printf("\n");
    */
   printf ("(");
   
   printf (")");
  }
}


int wg_print_frm_tptp(void* db, void* ptr, char** buf, int *len, int pos) {  
  int p;
  p=wg_print_subfrm_tptp(db,ptr,0,0,0,buf,len,pos);  
  //printf("\nwg_print_frm_tptp calcs p: %d\n",p);
  return p;
}


int wg_print_subfrm_tptp(void* db, void* ptr,int depth,int pflag, int termflag, char** buf, int *len, int pos) {
  /*
  gptr recptr;
  gint  enc;
  int i, clen;
  */
#ifdef DEBUG  
  printf("print_term called"); 
#endif   
  int type;
  char* p;
  int count;
  int ppflag=0;
  int i;
  void *curptr;
  void *op;
  int lstlen;
  char* symb;
  void* varlst;
  void* varptr;
  int varcount;

  //printf("\nwg_print_subfrm_tptp called\n"); 
  if (!wg_str_guarantee_space(db,buf,len,pos+1000)) {
    wg_printerr_tptp(db,"wg_print_subfrm_tptp cannot guarantee buffer space"); 
    if (buf) sys_free(buf);
    return -1;
  }
  
  /*
  printf("\npos before2: %d",pos);
  pos+=snprintf((*buf)+pos,(*len)-pos,"efg");
  printf("\npos after2: %d",pos);
  return pos;
  */
  if (ptr==NULL) {
    //printf("()");
    pos+=snprintf((*buf)+pos,(*len)-pos,"()");
    return pos;
  } else if (wg_isatom(db,ptr)) {
    type=wg_atomtype(db,ptr);    
    p=wg_atomstr1(db,ptr);   
    if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1; 
    switch (type) {
      /*
      case 0: printf("_:"); break;
      case WG_NULLTYPE: printf("n:"); break;
      case WG_RECORDTYPE: 
        printf("r:"); 
        wg_print_record(db, ((gint*)(offsettoptr((db),(wg_atomrec(db,ptr))))));
        printf(" ");
        break;
      */  
      case WG_INTTYPE: pos+=snprintf((*buf)+pos,(*len)-pos,"%s",p); break;
      case WG_DOUBLETYPE: pos+=snprintf((*buf)+pos,(*len)-pos,"%s",p); break;
      /*
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
      */
      /*
      case WG_ANONCONSTTYPE: {
        if (p!=NULL) {
          pos+=snprintf((*buf)+pos,(*len)-pos,"\'c:%s\'",p);          
        }  
        break;
      }
      */  
      default: {          
        if (p!=NULL) {
          if (wg_contains_quote(p)) {            
            pos+=wg_print_quoted(buf,*len,pos,p); 
          } else if (wg_should_quote(p)) {
            //printf("\"%s\"",p);
            pos+=snprintf((*buf)+pos,(*len)-pos,"\'%s\'",p);
          } else {
            //printf("%s",p);
            pos+=snprintf((*buf)+pos,(*len)-pos,"%s",p);
          }
        } else {
          //printf("\'\'");
          pos+=snprintf((*buf)+pos,(*len)-pos,"\' \'" );
        }
        p=wg_atomstr2(db,ptr);
        if (p!=NULL) {
          if (wg_contains_quote(p)) {   
            pos+=snprintf((*buf)+pos,(*len)-pos,"^^");         
            pos+=wg_print_quoted(buf,*len,pos,p); 
          } else if (wg_should_quote(p)) {
            //printf("^^\"%s\"",p);
            pos+=snprintf((*buf)+pos,(*len)-pos,"^^\"%s\"",p);
          } else {
            //printf("^^%s",p);
            pos+=snprintf((*buf)+pos,(*len)-pos,"^^%s",p);
          }
        }
      }
    }   
    return pos;
  } else {    

    op=wg_first(db,ptr);
    lstlen=wg_list_len(db,ptr);   
    if (pflag && wg_listtreecount(db,ptr)>10) ppflag=1;
    if (!termflag &&
       wg_isatom(db,op) && 
       (wg_atomtype(db,op)==WG_ANONCONSTTYPE || wg_atomtype(db,op)==WG_URITYPE) &&
       wg_atomstr1(db,op) &&
       (wg_atomstr1(db,op))[0]=='=' &&
       !(wg_atomstr1(db,op))[1]) {
      // equality predicate
      pos+=snprintf((*buf)+pos,(*len)-pos,"(");
      pos=wg_print_subfrm_tptp(db,wg_nth(db,ptr,1),depth+1,0,1,buf,len,pos);
      if (pos<0) return pos;
      if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
      pos+=snprintf((*buf)+pos,(*len)-pos," = ");
      pos=wg_print_subfrm_tptp(db,wg_nth(db,ptr,2),depth+1,0,1,buf,len,pos);
      if (pos<0) return pos;
      if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
      pos+=snprintf((*buf)+pos,(*len)-pos,")");
      return pos;
    } else if (!termflag && (wg_islogconnective(db,op) || wg_islogat(db,op))) {
      //printf("@");
      //pos+=snprintf((*buf)+pos,(*len)-pos,"@");
        
      // special handling of connective printing 
      if (wg_islogneg(db,op)) {
        pos+=snprintf((*buf)+pos,(*len)-pos,"~");
        pos=wg_print_subfrm_tptp(db,wg_nth(db,ptr,1),depth+1,0,termflag,buf,len,pos);
        if (pos<0) return pos; 
      } else if (wg_islogor(db,op) || wg_islogand(db,op) 
                 || wg_islogimp(db,op) || wg_islogeqv(db,op) || wg_islogat(db,op)) {
        if (wg_islogor(db,op)) symb=" | ";
        if (wg_islogand(db,op)) symb=" & ";
        if (wg_islogimp(db,op)) symb=" => ";
        if (wg_islogeqv(db,op)) symb=" <=> ";
        if (wg_islogat(db,op)) symb=" @ ";
        pos+=snprintf((*buf)+pos,(*len)-pos,"(");
        pos=wg_print_subfrm_tptp(db,wg_nth(db,ptr,1),depth+1,0,termflag,buf,len,pos);
        if (pos<0) return pos;
        if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;        
        pos+=snprintf((*buf)+pos,(*len)-pos,"%s",symb);        
        pos=wg_print_subfrm_tptp(db,wg_nth(db,ptr,2),depth+1,0,termflag,buf,len,pos);
        if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1; 
        pos+=snprintf((*buf)+pos,(*len)-pos,")");
        if (pos<0) return pos;        
      } else if (wg_islogall(db,op) || wg_islogexists(db,op)) {
        if (wg_islogall(db,op)) symb="! ";
        if (wg_islogexists(db,op)) symb="? ";      
        pos+=snprintf((*buf)+pos,(*len)-pos,"(");
        pos+=snprintf((*buf)+pos,(*len)-pos,"%s",symb); 
        pos+=snprintf((*buf)+pos,(*len)-pos,"[");
        varlst=wg_nth(db,ptr,1);
        for(varptr=varlst, varcount=0;
            varptr!=NULL && !wg_isatom(db,varlst);
            varptr=wg_rest(db,varptr), varcount++) {
          pos=wg_print_subfrm_tptp(db,wg_first(db,varptr),depth+1,0,1,buf,len,pos);
          if (pos<0) return pos;
          if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
          if (wg_rest(db,varptr)) {
            pos+=snprintf((*buf)+pos,(*len)-pos,",");
          }    
        }      
        pos+=snprintf((*buf)+pos,(*len)-pos,"] : ");
        pos=wg_print_subfrm_tptp(db,wg_nth(db,ptr,2),depth+1,0,termflag,buf,len,pos);
        if (pos<0) return pos;
        if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;              
        pos+=snprintf((*buf)+pos,(*len)-pos,")");
        if (pos<0) return pos;
      } 
      return pos;   
    } 
    // normal term printing
    for(curptr=ptr, count=0;curptr!=NULL && !wg_isatom(db,curptr);curptr=wg_rest(db,curptr), count++) {
      if (count>0) {
        // tail of list
        if (ppflag) {
          //printf("\n");
          pos+=snprintf((*buf)+pos,(*len)-pos,"\n");
          if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
          for(i=0;i<depth;i++) {
            //printf(" ");
            pos+=snprintf((*buf)+pos,(*len)-pos," ");
            if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
          }  
        }        
        pos=wg_print_subfrm_tptp(db,wg_first(db,curptr),depth+1,0,1,buf,len,pos);
        if (pos<0) return pos;
        if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
        if ((count+1)<lstlen) {
          // not the last element
          //printf(",");
          pos+=snprintf((*buf)+pos,(*len)-pos,",");
        }
      } else {
        // header of list       
        pos=wg_print_subfrm_tptp(db,wg_first(db,curptr),depth+1,0,1,buf,len,pos);
        if (pos<0) return pos;
        if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
        //printf ("(");
        pos+=snprintf((*buf)+pos,(*len)-pos,"(");
      }      
    }
    if (wg_isatom(db,curptr)) {
      //printf(" . ");
      pos+=snprintf((*buf)+pos,(*len)-pos," . ");
      pos=wg_print_subfrm_tptp(db,curptr,depth+1,pflag,1,buf,len,pos);
      if (pos<0) return pos;
      if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
    }
    //printf (")");
    if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
    pos+=snprintf((*buf)+pos,(*len)-pos,")");
    if (ppflag) {
      //printf("\n");
      pos+=snprintf((*buf)+pos,(*len)-pos,"\n");
    }
  }
  return pos;
}

int wg_print_cnf_tptp(void* db, void* ptr, char** buf, int *len, int pos) {  
  int p;
  p=wg_print_subcnf_tptp(db,ptr,0,0,0,buf,len,pos);  
  //printf("\nwg_print_frm_tptp calcs p: %d\n",p);
  return p;
}


int wg_print_subcnf_tptp(void* db, void* ptr,int depth,int pflag, int termflag, char** buf, int *len, int pos) {
  int type;
  char* p;
  int count;
  int ppflag=0;
  int i;
  void *curptr;
  void *op;
  int lstlen;
  void* el;
  char* symb;  

  //printf("\nwg_print_subfrm_tptp called\n"); 
  if (!wg_str_guarantee_space(db,buf,len,pos+1000)) {
    wg_printerr_tptp(db,"wg_print_subcnf_tptp cannot guarantee buffer space"); 
    if (buf) sys_free(buf);
    return -1;
  }
   
  if (ptr==NULL) {
    //printf("()");
    pos+=snprintf((*buf)+pos,(*len)-pos,"()");
    return pos;
  } else if (wg_isatom(db,ptr)) {
    type=wg_atomtype(db,ptr);    
    p=wg_atomstr1(db,ptr);        
    switch (type) {    
      case WG_INTTYPE: pos+=snprintf((*buf)+pos,(*len)-pos,"%s",p); break;
      case WG_DOUBLETYPE: pos+=snprintf((*buf)+pos,(*len)-pos,"%s",p); break;
      /*
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
      */
      default: {          
        if (p!=NULL) {
          if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
          if (wg_contains_quote(p)) {            
            pos+=wg_print_quoted(buf,*len,pos,p); 
          } else if (wg_should_quote(p)) {
            //printf("\"%s\"",p);
            pos+=snprintf((*buf)+pos,(*len)-pos,"\'%s\'",p);
          } else {
            //printf("%s",p);
            pos+=snprintf((*buf)+pos,(*len)-pos,"%s",p);
          }
        } else {
          //printf("\'\'");
          pos+=snprintf((*buf)+pos,(*len)-pos,"\' \'" );
        }
        p=wg_atomstr2(db,ptr);
        if (p!=NULL) {
          if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
          if (wg_contains_quote(p)) {   
            pos+=snprintf((*buf)+pos,(*len)-pos,"^^");         
            pos+=wg_print_quoted(buf,*len,pos,p); 
          } else if (wg_should_quote(p)) {
            //printf("^^\"%s\"",p);
            pos+=snprintf((*buf)+pos,(*len)-pos,"^^\"%s\"",p);
          } else {
            //printf("^^%s",p);
            pos+=snprintf((*buf)+pos,(*len)-pos,"^^%s",p);
          }
        }
      }
    }   
    return pos;
  } else {    

    op=wg_first(db,ptr);
    lstlen=wg_list_len(db,ptr);   
    if (pflag && wg_listtreecount(db,ptr)>10) ppflag=1;
    if (!termflag &&
       wg_isatom(db,op) && 
       (wg_atomtype(db,op)==WG_ANONCONSTTYPE || wg_atomtype(db,op)==WG_URITYPE) &&
       wg_atomstr1(db,op) &&
       (wg_atomstr1(db,op))[0]=='=' &&
       !(wg_atomstr1(db,op))[1]) {
      // equality predicate
      pos+=snprintf((*buf)+pos,(*len)-pos,"(");
      pos=wg_print_subcnf_tptp(db,wg_nth(db,ptr,1),depth+1,0,1,buf,len,pos);
      if (pos<0) return pos;
      if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
      pos+=snprintf((*buf)+pos,(*len)-pos," = ");
      pos=wg_print_subcnf_tptp(db,wg_nth(db,ptr,2),depth+1,0,1,buf,len,pos);
      if (pos<0) return pos;
      if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
      pos+=snprintf((*buf)+pos,(*len)-pos,")");
      return pos;
    } else if (!termflag && (wg_islogconnective(db,op) || wg_islogat(db,op))) {
      //printf("@");
      //pos+=snprintf((*buf)+pos,(*len)-pos,"@");
        
      // special handling of connective printing 
      if (wg_islogneg(db,op)) {
        pos+=snprintf((*buf)+pos,(*len)-pos,"~");
        pos=wg_print_subcnf_tptp(db,wg_nth(db,ptr,1),depth+1,0,termflag,buf,len,pos);
        if (pos<0) return pos; 
      } else if (wg_islogor(db,op)) {
        symb=" | ";       
        //pos+=snprintf((*buf)+pos,(*len)-pos,"(");
        pos=wg_print_subcnf_tptp(db,wg_nth(db,ptr,1),depth+1,0,termflag,buf,len,pos);
        if (pos<0) return pos;
        if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;        
        pos+=snprintf((*buf)+pos,(*len)-pos,"%s",symb);        
        pos=wg_print_subcnf_tptp(db,wg_nth(db,ptr,2),depth+1,0,termflag,buf,len,pos);
        if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1; 
        //pos+=snprintf((*buf)+pos,(*len)-pos,")");
        if (pos<0) return pos;        
      }
      return pos;   
    } 
    // normal term printing
    for(curptr=ptr, count=0;curptr!=NULL && !wg_isatom(db,curptr);curptr=wg_rest(db,curptr), count++) {
      if (count>0) {
        // tail of list
        if (ppflag) {
          //printf("\n");
          pos+=snprintf((*buf)+pos,(*len)-pos,"\n");
          if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
          for(i=0;i<depth;i++) {
            //printf(" ");
            pos+=snprintf((*buf)+pos,(*len)-pos," ");
            if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
          }  
        }        
        pos=wg_print_subcnf_tptp(db,wg_first(db,curptr),depth+1,0,1,buf,len,pos);
        if (pos<0) return pos;
        if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
        if ((count+1)<lstlen) {
          // not the last element
          //printf(",");
          pos+=snprintf((*buf)+pos,(*len)-pos,",");
        }
      } else {
        // header of list
        el=wg_first(db,curptr);
        if (termflag && wg_isatom(db,el) && wg_atomstr1(db,el) && strlen(wg_atomstr1(db,el))==1) {
          if (!strcmp(wg_atomstr1(db,el),"+")) {
            pos+=snprintf((*buf)+pos,(*len)-pos,"$sum");
          } else if (!strcmp(wg_atomstr1(db,el),"*")) {
            pos+=snprintf((*buf)+pos,(*len)-pos,"$product");
          } else if (!strcmp(wg_atomstr1(db,el),"-")) {
            pos+=snprintf((*buf)+pos,(*len)-pos,"$difference");
          } else if (!strcmp(wg_atomstr1(db,el),"/")) {
            pos+=snprintf((*buf)+pos,(*len)-pos,"$quotient");
          //} else if (!strcmp(wg_atomstr1(db,el),"<")) {
          //  pos+=snprintf((*buf)+pos,(*len)-pos,"$less");
          } else {
            pos=wg_print_subcnf_tptp(db,el,depth+1,0,1,buf,len,pos);
          } 
        } else if (!termflag && wg_isatom(db,el) && wg_atomstr1(db,el) && strlen(wg_atomstr1(db,el))==1 &&
                   !strcmp(wg_atomstr1(db,el),"<")) {
            pos+=snprintf((*buf)+pos,(*len)-pos,"$less");                  
        } else {
          pos=wg_print_subcnf_tptp(db,el,depth+1,0,1,buf,len,pos);
        }  
        if (pos<0) return pos;
        if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
        //printf ("(");
        pos+=snprintf((*buf)+pos,(*len)-pos,"(");
      }      
    }
    if (wg_isatom(db,curptr)) {
      //printf(" . ");
      pos+=snprintf((*buf)+pos,(*len)-pos," . ");
      pos=wg_print_subcnf_tptp(db,curptr,depth+1,pflag,1,buf,len,pos);
      if (pos<0) return pos;
      if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
    }
    //printf (")");
    if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
    pos+=snprintf((*buf)+pos,(*len)-pos,")");
    if (ppflag) {
      //printf("\n");
      pos+=snprintf((*buf)+pos,(*len)-pos,"\n");
    }
  }
  return pos;
}

int wg_print_term_tptp(void* db, void* ptr,int depth,int pflag,char** buf, int *len, int pos) {
  /*
  gptr recptr;
  gint  enc;
  int i, clen;
  */
#ifdef DEBUG  
  printf("print_term called"); 
#endif   
  int type;
  char* p;
  int count;
  int ppflag=0;
  int i;
  void *curptr;

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
      if (!wg_str_guarantee_space(db,buf,len,pos+strlen(p)+1000)) return -1; 
      if (wg_should_quote(p)) {
        printf("\"%s\"",p);
      } else {
        printf("%s",p);
      }
    } else {
      printf("\"\"");
    }
    p=wg_atomstr2(db,ptr);
    if (p!=NULL) {
      if (!wg_str_guarantee_space(db,buf,len,pos+strlen(p)+1000)) return -1;
      if (wg_should_quote(p)) {
        printf("^^\"%s\"",p);
      } else {
        printf("^^%s",p);
      }
    }
  } else {    
    if (pflag && wg_listtreecount(db,ptr)>10) ppflag=1;
    printf ("(");
    for(curptr=ptr, count=0;curptr!=NULL && !wg_isatom(db,curptr);curptr=wg_rest(db,curptr), count++) {
      if (count>0) {
        if (ppflag) {
          printf("\n");
          for(i=0;i<depth;i++) printf(" ");
        }
        printf(" ");
      }
      wg_print_term_tptp(db,wg_first(db,curptr),depth+1,0,buf,len,pos);
    }
    if (wg_isatom(db,curptr)) {
      printf(" . ");
      wg_print_term_tptp(db,curptr,depth+1,ppflag,buf,len,pos);
    }
    printf (")");
    if (ppflag) printf("\n");
  }
  return pos; 
}




// =========== json printing ================



int wg_print_frm_json(void* db, void* ptr, char** buf, int *len, int pos) {  
  int p;
  p=wg_print_subfrm_json(db,ptr,0,0,0,buf,len,pos,0);
  //printf("\nwg_print_frm_json calcs p: %d\n",p);
  return p;
}


int wg_print_subfrm_json(void* db, void* ptr,int depth,int pflag, 
                         int termflag, char** buf, int *len, int pos, int negflag) {
  /*
  gptr recptr;
  gint  enc;
  int i, clen;
  */
#ifdef DEBUG  
  printf("print_term called"); 
#endif   
  //int type;
  char *p;
  int count;
  int ppflag=0;
  int i;
  void *curptr;
  void *op;
  int lstlen, type;
  char* symb;
  void* varlst;
  void* varptr;
  int varcount;

  //printf("\nwg_print_subfrm_json called\n"); 
  if (!wg_str_guarantee_space(db,buf,len,pos+1000)) {
    wg_printerr_json(db,"wg_print_subfrm_json cannot guarantee buffer space"); 
    if (buf) sys_free(buf);
    return -1;
  }
  
  /*
  printf("\npos before2: %d",pos);
  pos+=snprintf((*buf)+pos,(*len)-pos,"efg");
  printf("\npos after2: %d",pos);
  return pos;
  */
  if (ptr==NULL) {
    //printf("()");
    pos+=snprintf((*buf)+pos,(*len)-pos,"[]");
    return pos;
  } else if (wg_isatom(db,ptr)) {
    type=wg_atomtype(db,ptr); 
    p=wg_atomstr1(db,ptr); 
    //p2=wg_atomstr2(db,ptr);  
    if (!wg_str_guarantee_space(db,buf,len,pos+strlen(p)+1000)) return -1; 
    switch (type) {
      /*
      case 0: printf("_:"); break;
      case WG_NULLTYPE: printf("n:"); break;
      case WG_RECORDTYPE: 
        printf("r:"); 
        wg_print_record(db, ((gint*)(offsettoptr((db),(wg_atomrec(db,ptr))))));
        printf(" ");
        break;
      */  
      case WG_INTTYPE: pos+=snprintf((*buf)+pos,(*len)-pos,"%s",p); break;
      case WG_DOUBLETYPE: pos+=snprintf((*buf)+pos,(*len)-pos,"%s",p); break;
      /*
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
      */
      default: {        
        if (p==NULL) {     
          pos+=snprintf((*buf)+pos,(*len)-pos,"\"\"" );                
        } else if (!strncmp(p,"$true",5)) {
          if (negflag) pos+=snprintf((*buf)+pos,(*len)-pos,"false");
          else pos+=snprintf((*buf)+pos,(*len)-pos,"true");
        } else if (!strncmp(p,"$false",6)) {
          if (negflag) pos+=snprintf((*buf)+pos,(*len)-pos,"true");
          else pos+=snprintf((*buf)+pos,(*len)-pos,"false");
        } else if (wg_contains_dquote(p)) { 
          if (negflag) {                  
            pos+=wg_print_dquoted(buf,*len,pos,p,1);        
          } else {
            pos+=wg_print_dquoted(buf,*len,pos,p,0); 
          }  
        } else {
          if (negflag) {
            pos+=snprintf((*buf)+pos,(*len)-pos,"\"~%s\"",p);
          } else {
            pos+=snprintf((*buf)+pos,(*len)-pos,"\"%s\"",p);
          }  
        }
      }
    }    
    return pos;   
    // -- old ---
    /*
    p=wg_atomstr1(db,ptr);   
    if (p!=NULL) {     
      pos+=snprintf((*buf)+pos,(*len)-pos,"\"%s\"",p);      
    } else {
      //printf("\'\'");
      pos+=snprintf((*buf)+pos,(*len)-pos,"\"\"" );
    }
    p=wg_atomstr2(db,ptr);
    if (p!=NULL) {    
      pos+=snprintf((*buf)+pos,(*len)-pos,"\"^^%s\"",p);      
    }    
    return pos;
    */
  } else {    

    op=wg_first(db,ptr);
    lstlen=wg_list_len(db,ptr);   
    if (pflag && wg_listtreecount(db,ptr)>10) ppflag=1;
    if (!termflag &&
       wg_isatom(db,op) && 
       (wg_atomtype(db,op)==WG_ANONCONSTTYPE || wg_atomtype(db,op)==WG_URITYPE) &&
       wg_atomstr1(db,op) &&
       (wg_atomstr1(db,op))[0]=='=' &&
       !(wg_atomstr1(db,op))[1]) {
      // equality predicate
      pos+=snprintf((*buf)+pos,(*len)-pos,"[");
      pos=wg_print_subfrm_json(db,wg_nth(db,ptr,1),depth+1,0,1,buf,len,pos,0);
      if (pos<0) return pos;
      if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
      pos+=snprintf((*buf)+pos,(*len)-pos,", \"=\", ");
      pos=wg_print_subfrm_json(db,wg_nth(db,ptr,2),depth+1,0,1,buf,len,pos,0);
      if (pos<0) return pos;
      if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
      pos+=snprintf((*buf)+pos,(*len)-pos,"]");
      return pos;
    } else if (!termflag && (wg_islogconnective(db,op) || wg_islogat(db,op))) {
      //printf("@");
      //pos+=snprintf((*buf)+pos,(*len)-pos,"@");
        
      // special handling of connective printing 
      if (wg_islogneg(db,op)) {              
        if (lstlen==2 && wg_list_is_term(db,wg_nth(db,ptr,1))) {
          // add neg to first el of the ptr list
          pos=wg_print_subfrm_json(db,wg_nth(db,ptr,1),depth+1,0,termflag,buf,len,pos,1);
          if (pos<0) return pos; 
        } else {
          // write neg as a separate op
          pos+=snprintf((*buf)+pos,(*len)-pos,"[\"~\", ");
          pos=wg_print_subfrm_json(db,wg_nth(db,ptr,1),depth+1,0,termflag,buf,len,pos,0);
          if (pos<0) return pos; 
          if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
          pos+=snprintf((*buf)+pos,(*len)-pos,"]");
        }
      } else if (wg_islogor(db,op) || wg_islogand(db,op) 
                 || wg_islogimp(db,op) || wg_islogeqv(db,op) || wg_islogat(db,op)) {
        if (wg_islogor(db,op)) symb=", \"|\", ";
        if (wg_islogand(db,op)) symb=", \"&\", ";
        if (wg_islogimp(db,op)) symb=", \"=>\", ";
        if (wg_islogeqv(db,op)) symb=", \"<=>\", ";
        if (wg_islogat(db,op)) symb=", \"@\", ";
        pos+=snprintf((*buf)+pos,(*len)-pos,"[");
        pos=wg_print_subfrm_json(db,wg_nth(db,ptr,1),depth+1,0,termflag,buf,len,pos,0);
        if (pos<0) return pos;
        if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;        
        pos+=snprintf((*buf)+pos,(*len)-pos,"%s",symb);        
        pos=wg_print_subfrm_json(db,wg_nth(db,ptr,2),depth+1,0,termflag,buf,len,pos,0);
        if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1; 
        pos+=snprintf((*buf)+pos,(*len)-pos,"]");
        if (pos<0) return pos;        
      } else if (wg_islogall(db,op) || wg_islogexists(db,op)) {
        if (wg_islogall(db,op)) symb="\"forall\"";
        if (wg_islogexists(db,op)) symb="\"exists\"";      
        pos+=snprintf((*buf)+pos,(*len)-pos,"[");
        pos+=snprintf((*buf)+pos,(*len)-pos,"%s, ",symb); 
        pos+=snprintf((*buf)+pos,(*len)-pos,"[");
        varlst=wg_nth(db,ptr,1);
        for(varptr=varlst, varcount=0;
            varptr!=NULL && !wg_isatom(db,varlst);
            varptr=wg_rest(db,varptr), varcount++) {
          pos=wg_print_subfrm_json(db,wg_first(db,varptr),depth+1,0,1,buf,len,pos,0);
          if (pos<0) return pos;
          if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
          if (wg_rest(db,varptr)) {
            pos+=snprintf((*buf)+pos,(*len)-pos,",");
          }    
        }      
        pos+=snprintf((*buf)+pos,(*len)-pos,"], ");
        pos=wg_print_subfrm_json(db,wg_nth(db,ptr,2),depth+1,0,termflag,buf,len,pos,0);
        if (pos<0) return pos;
        if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;              
        pos+=snprintf((*buf)+pos,(*len)-pos,"]");
        if (pos<0) return pos;
      } 
      return pos;   
    } 
    // normal term printing
    for(curptr=ptr, count=0;curptr!=NULL && !wg_isatom(db,curptr);curptr=wg_rest(db,curptr), count++) {
      if (count>0) {
        // tail of list
        if (ppflag) {
          //printf("\n");
          pos+=snprintf((*buf)+pos,(*len)-pos,"\n");
          if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
          for(i=0;i<depth;i++) {
            //printf(" ");
            pos+=snprintf((*buf)+pos,(*len)-pos," ");
            if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
          }  
        }        
        pos=wg_print_subfrm_json(db,wg_first(db,curptr),depth+1,0,1,buf,len,pos,0);
        if (pos<0) return pos;
        if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
        if ((count+1)<lstlen) {
          // not the last element
          //printf(",");
          pos+=snprintf((*buf)+pos,(*len)-pos,",");
        }
      } else {
        // header of list
        if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
        //printf ("(");
        pos+=snprintf((*buf)+pos,(*len)-pos,"[");
        pos=wg_print_subfrm_json(db,wg_first(db,curptr),depth+1,0,1,buf,len,pos,negflag);
        if (pos<0) return pos;
        if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
        //printf ("(");
        if (lstlen>1) {
          // not a single element
          pos+=snprintf((*buf)+pos,(*len)-pos,",");        
        }  
      }      
    }
    if (wg_isatom(db,curptr)) {
      //printf(" . ");
      pos+=snprintf((*buf)+pos,(*len)-pos," . ");
      pos=wg_print_subfrm_json(db,curptr,depth+1,pflag,1,buf,len,pos,0);
      if (pos<0) return pos;
      if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
    }
    //printf (")");
    if (!wg_str_guarantee_space(db,buf,len,pos+1000)) return -1;
    pos+=snprintf((*buf)+pos,(*len)-pos,"]");
    if (ppflag) {
      //printf("\n");
      pos+=snprintf((*buf)+pos,(*len)-pos,"\n");
    }
  }
  return pos;
}

int wg_print_term_json(void* db, void* ptr,int depth,int pflag,char** buf, int *len, int pos) {
  /*
  gptr recptr;
  gint  enc;
  int i, clen;
  */
#ifdef DEBUG  
  printf("print_term called"); 
#endif   
  int type;
  char* p;
  int count;
  int ppflag=0;
  int i;
  void *curptr;

  if (ptr==NULL) {
    printf("[]");
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
      printf("\"%s\"",p);      
    } else {
      printf("\"\"");
    }
    p=wg_atomstr2(db,ptr);
    if (p!=NULL) {
      printf("\"^^%s\"",p);      
    }
  } else {    
    if (pflag && wg_listtreecount(db,ptr)>10) ppflag=1;
    printf ("[");
    for(curptr=ptr, count=0;curptr!=NULL && !wg_isatom(db,curptr);curptr=wg_rest(db,curptr), count++) {
      if (count>0) {
        if (ppflag) {
          printf("\n");
          for(i=0;i<depth;i++) printf(" ");
        }
        printf(" ");
      }
      wg_print_term_json(db,wg_first(db,curptr),depth+1,0,buf,len,pos);
    }
    if (wg_isatom(db,curptr)) {
      printf(" . ");
      wg_print_term_json(db,curptr,depth+1,ppflag,buf,len,pos);
    }
    printf ("]");
    if (ppflag) printf("\n");
  }
  return pos;
}

int wg_list_is_term(void* db, void* lst) {
  void *el, *ptr;
  int i; //, type;;

  if (!lst || !wg_ispair(db,lst)) return 0;
  ptr=lst;
  for(i=0; ptr && wg_ispair(db,ptr); i++, ptr=wg_rest(db,ptr)) {   
    el=wg_first(db,ptr);
    if (wg_islogconnective(db,el)) return 0;
    if (!i) {      
      // first el should be symbol
      if (!wg_isatom(db,el) || wg_atomtype(db,el)!=WG_URITYPE) {
        return 0;
      }      
    }    
  }
  return 1;
}


// ============ end json printing ==============


int wg_print_nice_var_tptp(void* db, gint i, char** buf, int *len, int pos) {
  char strbuf[80];

  strbuf[0]=(char)0;
  if (!wg_str_guarantee_space(db,buf,len,pos+20)) return -1;

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
    if (wg_print_tptp_json(db)) {
      snprintf(strbuf,70,"%d", (int)i);
    } else {
      snprintf(strbuf,70,"?%d", (int)i);
    }
  }  
  if (wg_print_tptp_json(db)) {
    return pos+snprintf((*buf)+pos,(*len)-pos,"\"?%s\"",strbuf);
  } else {
    return pos+snprintf((*buf)+pos,(*len)-pos,"%s",strbuf);
  }  
}


/** Guarantee string space: realloc if necessary, then set last byte to 0
*
*/

int wg_str_guarantee_space(void* db, char** stradr, int* strlenadr, int needed) {
  char* tmp;
  int newlen;
  int j;
  
  //printf("str_guarantee_space, needed: %d, *strlenadr: %d\n",needed,*strlenadr);
  if (needed<=(*strlenadr)) return 1;
  // now need more space
  newlen=(*strlenadr);
  while(1) {
    newlen=newlen*2;      
    if (newlen>1000000000) return 0; // too big, over one gig
    if (newlen>needed) break;          
  }  
  tmp=sys_realloc(*stradr,newlen);
  if (tmp==NULL) {
    wg_printerr_tptp(db,"Cannot reallocate memory for a string");    
    return 0;
  }  
  for(j=(*strlenadr)-1;j<newlen;j++) *(tmp+j)=' '; // clear new space
  tmp[newlen-1]=0;   // set last byte to 0  
  *stradr=tmp;
  *strlenadr=newlen;
  return 1;
}
    
/** Free the passed string.
*
*/

void wg_str_free(void* db, char* str) {
  //UNUSED(db);
  if (str!=NULL) sys_free(str);
}  


/** Free the string pointed to at passed address and set address ptr to NULL
*
*/

void wg_str_freeref(void* db, char** strref) {
  //UNUSED(db);
  if (*strref!=NULL) sys_free(*strref);
  *strref=NULL;  
}  


int wg_print_tptp_json(void* db) {
  //UNUSED(db);
  return 0;
}


/* ------------ errors ---------------- */


void wg_printerr_tptp(void* db, char* str) {
  printf("\nERROR while printing: %s\n",str);
  return;
}

void wg_printerr_json(void* db, char* str) {
  printf("\nERROR while printing: %s\n",str);
  return;
}