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

 /** @file clterm.c
 *  Procedures for building clauses/terms and fetching parts.
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



/* ======= Private protos ================ */
  
  
/* ====== Functions ============== */

/* ---------------- wr functions --------------------- */    




gptr wr_create_raw_record(glb* g, gint length, gint meta, gptr buffer) {
  gptr rec;
  
  if (buffer==NULL) {
    rec=wg_create_raw_record(g->db,length); 
    if (rec==NULL) return NULL;
    rec[RECORD_META_POS]=meta;    
  } else {
    rec=wr_alloc_from_cvec(g,buffer,length+RECORD_HEADER_GINTS);
    if (rec==NULL) return NULL;
    rec[0]=(length+RECORD_HEADER_GINTS)*sizeof(gint);
    rec[RECORD_BACKLINKS_POS]=0; 
    rec[RECORD_META_POS]=meta;    
  }    
  return rec;
}  

#ifdef _WIN32
int wr_term_unify_endpos(glb* g, gptr term) {
  if ( (g->unify_maxuseterms) &&
       (((g->unify_maxuseterms)+(g->unify_firstuseterm))<get_record_len(term)) ) 
  return ((g->unify_maxuseterms)+(g->unify_firstuseterm)+RECORD_HEADER_GINTS);
  else return get_record_len(term)+RECORD_HEADER_GINTS; 
}
#else
inline int wr_term_unify_endpos(glb* g, gptr term) {
  if ( (g->unify_maxuseterms) &&
       (((g->unify_maxuseterms)+(g->unify_firstuseterm))<get_record_len(term)) ) 
  return ((g->unify_maxuseterms)+(g->unify_firstuseterm)+RECORD_HEADER_GINTS);
  else return get_record_len(term)+RECORD_HEADER_GINTS; 
}
#endif
  
/* ---------------- wr functions --------------------- */  
  
void* wr_create_rule_clause(glb* g, int litnr) {
  void* res;
  
  res=wg_create_raw_record(g->db,CLAUSE_EXTRAHEADERLEN+(LIT_WIDTH*litnr));
  if (!res) return NULL;
  *((gint*)res+RECORD_META_POS)=(RECORD_META_NOTDATA | RECORD_META_RULE_CLAUSE);
  return res;  
} 

void* wr_create_fact_clause(glb* g, int litnr) {
  void* res;
  
  res=wg_create_raw_record(g->db,(g->unify_firstuseterm)+litnr+(g->unify_footerlen));
  if (!res) return NULL;
  *((gint*)res+RECORD_META_POS)=RECORD_META_FACT_CLAUSE;
  *((gint*)res+(RECORD_HEADER_GINTS+TERM_META_POS))=0;
  return res;  
} 

void* wr_create_atom(glb* g, int termnr) {
  void* res;
  
  res=wg_create_raw_record(g->db,(g->unify_firstuseterm)+termnr+(g->unify_footerlen));
  if (!res) return NULL;
  *((gint*)res+RECORD_META_POS)=(RECORD_META_NOTDATA | RECORD_META_ATOM);
  *((gint*)res+(RECORD_HEADER_GINTS+TERM_META_POS))=0;
  return res;  
} 


void* wr_create_term(glb* g, int termnr) {
  void* res;
  
  res=wg_create_raw_record(g->db,(g->unify_firstuseterm)+termnr+(g->unify_footerlen));
  if (!res) return NULL;
  *((gint*)res+RECORD_META_POS)=(RECORD_META_NOTDATA | RECORD_META_TERM);
  *((gint*)res+(RECORD_HEADER_GINTS+TERM_META_POS))=0;
  return res;  
} 



void* wr_convert_atom_fact_clause(glb* g, void* atom, int isneg) {
  void* res;
  
  res=atom;
  if (!res) return NULL;
  *((gint*)res+RECORD_META_POS)=(RECORD_META_ATOM | RECORD_META_FACT_CLAUSE);
  *((gint*)res+(RECORD_HEADER_GINTS+TERM_META_POS))=0;
  return res;  
} 

