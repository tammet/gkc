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

 /** @file clstore.h
 * Headers for clause storage functions.
 */


#ifndef DEFINED_CLSTORE_H
#define DEFINED_CLSTORE_H

/* ==== Includes ==== */

#include "types.h"
#include "glb.h"
#include "clterm.h"

/* ==== Global defines ==== */


#define CLTERM_HASHNODE_GINT_NR 4
#define CLTERM_HASHNODE_LEN_POS 0
#define CLTERM_HASHNODE_TERM_POS 0
#define CLTERM_HASHNODE_CL_POS 1
#define CLTERM_HASHNODE_PATH_POS 2
#define CLTERM_HASHNODE_NEXT_POS 3

#define MAXHASHPOS 30


// --- top level queue structure ----

// 

#define NROF_CLPICK_QUEUES       4    // how many queues in (g->clpick_queues)
#define CLPICK_QUEUE_BLOCKGINTS 10    // how many gints for one queue in (g->clpick_queues)

// meaning of positions in each queue block

#define CLPICK_QUEUE_POS                0
#define CLPICK_PRIORQUEUE_POS           1
#define CLPICK_QUEUE_GIVEN_POS          2
#define CLPICK_PRIORQUEUE_RATIO         4
#define CLPICK_PRIORQUEUE_RATIO_COUNTER 5
#define CLPICK_THIS_QUEUE_RATIO         6 
#define CLPICK_THIS_QUEUE_RATIO_COUNTER 7

// starting positions of queue blocks

#define GOAL_ASSUMPTIONS_QUEUE_POS  1 // start pos for this queue in (g->clpick_queues)
#define GOAL_QUEUE_POS             11 // ...
#define ASSUMPTIONS_QUEUE_POS      21
#define AXIOMS_QUEUE_POS           31


// --- single priority queue structure ---

#define PRIORQUEUE_NODE_GINT_NR 4  

#define PRIORQUEUE_MAX_PRIOR_POS 0
#define PRIORQUEUE_MIN_PICKABLE_PRIOR_POS 1
#define PRIORQUEUE_MAX_USED_PRIOR_POS 2
#define PRIORQUEUE_MAX_KEEP_PRIOR_POS 3

#define PRIORQUEUE_ARR_LEN_POS 0
#define PRIORQUEUE_NEXT_FREE_INDEX_POS 1
#define PRIORQUEUE_NEXT_PICK_INDEX_POS 2
#define PRIORQUEUE_ARR_OFFSET_POS 3

#define PRIORQUEUE_INITIAL_BUCKETELS 8

// -- paramodulation ---

//#define PARA_INTO_FUNSYMB   // if defined, adds funsymbs and predsymbs into para_term_hash
#undef PARA_INTO_FUNSYMB

//#define PARA_INTO_ATOM  // if defined, adds full atoms into para_term_hash
#undef PARA_INTO_ATOM

#define PARA_TERMPATH_MAXLITNR 255
#define PARA_TERMPATH_MAXPOS   60000
#define PARA_TERMPATH_LITNRSHIFT 16
#define PARA_TERMPATH_POSMASK      0xFFFF
#define PARA_TERMPATH_LITNRMASK  0xFF0000
#define PARA_TERMPATH_LEFTMASK  0x1000000

// -- active clause vec block structure --

#define CLACTIVE_NODE_GINT_NR 5
#define CLACTIVE_NODE_META_POS 0
#define CLACTIVE_NODE_PATH0HASH_POS 1
#define CLACTIVE_NODE_PATH1HASH_POS 2
#define CLACTIVE_NODE_PATH2HASH_POS 3
#define CLACTIVE_NODE_CL_POS 4


/* ==== Protos ==== */


void wr_push_clpickstack_cl(glb* g, gptr cl);
void wr_show_clpickstack(glb* g);
void wr_push_clqueue_cl(glb* g, gptr cl);
void wr_show_clqueue(glb* g);
void wr_show_clactive(glb* g);
void wr_show_clactivesubsume(glb* g);

int wr_cl_store_res_terms(glb* g, gptr cl);
int wr_cl_store_para_terms(glb* g, gptr cl); 
int wr_cl_store_eq_arg(glb* g, gptr cl, gint term, int termtype, int litnr, int leftflag);
int wr_cl_store_para_subterms(glb* g, gptr cl, gint term, int depth, int litnr, int* termpath);
int wr_encode_para_termpath(glb* g, int litnr, int pos);
int wr_decode_para_termpath_litnr(glb* g, int n);
int wr_decode_para_termpath_pos(glb* g, int n);

gint wr_create_clpick_queues(glb* g, int count);
void wr_push_cl_clpick_queues(glb* g, gint queues_offset, gptr cl, int weight);
int wr_is_positive_unit_cl(glb* g, gptr cl);
int wr_is_negative_cl(glb* g, gptr cl);
gptr wr_pick_from_clpick_queues(glb* g, gptr queues, gptr given_cl_metablock);
gptr wr_pick_from_clpick_queues(glb* g, gptr queues, gptr given_cl_metablock);
gptr wr_pick_from_clpick_queues_aux(glb* g, gptr queues, int queuenr, gptr given_cl_metablock);
int wr_cl_ismarked_given(glb* g, gptr cl);
void wr_cl_mark_given(glb* g, gptr cl);
void wr_free_clpick_queues(glb* g, gint* queue);
void wr_print_clpick_queues(glb* g, gint* queues);

gint* wr_make_priorqueue(glb* g, int maxprior);
gint* wr_push_priorqueue(glb* g, gint* queue, gptr cl, int prior);
gint* wr_pick_priorqueue(glb* g, gint* queue);
void wr_free_priorqueue(glb* g, gint* queue);
void wr_print_priorqueue(glb* g, gint* queue);

#endif
