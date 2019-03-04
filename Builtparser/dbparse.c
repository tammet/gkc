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

 /** @file dbparse.c
 *  Top level procedures for parsers
 *
 */

/* ====== Includes =============== */



#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>


#include "../Db/dbdata.h"
#include "../Db/dbmem.h"
#include "../Db/dballoc.h"
#include "../Db/dbdata.h"
#include "../Db/dbmpool.h"
#include "../Printer/dbotterprint.h"
#include "../Reasoner/clterm.h"
#include "../Reasoner/history.h"
#include "dbparse.h"
#include "dbgenparse.h"
#include "dbotterparse.h"
#include "dbprologparse.h"
#include "dbclausify.h"

               
/* ====== Private headers and defs ======== */


#define MAX_URI_SCHEME 10
#define VARDATALEN 1000
#define MARK_IMPORTED_NAMES
#define IMPORTED_NAME_PREFIX "$imp::"

//#define DEBUG
//#undef DEBUG

#ifdef DEBUG
#define DPRINTF(...) { printf(__VA_ARGS__); }
#else
#define DPRINTF(...) ;
#endif


//static void otter_escaped_str(void *db, char *iptr, char *buf, int buflen);

static int show_parse_error(void* db, char* format, ...);
static int show_parse_warning(void* db, char* format, ...);


/* ======== Data ========================= */

/** Recognized URI schemes (used when parsing input data)
 * when adding new schemes, check that MAX_URI_SCHEME is enough to
 * store the entire scheme + '\0'
 */

struct uri_scheme_info {
  char *prefix;
  int length;
} uri_scheme_table_otter[] = {
  { "urn:", 4 },
  { "file:", 5 },
  { "http://", 7 },
  { "https://", 8 },
  { "mailto:", 7 },
  { NULL, 0 }
};


/* ====== Private protos ======== */

int wr_is_tptp_cnf_clause(void* db, void* cl);
int wr_is_tptp_fof_clause(void* db, void* cl);
int wr_is_tptp_import_clause(void* db, void* cl);


void* wr_preprocess_tptp_cnf_clause(glb* g, void* mpool, void* cl);
void* wr_preprocess_tptp_fof_clause(glb* g, void* mpool, void* cl);
void* wr_process_tptp_import_clause(glb* g, void* mpool, void* cl);



/* ====== Functions ============== */

int wr_import_otter_file(glb* g, char* filename, char* strasfile, cvec clvec, int isincluded) {
  void* db=g->db;
  parse_parm  pp;
  char* fnamestr;  
  FILE* fp __attribute__((unused));    
  //char* buf; 
  int pres=1;
  void* preprocessed=NULL;
  void* pres2=NULL;
  void *mpool;  
  int tmp_comp_funs;
  
#ifdef DEBUG
  DPRINTF("wr_import_otter_file called\n");
  printf("\n filename %s \n",filename);
  printf("\n strasfile %s \n",strasfile);
  printf("\n isincluded %d \n",isincluded);
  (g->print_initial_parser_result)=1;
  (g->print_generic_parser_result)=1;
#endif        
  // set globals for parsing
  (g->parse_is_included_file)=isincluded;
  (g->parse_skolem_prefix)=wr_str_new(g,100);
  strncpy((g->parse_skolem_prefix),DEFAULT_SKOLEM_PREFIX,99);
  //(g->parse_skolem_nr)=0;
  (g->parse_newpred_prefix)=wr_str_new(g,100);
  strncpy((g->parse_newpred_prefix),DEFAULT_NEWPRED_PREFIX,99);
  //(g->parse_newpred_nr)=0;
  (g->parse_errmsg)=NULL;
  (g->parse_errflag)=0;

  tmp_comp_funs=(g->use_comp_funs);
  (g->use_comp_funs)=0;
  
  if (strasfile==NULL) {  
    // input from file
    fnamestr=filename;
    //if (fnamestr && fnamestr[0]=='#') fnamestr=fnamestr+1;
#ifdef DEBUG
    printf("\nreading from file %s\n",fnamestr);
#endif       
    fp=freopen(fnamestr, "r", stdin);
    if (!fp) {
      //if (g->print_flag)
      printf("\nCould not open file %s.\n",fnamestr);
      return 1;
    }
    pp.db=db;
    pp.filename=fnamestr;
    pp.foo=NULL; // indicates file case in YY_INPUT in dbotter.l
    pp.result=NULL;
  } else {
    // input from string
    fnamestr="string";    
    pp.db=db;
    pp.filename=fnamestr;
    pp.foo="a"; // non-NULL indicates string case in YY_INPUT in dbotter.l    
    pp.buf = strasfile;
#ifdef DEBUG
    printf("\nreading from string %s\n",strasfile);
#endif    
    pp.length = strlen(strasfile);
    pp.pos = 0;
    pp.result=NULL;
  }   
  mpool=wg_create_mpool(db,PARSER_MEMPOOL_SIZE); 
  pp.mpool=mpool;  
#ifdef DEBUG  
  printf("otter input:%s\n",(char*)pp.buf);  
#endif  
  wg_yyotterlex_init(&pp.yyscanner);
  wg_yyotterset_extra(&pp, pp.yyscanner);
  pres=wg_yyotterparse(&pp, pp.yyscanner);   
  wg_yyotterlex_destroy(pp.yyscanner);     
  //printf("\nwg_yyotterparse finished\n");
  DPRINTF("result: %d pp.result %s\n",pres,(char*)pp.result);  
  if (!pres && pp.result!=NULL) { 
    if ((g->print_initial_parser_result)>0) {
      printf("\nOtter parser result:\n");    
      wg_mpool_print(db,pp.result);
    }        
    preprocessed=wr_preprocess_clauselist(g,mpool,clvec,pp.result,isincluded);
    //printf("\nwr_preprocess_clauselist finished\n");
    //preprocessed=wr_clausify_clauselist(g,mpool,clvec,pp.result);
    pres2=wr_parse_clauselist(g,mpool,clvec,preprocessed);
    //printf("\nwr_parse_clauselist finished\n");
  }
  //printf("\notterparse quitting.\n");
  /*
  if (pres2==NULL) {  
    DPRINTF("\npres2 is null.\n");  
  } else {
    //wg_mpool_print(db,pres2);
    if ((g->print_generic_parser_result)>0) {    
      printf("\nGeneric parser result:\n");
      wr_print_db_otter(g,(g->print_clause_detaillevel));
    }  
  } 
  */ 
  wg_free_mpool(db,mpool);
  //exit(0);

  (g->use_comp_funs)=tmp_comp_funs;
  if (pres || pres2==NULL) return 1;
  else return 0;  
}

