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

 /** @file dbgenparse.h
 *  Top level/generic headers and defs for parsers
 *
 */

#ifndef DEFINED_DBGENPARSE_H
#define DEFINED_DBGENPARSE_H

#include "../Db/dbdata.h"
#include "../Db/dbmpool.h"
#include "dbparse.h"

#define parseprintf(...)
//#define parseprintf printf

#define MKWGPAIR(pp,x,y) (wg_mkpair(((parse_parm*)pp)->db,((parse_parm*)pp)->mpool,x,y))

#define MKWGINT(pp,x)   (wg_mkatom(((parse_parm*)pp)->db,((parse_parm*)pp)->mpool,WG_INTTYPE,x,NULL))
#define MKWGFLOAT(pp,x) (wg_mkatom(((parse_parm*)pp)->db,((parse_parm*)pp)->mpool,WG_DOUBLETYPE,x,NULL))
#define MKWGDATE(pp,x)  (wg_mkatom(((parse_parm*)pp)->db,((parse_parm*)pp)->mpool,WG_DATETYPE,x,NULL))
#define MKWGTIME(pp,x)  (wg_mkatom(((parse_parm*)pp)->db,((parse_parm*)pp)->mpool,WG_TIMETYPE,x,NULL))
#define MKWGID(pp,x)      (wg_mkatom(((parse_parm*)pp)->db,((parse_parm*)pp)->mpool,WG_URITYPE,x,NULL))
#define MKWGURI(pp,x)     (wg_mkatom(((parse_parm*)pp)->db,((parse_parm*)pp)->mpool,WG_URITYPE,x,NULL))
#define MKWGCONSTURI(pp,x)     (wg_mkatom(((parse_parm*)pp)->db,((parse_parm*)pp)->mpool,WG_URITYPE,x,"c"))
#define MKWGUNIQUEURI(pp,x)     (wg_mkatom(((parse_parm*)pp)->db,((parse_parm*)pp)->mpool,WG_URITYPE,x,"#"))
#define MKWGSTRING(pp,x)  (wg_mkatom(((parse_parm*)pp)->db,((parse_parm*)pp)->mpool,WG_STRTYPE,x,NULL))
#define MKWGCONST(pp,x)  (wg_mkatom(((parse_parm*)pp)->db,((parse_parm*)pp)->mpool,WG_ANONCONSTTYPE,x,NULL))
#define MKWGVAR(pp,x)     (wg_mkatom(((parse_parm*)pp)->db,((parse_parm*)pp)->mpool,WG_VARTYPE,x,NULL))
#define MKWGNIL NULL

#define MKWGURILEN(pp,x,l)  (wg_mkatom_len(((parse_parm*)pp)->db,((parse_parm*)pp)->mpool,WG_URITYPE,x,NULL,l,0))
#define MKWGSTRINGLEN(pp,x,l)  (wg_mkatom_len(((parse_parm*)pp)->db,((parse_parm*)pp)->mpool,WG_STRTYPE,x,NULL,l,0))
#define MKWGINTDIRECT(pp,x)     (wg_mkatom_int(((parse_parm*)(pp))->db,((parse_parm*)(pp))->mpool,(x)))
#define MKWGDOUBLEDIRECT(pp,x)   (wg_mkatom_double(((parse_parm*)(pp))->db,((parse_parm*)(pp))->mpool,(x)))

#define PARSE_NESTING_DEPTH 256

// ---- reeentrant ----

typedef struct parse_parm_s {
  void  *yyscanner; // has to be present
  char  *buf;       // for parse from str case
  int   pos;        // for parse from str case
  int   length;     // for parse from str case
  char* filename;   // for err handling
  void* result;     // parser result  
  void* db;         // database pointer
  void* mpool;      // mpool pointer
  char* foo;        // if NULL, use input from stdin, else from buf (str case)
  char* errmsg;     // if not NULL, resulted in error

  // for json
  int depth;        // for parsing json
  
  int formulanr;     // nr of formula in the list being processed
  char* formulaname; // name of formula
  char* formularole; // role of formula

  void* jsonnull;    // pre-built atom for json null
  void* jsonstruct;  // pre-built atom for struct list prefix
  void* jsontrue;    // pre-built atom for json true: same as logical true
  void* jsonfalse;   // pre-built atom for json true: same as logical true
  void* jsonempty;   // pre-built atom for json empty list []

  void* logfof;    // json preparation inserts this as top level
  void* logtrue;    // json preparation inserts this as a connective for clausifier
  void* logfalse;   // ...
  void* logneg; 
  void* logand;
  void* logprefixand;
  void* logor;
  void* logprefixor;
  void* logimp;
  void* logeqv;
  void* logall;
  void* logexists; // ..
  void* logask;
  void* logat;
  void* atomeq;    // json preparation inserts this as an eq predicate for clausifier  

  void* freevars;  // json preparation collects a list of freevars  
  void* nullvars;   // json preparation creates a list of vars replacing nulls   
  void* boundvars;  // json preparation keeps list of lists of quantifier-bound vars
  int freevarsnr; // length of the previous list
  int nullvarsnr; // one less than the nr used for next var created

  int askpolarity;
  int askinfo;
  int json; // set to 1 if json output wanted (used in error printing)
  int parse_top_level; // set to 1 for json top level handling, 0 inside:
                       // print conversion if requested and value is 1
  void* jsonld_typekey;  // set to mpool atom "@type"
  void* jsonld_typerepl; // set to mpool atom "http://www.w3.org/1999/02/22-rdf-syntax-ns#type"  
  void* jsonld_vocabkey;  // set to mpool atom "@vocab"
  void* jsonld_basekey;  // set to mpool atom "@base"
  int jsonld_blankseed; // seed for json-ld blank random, diff for each doc 
  int jsonld_blankcount; // 0 means none have been made, to be increased each time        
  void* jsonld_blankprefatom; // mpool atom containing id unique prefix for this doc             
  void* jsonld_blanks; // an mpool key-value object of json blank node name and logic value
  void* jsonld_ctxt;  // an mpool key-value object of context elements, used as a stack
  void* jsonld_base;  // an mpool atom for base of id
  void* jsonld_graphid;  // an mpool atom for graph id creating $narc

  void* nests[PARSE_NESTING_DEPTH]; // stack for pure json parsing

} parse_parm;

#define YYSTYPE         char*
#define YY_EXTRA_TYPE   parse_parm *

#ifdef _WIN32
#define YY_NO_UNISTD_H  // for windows 
#else
#undef YY_NO_UNISTD_H
#endif


#endif
