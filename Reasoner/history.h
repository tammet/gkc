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

 /** @file history.h
 * Procedures for managing clause history.
 */

#ifndef DEFINED_HISTORY_H
#define DEFINED_HISTORY_H

#ifdef _WIN32
#include "../config-w32.h"
#else
#include "../config.h"
#endif

#include "../Db/dballoc.h"
#include "../Db/dbmpool.h"

#include "types.h"
#include "glb.h"

/*

history structure for input, factorial, resolve, para:

priority     name 
priority     name          tag  parent1  cut1                .... cutn 
priority     name          tag  parent1  parent2       cut1                .... cutn 
priority     name          tag  path     para_parent1  para_parent2 cut1  .... cutn 
 history_prefix_len  | 

where 

*/

#define RECORD_HISTORY_ORDER

#ifdef RECORD_HISTORY_ORDER

#undef SHOW_HISTORY_ORDER
#define HISTORY_PRIORITY_POS 0
#define HISTORY_NAME_POS 1
#define HISTORY_DERIVED_ORDER_POS 2
#define HISTORY_GIVEN_ORDER_POS 3

#define HISTORY_PREFIX_LEN 4
#define HISTORY_DERIVATION_TAG_POS HISTORY_PREFIX_LEN+0

#define HISTORY_PARENT1_POS HISTORY_PREFIX_LEN+1
#define HISTORY_PARENT2_POS HISTORY_PREFIX_LEN+2

#define HISTORY_PATH_POS HISTORY_PREFIX_LEN+1
#define HISTORY_PARA_PARENT1_POS HISTORY_PREFIX_LEN+2
#define HISTORY_PARA_PARENT2_POS HISTORY_PREFIX_LEN+3

#else

#undef SHOW_HISTORY_ORDER
#define HISTORY_PRIORITY_POS 0
#define HISTORY_NAME_POS 1

#define HISTORY_PREFIX_LEN 2
#define HISTORY_DERIVATION_TAG_POS HISTORY_PREFIX_LEN+0

#define HISTORY_PARENT1_POS HISTORY_PREFIX_LEN+1
#define HISTORY_PARENT2_POS HISTORY_PREFIX_LEN+2

#define HISTORY_PATH_POS HISTORY_PREFIX_LEN+1
#define HISTORY_PARA_PARENT1_POS HISTORY_PREFIX_LEN+2
#define HISTORY_PARA_PARENT2_POS HISTORY_PREFIX_LEN+3

#endif


#define WR_HISTORY_TAG_RESOLVE 1
#define WR_HISTORY_TAG_FACTORIAL 2
#define WR_HISTORY_TAG_PARA 3
#define WR_HISTORY_TAG_EQUALITY_REFLEXIVE 4
#define WR_HISTORY_TAG_SIMPLIFY 5
#define WR_HISTORY_TAG_PROPAGATE 6

#define WR_HISTORY_TAG_SHIFT 20
#define WR_HISTORY_EXTRA_SHIFT 16
#define WR_HISTORY_POS1_SHIFT 8
#define WR_HISTORY_TAG_BITS 15
#define WR_HISTORY_EXTRA_BITS 15

#define WR_HISTORY_USED_BIT_POS  8                // if this bit is set (after decoding!) cl is marked used / given
#define WR_HISTORY_GOAL_ROLENR 1                  // original goal
#define WR_HISTORY_ASSUMPTION_ROLENR 2            // original assumption
#define WR_HISTORY_FROMGOALASSUMPTION_ROLENR 3    // parents are both from goals and assumptions
#define WR_HISTORY_FROMGOAL_ROLENR 4              // some parents are goals, some are assumptions or axioms
#define WR_HISTORY_FROMASSUMPTION_ROLENR 5        // some parents are assumptions, some are axioms 
#define WR_HISTORY_AXIOM_ROLENR 10                // original axiom
#define WR_HISTORY_FROMAXIOM_ROLENR 11            // parents are only axioms
#define WR_HISTORY_EXTAXIOM_ROLENR 12             // direct axiom from external file

typedef struct {  
  int len;  // buf length
  int pos;     // next pos in buf to print
  char* buf;   // char array to print into
} printstr;

gint wr_build_input_history(glb* g, gptr cl1, char* name, gint priority);
gint wr_build_resolve_history(glb* g, gptr cl1, gptr cl2, int pos1, int pos2, gptr cut_clvec);

gint wr_build_propagate_history(glb* g, gptr cl1, gptr cl2, int pos1, int pos2);
gint wr_build_factorial_history(glb* g, gptr cl, int pos1, int pos2, gptr cut_clvec);
gint wr_build_para_history(glb* g, gptr cl1, gptr cl2, int pos1, int pos2, gptr cut_clvec,
                           gint path, int leftflag, int fromflag);
gint wr_build_simplify_history(glb* g, gptr cl, gptr cut_clvec, gptr rewrite_clvec);          

gint wr_calc_history_priority2(glb* g, gptr cl1, gptr cl2);

gint wr_encode_history_resolve(glb* g, int pos1, int pos2);
gint wr_encode_history_propagate(glb* g, int pos1, int pos2);
gint wr_encode_history_factorial(glb* g, int pos1, int pos2);
gint wr_encode_history_equality_reflexive(glb* g, int pos1);
gint wr_encode_history_para(glb* g, int pos1, int pos2, int leftflag);
gint wr_encode_history_simplify(glb* g);

int wr_get_history_tag(glb* g, gint head);
int wr_get_history_extra(glb* g, gint head);
int wr_get_history_pos1(glb* g, gint head);
int wr_get_history_pos2(glb* g, gint head);

void wr_set_history(glb* g, gptr clause, gint history);
gint wr_get_history(glb* g, gptr clause);

void wr_set_history_record_derived_order(glb* g, gptr rec);
void wr_set_history_record_given_order(glb* g, gptr rec);

void wr_show_history(glb* g, gint history);
gptr wr_flatten_history(glb* g, void* mpool, gint history, gptr cl, int depth, int* clnr, void **assoc);
int wr_flatten_history_addcl(glb* g, void* mpool, gint cl, int* clnr, void **assoc);
char* wr_print_flat_history(glb* g, void* mpool, int maxclnr, void **assoc);
char* wr_print_one_history
        (glb* g, void* mpool, gint history, gptr cl, char* clns, int maxclnr, void **assoc);
void wr_print_clause_name_history(glb* g, gint history);

int wr_show_result(glb* g, gint history);
int wr_strprint_flat_history(glb* g, void* mpool, char** buf, int* blen, int bpos,
                             int maxclnr, void **assoc);
int wr_strprint_one_history
        (glb* g, void* mpool, char** buf, int* blen, int bpos, 
         gint history, gptr cl, char* clns, int maxclnr, void **assoc);
int wr_strprint_history_extra(glb* g, char** buf, int* blen, int bpos, gint history);

void wr_set_history_record_field(glb* g, gptr rec, int pos, gint val);
gint wr_get_history_record_field(glb* g, gptr rec, int pos);
gptr wr_create_raw_history_record(glb* g, gint length, gptr buffer);

gint wr_decode_priority(glb* g, gint encpriority);

int wr_register_answer(glb* g, gptr cl, gint history);
int wr_enough_answers(glb* g);
int wr_have_answers(glb* g);

#endif