int wr_set_rule_clause_atom(glb* g, void* clause, int litnr, gint atom) {
   
  *((gint*)clause+RECORD_HEADER_GINTS+CLAUSE_EXTRAHEADERLEN+(LIT_WIDTH*litnr)+1)=atom;
  return 0;
} 

int wr_set_rule_clause_atom_meta(glb* g, void* clause, int litnr, gint meta) {
  
  *((gint*)clause+RECORD_HEADER_GINTS+CLAUSE_EXTRAHEADERLEN+(LIT_WIDTH*litnr))=meta;
  return 0;
} 

int wr_set_atom_subterm(glb* g, void* atom, int termnr, gint subterm) {
  
  wg_set_new_field(g->db,atom,(g->unify_firstuseterm)+termnr,subterm);  
  return 0;
} 

int wr_set_kb_atom_subterm(glb* g, void* atom, int termnr, gint subterm) {
  
  wg_set_new_field(g_to_kb_db(g),atom,(g->unify_firstuseterm)+termnr,subterm);  
  return 0;
}  

int wr_set_term_subterm(glb* g, void* term, int termnr, gint subterm) {
  
  wg_set_new_field(g->db,term,(g->unify_firstuseterm)+termnr,subterm);  
  return 0;
}

int wr_set_kb_term_subterm(glb* g, void* term, int termnr, gint subterm) {
  
  wg_set_new_field(g_to_kb_db(g),term,(g->unify_firstuseterm)+termnr,subterm);  
  return 0;
}

int wr_blocked_clause_simple(glb* g, gptr clause) {
  void* db=g->db;
  gint history;
  gptr historyptr;
  int len;
  int htype,tag,dechead;
  gint head, cl1;

  //printf("\n wr_blocked_clause starts \n ");
  if (wg_rec_is_blocked_clause(db,clause)) {
    wr_printf("\n direct block ");
    wr_print_clause(g,clause);
    wr_printf("\n");
    return 1;
  }    
  history=wr_get_history(g,clause);
  if (history==(gint)NULL) return 0;
  historyptr=otp(db,history);
  len=wg_get_record_len(db,historyptr); 
  if (len==HISTORY_PREFIX_LEN) return 0; // input
  head=wr_get_history_record_field(db,historyptr,HISTORY_DERIVATION_TAG_POS);
  htype=wg_get_encoded_type(db,head);
  if (htype!=WG_INTTYPE) return 0;
  dechead=wg_decode_int(db,head);
  tag=wr_get_history_tag(g,dechead);
  if (tag!=WR_HISTORY_TAG_RESOLVE) return 0;
  cl1=wr_get_history_record_field(g,historyptr,HISTORY_PARENT1_POS);
  if (wg_rec_is_blocked_clause(db,wg_decode_record(db,cl1))) {
    wr_printf("\n parent1 block \n");
    wr_print_clause(g,clause);
    wr_printf("\n");
    return 1;
  }  
  cl1=wr_get_history_record_field(g,historyptr,HISTORY_PARENT2_POS);
  if (wg_rec_is_blocked_clause(db,wg_decode_record(db,cl1)))  {
    wr_printf("\n parent2 block \n");
    wr_print_clause(g,clause);
    wr_printf("\n");
    return 1;
  }  
  return 0;
  //return wg_rec_is_blocked_clause(g->db,clause);
}


