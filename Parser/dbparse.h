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

 /** @file dbparse.h
 *  Top level/generic headers and defs for parsers
 *
 */

#ifndef DEFINED_DBPARSE_H
#define DEFINED_DBPARSE_H

#include "../Db/dballoc.h"
#include "../Reasoner/mem.h"
#include "../Reasoner/glb.h" 
#include "../Reasoner/history.h" 

#include "../cjson/cjson.h"

#define OTTER_DECIMAL_SEPARATOR '.'
#define DEFAULT_SKOLEM_PREFIX "$sk"
#define DEFAULT_NEWPRED_PREFIX "$df"
#define PROPVAR_PREDSYMB "$pr"
#define PARSER_MEMPOOL_SIZE 1000000

#define PARSER_AXIOM_ROLENR 10
#define PARSER_ASSUMPTION_ROLENR 2
#define PARSER_GOAL_ROLENR 1
#define PARSER_EXTAXIOM_ROLENR 12

#define VARDATALEN 1000
#define IMPORTED_NAME_PREFIX "$inc_"

#define STORE_SOURCE_FRM 
#define STORE_SKOLEM_STEPS
#define SKOLEM_CLNAME_SUFFIX "_$sk"
#define SKOLEM_CLNAME_SUFFIX_LEN 4

int wr_import_json_file(glb* g, char* filename, char* strasfile, cvec clvec, int isincluded);
int wr_import_otter_file(glb* g, char* filename, char* strasfile, cvec clvec, int isincluded);
//int wg_import_otter_file(void* db, char* filename, int printlevel);
int wr_import_prolog_file(glb* g, char* filename, char* strasfile, cvec clvec);


void* wr_preprocess_clauselist(glb* g,void* mpool,cvec clvec,void* clauselist,int isincluded);
void* wr_parse_clauselist(glb* g,void* mpool,cvec clvec,void* clauselist);
void* wr_parse_clause(glb* g,void* mpool,void* cl,cvec clvec,
       char** vardata,void* propfun, void* name, void* role);
void* wr_parse_atom(glb* g,void* mpool,void* term, int isneg, int issimple, char** vardata);
void* wr_parse_term(glb* g,void* mpool,void* term, char** vardata, gint* primitiveres);
void* wr_parse_preprocess_list(glb* g,void* mpool,void* term);
gint wr_parse_primitive(glb* g,void* mpool,void* atomptr, char** vardata, int pos);

void wg_expand_frm_for_print(void* db, void* mpool, void* frm);
void* wg_expand_frm_for_print_aux(void* db, void* mpool, void* cl);

gint wr_parse_and_encode_otter_prim(glb* g, char *buf);
gint wr_parse_and_encode_otter_uri(glb* g, char *buf);

gint wr_print_parseres(glb* g, gint x);

void* wr_preprocess_tptp_cnf_clause(glb* g, void* mpool, void* cl);
int wr_is_parse_var(glb* g,void* ptr);
void* wr_preprocess_tptp_fof_clause(glb* g, void* mpool, void* cl, void* clname);
void* wr_process_tptp_import_clause(glb* g, void* mpool, void* cl);

int wr_show_parse_error(glb* g, char* format, ...);

#endif
