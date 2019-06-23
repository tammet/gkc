/*
*
* Copyright (c) Tanel Tammet 2019
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

 /** @file chains.c
 * Chain and other non-direct resolution,
 */

/* ====== Includes =============== */


#ifdef __cplusplus
extern "C" {
#endif

#include "rincludes.h"   
 

/* ====== Private defs =========== */


//#define DEBUG
#undef DEBUG  


/* ====== Private headers ======== */

  
/* ====== Functions ============== */


/* =====================================================

 

====================================================== */


void check_process_chain_clause(glb* g, gptr cl) {
  void* db=g->db;
  int len, start, a0end, a1end;
  gint atom0, atom1, meta0, meta1; // negsymb, possymb;
  gptr a0ptr, a1ptr;

  len=wg_count_clause_atoms(db,cl);
  if (len!=2) return;

  atom0=wg_get_rule_clause_atom(db,cl,0);
  meta0=wg_get_rule_clause_atom_meta(db,cl,0);
  atom1=wg_get_rule_clause_atom(db,cl,1);
  meta1=wg_get_rule_clause_atom_meta(db,cl,1);

  a0ptr=decode_record(db,atom0);
  a1ptr=decode_record(db,atom1);
 
  start=wr_term_unify_startpos(g);
  a0end=wr_term_unify_endpos(g,a0ptr);
  a1end=wr_term_unify_endpos(g,a0ptr);
  if (start+1>=a0end || start+1>=a1end ||
      start+2!=a0end || start+2!=a1end) return;
  // check first argument: same and var?
  if (a0ptr[start+1]!=a1ptr[start+1] ||
      !isvar(a0ptr[start+1])) 
    return;
  
  // wr_printf("\nappears to be chain clause ok");

  if (wg_atom_meta_is_neg(db,meta0) &&
      !wg_atom_meta_is_neg(db,meta1)) {
    //negsymb=a0ptr[start];
    //possymb=a1ptr[start];
  } else if (!wg_atom_meta_is_neg(db,meta0) &&
             wg_atom_meta_is_neg(db,meta1)) {
    //negsymb=a1ptr[start];
    //possymb=a0ptr[start];
  } else {
    // same polarity
    return;
  }

  (g->in_chain_clause_count)++;
  //printf("\nnegsymb ");
  //wr_print_term(g,negsymb);
  //printf("\npossymb "); 
  //wr_print_term(g,possymb);
  //printf("\n");
}

#ifdef __cplusplus
}
#endif



