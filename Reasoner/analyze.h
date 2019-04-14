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


/* ==== Protos ==== */

int wr_analyze_clause_list(glb* g, void* db, void* child_db);
int wr_analyze_clause(glb* g, gptr cl);
int wr_analyze_term(glb* g, gint x, int depth, int* size, int* maxdepth, int polarity);

void wr_show_in_stats(glb* g);

#endif
