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

 /** @file rgenloop.h
 *  Procedures for reasoner top level search loops: given-clause, usable, sos etc.
 *
 */

#ifndef DEFINED_RGENLOOP_H
#define DEFINED_RGENLOOP_H

#include "types.h"
#include "glb.h"

int wr_genloop(glb* g);
gptr wr_pick_given_cl(glb* g, gptr cl_metablock);
gptr wr_activate_passive_cl(glb* g, gptr picked_given_cl_cand, gptr cl_metablock);
gptr wr_add_given_cl_active_list(glb* g, gptr given_cl, gptr given_cl_metablock, 
  int subsflag,  gptr build_buffer, cvec resolvability);
gptr wr_process_given_cl(glb* g, gptr given_cl_cand, gptr buf);
int wr_add_cl_to_active_unithash(glb* g, gptr cl);

void wr_process_given_cl_setupsubst(glb* g, gptr buf, gint banknr, int reuseflag);
void wr_process_given_cl_cleanupsubst(glb* g);

#endif
