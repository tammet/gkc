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

/** @file rewrite.h
 *  Term rewriting (demodulation) header.
 *
 */

#ifndef DEFINED_REWRITE_H
#define DEFINED_REWRITE_H

#include "glb.h"

/* the index of the demodulators: 64-bit builds only; a 32-bit build and a
   build with -DGKC_NO_DEMOD_INDEX walk the lists */
#if defined(USE_TERM_INDEX) && !defined(GKC_NO_DEMOD_INDEX)
#define USE_DEMOD_INDEX
#endif
#ifdef USE_DEMOD_INDEX
void wr_dmi_insert(glb* g, gint term, gint node);
int wr_dmu_insert(glb* g, gint term, gint node);
int wr_dmi_best(glb* g, gint x, gint* node, gint* rhs);
int wr_match_is_renaming(glb* g);
int wr_demod_derived(glb* g, gptr rptr, int rpos);
int wr_demod_repair(glb* g, gptr rptr, int* rpos);
gint wr_demod_norm(glb* g, gint t, int eqatom);
void wr_demod_add_rewriters(glb* g, int cuts);
#endif
void wr_dmi_free(glb* g);
gint wr_rewrite_term(glb* g, gint x); 
gint wr_rewrite_constant(glb* g, gint x);

#endif