/*
void wg_yyottererror (parse_parm* parm, void* scanner, char* msg) {
  printf("\n yyerror called with msg %s\n",msg);
  printf("\ input error at line %d token %s \n", yylineno,yytext);
  return;  
}  
*/

/*
void wg_yyottererror (const char *s) {
   char* errbuf;
   char* tmp;
  
   errbuf=malloc(1000);
   //(g->parser_errbuf)=errbuf;
   //snprintf(errbuf,1000,"input error at line %d: %s", wg_yyotterlineno, s);  
   sprintf(errbuf,1000,"input error at line %d: %s", wg_yyotterlineno, s);
   //tmp=xml_encode_str(errbuf);
   tmp=errbuf;
   //rqlgandalferr(-1,tmp);
   printf ("otterparse error at line %d: %s\n", wg_yyotterlineno, s);
   exit(0); 
   //printf ("parse error at line %d: %s\n", wg_yyotterlineno, s);
}

*/


int wr_import_prolog_file(glb* g, char* filename, char* strasfile, cvec clvec) {
  void *db=g->db;
  parse_parm  pp;
  char* fnamestr;  
  FILE* fp __attribute__((unused));    
  
  DPRINTF("Hello from dbprologparse!\n"); 

  fnamestr=filename;   
  fp=freopen(fnamestr, "r", stdin);
  pp.db=db;
  pp.filename=fnamestr;
  pp.foo="abcba";
  
  wg_yyprologlex_init(&pp.yyscanner);
  wg_yyprologset_extra(&pp, pp.yyscanner);
  wg_yyprologparse(&pp, pp.yyscanner);
         
  wg_yyprologlex_destroy(pp.yyscanner);
  DPRINTF("\nprologparse quitting.\n");
  
  return 0;  
}


/* ------- preprocess clauselist -------- */



void* wr_preprocess_clauselist
        (glb* g,void* mpool,cvec clvec,void* clauselist, int isincluded) {
  void* db=g->db;
  void* lpart;
  void *cl, *clname, *clrole;
  void* resultclause=NULL;
  void* resultlist=NULL;
  int clnr=0;
  char namebuf[1000];
#ifdef DEBUG  
  printf("wr_preprocess_clauselist starting with clauselist\n");  
  wg_mpool_print(db,clauselist);
  printf("\n");
#endif       
  // loop over clauses
  
  for(lpart=clauselist,clnr=0;wg_ispair(db,lpart);lpart=wg_rest(db,lpart),clnr++) {
    cl=wg_first(db,lpart);
#ifdef DEBUG    
    printf("\nclause nr %d:",clnr);    
    wg_mpool_print(db,cl); 
    printf("\n");   
#endif    
    /*
    if (!wg_ispair(db,cl)) {
      show_parse_warning(db,"clause nr %d is atomic and hence ignored: ",clnr);     
      continue;
    } 
    */    
    // process one clause
#ifdef DEBUG
    printf("\nin wr_preprocess_clauselist  starting to process clause nr %d:\n",clnr);
    wg_mpool_print(db,cl); 
    printf("\n");   
#endif
    if (wr_is_tptp_import_clause(db,cl)) {
      // tptp import clause 
      wr_process_tptp_import_clause(g,mpool,cl);
      resultclause=NULL;
    } else if (wr_is_tptp_cnf_clause(db,cl)) {
      // tptp cnf clause
      clname=wg_nth(db,cl,1);
      clrole=wg_nth(db,cl,2);
      resultclause=wr_preprocess_tptp_cnf_clause(g,mpool,cl);     
      resultclause=wg_mklist3(db,mpool,clname,clrole,resultclause);
    } else if (wr_is_tptp_fof_clause(db,cl)) {
      // tptp fof clause
      clname=wg_nth(db,cl,1);
#ifdef MARK_IMPORTED_NAMES      
      //printf("\n!!! clname %s\n",wg_atomstr1(db,clname));
      if (g->parse_is_included_file) {
        strncpy(namebuf,IMPORTED_NAME_PREFIX,900);
        strncat(namebuf,wg_atomstr1(db,clname),900);
        clname=wg_mkatom(db,mpool,WG_URITYPE,namebuf, NULL);
      }        
      //printf("\n!!! clnamenew %s\n",wg_atomstr1(db,clname));
#endif      
      clrole=wg_nth(db,cl,2);
      resultclause=wr_preprocess_tptp_fof_clause(g,mpool,cl); 
      resultclause=wg_mklist3(db,mpool,clname,clrole,resultclause);     
    } else { 
      // otter clause  

      //printf("in wr_preprocess_clauselist cl:\n");  
      //wg_mpool_print(db,cl); 
      //printf("\n");

      resultclause=cl;    
      resultclause=wg_mklist3(db,mpool,NULL,NULL,resultclause);
    }        
#ifdef DEBUG
    printf("\nin wr_preprocess_clauselist resultclause:\n");
    wg_mpool_print(db,resultclause); 
    printf("\n\n"); 
#endif
    if (resultclause!=NULL) {
      resultlist=wg_mkpair(db,mpool,resultclause,resultlist);  
    }  
  } // end clause list loop 
  //free(vardata); // if taken from mpool, not freed
#ifdef DEBUG
  printf("\nin wr_preprocess_clauselist resultlist:\n");
  wg_mpool_print(db,resultlist); 
  printf("\n\n");  
#endif
  return resultlist;
}  


