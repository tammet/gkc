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

int wr_clmetablock_can_subsume(glb* g, gptr genblock, gptr specblock);
int wr_active_subsumed_lit(glb* g, gint atom, int negflag);
int wr_atom_cut_and_subsume(glb* g, gint xatom, gint xatommeta, cvec* foundbucket, int subsflag);
int wr_derived_cl_cut_and_subsume(glb* g, gptr rptr, int rpos);

gint wr_subsume_cl(glb* g, gptr cl1, gptr cl2, int uniquestrflag);
gint wr_subsume_cl_aux(glb* g,gptr cl1vec, gptr cl2vec, 
	                  gptr litpt1, gptr litpt2, 
	                  int litind1, int litind2, 
                    int cllen1, int cllen2,
                    int uniquestrflag);                    

#endif
