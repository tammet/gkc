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
#define IMPORTED_NAME_PREFIX "$inc_"

//#define DEBUG
//#undef DEBUG

//#define IDEBUG

/*
#ifdef DEBUG
#define DPRINTF(...) { printf(__VA_ARGS__); }
#else
#define DPRINTF(...) ;
#endif
*/

#define ERRPRINT

#define DPRINTF(...) 

#define json_get(obj,name) cJSON_GetObjectItemCaseSensitive(obj,name)
#define json_isstring(name) cJSON_IsString(name)
#define json_isnumber(name) cJSON_IsNumber(name)
#define json_isarray(name) cJSON_IsArray(name)
#define json_isobject(name) cJSON_IsObject(name)
#define json_key(obj) ((obj)->string)
#define json_valuestring(obj) ((obj)->valuestring)
#define json_valueint(obj) ((obj)->valueint)

//static void otter_escaped_str(void *db, char *iptr, char *buf, int buflen);

//static int wr_show_parse_error(glb* g, char* format, ...);
//static int show_parse_error(void* db, char* format, ...);
//static int show_parse_warning(void* db, char* format, ...);


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

cJSON* wr_parse_json_infile(char* filename,char** guidebuf);
cJSON* wr_parse_json_infile_str(char* buf);
int wr_parse_injson_data(glb* g, cJSON* jdata);
void* wr_parse_injson_clause(glb* g, cJSON* clause);
void* wr_parse_injson_term(glb* g, cJSON* term);

int wr_is_tptp_cnf_clause(void* db, void* cl);
int wr_is_tptp_fof_clause(void* db, void* cl);
int wr_is_tptp_import_clause(void* db, void* cl);

void *wr_parse_freeoccs(glb* g, void* mpool, void* vars, void* frm, int* clauseflag);

//void* wr_preprocess_tptp_cnf_clause(glb* g, void* mpool, void* cl);
//void* wr_preprocess_tptp_fof_clause(glb* g, void* mpool, void* cl, void* clname);
void* wr_process_tptp_import_clause(glb* g, void* mpool, void* cl);

void db_err_printf2(char* s1, char* s2);

/* ====== Functions ============== */

/* ====== Parse json ============== */


/* ====== Parse otter and tptp ============== */