int wr_blocked_clause(glb* g, gptr clause) {
  void* db=g->db;
  gint history;
  gptr historyptr;
  int len;
  int htype,tag,dechead;
  gint head, cl1;

  //printf("\n wr_blocked_clause starts \n ");
  if (wg_rec_is_blocked_clause(db,clause)) {
    /*
    wr_printf("\n direct block ");
    wr_print_clause(g,clause);
    wr_printf("\n");
    */
    return 1;
  }   
  history=wr_get_history(g,clause);
  if (history==(gint)NULL) return 0;
  historyptr=otp(db,history);
  len=wg_get_record_len(db,historyptr); 
  if (len==HISTORY_PREFIX_LEN) return 0; // input
  head=wr_get_history_record_field(db,historyptr,HISTORY_DERIVATION_TAG_POS);
  htype=wg_get_encoded_type(db,head);
  if (htype!=WG_INTTYPE) return 0;
  dechead=wg_decode_int(db,head);
  tag=wr_get_history_tag(g,dechead);
  if (tag!=WR_HISTORY_TAG_RESOLVE) return 0;
  cl1=wr_get_history_record_field(g,historyptr,HISTORY_PARENT1_POS);
  if (wr_blocked_clause(g,wg_decode_record(db,cl1))) {
    wr_printf("\n parent1 block \n");
    wr_print_clause(g,clause);
    wr_printf("\n");
    return 1;
  }  
  cl1=wr_get_history_record_field(g,historyptr,HISTORY_PARENT2_POS);
  if (wr_blocked_clause(g,wg_decode_record(db,cl1)))  {
    wr_printf("\n parent2 block \n");
    wr_print_clause(g,clause);
    wr_printf("\n");
    return 1;
  }  
  return 0;
  //return wg_rec_is_blocked_clause(g->db,clause);
}

void wr_mark_clause_blocked(glb* g, gptr clause) {
  gint meta;

  meta=*((gint*)(clause)+RECORD_META_POS);
  *((gint*)(clause)+RECORD_META_POS)=meta | RECORD_META_BLOCKED;
}

void wr_clear_clause_blocked(glb* g, gptr clause) {
  gint meta;

  meta=*((gint*)(clause)+RECORD_META_POS);
  *((gint*)(clause)+RECORD_META_POS)=meta & ~RECORD_META_BLOCKED;

}

/* *********** instgen funs ************* */

int wr_ground_term(glb* g, gint x) {
  void* db;
  gptr xptr;
  int start,end,i;

#ifdef DEBUG  
  wr_printf("wr_ground_term called with x %d type %d\n",x,wg_get_encoded_type(g->db,x));
#endif  
  if (!isdatarec(x)) {
    if (isvar(x)) return 0;
    else return 1;        
  }   
  // now we have a datarec
  db=g->db;
  xptr=decode_record(db,x);
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,xptr);
  for(i=start;i<end;i++) {
    if (!wr_ground_term(g,xptr[i])) return 0;  
  }   
  return 1;
}

/* ***** shared clauses *********** */



gint wr_copy_clause(glb* g, glb* shared_g, gint x) {
  //int histlen;  
  gint res;
  //gptr xptr, historyptr;
  //void* db;

  if (!x) return 0;

  /*
  wr_printf("\nwr_copy_clause called for record "); 
  wr_print_clause(g,rotp(g,x));
  wr_printf("\n");
  //wg_print_record(g->db,rotp(g,x));
  //wr_printf("\n");
  */
  /*
  gint xlen,uselen;
  gptr yptr;
  int i,type;
  */
  /*
  db=g->db;
  xptr=decode_record(db,x); 
  xlen=get_record_len(xptr);
  uselen=xlen+RECORD_HEADER_GINTS;
  */
  //for(i=0;i<RECORD_HEADER_GINTS;i++) {    
  //    yptr[i]=xptr[i];
  //}   
  /*
  printf("\nxlen %d RECORD_HEADER_GINTS %ld uselen %ld\n",xlen,RECORD_HEADER_GINTS,uselen);
  gint history;
  history=wr_get_history(g,xptr);
  historyptr=otp(db,history); 
  histlen=wg_get_record_len(db,historyptr);
  wg_print_record(db,historyptr);
  printf("\nhistlen %d history:\n",histlen);
  */
  /*
  for(i=RECORD_HEADER_GINTS;i<uselen;i++) {   
    printf("\ni %ld",i);
    printf("\nxptr[i] %ld",xptr[i]);

    // new part
    if (xptr[i]==(gint)NULL) {
      printf(" NULL ");    
    } else {
      //printf(" val %ld ",(gint)(xptr[i]));
      type=wg_get_encoded_type(db, xptr[i]);
      printf(" type %d ",type);
    }
  } 
  */     

  res=wr_copy_record(g,shared_g,x,1);
  /*
  if (res) {
    wr_printf("\nwr_copy_clause returns "); 
    //wr_print_term(g,res);
    //wr_printf("\n");
    wr_print_clause(g,rotp(g,res));
    wr_printf("\n");
    //wg_print_record(g->db,rotp(g,res));
    //wr_printf("\n");
  }  
  */
  return res;

}  