// like:
// ((u:cnf u:clause_2 u:axiom (((u:r2least u:X1) (a:not (u:c u:X1))))))

int wr_is_tptp_cnf_clause(void* db, void* cl) {
  void* fun;

  if (wg_isatom(db,cl)) return 0;
  fun=wg_first(db,cl);
  if (!wg_islog_uri(db,fun,"cnf")) return 0;
  if (wg_list_len(db,cl)!=4) return 0;
  return 1;
}

int wr_is_tptp_fof_clause(void* db, void* cl) {
  void* fun;

  if (wg_isatom(db,cl)) return 0;
  fun=wg_first(db,cl);
  if (!wg_islog_uri(db,fun,"fof")) return 0;
  if (wg_list_len(db,cl)!=4) return 0;
  return 1;
}

// like: ((u:include u:Axioms/SWC001-0.ax))

int wr_is_tptp_import_clause(void* db, void* cl) {
  void* fun;

  if (wg_isatom(db,cl)) return 0;
  fun=wg_first(db,cl);
  if (!wg_islog_uri(db,fun,"include")) return 0;
  if (wg_list_len(db,cl)!=2) return 0;
  return 1;
}

void* wr_preprocess_tptp_cnf_clause(glb* g, void* mpool, void* cl) {
  void* db=g->db;
  void* clpart;
  void* res;
  
  clpart=wg_first(db,wg_rest(db,wg_rest(db,wg_rest(db,cl))));
  res=clpart;
  /*
  if (wg_ispair(db,wg_first(db,clpart))) {
    res=clpart;
  } else {
    res=clpart;
    //res=wg_mkpair(db,mpool,clpart,NULL);
  }
  */
  return res;
}

void* wr_preprocess_tptp_fof_clause(glb* g, void* mpool, void* cl) {
  void* db=g->db;
  void* clpart;
  void* res;
  void* tmp;
  void* cltype;


#ifdef DEBUG
  printf("in wr_preprocess_tptp_fof_clause given cl:\n");  
  wg_mpool_print(db,cl); 
  printf("\n");
#endif   
  cltype=wg_first(db,wg_rest(db,wg_rest(db,cl)));
  clpart=wg_first(db,wg_rest(db,wg_rest(db,wg_rest(db,cl))));
#ifdef DEBUG
  printf("in wr_preprocess_tptp_fof_clause cltype:\n");  
  wg_mpool_print(db,cltype); 
  printf("\n");
#endif 
#ifdef DEBUG
  printf("in wr_preprocess_tptp_fof_clause clpart:\n");  
  wg_mpool_print(db,clpart); 
  printf("\n");
#endif 
  if (!strcmp(wg_atomstr1(db,cltype),"conjecture")) {
    // must be negated
    // clpart=wg_mkpair(db,mpool,wg_makelogneg(db,mpool),clpart); // old
    clpart=wg_mklist2(db,mpool,wg_makelogneg(db,mpool),clpart);
  }
#ifdef DEBUG
  printf("in wr_preprocess_tptp_fof_clause clpart:\n");  
  wg_mpool_print(db,clpart); 
  printf("\n");
#endif
  tmp=wr_clausify_formula(g,mpool,clpart);
#ifdef DEBUG
  printf("in wr_preprocess_tptp_fof_clause wr_clausify_formula resulting with frm\n");  
  wg_mpool_print(db,tmp); 
  printf("\n");
#endif
  res=tmp;

  /*
  if (wg_ispair(db,wg_first(db,clpart))) {
    res=clpart;
  } else {
    res=clpart;
    //res=wg_mkpair(db,mpool,clpart,NULL);
  }
  */
  return res;
}

void* wr_process_tptp_import_clause(glb* g, void* mpool, void* cl) {
  void* db=g->db;
  void* pathatom;
  char* str;
  char* filename=NULL;
  FILE* fp;
  int bytes;
  char* axiomfolder="/opt/TPTP/";
  //int tmp;

#ifdef DEBUG
  printf("\nwr_process_tptp_import_clause starts with\n");
  wg_mpool_print(db,cl); 
  printf("\n");
#endif

  pathatom=wg_nth(db,cl,1);
#ifdef DEBUG
  printf("\npathatom\n");
  wg_mpool_print(db,pathatom); 
  printf("\nwg_atomtype(db,pathatom) %d WG_URITYPE %d\n",wg_atomtype(db,pathatom),WG_URITYPE);
  printf("\n");
#endif  
  if (wg_atomtype(db,pathatom)!=WG_URITYPE) return 0;  
  str=wg_atomstr1(db,pathatom); 
  if (str && str[0]=='#') str=str+1;
#ifdef DEBUG
  printf("\nstr %s\n",str);
#endif
  //printf("\nfirst import path str: %s\n",str);
  fp=fopen(str,"r");
  if (fp!=NULL) {
    // succeeded to open axiom file: use that
    fclose(fp);
    filename=str;
  } else {
    // failed to open axiom file: try axiom folder
    bytes=strlen(str)+strlen(axiomfolder)+10;
    filename=wg_alloc_mpool(db,mpool,bytes);
#ifdef DEBUG
    printf("\nalloced filename %s\n",filename);
#endif    
    if (!filename) {
      show_parse_error(db,"cannot allocate filename in wr_process_tptp_import_clause\n");
      return NULL;
    }
    strcpy(filename,axiomfolder);
    strcat(filename,str);
    //printf("\nsecond import path str: %s\n",filename);
  }
  //tmp=
#ifdef DEBUG
  printf("\nfilename %s\n",filename);
#endif   
  wr_import_otter_file(g,filename,NULL,NULL,1);  
  return NULL;
}


/* ---- convert parser-returned list to db records --------- */

/*

 a clause is a list of three els: name, role, clause
 where name and role could be NULL

*/


