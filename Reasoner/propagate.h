/*
* Copyright (c) Tanel Tammet 2004-2019
*
* Contact: tanel.tammet@gmail.com                 
*
* This file is part of WhiteDB
*
* WhiteDB is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
* 
* WhiteDB is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License
* along with WhiteDB.  If not, see <http://www.gnu.org/licenses/>.
*
*/

 /** @file propagate.h
 *  Propagating units and setting up required datastructures.
 *
 */
 


#ifndef DEFINED_PROPAGATE_H
#define DEFINED_PROPAGATE_H

#include "glb.h" 

void wr_propagate_unit(glb* g, gptr cl, gint clmeta, gint hash, gptr cl_as_active);
void wr_resolve_propagated_clause(glb* g, gint atom, gptr cl, gptr cutcl, int negflag, gptr cl_as_active);  
void wr_process_propagated_result
      (glb* g, gint xatom, gptr xcl, gint yatom, gptr ycl, gptr xcl_as_active, int cutpos2);        

int wr_store_atom_occurrences(glb* g, gptr cl);  

#endif
