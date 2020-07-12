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
int wr_prop_truefalsity_clause(glb* g, gptr cl);
int wr_special_prop_lit(glb* g, gint atom, char* argstr);
int wr_equality_atom(glb* g, gint atom);


int wr_computable_termptr(glb* g, gptr yptr);
gint wr_compute_from_termptr(glb* g, gptr tptr, int comp);
gint wr_compute_fun_plus(glb* g, gptr tptr);
gint wr_compute_fun_equal(glb* g, gptr tptr);
gint wr_compute_fun_less(glb* g, gptr tptr);
gint wr_compute_fun_arith2(glb* g, gptr tptr, int comp);

#define COMP_FUN_EQUAL 1
#define COMP_FUN_LESS 3
#define COMP_FUN_PLUS 1000
#define COMP_FUN_MINUS 1001
#define COMP_FUN_MULT 1002
#define COMP_FUN_DIV 1003

#endif