void* wr_parse_clauselist(glb* g,void* mpool,cvec clvec,void* clauselist) {
  void* db=g->db;
  void* lpart;
  void* cl;
  int clnr=0;
  void *clause, *name, *role;
  void* record=NULL;
  void* resultlist=NULL;
  char** vardata;
  int i;
  void* propfun;
  void *frm, *flt, *fltpart;
  
#ifdef DEBUG  
  DPRINTF("wr_parse_clauselist starting with clauselist\n");  
  wg_mpool_print(db,clauselist); 
  DPRINTF("\n");
#endif
   
  if (clvec!=NULL) CVEC_NEXT(clvec)=CVEC_START; 
  // create vardata block by malloc or inside mpool

  vardata=(char**)(wg_alloc_mpool(db,mpool,sizeof(char*)*VARDATALEN));
  if (vardata==NULL) {
    show_parse_error(db,"cannot allocate vardata in wg_parse_clauselist\n");
    return NULL;
  }  
  //vardata=(char**)(malloc(sizeof(char*)*VARDATALEN));
  for(i=0;i<VARDATALEN;i++) vardata[i]=NULL; // pointers to varstrings  
  propfun=wg_mkatom(db,mpool,WG_URITYPE,PROPVAR_PREDSYMB,NULL);
  // loop over clauses  
  for(lpart=clauselist,clnr=0;wg_ispair(db,lpart);lpart=wg_rest(db,lpart),clnr++) {
    //if (clnr%10000==0) printf("clnr %d\n",clnr);
    clause=wg_first(db,lpart);  
    if (!wg_ispair(db,clause)) {
      name=NULL;
      role=NULL;
      frm=clause;
    } else {
      if (wg_list_len(db,clause)!=3) {
        show_parse_error(db,"clause in wg_parse_clauselist not a triple\n");
        return NULL;
      }
      name=wg_nth(db,clause,0);
      role=wg_nth(db,clause,1);
      frm=wg_nth(db,clause,2);     
    }
#ifdef DEBUG     
    printf("\nin wr_parse_clauselist frm nr %d:\n",clnr);    
    wg_mpool_print(db,frm); 
    printf("\n");
#endif
    flt=wr_flatten_logclause(g,mpool,frm);
#ifdef DEBUG 
    printf("\nin wr_parse_clauselist got flt nr %d:\n",clnr);    
    wg_mpool_print(db,flt); 
    printf("\n");
#endif
    for(fltpart=flt; fltpart!=NULL; fltpart=wg_rest(db,fltpart)) {
      cl=wg_first( db,fltpart);

      //printf("\nin wr_parse_clauselist inner cl nr %d:\n",clnr);    
      //wg_mpool_print(db,cl); 
      //printf("\n");
      record=wr_parse_clause(g,mpool,cl,clvec,vardata,propfun,name,role);    
      if (!record) continue; // parsing failed if NULL    
      resultlist=wg_mkpair(db,mpool,record,resultlist); // not really needed!!
      if (!resultlist) {
        show_parse_error(db,"\nfailed to add a clause to resultlist\n");
        return NULL;
      }
      // if clvec present, store record to clvec, given enough space
      if (clvec!=NULL) {
        if ((clvec[1]+1)<clvec[0]) {
          clvec[clvec[1]]=(gint)record; 
          ++(clvec[1]);
        } 
      }    
    }
  } // end clause list loop 
  DPRINTF("\nwr_parse_clauselist ending\n");
  //free(vardata); // if taken from mpool, not freed
  return resultlist;
}  

