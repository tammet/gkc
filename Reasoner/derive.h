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

 /** @file derive.h
 * Headers for clause derivation functions. 
 */


#ifndef DEFINED_DERIVE_H
#define DEFINED_DERIVE_H

/* ==== Includes ==== */

#include "types.h"
#include "glb.h"

/* ==== Global defines ==== */

#define BUILD_BUFFER_SIZE 1000 // new clauses built in a buffer dynamically allocated

/* ==== Protos ==== */

void wr_process_resolve_result
      (glb* g, gint xatom, gptr xcl, gint yatom, gptr ycl, gptr xcl_as_active);
void wr_process_factor_result
      (glb* g, gptr xcl, int len, int x, gint xatom, int y, gint yatom, gptr xcl_as_active);
void wr_process_paramodulate_result
      (glb* g, gint xatom, gptr xcl, gint yatom, gptr ycl, gptr xcl_as_active, 
      gint aterm, gint bterm, gint path, int leftflag, int fromflag);

int wr_process_resolve_result_aux
      (glb* g, gptr cl, gint cutatom, int atomnr, gptr rptr, int* rpos, int* cutpos,
       gint replpath, gint replterm);
void wr_process_resolve_result_remove_cuts(glb* g, gptr rptr, int* rpos, int cuts);
gptr wr_derived_build_cl_from_initial_cl(glb* g, gptr rptr, int rpos, int ruleflag, gint history);

int wr_derived_weight_check(glb* g, double avg, int weight,  int size, int depth, int length);

gint wr_add_cl_to_unithash(glb* g, gptr cl, gint clmeta);
int wr_process_resolve_result_isrulecl(glb* g, gptr rptr, int rpos);
int wr_cl_derived_is_answer(glb* g,gptr cl);

void wr_process_resolve_result_setupsubst(glb* g);
void wr_process_resolve_result_setupgivencopy(glb* g);
void wr_process_resolve_result_setupquecopy(glb* g);
void wr_process_resolve_result_setupclpickstackcopy(glb* g) ;
void wr_process_simp_setupquecopy(glb* g);

void wr_process_resolve_result_cleanupsubst(glb* g);

int wr_hyperres_satellite_cl(glb* g,gptr cl);
int wr_hyperres_satellite_tmpres(glb* g,gptr tmpres, int respos);



// void resolve_binary_all_active(gptr cl1);

/*
void resolve_binary(gptr cl1, gptr cl2);
gptr factor_step(gptr incl);
int simplify_cl_destr(gptr cl, int given_flag);
int can_cut_lit(gptr litpt1, int unify_flag, int given_flag);

void proc_derived_cl(gptr incl);
void proc_derived_cl_binhist(gptr incl, gint clid1, gint clid2, gint litpos1, gint litpos2);
void proc_input_cl(gptr incl);
*/

#endif
