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
#if defined(__SIZEOF_POINTER__) && (__SIZEOF_POINTER__ == 8) && !defined(GKC_NO_DEMOD_INDEX)
#define USE_DEMOD_INDEX
#endif
#ifdef USE_DEMOD_INDEX
void wr_dmi_insert(glb* g, gint term, gint node);
int wr_dmi_lookup(glb* g, gint x, gint** nodes);
#endif
void wr_dmi_free(glb* g);
gint wr_rewrite_term(glb* g, gint x); 
gint wr_rewrite_constant(glb* g, gint x);

#endif