void* wr_parse_clause(glb* g,void* mpool,void* cl,cvec clvec,
        char** vardata,void* propfun, void* name, void* role) {
  void* db=g->db;
  void *clpart, *lit, *atom;
  int clnr=0;
  int litnr=0;
  int isneg=0;
  void *fun, *tmpptr, *atomres;
  gint ameta, tmpres2, setres, setres2, history;
  void* record=NULL;
  int issimple, i;
  void *termpart, *subterm;
  char *namestr, *rolestr;
  int rolenr;
  void* resultlist=NULL;

#ifdef DEBUG    
  printf("\n++ in wr_parse_clause clause is: %d\n",clnr);    
  wg_mpool_print(db,cl); 
  printf("\n");
#endif    
  if (!wg_ispair(db,cl)) {    
    //show_parse_warning(db,"clause nr %d is atomic: ",clnr); 
    cl=wg_mklist1(db,mpool,cl);
#ifdef DEBUG      
    wg_mpool_print(db,cl); 
    printf("\n");
#endif      
    //return NULL;
  }  
  
  // examine clause: find clause length and check if simple clause    

  issimple=1;
  litnr=0;
  for(clpart=cl;wg_ispair(db,clpart);clpart=wg_rest(db,clpart),litnr++) {
    lit=wg_first(db,clpart); 
#ifdef DEBUG      
    DPRINTF("lit: ");
    wg_mpool_print(db,lit); 
    printf("\n");      
#endif      
    if (!wg_ispair(db,lit)) { issimple=0; continue; }
    fun=wg_first(db,lit);
    if (wg_atomtype(db,fun)==WG_ANONCONSTTYPE && !strcmp(wg_atomstr1(db,fun),"not")) { issimple=0; continue; }
    for(termpart=lit;wg_ispair(db,termpart);termpart=wg_rest(db,termpart)) {
      subterm=wg_first(db,termpart);
#ifdef DEBUG        
      DPRINTF("subterm: ");
      wg_mpool_print(db,subterm); 
      printf("\n");
#endif        
      if (subterm!=NULL && wg_ispair(db,subterm)) { issimple=0; break; }     
      if (wg_atomtype(db,subterm)==WG_VARTYPE) { issimple=0; break; } 
      if (wg_atomtype(db,subterm)==WG_URITYPE &&  
          ( (g->parse_caps_as_var && isupper(wg_atomstr1(db,subterm)[0])) ||
            (g->parse_question_as_var && wg_atomstr1(db,subterm)[0]=='?') )) {issimple=0; break;} 
    }      
  }        
  if (litnr>1) issimple=0;
  DPRINTF("\nclause issimple res %d length %d\n",issimple,litnr);    
  // create record for a rule clause
  if (!issimple) {
    record=wr_create_rule_clause(g,litnr);   
    if (((gint)record)==0) {
      //free(vardata);
      return NULL;
    }
    resultlist=wg_mkpair(db,mpool,record,resultlist);       
  }  
  // clear vardata block for the next clause
  for(i=0;i<VARDATALEN;i++) {
    if (vardata[i]==NULL) break;
    vardata[i]=NULL;       
  }      
  // process one clause
  for(clpart=cl,litnr=0;wg_ispair(db,clpart);clpart=wg_rest(db,clpart),litnr++) {      
    lit=wg_first(db,clpart);
#ifdef DEBUG    
    DPRINTF("\nlit nr  %d:",litnr);    
    wg_mpool_print(db,lit); 
    printf("\n");
#endif      
    if (!wg_ispair(db,lit)) {        
      lit=wg_mkpair(db,mpool,propfun,wg_mkpair(db,mpool,lit,NULL));
      if (!lit) {
         show_parse_warning(db,"failed to make a literal"); 
         return NULL;
      }
      //show_parse_warning(db,"lit nr %d in clause nr %d is atomic and hence prefixed: ",litnr,clnr); 
      //wg_mpool_print(db,lit);
      //printf("\n");
#ifdef DEBUG        
      wg_mpool_print(db,lit); 
      printf("\n");
#endif        
      //continue;
    }               
    fun=wg_first(db,lit);
    if (!wg_isatom(db,fun)) {
      show_parse_warning(db,"lit nr %d in clause nr %d has nonatomic leadfun and hence ignored: ",litnr,clnr); 
#ifdef DEBUG        
      wg_mpool_print(db,fun); 
      printf("\n");
#endif        
      continue;
    }     
    isneg=0;
    if (wg_atomtype(db,fun)==WG_ANONCONSTTYPE && 
        !strcmp(wg_atomstr1(db,fun),"not") &&
        wg_atomstr2(db,fun)==NULL) {
      DPRINTF("detected negation");
      isneg=1;            
      tmpptr=wg_rest(db,lit);    
      if (!wg_ispair(db,tmpptr)) {          
        show_parse_warning(db,"lit nr %d in clause nr %d does not contain proper atom after negation and hence ignored: ",litnr,clnr); 
#ifdef DEBUG          
        wg_mpool_print(db,lit); 
        printf("\n");
#endif          
        //continue;
      } 
      atom=wg_first(db,tmpptr);
      if (!wg_ispair(db,atom)) {
        atom=wg_mkpair(db,mpool,propfun,wg_mkpair(db,mpool,atom,NULL));
        if (!atom) {
          show_parse_warning(db,"failed to make an atom"); 
          return NULL;
        }        
        //show_parse_warning(db,"lit nr %d in clause nr %d was atomic and is prefixed\n: ",litnr,clnr); 
#ifdef DEBUG          
        wg_mpool_print(db,lit); 
        printf("\n");
#endif          
        //continue;
      }
      fun=wg_first(db,atom);
      if (!wg_isatom(db,fun)) {
        show_parse_warning(db,"lit nr %d in clause nr %d has nonatomic leadfun and hence ignored: ",litnr,clnr); 
#ifdef DEBUG          
        wg_mpool_print(db,fun); 
        printf("\n");
#endif          
        continue;
      }         
    } else {
      atom=lit;
    }      
#ifdef DEBUG      
    DPRINTF("atom isneg %d: ",isneg);
    wg_mpool_print(db,atom);
#endif      
    
    // parse an atom in the clause
    atomres=wr_parse_atom(g,mpool,atom,isneg,issimple,vardata); 
    if (atomres==NULL) {
      show_parse_error(db,"problem converting an atom to record");
      //free(vardata);
      return NULL;        
    }
    if (issimple) {
      wr_convert_atom_fact_clause(g,atomres,isneg);
      resultlist=wg_mkpair(db,mpool,atomres,resultlist);
      if (!resultlist) {
        show_parse_warning(db,"failed to extend resultlist"); 
        return NULL;          
      }       
      record=atomres; // for storage of record in clvec
      break;        
    } else {     
      ameta=0;        
      if (isneg) ameta=(ameta | ATOM_META_NEG);      
      setres=wr_set_rule_clause_atom_meta(g,record,litnr,ameta);                
      tmpres2=wg_encode_record(db,atomres);
      setres2=wr_set_rule_clause_atom(g,record,litnr,tmpres2);
      if (setres!=0 || setres2!=0) {
        wg_delete_record(db,atomres);
        free(vardata);
        return NULL; 
      }   
    }        
          
  } // end one clause processing loop
  // set history 

  if (name && wg_isatom(db,name) && wg_atomtype(db,name)==WG_URITYPE) {
    namestr=wg_atomstr1(db,name);
    if (!(namestr[0])) namestr=NULL;    
  } else {
    namestr=NULL;
  }  
  if (role && wg_isatom(db,role) && wg_atomtype(db,role)==WG_URITYPE) {
    rolestr=wg_atomstr1(db,role);    
    /*
    printf("\n rolestr %s \n",rolestr);
    wr_print_clause(g,record);
    printf("\n is positive %d\n",wr_is_positive_unit_cl(g,record));
    */
    
    if (!strcmp("conjecture",rolestr) || !strcmp("negated_conjecture",rolestr)) {       
      rolenr=PARSER_GOAL_ROLENR;          
    } else if (!strcmp("hypothesis",rolestr) || !strcmp("assumption",rolestr)) {     
      rolenr=PARSER_ASSUMPTION_ROLENR;  
    } else if (!strcmp("axiom",rolestr) && 
             //  (g->parse_is_included_file) &&
             namestr!=NULL &&
             !strncmp(namestr,IMPORTED_NAME_PREFIX,strlen(IMPORTED_NAME_PREFIX)) ) {  
      rolenr=PARSER_EXTAXIOM_ROLENR;   
    } else {
      rolenr=PARSER_AXIOM_ROLENR; 
    }  
    //printf("\n rolenr %d\n",rolenr);

    /* old stuff, delete later    

    if (!strcmp("conjecture",rolestr) || !strcmp("negated_conjecture",rolestr)) {
      if (((g->cl_pick_queue_strategy)==2)
          && wr_is_positive_unit_cl(g,record)) rolenr=PARSER_ASSUMPTION_ROLENR;   
      else if (((g->cl_pick_queue_strategy)==3)
          && !wr_is_fully_negative_cl(g,record)) rolenr=PARSER_AXIOM_ROLENR;     
      else rolenr=PARSER_GOAL_ROLENR;          
    } else if (!strcmp("hypothesis",rolestr) || !strcmp("assumption",rolestr)) {
      if ((g->cl_pick_queue_strategy)==3)
        rolenr=PARSER_AXIOM_ROLENR;  
      else
        rolenr=PARSER_ASSUMPTION_ROLENR;  
    }     
    else if ((g->cl_pick_queue_strategy)==2 &&
             !strcmp("axiom",rolestr) && 
             (g->parse_is_included_file) &&
             namestr!=NULL &&
             strncmp(namestr,IMPORTED_NAME_PREFIX,strlen(IMPORTED_NAME_PREFIX)) ) {  
      rolenr=PARSER_ASSUMPTION_ROLENR; 
      //printf("\n !!!! detected nonimported axiom, made assumption for %s\n",namestr);
    } else if ((g->cl_pick_queue_strategy)==2 &&
             !strcmp("axiom",rolestr) && 
             (g->parse_is_included_file) &&
             namestr!=NULL &&
             strncmp(namestr,IMPORTED_NAME_PREFIX,strlen(IMPORTED_NAME_PREFIX)) ) {  
      rolenr=PARSER_EXTAXIOM_ROLENR; 
      //printf("\n !!!! detected nonimported axiom, made assumption for %s\n",namestr);
    } else rolenr=PARSER_AXIOM_ROLENR;

    */

  } else {
    rolenr=PARSER_AXIOM_ROLENR;
  }    
  history=wr_build_input_history(g,record,namestr,rolenr);
  if (!history) {
    //show_parse_warning(db,"failed to make history"); 
    //return NULL;          
  }   
  wr_set_history(g,record,history);
  /*
  printf("\n built a record with rolenr: %d and (g->cl_pick_queue_strategy) %d\n",rolenr,(g->cl_pick_queue_strategy));
  wg_print_record(db,record);
  printf("\n");
  */
 
#ifdef DEBUG
  printf("\n built a record:\n");
  wg_print_record(db,record);
  printf("\n");
#endif
  return record;
} 
    


