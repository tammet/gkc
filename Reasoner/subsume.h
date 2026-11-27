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

 /** @file subsume.h
 *  Public headers for subsumption functions.
 *
 */
 


#ifndef DEFINED_SUBSUME_H
#define DEFINED_SUBSUME_H

#include "glb.h" 

int wr_given_cl_subsumed(glb* g, gptr given_cl, gptr given_cl_metablock);
int wr_given_cl_backsubsume(glb* g, gptr given_cl, gptr given_cl_metablock);

void wr_set_cl_backsubsumed(glb* g,gptr cl);
int wr_get_cl_backsubsumed(glb* g,gptr cl);

int wr_clmetablock_can_subsume(glb* g, gptr genblock, gptr specblock);
int wr_active_subsumed_lit(glb* g, gint atom, int negflag);
int wr_atom_cut_and_subsume(glb* g, gint xatom, gint xatommeta, cvec* foundbucket, int subsflag);
int wr_atom_doublecut(glb* g, gint xatom, gint xatommeta, gptr xcl, int pos, cvec* foundbucket);
int wr_atom_doublecut_h(glb* g, gint xatom, gint xatommeta, gptr xcl, int pos,
                        cvec* foundbucket, gint hash);
#define DOUBLEPAIR_MAXLEN 16 // longer clauses use the bucket walk of wr_atom_doublecut_h
#define DOUBLEPAIR_MINENTRIES 2 // a bucket with more entries per pair probe uses the pair table
void wr_push_doublepair(glb* g, gptr cl, gint h0, int neg0, gint h1, int neg1);
int wr_atom_doublecut_hp(glb* g, gint xatom, gint xatommeta, gptr xcl, int len, int pos,
                         cvec* foundbucket, gint hash, gint* hashes);
int wr_atom_doublecut_aux(glb* g, gint xatom, gint xatommeta, gptr xcl, int pos, cvec bucket);
int wr_derived_cl_strong_cut(glb* g, gptr rptr, int rpos, int prevcuts); 
gptr strongly_cut_atom(glb* g, gint xatom, vec hashvec, int usematch);
gptr strongly_cut_atom_lists(glb* g, gint xatom, vec hashvec, int usematch);

/* the strong cut on the trees of termindex.c: 64-bit builds only; a 32-bit
   build (wasm) and a build with -DGKC_NO_SCUT_INDEX use the lists */
#if defined(USE_TERM_INDEX) && !defined(GKC_NO_SCUT_INDEX)
#define USE_SCUT_INDEX
#endif
#ifdef USE_SCUT_INDEX
void wr_sci_insert(glb* g, gint atom, gptr cl, int neg, int preflen);
gptr wr_sci_cut_atom(glb* g, gint xatom, int neg, int usematch, int* used, vec hashvec);
#endif
void wr_sci_free(glb* g);
int wr_derived_cl_cut_and_subsume(glb* g, gptr rptr, int rpos,  gptr clhashptr);
void wr_ucm_free(glb* g);
void wr_ucm_insert(glb* g, gptr cl, gptr xatom, int neg);

gint wr_subsume_cl(glb* g, gptr cl1, gptr cl2, int uniquestrflag);
void wr_fsi_insert(glb* g, gptr cl, gint pos);
void wr_fsi_free(glb* g);
gint wr_subsume_cl_aux(glb* g,gptr cl1vec, gptr cl2vec, 
	                  gptr litpt1, gptr litpt2, 
	                  int litind1, int litind2, 
                    int cllen1, int cllen2,
                    int uniquestrflag);                    

#endif
