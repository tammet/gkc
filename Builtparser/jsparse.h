/*
*
* Copyright (c) Tanel Tammet 2004-2019
*
* Contact: tanel.tammet@gmail.com                 
*
* This file is part of GK
*
*/

 /** @file jsparse.h
 *  headers for json input data parser 
 *
 */

#ifndef DEFINED_JSPARSE_H
#define DEFINED_JSPARSE_H

#include "../Db/dballoc.h"
#include "../Reasoner/mem.h"
#include "../Reasoner/glb.h" 
#include "../Reasoner/history.h" 

#include "../cjson/cjson.h"
#include "dbgenparse.h"  

#define OTTER_DECIMAL_SEPARATOR '.'
#define DEFAULT_SKOLEM_PREFIX "$sk"
#define DEFAULT_NEWPRED_PREFIX "$df"
#define PROPVAR_PREDSYMB "$pr"
#define PARSER_MEMPOOL_SIZE 1000000

#define PARSER_AXIOM_ROLENR 10
#define PARSER_ASSUMPTION_ROLENR 2
#define PARSER_GOAL_ROLENR 1
#define PARSER_EXTAXIOM_ROLENR 12

#define MPOOL_STRUCT_PREFSTR "$gk_struct$"
#define MPOOL_JSON_NULL "$gk_null$"
#define MPOOL_JSON_EMPTY "$nil"
#define MPOOL_NULLVAR_PREFIX "Nvar"
#define MPOOL_JSON_FOF "fof"
#define MPOOL_JSON_ASK "ask"
#define MPOOL_JSON_ARC "$arc"
#define MPOOL_JSON_NARC "$narc"

//#define MPOOL_JSON_TRUE "$gk_true$" // use logical true instead
//#define MPOOL_JSON_FALSE "$gk_false$" // use logical false instead

int wg_import_js_file(void *db, char* filename, int iskb, int* informat,
        int askpolarity, int* askinfo, int streaming);
int wg_import_js_string(void *db, char* instr, int iskb, int* informat,
        int askpolarity, int* askinfo, int streaming);
        
int wr_import_js_file(glb* g, char* filename, char* strasfile, cvec clvec, int isincluded,
      int askpolarity, int* askinfo, int streaming);

int wr_yajl_parse_file(glb* g, parse_parm* pp, char* filename,
      cvec clvec, int isincluded,
      int askpolarity, int* askinfo, void* mpool, int streaming);

int wr_yajl_parse_string(glb* g, parse_parm* pp, char* strasfile);

int wg_is_jsfile(void* db,char* filename);
int wg_is_js_streaming_file(void* db,char* filename);
//int wg_getline(unsigned char **restrict line, size_t *restrict len, FILE *restrict fp);

int wg_getline(unsigned char **line, size_t *len, FILE *fp);

void* wr_preprocess_json_clauselist
        (glb* g,parse_parm* pp,cvec clvec,void* clauselist, int isincluded,
        int streaming);


int wr_is_json_cnf_clause(void* db, void* cl);
int wr_is_json_fof_clause(void* db, void* cl);
int wr_is_json_import_clause(void* db, void* cl);
void* wr_preprocess_json_cnf_clause(glb* g, void* mpool, void* cl);
void* wr_preprocess_json_fof_clause(glb* g, void* mpool, void* cl);
void* wr_process_json_import_clause(glb* g, void* mpool, void* cl, cvec clvec);

void* wr_process_json_formula(glb* g,parse_parm* pp, void* cl, int isincluded);
void* wr_process_json_formula_aux(glb* g, parse_parm* pp, void* cl);
void* wr_process_json_term(glb* g, parse_parm* pp, void* ptr, int atomlevel, int pos);
void* wr_process_json_list(glb* g, parse_parm* pp, void* cl);
//void* wg_make_nested_conn(void* db, void* mpool, void* op, void* cl);

void* wr_json_list_logconn(glb* g, parse_parm* pp, void* cl);
void* wr_json_translate_connective(glb* g,parse_parm* pp,void* ptr);
int wr_json_negtype_connective(glb* g,parse_parm* pp,void* ptr);
int wr_is_json_freevar(glb* g,parse_parm* pp,void* ptr);

void* wr_json_unflatten_logic(glb* g, parse_parm* pp, void* ptr, void* conn, void* newconn, 
        int ispos,int isreverse);
void* wr_json_make_nullvar(glb* g, parse_parm* pp);

int wr_json_is_atomlist(void* db,void* ptr);
int wr_json_is_connective(glb* g,void* ptr);
int wr_json_is_eq_op(glb* g,void* ptr);
int wr_json_equal_atoms(glb* g,void* a1, void* a2);
int wr_json_really_equal_atoms(glb* g,void* a1, void* a2);

void* wr_json_process_if_then(glb* g, parse_parm* pp, void* ptr);
void* wr_collect_json_freevars(glb* g, void* vars, void* cl);

void* wr_js_parse_clauselist(glb* g,void* mpool,cvec clvec,void* clauselist);
void* wr_js_parse_clause(glb* g,void* mpool,void* cl,cvec clvec,
        char** vardata,void* propfun, void* name, void* role, void* context);

void* wr_process_json_formula_struct(glb* g, parse_parm* pp, void* cl, int isincluded);
void* wr_add_struct_key_atoms(glb* g, parse_parm* pp, 
         void* id, char* keystr, void* values, void* extended);
int wr_json_is_expandable_uristr(glb* g, char* keystr);    
void* wr_json_mkarc(glb* g, parse_parm *pp, void* id, void* pred, void* obj);
void wr_json_push_context(glb* g, parse_parm *pp, gint* array);
void wr_json_pop_context(glb* g, parse_parm *pp, gint* array);     
void* wr_json_expand_list_prefixes(glb* g, void* mpool, parse_parm* pp, void* lst,int depth);
void* wr_process_json_formula_keyextend(glb* g, parse_parm* pp, void* cl, void* origcl, int isincluded);
void* wr_json_mkid(glb* g, void* mpool, parse_parm* pp, void* givenid, void* map);
int wr_json_special_keystr(glb* g, char* keystr);
int wr_json_special_logicstr(glb* g, char* keystr);
int wr_json_freevarstr(glb* g, char* keystr);
int wr_json_isvar(glb* g, parse_parm* pp, void* atom);
void* wr_json_add_with_and(glb* g, parse_parm* pp, void* frm, void* previous);
void* wr_json_add_with_negated_or(glb* g, parse_parm* pp, void* frm, void* previous);
void* wr_add_keyval_jsstruct(glb *g, parse_parm* pp, 
        void* key, void* keyval, void* jstruct);
int wr_json_extend_context(glb* g, parse_parm* pp, void* ctxt);
char* wg_modify_keystr_by_ctxt(glb* g, parse_parm* pp, char* buf, int buflen, char* keystr);
void* wr_json_expand_atom(glb* g, void* mpool, parse_parm* pp, void* lst, int depth);
int wg_suitable_context_uri_end(void* db, char c);

#endif
