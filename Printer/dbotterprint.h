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

 /** @file dbotterprint.h
 *  Top level/generic headers and defs for otterprinter
 *
 */

#ifndef DEFINED_DBOTTERPRINT_H
#define DEFINED_DBOTTERPRINT_H

#include "../Db/dballoc.h"

#include "../Reasoner/types.h"
#include "../Reasoner/mem.h"
#include "../Reasoner/glb.h"
#include "../Reasoner/prop.h"

#define UNUSEDVAR(x) (void)(x);

void wr_print_clause(glb* g, gptr rec);
void wr_print_halfbuilt_clause(glb* g, gptr rptr, int rpos);
void wr_print_term(glb* g, gint rec);
void wr_print_record(glb* g, gptr rec);

void wr_print_db_otter(glb* g,int printlevel);

void wr_print_clause_otter(glb* g, gint* rec,int printlevel);
void wr_print_rule_clause_otter(glb* g, gint* rec,int printlevel);
void wr_print_fact_clause_otter(glb* g, gint* rec,int printlevel);
void wr_print_prop_clause_otter(glb* g, gint* rec,int printlevel);
void wr_print_atom_otter(glb* g, gint rec,int printlevel);
void wr_print_term_otter(glb* g, gint rec,int printlevel);
void wr_print_simpleterm_otter(glb* g, gint enc,int printlevel);
void wg_nice_print_var(void* db, gint i);

int wr_strprint_clause(glb* g, gptr rec, char** buf, int *len, int pos);
int wr_strprint_clause_otter(glb* g, gptr rec, int printlevel, char** buf, int *len, int pos);
int wr_strprint_rule_clause_otter(glb* g, gint* rec,int printlevel, char** buf, int *len, int pos);
int wr_strprint_fact_clause_otter(glb* g, gint* rec, int printlevel, char** buf, int *len, int pos);
int wr_strprint_prop_clause_otter(glb* g, gint* rec,int printlevel, char** buf, int *len, int pos);
int wr_strprint_atom_otter(glb* g, gint rec, int printlevel,char** buf, int *len, int pos, int isneg);
int wr_strprint_term_otter(glb* g, gint rec,int printlevel, char** buf, int *len, int pos);
int wr_strprint_simpleterm_otter(glb* g, gint enc,int printlevel, char** buf, int *len, int pos, int isneg);
int wg_nice_strprint_var(glb* g, gint i, char** buf, int *len, int pos);

#endif
