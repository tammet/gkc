/*
*
* Copyright (c) Tanel Tammet 2004,2005,2006,2007,2008,2009,2010
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

 /** @file resolve.h
 *  Procedures for the first part of new clause derivation (binary, factor, para, ...),
 *  with the result clause created by procedures in derive.c
 *
 */

#ifndef DEFINED_RESOLVE_H
#define DEFINED_RESOLVE_H

#include "types.h"
#include "glb.h"

void wr_resolve_binary_all_active(glb* g, gptr cl, gptr cl_as_active);
void wr_paramodulate_from_all_active(glb* g, gptr cl, gptr cl_as_active);
void wr_factor(glb* g, gptr cl, gptr cl_as_active);

void wr_paramodulate_into_all_active(glb* g, gptr cl, gptr cl_as_active);
int wr_paramodulate_into_subterms_all_active(glb* g, gptr cl, gptr cl_as_active, gint atom, gint term, 
      int depth, int litnr, int* termpath, int nonanslen);

void wr_resolve_equality_reflexive(glb* g, gptr cl, gptr cl_as_active);

#endif
