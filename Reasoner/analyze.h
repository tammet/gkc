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

 /** @file analyze.h
 * Analyze the clause set before search.
 */


#ifndef DEFINED_ANALYZE_H
#define DEFINED_ANALYZE_H

/* ==== Includes ==== */

#include "glb.h"


/* ==== Global defines ==== */

// fit occurrence counts into one gint

#define URI_COUNT_POSCOUNT_SHIFT 16  // shift decoded val this much to right to get poscount: shift 16 bits
#define URI_COUNT_POSCOUNT_MASK 0xEFFF<<URI_COUNT_POSCOUNT_SHIF  // mask decoded val with this to get poscont
#define URI_COUNT_NEGCOUNT_MASK 0x1FFF  // mask decoded val with this to get negcount: 13 last bits ie 5+8 bits

// SINE

#define INITIAL_SINE_OCCVECLEN 4

/* ==== Protos ==== */

int wr_analyze_clause_list(glb* g, void* db, void* child_db);
int wr_analyze_clause(glb* g, gptr cl, int haveextdb);
int wr_analyze_term(glb* g, gint x, 
      int depth, int* size, int* maxdepth, int polarity, int haveextdb, int argpos);

int wr_analyze_sine(glb* g, void* db, void* child_db);
void wr_set_cl_sine_k(glb* g,gptr cl,int k);
int wr_get_cl_sine_k(glb* g,gptr cl);
void wr_set_uri_sine_k(glb* g,gint uri,int k);
int wr_get_uri_sine_k(glb* g,gint uri);

void wr_get_clause_symbols(glb* g, gptr cl, cvec* uriinfo);
void wr_get_term_symbols(glb* g, gint x, cvec* uriinfo);
int wr_sine_check_trigger_cl(glb* g, gptr cl, gint scount, float tolerance, gint common);
int wr_sine_check_trigger_term(glb* g, gint x, gint scount, float tolerance, gint common);

char* make_auto_guide(glb* g, glb* kb_g, int guideparam);
void make_sum_input_stats(glb* g, glb* kb_g);
void wr_copy_sin_stats(glb* fromg, glb* tog);

void wr_show_in_stats(glb* g);
void wr_show_in_summed_stats(glb* g);

void make_ltb_guide(glb* g, char** strats, int stratscount);

#endif
