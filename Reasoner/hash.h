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

 /** @file hash.h
 * Headers for hash and hash-based storage/search functions..
 */


#ifndef DEFINED_HASH_H
#define DEFINED_HASH_H

/* ==== Includes ==== */

#include "types.h"
#include "glb.h"
#include "clterm.h"

/* ==== Global defines ==== */

/*
#define CLTERM_HASHNODE_GINT_NR 4
#define CLTERM_HASHNODE_LEN_POS 0
#define CLTERM_HASHNODE_TERM_POS 0
#define CLTERM_HASHNODE_CL_POS 1
#define CLTERM_HASHNODE_PATH_POS 2
#define CLTERM_HASHNODE_NEXT_POS 3
*/

#define ATOMHASH_NODE_SIZE 4
#define ATOMHASH_ATOM_POS 0
#define ATOMHASH_ATOMNR_POS 1
#define ATOMHASH_POSCLAUSES_POS 2
#define ATOMHASH_NEGCLAUSES_POS 3

#define ATOMHASH_CLAUSE_CVEC_INITIAL_SIZE 2

#define MAXHASHPOS 30


#define WR_HASH_ADD(toadd,orig) ((toadd) + ((orig) << 6) + ((orig) << 16) - (orig))
#define WR_HASH_NORM(tonorm,maxsize) (1+((abs((int)tonorm)) % ((maxsize)-2)))

/* ==== Protos ==== */



int wr_term_hashstore(glb* g, void* hashdata, gint atom, gptr cl);

gint wr_term_complexhash(glb* g, gint* hasharr, gint hashposbits, gint term);
gint wr_atom_funhash(glb* g, gint atom);
gint wr_term_funhash(glb* g, gint term);

int wr_atom_calc_prefhashes(glb* g, gint x, gint* prefhashes);
int wr_atom_calc_prefhashes_aux(glb* g, gint x, int* preflen, gint* prefhashes);

gint wr_lit_hash(glb* g, gint x);
gint wr_term_hash(glb* g, gint x);
gint wr_term_basehash(glb* g, gint enc);

int wr_clterm_add_hashlist(glb* g, vec hashvec, gint hash, gint term, gptr cl);
int wr_clterm_add_hashlist_withpath(glb* g, vec hashvec, gint hash, gint term, gptr cl, int path);
int wr_clterm_hashlist_len(glb* g, vec hashvec, gint hash);
gint wr_clterm_hashlist_start(glb* g, vec hashvec, gint hash);
gint wr_clterm_hashlist_next(glb* g, vec hashvec, gint lastel);

gptr wr_clterm_alloc_hashnode(glb* g);
void wr_clterm_free_hashnode(glb* g, gptr node);
void wr_clterm_hashlist_free(glb* g, vec hashvec);
void wr_clterm_hashlist_print(glb* g, vec hashvec);
void wr_clterm_hashlist_print_para(glb* g, vec hashvec);

gint* wr_push_termhash(glb* g, gint* hasharr, int hash, gptr term, gptr cl);
gint* wr_push_offset_termhash(glb* g, gint* hasharr, int hash, gptr term, gptr cl);
gint* wr_find_termhash(glb* g, gint* hasharr, gptr term, int hash);
gint* wr_find_offset_termhash(glb* g, gint* hasharr, gptr term, int hash);
gint wr_find_offset_termbucket(glb* g, gint* hasharr, gptr term, int hash);

void wr_free_termhash(glb* g, gint* hasharr);
void wr_free_offset_termhash(glb* g, gint* hasharr);
void wr_print_termhash(glb* g, gint* hasharr);
void wr_print_offset_termhash(glb* g, gint* hasharr);

gint* wr_push_atomhash(glb* g, gint* hasharr, int hash, gint atom, gptr cl, int pos, int negflag);
cvec wr_add_atomhash_node(glb* g, gint atom, cvec bucket);
gint* wr_find_atomhash_bucketnode(glb* g, gint* hasharr, gint term, int hash);
void wr_free_atomhash(glb* g, gint* hasharr);
void wr_print_atomhash(glb* g, gint* hasharr);
void wr_print_atomhash_clvec(glb* g, cvec clvec);

void wr_clterm_hashlist_explore(glb* g, vec hashvec, char* note);

#endif
