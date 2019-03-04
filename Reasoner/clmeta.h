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

 /** @file clmeta.h
 * Clause and term metainfo functions. 
 */


#ifndef DEFINED_CLMETA_H
#define DEFINED_CLMETA_H

/* ==== Includes ==== */

#include "types.h"
#include "glb.h"
#include "clterm.h"

/* ==== Global defines ==== */

#define CONSTANT_WEIGHT 10
#define VAR_WEIGHT 5

#define CLMETABLOCK_ELS 6
#define CLMETABLOCK_LENGTHS_POS 0
#define CLMETABLOCK_SIZES_POS 1
#define CLMETABLOCK_PREF1BITS_POS 2
#define CLMETABLOCK_PREF2BITS_POS 3
#define CLMETABLOCK_PREF3BITS_POS 4
#define CLMETABLOCK_CL_POS 5

#define CLMETABLOCK_ISGROUND_SHIFT 8*3
#define CLMETABLOCK_LENGTH_SHIFT 8*2
#define CLMETABLOCK_NEGLENGTH_SHIFT 8
#define CLMETABLOCK_GROUNDLENGTH_SHIFT 0
#define CLMETABLOCK_DEPTH_SHIFT 8*2
#define CLMETABLOCK_SIZE_SHIFT 8
#define CLMETABLOCK_PREFLEN_SHIFT 0

// metacalc is used while calculating metainfo of a clause and a term

typedef struct {  
  int hasvars; // 1 iff ground
  int depth;  // clause depth: 1 is predicate without nesting
  int size;   // clause size: count of all pred/fun/const/var occurrences
  int weight;  // weighted size
  int preflen; // length of ground prefix: for ground lit it is size
  int topvars;  // nr of var occurrences on top lit level
  int prefhash;  // hash of ground prefix
  int pref1hash;  // pref len 1 (pred symbs with sign)
  int pref2hash;  // pref len 2 (pred symbs with sign + immediate args
  int pref3hash;  // pref len 3
  int groundhash; // not used ??
} term_metacalc;

typedef struct {  
  int length;  // clause length (nr of literals)
  int neglength;  // nr of negative literals
  int groundlength;  // nr of ground literals
  int hasvars; // 1 iff ground
  int depth;  // clause depth: 1 is predicate without nesting
  int size;   // clause size: count of all pred/fun/const/var occurrences
  int weight;  // weighted size
  int preflen; // length of ground prefix: for ground lit it is size
  int topvars;  // nr of var occurrences on top lit level
  int prefhash;  // hash of ground prefix

  gint pref1bits; // pref len 1 (pred symbs with sign)
  gint pref2bits; // pref len 2 (pred symbs with sign + immediate args
  gint pref3bits; // pref len 3
  gint groundbits; // one bit for each full-ground lit (from ground lit hash)

  int pref1hash;
  int pref2hash;
  int pref3hash;
  int groundhash; 

} cl_metacalc;

/* ==== Protos ==== */

int wr_calc_clause_weight(glb* g, gptr xptr, int* size, int* depth, int* length);
int wr_calc_term_weight(glb* g, gint x, int depth, int* size, int* maxdepth);

gint wr_calc_clause_meta(glb* g, gptr xptr, gptr cl_metablock);
void wr_print_term_metacalc(glb* g, term_metacalc* metaptr);
void wr_print_cl_metacalc(glb* g, cl_metacalc* metaptr);
gint wr_litmeta_to_gint(glb* g, term_metacalc* metaptr);
gint wr_signed_lithash_to_bitmask(glb* g, int hash, int negflag);
gint wr_lithash_to_bitmask(glb* g, int hash);
void wr_calc_term_meta(glb* g, gint x, int depth, int pos, term_metacalc* metaptr);

gint wr_decorate_clause(glb* g, gptr xptr);
void wr_decorate_term(glb* g, gint x, int depth, int pos, term_metacalc* metaptr);

void wr_print_cl_meta(glb* g, gint clmeta1);
void wr_print_gint_hashmask(glb* g, gint mask);

void wr_sort_cl(glb* g, gptr cl);
int wr_sort_meta_bigger(gint jmeta,  gint imeta);  

#endif