void* wr_parse_atom(glb* g,void* mpool,void* term, int isneg, int issimple, char** vardata) {
  void* db=g->db;
  void* termpart; 
  void* ret;
  void* subterm;
  int termnr=0;
  int deeptcount=0;
  int vartcount=0;
  void* tmpres=NULL;
  gint tmpres2;
  gint setres;
  void* record;
  
  DPRINTF("\nwg_parse_atom starting with isneg %d atom\n",isneg);
#ifdef DEBUG  
  wg_mpool_print(db,term); 
  printf("\n");
#endif  
  // examine term
  
  termnr=0;
  deeptcount=0;
  vartcount=0;
  for(termpart=term;wg_ispair(db,termpart);termpart=wg_rest(db,termpart),termnr++) {
    subterm=wg_first(db,termpart);
    if (subterm!=NULL && wg_ispair(db,subterm)) deeptcount++;      
    else if (wg_atomtype(db,subterm)==WG_VARTYPE) vartcount++;
  }  
  // create data record
 
  record=wr_create_atom(g,termnr); 
  if (((gint)record)==0) {
    return NULL;
  }  
  // fill data record and do recursive calls for subterms
  for(termpart=term,termnr=0;wg_ispair(db,termpart);termpart=wg_rest(db,termpart),termnr++) {
    term=wg_first(db,termpart);
    
#ifdef DEBUG    
    DPRINTF("\nterm nr  %d:",termnr);    
    wg_mpool_print(db,term); 
    printf("\n");
#endif    
    if (!wg_ispair(db,term)) {
      DPRINTF("term nr %d is primitive \n",termnr); 
      tmpres2=wr_parse_primitive(g,mpool,term,vardata,termnr);    
      if (!tmpres2 || tmpres2==WG_ILLEGAL) {
        wg_delete_record(db,record);
        return NULL;
      }  
    } else {
      DPRINTF("term nr %d is nonprimitive \n",termnr);
      tmpres=wr_parse_term(g,mpool,term,vardata);
      if (tmpres==NULL) {
        wg_delete_record(db,record);
        return NULL;
      } 
      tmpres2=wg_encode_record(db,tmpres);       
    }     
    if (!tmpres2 || tmpres2==WG_ILLEGAL) return NULL;
    setres=wr_set_atom_subterm(g,record,termnr,tmpres2);
    if (setres!=0) {
      wg_delete_record(db,record);
      return NULL; 
    }  
  }    
  ret=record;
  DPRINTF("\nwg_parse_atom ending\n");
  if (ret==NULL) DPRINTF("\nwg_parse_atom returns NULL\n");
  return ret;
}



