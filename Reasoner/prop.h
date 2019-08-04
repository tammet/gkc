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

 /** @file prop.h
 * Propositional reasoning
 */

/* ====== Includes =============== */


#ifndef DEFINED_PROP_H
#define DEFINED_PROP_H

//#include "rincludes.h"  
#include "glb.h"

int wr_cl_create_propinst(glb* g, gptr cl);
gint wr_make_get_propvar(glb* g, gint atom);

void wr_free_prop_clauses(glb* g, gptr arr);

gint* wr_push_prop_termhash(glb* g, gint* hasharr, int hash, gptr term, gint gconst, gint pvar);
gint* wr_push_offset_prop_termhash(glb* g, gint* hasharr, int hash, gptr term, gint gconst, gint pvar);
gint wr_find_prop_termhash(glb* g, gint* hasharr, gptr term, int hash, gint gconst);
gint wr_find_offset_prop_termhash(glb* g, gint* hasharr, gptr term, int hash, gint gconst);
void wr_free_prop_termhash(glb* g, gint* hasharr);
void wr_free_offset_prop_termhash(glb* g, gint* hasharr);
void wr_print_prop_termhash(glb* g, gint* hasharr);
void wr_print_offset_prop_termhash(glb* g, gint* hasharr);

gint* wr_push_prop_clausehash(glb* g, gint* hasharr, int hash, gptr term);
gint* wr_push_offset_prop_clausehash(glb* g, gint* hasharr, int hash, gptr term);
int wr_find_prop_clausehash(glb* g, gint* hasharr, gptr rptr, int rpos, int hash);
int wr_find_offset_prop_clausehash(glb* g, gint* hasharr, gptr rptr, int rpos, int hash);
void wr_free_prop_clausehash(glb* g, gint* hasharr);
void wr_free_offset_prop_clausehash(glb* g, gint* hasharr);
void wr_print_prop_clausehash(glb* g, gint* hasharr);
void wr_print_offset_prop_clausehash(glb* g, gint* hasharr);

int wr_equal_instgen_halfblt_queue_clause(glb* g, gptr rptr, int rpos, gptr queuecl);
int wr_equal_instgen_clauses(glb* g, gptr xcl, gptr ycl);
gint wr_equal_grounded_term(glb* g, gint x, gint y, gint xg, gint yg, int uniquestrflag);
//gint wr_equal_grounded_term(glb* g, gint x, gint y, gint xg, gint yg, int uniquestrflag);

int wr_prop_solve_current(glb* g);
int wr_make_prop_file(glb* g, char* fname);
int wr_fprint_prop_clause(glb* g, FILE* fptr, gint* rec);
int wr_fprint_prop_clause_atom_term(glb* g, FILE* fptr, gint* rec);

#endif
