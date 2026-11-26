/*
*
* Copyright (c) Tanel Tammet 2004-2026
*
* Contact: tanel.tammet at gmail.com
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
* GNU Affero General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with GKC.  If not, see <http://www.gnu.org/licenses/>.
*
*/

/** @file termindex.h
 *  A discrimination tree of terms (match retrieval); see termindex.c.
 */

#ifndef DEFINED_TERMINDEX_H
#define DEFINED_TERMINDEX_H

#include "glb.h"

/* the tree: 64-bit builds only; a 32-bit build (wasm) and a build with
   -DGKC_NO_TERM_INDEX use the lists of today */
#if defined(__SIZEOF_POINTER__) && (__SIZEOF_POINTER__ == 8) && !defined(GKC_NO_TERM_INDEX)
#define USE_TERM_INDEX
#endif

/* children of a node kept in an array in the node; more go to a skip list */
#ifndef TI_INLINE
#define TI_INLINE 6
#endif

#ifdef USE_TERM_INDEX
typedef struct ti_index_s ti_index;
/* called by ti_match_all for each match: the payload, the number of slots,
   the slot values and the variables of the stored term in slot order */
typedef void (*ti_visit_fn)(glb* g, void* ctx, gint payload, int32_t nv, gint* vals, gint* vars);
typedef struct {
  long nodes, entries, bytes, steps;
  long maxchildren, maxvchildren, nodes_vchildren_gt1, skiplists;
} ti_stat;
ti_index* ti_new(void);
void ti_free(ti_index* x);
int ti_insert(glb* g, ti_index* x, gint term, gint payload);
int ti_failed(ti_index* x);
int32_t ti_tokens(glb* g, ti_index* x, gint term);
int32_t ti_token_end(ti_index* x, int32_t i);
int ti_token_is_record(ti_index* x, int32_t i);
int ti_token_is_var(ti_index* x, int32_t i);
gint ti_token_term(ti_index* x, int32_t i);
int ti_match_best(glb* g, ti_index* x, int32_t start, int32_t end,
                  int (*accept)(glb*, gint), int norenaming,
                  gint* payload, int32_t* nv, gint** vars, gint** vals);
void ti_match_all(glb* g, ti_index* x, int32_t start, int32_t end, ti_visit_fn visit, void* ctx);
void ti_stats(ti_index* x, ti_stat* s);
#endif
void wr_ti_selftest(glb* g);

#endif