int wr_import_otter_file(glb* g, char* filename, char* strasfile, cvec clvec, int isincluded) {
  void* db=g->db;
  parse_parm  pp;
  char* fnamestr=NULL;  
  FILE* fp=NULL;    
  //char* buf; 
  int pres=1;
  void* preprocessed=NULL;
  void* pres2=NULL;
  void *mpool=NULL;  
  int tmp_comp_funs,tmplen;
  
  //printf("\nstarting to read file %s \n",filename);
#ifdef DEBUG
  DPRINTF("wr_import_otter_file called\n");
  printf("\n filename %s \n",filename);
  printf("\n strasfile %s \n",strasfile);
  printf("\n isincluded %d \n",isincluded);
  (g->print_initial_parser_result)=1;
  (g->print_generic_parser_result)=1;
#endif        
  // set globals for parsing
  (g->filename)=filename;
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
  (g->in_has_fof)=0;
   
  if (strasfile==NULL) {  
    // input from file
    fnamestr=filename;
    //if (fnamestr && fnamestr[0]=='#') fnamestr=fnamestr+1;    
#ifdef DEBUG
    printf("\nreading from file %s\n",fnamestr);
#endif       
    fp=freopen(fnamestr, "r", stdin);
    //printf("\nfp is %ld",(long int) fp);    
    if (!fp) {
      //if (g->print_flag)
      //printf("\ncould not open file %s\n",fnamestr);
      db_err_printf2("could not open file",fnamestr);
      return 1;
    }
    pp.db=db;
    pp.filename=fnamestr;
    pp.foo=NULL; // indicates file case in YY_INPUT in dbotter.l
    pp.result=NULL;
    pp.errmsg=NULL;
    pp.json=(g->print_json);
  } else {
    // input from string
    if (!strasfile) {
      db_err_printf2("empty string given as input",strasfile);
      return 1;
    }  
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
    pp.errmsg=NULL;
    pp.json=(g->print_json);
  }   
  //mpool=wg_create_mpool(db,PARSER_MEMPOOL_SIZE); 
  mpool=(dbmemsegh(db)->infrm_mpool);
  pp.mpool=mpool;  
  //printf("otter input:%s\n",(char*)pp.buf);
#ifdef DEBUG  
  printf("otter input:%s\n",(char*)pp.buf);  
#endif  
  wg_yyotterlex_init(&pp.yyscanner);
  wg_yyotterset_extra(&pp, pp.yyscanner);
  pres=wg_yyotterparse(&pp, pp.yyscanner);   
  wg_yyotterlex_destroy(pp.yyscanner);     
  //printf("\nwg_yyotterparse finished\n");     
  //wg_mpool_print(db,pp.result);
  //printf("\nresult: %d pp.result %s\n",pres,(char*)pp.result);

  if (pp.errmsg) {
    (g->parse_errflag)=1;
    tmplen=strlen(pp.errmsg)+10;
    (g->parse_errmsg)=malloc(tmplen);
    if (g->parse_errmsg) {
      strncpy(g->parse_errmsg,pp.errmsg,tmplen);
    }  
    wr_free(g,pp.errmsg);
  }
  if (!pres && pp.result!=NULL) { 
    if ((g->print_initial_parser_result)>0) {
      printf("\nOtter parser result:\n");    
      wg_mpool_print(db,pp.result);
    }        
    preprocessed=wr_preprocess_clauselist(g,mpool,clvec,pp.result,isincluded);
    //printf("\nwr_preprocess_clauselist finished\n");
    //preprocessed=wr_clausify_clauselist(g,mpool,clvec,pp.result);
    if (!(g->parse_errflag))
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

  //wg_free_mpool(db,mpool); // TODO !!!!!!!!!!!!!!!!!!!

  if (fp) fclose(fp);
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
  //FILE* fp;    
  
  DPRINTF("Hello from dbprologparse!\n"); 

  fnamestr=filename;   
  //fp=freopen(fnamestr, "r", stdin);
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
  char rolebuf[100];
  gint cell;
  gcell *cellptr;
  void* copied;
  void* part;
  void* freevars;
  int clauseflag=1;
  int blen=990,bpos=0;

#ifdef DEBUG  
  printf("wr_preprocess_clauselist starting with clauselist\n");  
  wg_mpool_print(db,clauselist);
  printf("\n");
#endif       

  if (dbmemsegh(db)->convert) {
    clauselist=wg_inplace_reverselist(db,mpool,clauselist);    
  }    
  // loop over clauses
  clnr=1;
  if ((dbmemsegh(db)->convert) && (dbmemsegh(db)->json) && !isincluded) {
    printf("[\n");
  }
  //wr_show_parse_error(g,"test %d",2);
  for(lpart=clauselist;wg_ispair(db,lpart);lpart=wg_rest(db,lpart),clnr++) {
    if (g->parse_errflag) break;
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
#ifdef STORE_SOURCE_FRM  
/*
    //printf("\n in wr_preprocess_clauselist clause: ");
    //wg_mpool_print(db,cl); 
    //printf("\n");
    if (1) {
      if (wr_is_tptp_fof_clause(g,cl)) copied=wg_mpool_copy(db,mpool,cl);
      else copied=cl;
      //printf("\n copy: ");
      //wg_mpool_print(db,copied);
      //printf("\n");
      cell=alloc_listcell(db);
      if (!cell) {
        wr_show_parse_error(g,"failed to allocate a cell for storing source formula");        
        return NULL;
      }  
      cellptr = (gcell *) offsettoptr(db, cell);             
      (cellptr->car) = (gint)copied; //ptrtooffset(db, res);
      (cellptr->cdr) = (dbmemsegh(db)->infrmlist);
      (dbmemsegh(db)->infrmlist) = cell;    
    }  
*/    
#endif 
    if (wr_is_tptp_import_clause(db,cl)) {
      // tptp import clause 
      wr_process_tptp_import_clause(g,mpool,cl);
      resultclause=NULL;
    } else if (wr_is_tptp_cnf_clause(db,cl)) {
      // tptp cnf clause
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
/*      
#ifdef STORE_SOURCE_FRM  
      //printf("\n in wr_preprocess_clauselist clause: ");
      //wg_mpool_print(db,cl); 
      //printf("\n");
      if (1) {
        copied=wg_mpool_copy(db,mpool,cl);        
        printf("\n copy: ");
        wg_mpool_print(db,copied);
        printf("\n");

        //(void*)(*((gint*)ptr));
        part=wg_rest(db,copied);
        *((gint*)part)=(gint)clname;       

        cell=alloc_listcell(db);
        if (!cell) {
          wr_show_parse_error(g,"failed to allocate a cell for storing source formula");        
          return NULL;
        }  
        cellptr = (gcell *) offsettoptr(db, cell);             
        (cellptr->car) = (gint)copied; //ptrtooffset(db, res);
        (cellptr->cdr) = (dbmemsegh(db)->infrmlist);
        (dbmemsegh(db)->infrmlist) = cell;    
      }  
#endif      
*/
      if ((dbmemsegh(db)->convert) && (dbmemsegh(db)->tptp)) {
        bpos=0; 
        if (!wg_str_guarantee_space(db,&(g->tmp_printbuf),&blen,bpos+1000)) return NULL;
        bpos+=snprintf((g->tmp_printbuf)+bpos,blen-bpos,
                       "cnf('%s',%s,",wg_atomstr1(db,clname),wg_atomstr1(db,clrole)); 
        wg_expand_frm_for_print(db,mpool,cl);                             
        bpos=wg_print_frm_tptp(db,wg_nth(db,cl,3),&(g->tmp_printbuf),&blen,bpos);  
        bpos+=snprintf((g->tmp_printbuf)+bpos,blen-bpos,").");                       
        printf("%s\n",(g->tmp_printbuf));
      } else if ((dbmemsegh(db)->convert) && (dbmemsegh(db)->json)) {  
        bpos=0;
        if (!wg_str_guarantee_space(db,&(g->tmp_printbuf),&blen,
              strlen(wg_atomstr1(db,clname))+
              strlen(wg_atomstr1(db,clrole))+1000)) return NULL;
        bpos+=snprintf((g->tmp_printbuf)+bpos,blen-bpos,"{\"@name\":");
        if (wg_contains_dquote(wg_atomstr1(db,clname))) {                   
          bpos+=wg_print_dquoted(&(g->tmp_printbuf),blen,bpos,wg_atomstr1(db,clname),0);        
        } else {
          bpos+=snprintf((g->tmp_printbuf)+bpos,blen-bpos,"\"%s\"",wg_atomstr1(db,clname));         
        }
        bpos+=snprintf((g->tmp_printbuf)+bpos,blen-bpos,",\"@role\":\"%s\",\"@logic\":",
                       wg_atomstr1(db,clrole));   
        wg_expand_frm_for_print(db,mpool,cl);                              
        bpos=wg_print_frm_json(db,wg_nth(db,cl,3),&(g->tmp_printbuf),&blen,bpos);
        if (bpos<0) return NULL;
        if (wg_rest(db,lpart)) {
          printf("%s},\n",(g->tmp_printbuf));
        } else {
          printf("%s}\n",(g->tmp_printbuf));
        }  
      }  
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
#ifdef STORE_SOURCE_FRM  
      if (g->store_fof_source) {
        copied=wg_mpool_copy(db,mpool,cl);        
        part=wg_rest(db,copied);
        *((gint*)part)=(gint)clname;       
        cell=alloc_listcell(db);
        if (!cell) {
          wr_show_parse_error(g,"failed to allocate a cell for storing source formula");        
          return NULL;
        }  
        cellptr = (gcell *) offsettoptr(db, cell);             
        (cellptr->car) = (gint)copied; //ptrtooffset(db, res);
        (cellptr->cdr) = (dbmemsegh(db)->infrmlist);
        (dbmemsegh(db)->infrmlist) = cell;    
      }  
#endif
      /*
      printf("\nin wr_preprocess_clauselist fof cl:\n");
      wg_mpool_print(db,cl); 
      printf("\n"); 
      */
      if ((dbmemsegh(db)->convert) && (dbmemsegh(db)->tptp)) {
        bpos=0;
        if (!wg_str_guarantee_space(db,&(g->tmp_printbuf),&blen,bpos+1000)) return NULL;
        bpos+=snprintf((g->tmp_printbuf)+bpos,blen-bpos,
                       "fof('%s',%s,",wg_atomstr1(db,clname),wg_atomstr1(db,clrole));
        wg_expand_frm_for_print(db,mpool,cl);                
        bpos=wg_print_frm_tptp(db,wg_nth(db,cl,3),&(g->tmp_printbuf),&blen,bpos);
        bpos+=snprintf((g->tmp_printbuf)+bpos,blen-bpos,").");                       
        printf("%s\n",(g->tmp_printbuf));
      } else if ((dbmemsegh(db)->convert) && (dbmemsegh(db)->json)) {  
        bpos=0;
        if (!wg_str_guarantee_space(db,&(g->tmp_printbuf),&blen,
              strlen(wg_atomstr1(db,clname))+
              strlen(wg_atomstr1(db,clrole))+1000)) return NULL;
        bpos+=snprintf((g->tmp_printbuf)+bpos,blen-bpos,"{\"@name\":");
        if (wg_contains_dquote(wg_atomstr1(db,clname))) {                   
          bpos+=wg_print_dquoted(&(g->tmp_printbuf),blen,bpos,wg_atomstr1(db,clname),0);        
        } else {
          bpos+=snprintf((g->tmp_printbuf)+bpos,blen-bpos,"\"%s\"",wg_atomstr1(db,clname));         
        }
        bpos+=snprintf((g->tmp_printbuf)+bpos,blen-bpos,",\"@role\":\"%s\",\"@logic\":",
                       wg_atomstr1(db,clrole));
        wg_expand_frm_for_print(db,mpool,cl);                                 
        bpos=wg_print_frm_json(db,wg_nth(db,cl,3),&(g->tmp_printbuf),&blen,bpos);
        if (bpos<0) return NULL;
        if (wg_rest(db,lpart)) {
          printf("%s},\n",(g->tmp_printbuf));
        } else {
          printf("%s}\n",(g->tmp_printbuf));
        }  
      }  
      resultclause=wr_preprocess_tptp_fof_clause(g,mpool,cl,clname); 
      if (resultclause) {
        resultclause=wg_mklist3(db,mpool,clname,clrole,resultclause);     
      }  
      if (g->parse_errflag) break;
    } else { 
      // otter clause  
      resultclause=cl;
      clrole=NULL;
      clname=NULL;
#ifdef MARK_IMPORTED_NAMES      
      if (g->parse_is_included_file) {
        //printf("\n!!! clname %s\n",wg_atomstr1(db,clname));
        if (g->parse_is_included_file) {
          strncpy(namebuf,IMPORTED_NAME_PREFIX,900);
          strncat(namebuf,"ax1",900);
          clname=wg_mkatom(db,mpool,WG_URITYPE,namebuf, NULL);
        } else {
          strncpy(namebuf,"u:ax2",10);
          clname=wg_mkatom(db,mpool,WG_URITYPE,namebuf, NULL);
        }       
        //printf("\n!!! clnamenew %s\n",wg_atomstr1(db,clname)); 
        strncpy(rolebuf,"u:axiom",50);
        clrole=wg_mkatom(db,mpool,WG_URITYPE,rolebuf, NULL);
      }
#endif       
      if ((dbmemsegh(db)->convert) && (dbmemsegh(db)->tptp)) {       
        bpos=0;
        sprintf(namebuf,"frm_%d",clnr);       
        clname=wg_mkatom(db,mpool,WG_URITYPE,namebuf, NULL);
        sprintf(rolebuf,"axiom");   
        clrole=wg_mkatom(db,mpool,WG_URITYPE,rolebuf, NULL);       
        if (!wg_str_guarantee_space(db,&(g->tmp_printbuf),&blen,bpos+1000)) return NULL;              
        clauseflag=1;
        freevars=wr_parse_freeoccs(g,mpool,NULL,cl,&clauseflag);
        //printf("\n clauseflag %d freevars: \n",clauseflag);
        //wg_mpool_print(db,freevars); 
        //printf("\n");       
        wg_expand_frm_for_print(db,mpool,cl);
        if (clauseflag) {
          bpos+=snprintf((g->tmp_printbuf)+bpos,blen-bpos,
                       "cnf('%s',%s,",wg_atomstr1(db,clname),wg_atomstr1(db,clrole));                                                           
          bpos=wg_print_cnf_tptp(db,cl,&(g->tmp_printbuf),&blen,bpos);          
        } else {
          if (freevars) cl=wg_mklist3(db,mpool,wg_makelogall(db,mpool),freevars,cl); 
          bpos+=snprintf((g->tmp_printbuf)+bpos,blen-bpos,
                       "fof('%s',%s,",wg_atomstr1(db,clname),wg_atomstr1(db,clrole));                                                                     
          bpos=wg_print_frm_tptp(db,cl,&(g->tmp_printbuf),&blen,bpos);
        } 
        bpos+=snprintf((g->tmp_printbuf)+bpos,blen-bpos,").");               
        printf("%s\n",(g->tmp_printbuf));
      } else if ((dbmemsegh(db)->convert) && (dbmemsegh(db)->json)) {  
        bpos=0;
        if (!wg_str_guarantee_space(db,&(g->tmp_printbuf),&blen,1000)) return NULL; 
        wg_expand_frm_for_print(db,mpool,cl);                 
        bpos=wg_print_frm_json(db,cl,&(g->tmp_printbuf),&blen,bpos);
        if (bpos<0) return NULL;
        if (wg_rest(db,lpart)) {
          printf("%s,\n",(g->tmp_printbuf));
        } else {
          printf("%s\n",(g->tmp_printbuf));
        }  
      }  
      if (g->parse_is_included_file) {
        resultclause=wg_mklist3(db,mpool,clname,clrole,resultclause);
      } else {
        // new version for full logic for otter-style input
        resultclause=wr_clausify_formula(g,mpool,cl,NULL,NULL);
        resultclause=wg_mklist3(db,mpool,NULL,NULL,resultclause);
        // before was
        //resultclause=wg_mklist3(db,mpool,NULL,NULL,resultclause);  
      }  
      if (g->parse_errflag) break;  
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
  if ((dbmemsegh(db)->convert) && (dbmemsegh(db)->json) && !isincluded) {
    printf("]\n");
  }
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

void wg_expand_frm_for_print(void* db, void* mpool, void* frm) {
  void* part;
  void* term;
  void* expanded;

  /*
  printf("\ncalling wg_expand_frm_for_print:\n");
  wg_mpool_print(db,frm);
  printf("\n");
  */
  if (!frm) return;
  if (wg_isatom(db,frm)) return;  
  for(part=frm; part!=NULL; part=wg_rest(db,part)) {
    term=wg_first(db,part);
    if (wg_ispair(db,term) && wg_isatom(db,wg_first(db,term)) &&
       wg_atomstr1(db,wg_first(db,term)) &&
       !strcmp("$$lst",wg_atomstr1(db,wg_first(db,term))) ) {
      if (!wg_ispair(db,wg_rest(db,term))) {
        expanded=wg_mkatom(db,mpool,WG_URITYPE,"$nil",NULL);
      } else {  
        expanded=wg_expand_frm_for_print_aux(db,mpool,wg_first(db,wg_rest(db,term)));
      }       
      *((gint*)part)=(gint)expanded;    
    } else {
      wg_expand_frm_for_print(db,mpool,term);
    }  
  }
}

void* wg_expand_frm_for_print_aux(void* db, void* mpool, void* cl) { 
  void* res=NULL;
  void *term, *termpart, *fun;
  /*
  printf("\ncalling wg_expand_frm_for_print_aux:\n");
  wg_mpool_print(db,cl);
  printf("\n");
  */
  res=wg_mkatom(db,mpool,WG_URITYPE,"$nil",NULL);    
  if (!cl) return res;
  if (!wg_ispair(db,cl)) return cl;
  fun=wg_mkatom(db,mpool,WG_URITYPE,"$list",NULL);
  if (!res || !fun) return NULL;
  if (wg_isatom(db,wg_first(db,cl)) &&
      wg_atomstr1(db,wg_first(db,cl)) ) {
    if (!strcmp("$$lst",wg_atomstr1(db,wg_first(db,cl)))) {
      // $$lst term
      if (!wg_ispair(db,wg_rest(db,cl))) {
        term=wg_mkatom(db,mpool,WG_URITYPE,"$nil",NULL);
      } else {  
        term=wg_expand_frm_for_print_aux(db,mpool,wg_first(db,wg_rest(db,cl)));
      }    
      return term;
    } 
  } 
  term=NULL;
  termpart=wg_inplace_reverselist(db,mpool,cl);  
  for(;wg_ispair(db,termpart);termpart=wg_rest(db,termpart)) {        
    wg_expand_frm_for_print(db,mpool,termpart); 
    term=wg_first(db,termpart);     
    res=wg_mklist3(db,mpool,fun,term,res);
    if (!res) return NULL;
  }
  return res;
}

/*
int wr_check_fof_cnf_freevars(glb* g, void* mpool, void* cl, void** freevarsptr) {
  

}
*/


void *wr_parse_freeoccs(glb* g, void* mpool, void* vars, void* frm, int* clauseflag) {
  void *db=g->db;
  void *arg2, *arg3;
  void *op, *newvars, *termoccs, *freeoccs;
  void *term, *termpart;
  int len;
  /*
  printf("\nwr_parse_freeoccs on ");
  wg_mpool_print(db,frm);
  printf("\nvars \n");
  wg_mpool_print(db,vars);
  printf("\n");
  */
  if (frm==NULL) return NULL;
  if (wg_isatom(db,frm)) {
    // simple atom
    if ((g->parse_caps_as_var && wg_atomstr1(db,frm) &&
        isupper(wg_atomstr1(db,frm)[0])) ||
        (g->parse_question_as_var && wg_atomstr1(db,frm) &&
         wg_atomstr1(db,frm)[0]=='?') ) { 
      if (!wr_freeoccs_invars(g,mpool,vars,frm)) {
        return wg_mkpair(db,mpool,frm,NULL);
      }                 
    } 
    if (wg_islogtrue(db,frm) || wg_islogfalse(db,frm)) *clauseflag=0;  
    return NULL;
  }  
  op=wg_first(db,frm);
  len=wg_list_len(db,frm);
  if (wg_ispair(db,op) && len<2) {
    // pointless parenthesis around formula: lift up
    return wr_parse_freeoccs(g,mpool,vars,op,clauseflag);
  }
  if (wg_ispair(db,op)) {
    // should not happen
    return NULL;
  }
  if (wg_islogall(db,op) || wg_islogexists(db,op)) {
    // quantifier
    *clauseflag=0;
    arg2=wg_first(db,wg_rest(db,frm));
    arg3=wg_first(db,wg_rest(db,wg_rest(db,frm)));
    newvars=wr_miniscope_varssubset(g,mpool,arg2,vars);    
    termoccs=wr_parse_freeoccs(g,mpool,newvars,arg3,clauseflag);
    return termoccs;  
  } else {
    // logical or a nonlogical term: loop over    
    freeoccs=NULL;
    if (wg_islogconnective(db,op)) {
      if (!(wg_islogor(db,op) || wg_islogneg(db,op))) *clauseflag=0;
      termpart=wg_rest(db,frm);
    } else {
      termpart=wg_rest(db,frm); //frm;
    }  
    for(; termpart!=NULL; termpart=wg_rest(db,termpart)) {
      term=wg_first(db,termpart);
      if (!term) continue;
      termoccs=wr_parse_freeoccs(g,mpool,vars,term,clauseflag);     
      if (termoccs) freeoccs=wr_add_freeoccs(g,mpool,freeoccs,termoccs);      
    }
    return freeoccs;
  } 
}

int wr_is_parse_var(glb* g,void* ptr) {
  void* db=g->db;
  char *s;
  if (!wg_isatom(db,ptr) || wg_atomtype(db,ptr)!=WG_URITYPE) return 0;
  s=wg_atomstr1(db,ptr);
  if ((*s)=='\0') return 0;
  if (*s >= 'A' && *s <= 'Z') return 1; 
  if (*s == '?') return 1;     
  return 0;
}

void* wr_preprocess_tptp_fof_clause(glb* g, void* mpool, void* cl, void* clname) {
  void* db=g->db;
  void* clpart;
  void* res;
  void* tmp;
  void* cltype;
  void* name;
  void* skname=NULL;
  char* namestr;
  char namebuf[1000];
  //gint cell;
  //gcell *cellptr;


#ifdef DEBUG
  printf("in wr_preprocess_tptp_fof_clause given cl:\n");  
  wg_mpool_print(db,cl); 
  printf("\n");
#endif   
  (g->in_has_fof)=1;
  // insert to source list
  //printf("\n(g->in_has_fof) %d\n",(g->in_has_fof));
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
    cltype=wg_mkatom(db,mpool,WG_URITYPE,"negated_conjecture",NULL);
  }
  // naming part for skolemization
#ifdef STORE_SKOLEM_STEPS   
  if (g->store_fof_skolem_steps) {
    name=clname; //wg_first(db,wg_rest(db,cl));
    if (name && wg_isatom(db,name)) {
      namestr=wg_atomstr1(db,name);
      strncpy(namebuf,namestr,900);
      strncat(namebuf,SKOLEM_CLNAME_SUFFIX,900);
      skname=wg_mkatom(db,mpool,WG_URITYPE,namebuf, NULL);
    } else {
      skname=NULL;
    }
  }  
#endif  
#ifdef DEBUG
  printf("in wr_preprocess_tptp_fof_clause clpart:\n");  
  wg_mpool_print(db,clpart); 
  printf("\n");
#endif
  tmp=wr_clausify_formula(g,mpool,clpart,cltype,skname);
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
  char* envfolder=NULL;
  char *lastslash = NULL;
  //char *parent = NULL;
  char* p1;
  char* p2;
  char* str2;
  //int tmp;
 
#ifdef IDEBUG
  printf("\nwr_process_tptp_import_clause starts with\n");
  wg_mpool_print(db,cl); 
  printf("\n");
  printf("\nproblem filename is %s\n",g->filename);
#endif

  if (!(g->filename) || strlen(g->filename)<1) {
    wr_show_parse_error(g,"no filename given in wr_process_tptp_import_clause\n");
    return NULL;
  }
  pathatom=wg_nth(db,cl,1);
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
    if (!str2) return NULL;
    for(p1=(g->filename), p2=str2; p1<lastslash; p1++, p2++) {
      *p2=*p1;
    }
    *p2=(char)0;    
    strcat(str2,"/");
    strcat(str2,str);    
  } else {
    str2=str;
  }  
#ifdef IDEBUG
  //printf("\nstr %s\n",str);
  //printf("\nstr2 %s\n",str2);
  fp=fopen(str,"r");
  if (fp!=NULL) {
    printf("\nfile %s found\n",str); 
    fclose(fp);
  } else {
    printf("\nfile %s not found\n",str); 
  }  
  fp=fopen(str2,"r");
  if (fp!=NULL) {
    printf("\nfile %s found\n",str2); 
    fclose(fp);
  } else {
    printf("\nfile %s not found\n",str2); 
  }  
#endif
  //printf("\nfirst import path str2: %s\n",str2);  
  fp=fopen(str2,"r");
  if (fp!=NULL) {
    // succeeded to open axiom file: use that
    fclose(fp);
    filename=str2; 
  } else {
    if (envfolder) axiomfolder=envfolder;
    // failed to open axiom file: try env or default axiom folder
    bytes=strlen(str)+strlen(axiomfolder)+10;
    filename=wg_alloc_mpool(db,mpool,bytes);
#ifdef IDEBUG
    printf("\nalloced filename %s\n",filename);
#endif    
    if (!filename) {
      wr_show_parse_error(g,"cannot allocate filename in wr_process_tptp_import_clause\n");
      return NULL;
    }
    strcpy(filename,axiomfolder);
    if (envfolder) strcat(filename,"/");
    strcat(filename,str);
    //printf("\nsecond import path str: %s\n",filename);
  }
  //tmp=
#ifdef IDEBUG
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
  gint cell;
  gcell *cellptr;
  
#ifdef DEBUG  
  DPRINTF("wr_parse_clauselist starting with clauselist\n");  
  wg_mpool_print(db,clauselist); 
  DPRINTF("\n");
#endif
   
  if (clvec!=NULL) CVEC_NEXT(clvec)=CVEC_START; 
  // create vardata block by malloc or inside mpool

  vardata=(char**)(wg_alloc_mpool(db,mpool,sizeof(char*)*VARDATALEN));
  if (vardata==NULL) {
    wr_show_parse_error(g,"cannot allocate vardata in wg_parse_clauselist");
    //(g->parse_errflag)=1;
    return NULL;
  }  
  //vardata=(char**)(malloc(sizeof(char*)*VARDATALEN));
  for(i=0;i<VARDATALEN;i++) vardata[i]=NULL; // pointers to varstrings  
  propfun=wg_mkatom(db,mpool,WG_URITYPE,PROPVAR_PREDSYMB,NULL);
  // loop over clauses  
  for(lpart=clauselist,clnr=0;wg_ispair(db,lpart);lpart=wg_rest(db,lpart),clnr++) {
    //if (clnr%10000==0) printf("clnr %d\n",clnr)
    if (g->parse_errflag) break;
    clause=wg_first(db,lpart);  
    if (!wg_ispair(db,clause)) {
      name=NULL;
      role=NULL;
      frm=clause;
    } else {
      if (wg_list_len(db,clause)!=3) {
        wr_show_parse_error(g,"clause in wg_parse_clauselist not a triple");
        //(g->parse_errflag)=1;
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
      // add record to the clauselist in db
      cell=alloc_listcell(db);
      if (!cell) {
        wr_show_parse_error(g,"failed to allocate a cell");
        //(g->parse_errflag)=1;
        return NULL;
      }  
      cellptr = (gcell *) offsettoptr(db, cell);
      (cellptr->car) = ptrtooffset(db, record);
      (cellptr->cdr) = (dbmemsegh(db)->clauselist);
      (dbmemsegh(db)->clauselist) = cell;       
      // add to resultlist, which is not really needed   
      resultlist=wg_mkpair(db,mpool,record,resultlist); // not really needed!!
      if (!resultlist) {
        wr_show_parse_error(g,"failed to add a clause to resultlist");
        //(g->parse_errflag)=1;
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
    printf("\nCP1X\n");
#endif      
    //return NULL;
  }  
  // examine clause: find clause length and check if simple clause    
  issimple=1;
  litnr=0;
  for(clpart=cl;wg_ispair(db,clpart);clpart=wg_rest(db,clpart),litnr++) {
    if (g->parse_errflag) return NULL;
    lit=wg_first(db,clpart); 
#ifdef DEBUG      
    printf("\nlit: ");
    wg_mpool_print(db,lit); 
    printf("\n");      
#endif      
    if (!wg_ispair(db,lit)) { issimple=0; continue; }
    fun=wg_first(db,lit);
    if (wg_ispair(db,fun) && wg_mpool_bad_ptr(db,fun)) {
      wr_show_parse_error(g," wrong syntax in clause nr %d ",clnr);
      return NULL;
    }
    if (wg_atomtype(db,fun)==WG_ANONCONSTTYPE && !strcmp(wg_atomstr1(db,fun),"not")) { issimple=0; continue; }    
    for(termpart=lit;wg_ispair(db,termpart);termpart=wg_rest(db,termpart)) {
      subterm=wg_first(db,termpart);
#ifdef DEBUG        
      printf("\nsubterm: ");
      wg_mpool_print(db,subterm); 
      printf("\n");
#endif        
      if (subterm!=NULL && wg_ispair(db,subterm)) { issimple=0; break; }     
      if (wg_atomtype(db,subterm)==WG_VARTYPE) { issimple=0; break; } 
      if (wg_atomtype(db,subterm)==WG_URITYPE && wg_atomstr1(db,subterm) && 
          ( (g->parse_caps_as_var && isupper(wg_atomstr1(db,subterm)[0])) ||
            (g->parse_question_as_var && wg_atomstr1(db,subterm)[0]=='?') )) {issimple=0; break;} 
    }      
  }        
  if (litnr>1) issimple=0;   
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
    if (g->parse_errflag) return NULL;
    lit=wg_first(db,clpart);
#ifdef DEBUG    
    printf("\nlit nr  %d:",litnr);    
    wg_mpool_print(db,lit); 
    printf("\n");
#endif      
    if (!wg_ispair(db,lit)) {        
      lit=wg_mkpair(db,mpool,propfun,wg_mkpair(db,mpool,lit,NULL));
      if (!lit) {
        //show_parse_warning(db,"failed to make a literal"); 
        wr_show_parse_error(g,"failed to make a literal");
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
      //show_parse_warning(db,"lit nr %d in clause nr %d has nonatomic leadfun and hence ignored: ",litnr,clnr); 
      wr_show_parse_error(g,"lit nr %d in clause nr %d has nonatomic leadfun and hence ignored: ",litnr,clnr);
#ifdef DEBUG        
      wg_mpool_print(db,fun); 
      printf("\n");
#endif        
      //continue;
      return NULL;
    }     
    isneg=0;
    if (wg_atomtype(db,fun)==WG_ANONCONSTTYPE && 
        !strcmp(wg_atomstr1(db,fun),"not") &&
        wg_atomstr2(db,fun)==NULL) {
      DPRINTF("detected negation");
      isneg=1;            
      tmpptr=wg_rest(db,lit);    
      if (!wg_ispair(db,tmpptr)) {          
        //show_parse_warning(db,"lit nr %d in clause nr %d does not contain proper atom after negation and hence ignored: ",litnr,clnr);         
        wr_show_parse_error(g,"lit nr %d in clause nr %d does not contain proper atom after negation and hence ignored: ",litnr,clnr);
#ifdef DEBUG          
        wg_mpool_print(db,lit); 
        printf("\n");
#endif          
        //continue;
        return NULL;
      } 
      atom=wg_first(db,tmpptr);
      if (!wg_ispair(db,atom)) {
        atom=wg_mkpair(db,mpool,propfun,wg_mkpair(db,mpool,atom,NULL));
        if (!atom) {
          //show_parse_warning(db,"failed to make an atom"); 
          wr_show_parse_error(g,"failed to make an atom"); 
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
        //show_parse_warning(db,"lit nr %d in clause nr %d has nonatomic leadfun and hence ignored: ",litnr,clnr); 
        wr_show_parse_error(g,"lit nr %d in clause nr %d has nonatomic leadfun and hence ignored",litnr,clnr);
        //(g->parse_errflag)=1;
#ifdef DEBUG          
        wg_mpool_print(db,fun); 
        printf("\n");
#endif          
        //continue;
        return NULL;
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
      wr_show_parse_error(g,"problem converting an atom to record");
      //(g->parse_errflag)=1;
      //free(vardata);
      return NULL;        
    }
    if (issimple) {
      wr_convert_atom_fact_clause(g,atomres,isneg);
      resultlist=wg_mkpair(db,mpool,atomres,resultlist);
      if (!resultlist) {
        //show_parse_warning(db,"failed to extend resultlist"); 
        wr_show_parse_error(g,"failed to extend resultlist"); 
        //(g->parse_errflag)=1;         
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
        // wg_delete_record(db,atomres); // might leak memory
        wr_free(g,vardata);
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
  } else {
    rolenr=PARSER_AXIOM_ROLENR;
  }      
  history=wr_build_input_history(g,record,namestr,rolenr);
  if (!history) {
    //show_parse_warning(db,"failed to make history"); 
    //return NULL;          
  } 
  wr_set_history(g,record,history); 
#ifdef DEBUG
  printf("\n built a record:\n");
  wg_print_record(db_to_kb(db),record);
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
  gint primitiveres;

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
    if (g->parse_errflag) return NULL;
    term=wg_first(db,termpart);
    
#ifdef DEBUG    
    printf("\nterm nr  %d:",termnr);    
    wg_mpool_print(db,term); 
    printf("\n");
#endif    
    if (!wg_ispair(db,term)) {      
      DPRINTF("term nr %d is primitive \n",termnr); 
      // convert some primitives to others
      if (!termnr && wg_atomstr1(db,term) && wg_atomstr1(db,term)[0]=='$') {
        if (!strcmp("$less",wg_atomstr1(db,term))) {
          term=wg_mkatom(db,mpool,WG_URITYPE,"<",NULL);
        }
      }     
      // conversion ends
      tmpres2=wr_parse_primitive(g,mpool,term,vardata,termnr);    
      if (!tmpres2 || tmpres2==WG_ILLEGAL) {
        //wg_delete_record(db,record); // might leak memory
        return NULL;
      }  
    } else {
      DPRINTF("term nr %d is nonprimitive \n",termnr);
      primitiveres=0;
      tmpres=wr_parse_term(g,mpool,term,vardata,&primitiveres);
      if (primitiveres) {
        tmpres2=primitiveres;
      } else if (tmpres==NULL) {
        // wg_delete_record(db,record); // might leak memory
        return NULL;
      } else {            
        tmpres2=wg_encode_record(db,tmpres);       
      }  
    }     
    if (!tmpres2 || tmpres2==WG_ILLEGAL) return NULL;        
    setres=wr_set_kb_atom_subterm(g,record,termnr,tmpres2);    
    if (setres!=0) {
      // wg_delete_record(db,record); // might leak memory
      return NULL; 
    }  
  }    
  ret=record;  
  if (ret==NULL) DPRINTF("\nwg_parse_atom returns NULL\n");
  return ret;
}



void* wr_parse_term(glb* g,void* mpool,void* term, char** vardata, gint* primitiveres) {
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
  gint myprimitiveres;

#ifdef DEBUG  
  DPRINTF("\nwg_parse_term starting with ");
  wg_mpool_print(db,term); 
  printf("\n");
#endif
  
  // convert list
  if (wg_ispair(db,term) && wg_isatom(db,wg_first(db,term)) && 
      !strcmp(wg_atomstr1(db,wg_first(db,term)),"$$lst")) {     
    tmpres=wr_parse_preprocess_list(g,mpool,term);
    if (tmpres==NULL) return NULL;                
    term=tmpres;
    if (!wg_ispair(db,term)) {
      *primitiveres=wr_parse_primitive(g,mpool,term,vardata,0);
      return NULL;
    }
  }    
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
    if (g->parse_errflag) return NULL;
    term=wg_first(db,termpart);
#ifdef DEBUG    
    //DPRINTF("\nterm nr  %d:",termnr);    
    //wg_mpool_print(db,term); 
    //printf("\n");
#endif    
    if (!wg_ispair(db,term)) {
      DPRINTF("term nr %d is primitive \n",termnr); 
      // convert some primitives to others
      if (!termnr && wg_atomstr1(db,term) && wg_atomstr1(db,term)[0]=='$') {
        if (!strcmp("$sum",wg_atomstr1(db,term))) {
          term=wg_mkatom(db,mpool,WG_URITYPE,"+",NULL);
        } else if (!strcmp("$difference",wg_atomstr1(db,term))) {
          term=wg_mkatom(db,mpool,WG_URITYPE,"-",NULL);        
        } else if (!strcmp("$product",wg_atomstr1(db,term))) {
          term=wg_mkatom(db,mpool,WG_URITYPE,"*",NULL);        
        } else if (!strcmp("$quotient",wg_atomstr1(db,term))) {
          term=wg_mkatom(db,mpool,WG_URITYPE,"/",NULL);
        }
      }     
      // conversion ends
      tmpres2=wr_parse_primitive(g,mpool,term,vardata,termnr);    
      if (tmpres2==WG_ILLEGAL) { 
        //wg_delete_record(db,record); // might leak memory !!
        return NULL;
      }           
    } else {     
      DPRINTF("term nr %d is nonprimitive \n",termnr);
      myprimitiveres=0;
      tmpres=wr_parse_term(g,mpool,term,vardata,&myprimitiveres);
      if (myprimitiveres) {
        tmpres2=myprimitiveres;
      } else if (tmpres==NULL) {
        // wg_delete_record(db,record); // might leak memory
        return NULL;
      } else {            
        tmpres2=wg_encode_record(db,tmpres);       
      } 
    }       
    if (tmpres2==WG_ILLEGAL) return NULL;
    //setres=wr_set_term_subterm(g,record,termnr,tmpres2);
    setres=wr_set_kb_term_subterm(g,record,termnr,tmpres2);
    if (setres!=0) {
      //wg_delete_record(db,record); // might leak memory !!
      return NULL; 
    }  
  }    
  ret=record; 
  //if (ret==NULL) printf("\nwg_parse_term returns NULL\n");
  return ret;  
}

void* wr_parse_preprocess_list(glb* g,void* mpool,void* term) {
  void* db=g->db;
  void* res=NULL;
  void *termpart, *fun;
  
  res=wg_mkatom(db,mpool,WG_URITYPE,"$nil",NULL);  
  termpart=wg_rest(db,term);
  if (!termpart) return res;
  termpart=wg_first(db,termpart);
  if (!termpart) return res;
  termpart=wg_inplace_reverselist(db,mpool,termpart);
  fun=wg_mkatom(db,mpool,WG_URITYPE,"$list",NULL);
  for(;wg_ispair(db,termpart);termpart=wg_rest(db,termpart)) {
    if (g->parse_errflag) return NULL;
    term=wg_first(db,termpart);
    res=wg_mklist3(db,mpool,fun,term,res);
  }
  return res;
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
  printf("\nwg_parse_primitive starting with ");
  wg_mpool_print(db,atomptr); 
  printf("\n");  
  printf("\n type %d is anon %d\n",wg_atomtype(db,atomptr),wg_atomtype(db,atomptr)==WG_ANONCONSTTYPE);  
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
            //show_parse_warning(db,"too many variables in a clause: ignoring the clause");        
            wr_show_parse_error(g,"too many variables in a clause");
            return WG_ILLEGAL;
            //errno=0;
            //ret=WG_ILLEGAL;
            //break;          
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
          //show_parse_warning(db,"too many variables in a clause: ignoring the clause");        
          wr_show_parse_error(g,"too many variables in a clause");
          return WG_ILLEGAL;
          //errno=0;
          //ret=WG_ILLEGAL;
          //break;          
        }                   
        ret=wg_encode_var(db,intdata);
        DPRINTF("var %d encoded ok\n",intdata);       
        break; 
      default: 
        ret=wg_encode_null(db,NULL);
    }      
  } 
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
  int tmplen;

  /* Try matching to a known scheme */
  while(next->prefix) {
    if(!strncmp(buf, next->prefix, next->length)) {
      /* We have a matching URI scheme.
       * XXX: check this code for correct handling of prefix. */
      int urilen = strlen(buf);
      tmplen=urilen + 1;
      char *prefix = wr_malloc(g,tmplen);
      char *dataptr;

      if(!prefix) break;
      //strncpy(prefix, buf, tmplen);
      prefix[tmplen-1]=(char)0;
      strcpy(prefix, buf);

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
      wr_free(g,prefix);
      break;
    }
    next++;
  }
  return encoded;
}

/* ----------- db clones of object creation funs ---------- */



/* ------------ errors ---------------- */

/*
static int show_parse_error(void* db, char* msg) {
#ifdef ERRPRINT  
  printf("{\"error\": \"%s\"}\n",msg);
#endif     
  return -1;  
}
*/

int wr_show_parse_error(glb* g, char* format, ...) {
  //void* db=g->db;
  int tmp1,tmp2;
  va_list args;
  va_start (args, format);
  /*
  printf("*** Parser error: ");
  vprintf (format, args);  
  va_end (args);
  printf("\n");
  */  

  if (g->parse_errflag) return -1;
  (g->parse_errflag)=1;  
  if (g->parse_errmsg) return -1;
  (g->parse_errmsg)=malloc(1000);
  if (!(g->parse_errmsg)) return -1;
  if (1) { //(g->print_json) {
    tmp1=snprintf((g->parse_errmsg),50,"{\"error\": \"parser error: ");
    tmp2=vsnprintf((g->parse_errmsg)+tmp1,800,format,args);
    snprintf((g->parse_errmsg)+tmp1+tmp2,50,"\"}");
  } else {
    tmp1=snprintf((g->parse_errmsg),50,"parser error: ");
    tmp2=vsnprintf((g->parse_errmsg)+tmp1,800,format,args);
    snprintf((g->parse_errmsg)+tmp1+tmp2,50,"\n");
  }  

  /*
  printf("{\"error\": \"parser error: ");
  vprintf (format, args);
  va_end (args);
  printf("\"}\n");
  */
  //exit(1);
  return -1;  
}

/*
static int show_parse_error(void* db, char* format, ...) {
  va_list args;
  va_start (args, format);  
  printf("{\"error\": \"parser error: ");
  vprintf (format, args);
  va_end (args);
  printf("\"}\n");
  //exit(1);
  return -1;  
}
*/

/*
static int show_parse_warning(void* db, char* format, ...) {
  va_list args;
  va_start (args, format);
  //printf("*** Parser warning: ");
  printf("{\"error\": \"parser error: ");
  vprintf (format, args);
  va_end (args);
  printf("\"}\n");
  return -1;
}
*/

void db_err_printf2(char* s1, char* s2) {
#ifdef ERRPRINT  
  printf("{\"error\": \"%s %s\"}\n",s1,s2);
#endif  
}