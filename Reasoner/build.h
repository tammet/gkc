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


 /** @file build.h
 *  Term and clause building functions.
 *
 */


#ifndef DEFINED_BUILD_H
#define DEFINED_BUILD_H


#include "glb.h" 

#define CVEC_ALLOC_ALIGNMENT_BYTES 8

gptr wr_build_calc_cl(glb* g, gptr x);
gint wr_build_calc_term(glb* g, gint x);
gint wr_build_calc_term_copyground(glb* g, gint x);
gint wr_build_calc_term_replace(glb* g, gint x, int replpos, gint replterm, int* path);

int wr_answer_lit(glb* g, gint atom);
int wr_specialcomp_lit(glb* g, gint atom);
int wr_prop_truefalsity_clause(glb* g, gptr cl);
int wr_special_prop_lit(glb* g, gint atom, char* argstr);
int wr_equality_atom(glb* g, gint atom);


int wr_computable_termptr(glb* g, gptr yptr);
gint wr_compute_from_termptr(glb* g, gptr tptr, int comp);
//gint wr_compute_fun_plus(glb* g, gptr tptr);
gint wr_compute_fun_equal(glb* g, gptr tptr);
gint wr_compute_fun_equal_terms(glb* g, gint a, gint b, gptr tptr);
gint wr_compute_fun_lesseq_core(glb* g, gptr tptr, gint ifequal, int isless);
gint wr_compute_fun_arith2(glb* g, gptr tptr, int comp);
gint wr_compute_fun_str(glb* g, gptr tptr, int comp);
gint wr_compute_fun_arith1(glb* g, gptr tptr, int comp);
int wr_uri_is_unique(glb* g, gint fun);
int wr_uri_is_list(glb* g, gint fun);
int wr_record_is_list(glb* g, gptr tptr);
gint wr_compute_fun_list(glb* g, gptr tptr, int opcode);

// two argument predicate funs

#define COMP_FUN_EQUAL 1
#define COMP_FUN_LESS 3

#define COMP_FUN_LESSEQ 4
#define COMP_FUN_GREATEREQ 5
#define COMP_FUN_GREATER 6

// two argument computing funs

#define COMP_FUN_PLUS 10
#define COMP_FUN_MINUS 11
#define COMP_FUN_MULT 12
#define COMP_FUN_DIV 13
#define COMP_FUN_QUOTIENT_E 14
#define COMP_FUN_QUOTIENT_T 15
#define COMP_FUN_QUOTIENT_F 16
#define COMP_FUN_REMAINDER_E 17
#define COMP_FUN_REMAINDER_T 18
#define COMP_FUN_REMAINDER_F 19

// single argument predicate funs

#define COMP_FUN_IS_INT 20
#define COMP_FUN_IS_REAL 21
#define COMP_FUN_IS_NUMBER 22
#define COMP_FUN_IS_LIST 23
#define COMP_FUN_IS_MAP 24
#define COMP_FUN_IS_ATOM 25
#define COMP_FUN_IS_UNIQUE 26

// single argument computing funs

#define COMP_FUN_TO_INT 30
#define COMP_FUN_TO_REAL 31
#define COMP_FUN_FLOOR 32
#define COMP_FUN_CEILING 33
#define COMP_FUN_TRUNCATE 34
#define COMP_FUN_ROUND 35
#define COMP_FUN_UMINUS 36
#define COMP_FUN_STRLEN 37
#define COMP_FUN_IS_SUBSTR 38

#define COMP_FUN_FIRST 40
#define COMP_FUN_REST 41

// three argument funs

#define COMP_FUN_IS_SUBSTRAT 50



#endif