void* wr_parse_term(glb* g,void* mpool,void* term, char** vardata) {
  void* db=g->db;
  void* termpart; 
  void* ret;
  void* subterm;
  int termnr=0;
  int deeptcount=0;
  int vartcount=0;
  void* tmpres=NULL;
  gint tmpres2;
  gint setres;
  void* record;

#ifdef DEBUG  
  DPRINTF("\nwg_parse_term starting with ");
  wg_mpool_print(db,term); 
  printf("\n");
#endif
  
  // examine term
  
  termnr=0;
  deeptcount=0;
  vartcount=0;
  for(termpart=term;wg_ispair(db,termpart);termpart=wg_rest(db,termpart),termnr++) {
    subterm=wg_first(db,termpart);
    if (subterm!=NULL && wg_ispair(db,subterm)) deeptcount++;      
    else if (wg_atomtype(db,subterm)==WG_VARTYPE) vartcount++;
  }  
  
  // create data record
  record=wr_create_term(g,termnr);   
  if (((gint)record)==0) {
    return NULL;
  } 
  //DPRINTF("\nwg_parse_term termnr %d \n",termnr);

  // fill data record and do recursive calls for subterms
  
  for(termpart=term,termnr=0;wg_ispair(db,termpart);termpart=wg_rest(db,termpart),termnr++) {
    term=wg_first(db,termpart);
#ifdef DEBUG    
    //DPRINTF("\nterm nr  %d:",termnr);    
    //wg_mpool_print(db,term); 
    //printf("\n");
#endif    
    if (!wg_ispair(db,term)) {
      DPRINTF("term nr %d is primitive \n",termnr); 
      tmpres2=wr_parse_primitive(g,mpool,term,vardata,termnr);    
      if (tmpres2==WG_ILLEGAL) { 
        wg_delete_record(db,record);
        return NULL;
      }  
    } else {     
      DPRINTF("term nr %d is nonprimitive \n",termnr);
      tmpres=wr_parse_term(g,mpool,term,vardata);
      if (tmpres==NULL) {
        wg_delete_record(db,record);
        return NULL;
      } 
      tmpres2=wg_encode_record(db,tmpres);  
    }       
    if (tmpres2==WG_ILLEGAL) return NULL;
    setres=wr_set_term_subterm(g,record,termnr,tmpres2);
    if (setres!=0) {
      wg_delete_record(db,record);
      return NULL; 
    }  
  }
    
  ret=record;
  DPRINTF("\nwg_parse_term ending \n");  
  if (ret==NULL) DPRINTF("\nwg_parse_term returns NULL\n");
  return ret;  
}

gint wr_parse_primitive(glb* g,void* mpool,void* atomptr, char** vardata, int pos) {
  void *db=g->db;
  gint ret; 
  int type;
  char* str1;
  char* str2;
  int intdata;
  double doubledata;
  int i;

#ifdef DEBUG  
  DPRINTF("\nwg_parse_primitive starting with ");
  wg_mpool_print(db,atomptr); 
  printf("\n");  
#endif  
  
  if (atomptr==NULL) {
    ret=wg_encode_null(db,NULL);
  } else {    
    type=wg_atomtype(db,atomptr);
    str1=wg_atomstr1(db,atomptr);
    //str2=wg_atomstr2(db,atomptr);
    str2=NULL;

    #ifdef DEBUG  
      DPRINTF("\nstr1 %s\n",str1);
      DPRINTF("\nstr2 %s\n",str2);
      DPRINTF("\ntype %d\n",type);
    #endif  

    switch (type) {
      case 0: ret=wg_encode_null(db,NULL); break; 
      case WG_NULLTYPE: ret=wg_encode_null(db,NULL); break;      
      case WG_INTTYPE: 
        intdata = atol(str1);
        if(errno!=ERANGE && errno!=EINVAL) {
          ret = wg_encode_int(db, intdata);
        } else {
          errno=0;
          ret=WG_ILLEGAL;         
        }
        break;         
      case WG_DOUBLETYPE: 
        doubledata = atof(str1);
        if(errno!=ERANGE && errno!=EINVAL) {
          ret = wg_encode_double(db, doubledata);
        } else {
          errno=0;
          ret=WG_ILLEGAL;         
        }
        break; 
      case WG_STRTYPE: 
        ret=wg_encode_str(db,str1,str2); 
        break; 
      case WG_XMLLITERALTYPE: 
        ret=wg_encode_xmlliteral(db,str1,str2); 
        break; 
      case WG_URITYPE: 
        if (!pos || !str1 || str2) {
          ret=wg_encode_uri(db,str1,str2); 
        } else if ((g->parse_caps_as_var && isupper(str1[0])) ||
                   (g->parse_question_as_var && str1[0]=='?') ) {
          intdata=0;
          DPRINTF("starting WG_VARTYPE block\n");
          for(i=0;i<VARDATALEN;i++) {
            if (vardata[i]==NULL) {
              DPRINTF("no more vars to check\n");
              vardata[i]=str1;
              intdata=i;
              break;
            }  
            if (strcmp(vardata[i],str1)==0) {
              DPRINTF("found matching var\n");
              intdata=i;
              break;
            }            
          }  
          if (i>=VARDATALEN) {
            show_parse_warning(db,"too many variables in a clause: ignoring the clause");        
            errno=0;
            ret=WG_ILLEGAL;
            break;          
          }                   
          ret=wg_encode_var(db,intdata);
          DPRINTF("var %d encoded ok\n",intdata);
        } else {
          ret=wg_encode_uri(db,str1,str2);
        }
        break; 
      //case WG_BLOBTYPE: 
      //  ret=wg_encode_blob(db,str1,str2); 
      //  break; 
      case WG_CHARTYPE: 
        ret=wg_encode_char(db,*str1); 
        break; 
      case WG_FIXPOINTTYPE:
        doubledata = atof(str1);
        if(errno!=ERANGE && errno!=EINVAL) {
          ret = wg_encode_fixpoint(db, doubledata);
        } else {
          errno=0;
          ret=WG_ILLEGAL;         
        }        
        break; 
      case WG_DATETYPE: 
        intdata=wg_strp_iso_date(db,str1);
        ret=wg_encode_date(db,intdata); 
        break; 
      case WG_TIMETYPE: 
        intdata=wg_strp_iso_time(db,str1); 
        ret=wg_encode_time(db,intdata);
        break; 
      case WG_ANONCONSTTYPE: 
        ret=wg_encode_anonconst(db,str1); 
        break; 
      case WG_VARTYPE:
        intdata=0;
        DPRINTF("starting WG_VARTYPE block\n");
        for(i=0;i<VARDATALEN;i++) {
          if (vardata[i]==NULL) {
            DPRINTF("no more vars to check\n");
            vardata[i]=str1;
            intdata=i;
            break;
          }  
          if (strcmp(vardata[i],str1)==0) {
            DPRINTF("found matching var\n");
            intdata=i;
            break;
          }            
        }  
        if (i>=VARDATALEN) {
          show_parse_warning(db,"too many variables in a clause: ignoring the clause");        
          errno=0;
          ret=WG_ILLEGAL;
          break;          
        }                   
        ret=wg_encode_var(db,intdata);
        DPRINTF("var %d encoded ok\n",intdata);       
        break; 
      default: 
        ret=wg_encode_null(db,NULL);
    }      
  }      
  DPRINTF("\nwg_parse_term ending with %d\n",(int)ret);
  return ret;
}