gint wr_copy_record(glb* g, glb* shared_g, gint x, int toplevel) {
  void* db;
  gptr xptr,yptr;
  gint xlen,uselen;
  gint tmp; 
  int i;
  gint res;

  int type;
  char* str1;
  //char* str2;
  //int intdata;

  /*
  //printf("\nwr_copy_record called, x %ld toplevel %d g->build_buffer ptr is %lx \n",(gint)x, toplevel,(unsigned long int)g->build_buffer); 
  printf("\nwr_copy_record called, %d %ld\n",toplevel,(gint)(shared_g->build_buffer));
  wg_print_record(g->db,rotp(g,x));
  printf("\n");
  */
  
#ifdef DEBUG
  wr_printf("\nwr_copy_term called, g->build_buffer ptr is %lx \n", (unsigned long int)g->build_buffer); 
  wr_print_term(g,x);
  wr_printf("\n");
#endif    
  if (!x || !isdatarec(x)) {
    return x;
  }    
  // now we have a datarec  
  db=g->db;
  xptr=decode_record(db,x); 
  xlen=get_record_len(xptr);
  
  if (toplevel==2) {       
    if ((shared_g->build_buffer)!=NULL) {    
      yptr=wr_alloc_from_cvec(shared_g,shared_g->build_buffer,(RECORD_HEADER_GINTS+HISTORY_PREFIX_LEN));         
    } else {          
      //yptr=wg_create_raw_record(shared_g->db,HISTORY_PREFIX_LEN); 
      yptr=wg_create_record(shared_g->db,HISTORY_PREFIX_LEN);
    }   
    if (!yptr) {
      printf("\ncould not wg_create_raw_record in wr_copy_record, exiting\n");
      return 0;
    }
    /*
    for(i=RECORD_HEADER_GINTS;i<RECORD_HEADER_GINTS+HISTORY_PREFIX_LEN;i++) {
      //yptr[i]=(gint)0; //NULL;
      yptr[i]=wg_encode_int(g->db,10);
    }
    */
    //return (gint)yptr;
    /*
    printf("\nmade history\n");
    wg_print_record(g->db,yptr);
    printf("\n");
    */ 

    for(i=RECORD_HEADER_GINTS; i<RECORD_HEADER_GINTS+HISTORY_PREFIX_LEN; i++) {  
      if (!(xptr[i])) {
        yptr[i]=xptr[i];
      } else {  
        type=wg_get_encoded_type(db, xptr[i]);
        if (type==WG_URITYPE) {
          yptr[i]=xptr[i];
        } else if (type==WG_INTTYPE) {
          yptr[i]=xptr[i];
        } else {
          yptr[i]=(gint)0;          
        }
      }
    }        
    return encode_record(g->db,yptr);    
  }  

  if (xlen<1 || xlen>100) {
    /*
    wr_printf("\n xlen %d x %ld type %ld\n",xlen,x,(gint)(wg_get_encoded_type(db, x)));
    wr_print_term(g,x);
    wr_printf("\n");
    wg_print_record(g->db,rotp(g,x));
    wr_printf("\n");
    exit(0);
    */
    return 0;
  }  

  if (toplevel==2) {
    //x is history
    //printf("\nhistory len %ld of\n",xlen);
    //wg_print_record(g->db,rotp(g,x));
    xlen=HISTORY_PREFIX_LEN;
  }

  //printf("\nxlen next %ld\n",xlen);
  // allocate space 
  if ((shared_g->build_buffer)!=NULL) {    
    yptr=wr_alloc_from_cvec(shared_g,shared_g->build_buffer,(RECORD_HEADER_GINTS+xlen));         
  } else {    
    //printf("\nshared_g %ld shared_g->db %ld xlen %ld\n",(gint)shared_g,(gint)(shared_g->db),xlen);   
    //printf("\nto create for xlen %ld\n",xlen);
    yptr=wg_create_raw_record(shared_g->db,xlen); 
    //printf("\ncreated yptr %ld\n",(gint)yptr);
    if (!yptr) {
      printf("\ncould not wg_create_raw_record in wr_copy_record, exiting\n");
      return 0;
      //exit(0);
    }  
  }    
  if (yptr==NULL) {
    printf("\ncould not wg_create_raw_record v3 in wr_copy_record, exiting\n");
    return 0;
    //exit(0);
    //return WG_ILLEGAL;
  }  

  uselen=xlen+RECORD_HEADER_GINTS;
  if (toplevel!=2) {
    for(i=0;i<RECORD_HEADER_GINTS;i++) {    
      yptr[i]=xptr[i];
    }   
  }  
  for(i=RECORD_HEADER_GINTS;i<uselen;i++) {       
    /*
    printf("\n i %d uselen %d record ",i,uselen);
    printf("\nxptr[i] val %ld type %ld\n",(gint)(xptr[i]),(gint)(wg_get_encoded_type(db, xptr[i])));
    //if (toplevel!=2 && xptr[i] && isdatarec(xptr[i]))  wg_print_record(g->db,rotp(g,xptr[i]));
    printf(" end of record \n");
    */
    /*
    if (toplevel==1 && i==RECORD_HEADER_GINTS) {
      printf("\n i %d record ",i);
      wg_print_record(g->db,rotp(g,xptr[i]));
      printf(" end of record \n");
    }
    */
    //printf("\ni %ld",i);
    //printf("\nxptr[i] %ld",xptr[i]);

    // new part
    if (xptr[i]==(gint)NULL) {
      //tmp=wg_encode_null(g->db,xptr[i]);
      //tmp=xptr[i];
      tmp=(gint)0;
    } else {
      //printf(" val %ld ",(gint)(xptr[i]));
      type=wg_get_encoded_type(db, xptr[i]);
      //str1=wg_atomstr1(db,(void*)(xptr[i]));
      //str2=wg_atomstr2(db,atomptr);
      //str2=NULL;
      //printf("\ni %d type %d\n",i,type);
      if (type==WG_URITYPE) {
        str1=wg_decode_uri(db,xptr[i]);
        //printf("\nstr1 %s ptr %ld\n",str1,(gint)str1);
        //tmp=wg_encode_uri(shared_g->db,str1,NULL);
        tmp=wg_encode_uri(g->db,str1,NULL);
        //printf("\nstr2 %s ptr %ld\n",str2,(gint)str2);
        //printf("\ntmp %ld\n",tmp);      
      } else if (!isdatarec(xptr[i])) {      
        tmp=xptr[i];  
      } else if (toplevel==1 && i==RECORD_HEADER_GINTS) {      
        // xptr[i] is history
        tmp=wr_copy_record(g,shared_g,xptr[i],2);                      
        //tmp=wg_encode_int(db,10);
      } else {      
        //printf("\nxptr[i] val %ld\n",(gint)(xptr[i]));
        //if (toplevel!=2)  wg_print_record(g->db,rotp(g,xptr[i]));
        //CP6
        tmp=wr_copy_record(g,shared_g,xptr[i],0);          
      }
      if (!tmp) {
        /*
        printf("\ncould not wg_create_raw_record in wr_copy_record, place 2, exiting\n");
        printf("\ni %d value %ld toplevel %d\n",i,xptr[i],toplevel);
        exit(0);
        */
        return 0;
      }    
    }
    yptr[i]=tmp;
  }   
  //res=encode_record(shared_g->db,yptr); 
  res=encode_record(g->db,yptr); 
  /*
  printf("\ncopied to: ");
  wg_print_record(g->db,yptr);
  printf("\n");
  */
  return res;     
}  

#ifdef __cplusplus
}
#endif
