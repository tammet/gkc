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
void wr_process_instgen_result
      (glb* g, gint xatom, gptr xcl, gint yatom, gptr ycl, gptr xcl_as_active, int usexcl);      
void wr_process_factor_result
      (glb* g, gptr xcl, int len, int x, gint xatom, int y, gint yatom, gptr xcl_as_active);
void wr_process_paramodulate_result
      (glb* g, gint xatom, gptr xcl, gint yatom, gptr ycl, gptr xcl_as_active, 
      gint aterm, gint bterm, gint path, int leftflag, int fromflag);

int wr_process_resolve_result_aux
      (glb* g, gptr cl, gint cutatom, int atomnr, gptr rptr, int* rpos, int* cutpos,
       gint replpath, gint replterm);
int wr_tautology_cl(glb* g, gptr cl);       
int wr_find_strong_duplicates(glb* g, gptr rptr, int* rpos);  
int wr_mark_repeated_vars(glb* g, gint x);     
int wr_process_instgen_result_aux
      (glb* g, gptr cl, gint cutatom, int atomnr, gptr rptr, int* rpos, int* cutpos, 
       gint replpath, gint replterm);      
void wr_process_resolve_result_remove_cuts(glb* g, gptr rptr, int* rpos, int cuts);
gptr wr_derived_build_cl_from_initial_cl(glb* g, gptr rptr, int rpos, int ruleflag, gint history);

int wr_derived_weight_check(glb* g, double avg, int weight,  int size, int depth, int length, int xatomnr, int yatomnr);

gint wr_add_cl_to_unithash(glb* g, gptr cl, gint clmeta);
gint wr_add_cl_to_doublehash(glb* g, gptr cl);
int wr_process_resolve_result_isrulecl(glb* g, gptr rptr, int rpos);
int wr_cl_derived_is_answer(glb* g,gptr cl);

void wr_process_resolve_result_setupsubst(glb* g);
void wr_process_resolve_result_setupgivencopy(glb* g);
void wr_process_resolve_result_setupquecopy(glb* g);
void wr_process_resolve_result_setuphypercopy(glb* g);
void wr_process_resolve_result_setupclpickstackcopy(glb* g) ;
void wr_process_simp_setupquecopy(glb* g);


void wr_process_resolve_result_cleanupsubst(glb* g);

int wr_hyperres_satellite_cl(glb* g,gptr cl);
int wr_hyperres_satellite_tmpres(glb* g,gptr tmpres, int respos);


#endif