/* -------------- parsing utilities ----------------------- */





/** Parse value from string, encode it for WhiteDB
 *  returns WG_ILLEGAL if value could not be parsed or
 *  encoded.
 *  Supports following data types:
 *  NULL - empty string
 *  variable - ?x where x is a numeric character
 *  int - plain integer
 *  double - floating point number in fixed decimal notation
 *  date - ISO8601 date
 *  time - ISO8601 time+fractions of second.
 *  uri - string starting with an URI prefix
 *  string - other strings
 *  Since leading whitespace generally makes type guesses fail,
 *  it invariably causes the data to be parsed as string.
 */
 
gint wr_parse_and_encode_otter_prim(glb* g, char *buf) {
  void* db=g->db;
  int intdata;
  double doubledata;
  gint encoded = WG_ILLEGAL, res;
  char c = buf[0];

  if(c == 0) {
    /* empty fields become NULL-s */
    encoded = 0;
  }
  else if(c == '?' && buf[1] >= '0' && buf[1] <= '9') {
    /* try a variable */
    intdata = atol(buf+1);
    if(errno!=ERANGE && errno!=EINVAL) {
      encoded = wg_encode_var(db, intdata);
    } else {
      errno = 0;
    }
  }
  else if(c >= '0' && c <= '9') {
    /* This could be one of int, double, date or time */
    if((res = wg_strp_iso_date(db, buf)) >= 0) {
      encoded = wg_encode_date(db, res);
    } else if((res = wg_strp_iso_time(db, buf)) >= 0) {
      encoded = wg_encode_time(db, res);
    } else {
      /* Examine the field contents to distinguish between float
       * and int, then convert using atol()/atof(). sscanf() tends to
       * be too optimistic about the conversion, especially under Win32.
       */
      char *ptr = buf, *decptr = NULL;
      int decsep = 0;
      while(*ptr) {
        if(*ptr == OTTER_DECIMAL_SEPARATOR) {
          decsep++;
          decptr = ptr;
        }
        else if(*ptr < '0' || *ptr > '9') {
          /* Non-numeric. Mark this as an invalid number
           * by abusing the decimal separator count.
           */
          decsep = 2;
          break;
        }
        ptr++;
      }

      if(decsep==1) {
        char tmp = *decptr;
        *decptr = '.'; /* ignore locale, force conversion by plain atof() */
        doubledata = atof(buf);
        if(errno!=ERANGE && errno!=EINVAL) {
          encoded = wg_encode_double(db, doubledata);
        } else {
          errno = 0; /* Under Win32, successful calls don't do this? */
        }
        *decptr = tmp; /* conversion might have failed, restore string */
      } else if(!decsep) {
        intdata = atol(buf);
        if(errno!=ERANGE && errno!=EINVAL) {
          encoded = wg_encode_int(db, intdata);
        } else {
          errno = 0;
        }
      }
    }
  }
  else {
    /* Check for uri scheme */
    encoded = wr_parse_and_encode_otter_uri(g, buf);
  }
  
  if(encoded == WG_ILLEGAL) {
    /* All else failed. Try regular string. */
    encoded = wg_encode_str(db, buf, NULL);
  }
  return encoded;
}



/** Try parsing an URI from a string.
 *  Returns encoded WG_URITYPE field when successful
 *  Returns WG_ILLEGAL on error
 *
 *  XXX: this is a very naive implementation. Something more robust
 *  is needed.
 */
gint wr_parse_and_encode_otter_uri(glb* g, char *buf) {
  void* db=g->db;
  gint encoded = WG_ILLEGAL;
  struct uri_scheme_info *next = uri_scheme_table_otter;

  /* Try matching to a known scheme */
  while(next->prefix) {
    if(!strncmp(buf, next->prefix, next->length)) {
      /* We have a matching URI scheme.
       * XXX: check this code for correct handling of prefix. */
      int urilen = strlen(buf);
      char *prefix = malloc(urilen + 1);
      char *dataptr;

      if(!prefix)
        break;
      strncpy(prefix, buf, urilen);

      dataptr = prefix + urilen;
      while(--dataptr >= prefix) {
        switch(*dataptr) {
          case ':':
          case '/':
          case '#':
            *(dataptr+1) = '\0';
            goto prefix_marked;
          default:
            break;
        }
      }
prefix_marked:
      printf("for wg_encode_uri |%s|%s|\n",buf+((gint)dataptr-(gint)prefix+1),prefix);
      encoded = wg_encode_uri(db, buf+((gint)dataptr-(gint)prefix+1), prefix);
      free(prefix);
      break;
    }
    next++;
  }
  return encoded;
}


/* ------------ errors ---------------- */


static int show_parse_error(void* db, char* format, ...) {
  va_list args;
  va_start (args, format);
  printf("*** Parser error: ");
  vprintf (format, args);
  va_end (args);
  return -1;
}

static int show_parse_warning(void* db, char* format, ...) {
  va_list args;
  va_start (args, format);
  printf("*** Parser warning: ");
  vprintf (format, args);
  va_end (args);
  return -1;
}
