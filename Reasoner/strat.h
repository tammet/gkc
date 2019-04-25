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

/** @file strat.c
 *  Derivation strategy implementations/filters.
 *
 */

#ifndef DEFINED_STRAT_H
#define DEFINED_STRAT_H

#include "glb.h"


#define MIN_HARDNESS -100000000

typedef struct {  
  int size;
  int maxdepth;
  int newvars;
  int repvars;
  gint atomposocc;
  gint atomnegocc;
  gint internposocc;
  gint internnegocc;
} atom_hardnesscalc;


int wr_cl_is_goal(glb* g, gptr cl);
int wr_cl_is_assumption(glb* g, gptr cl);

int wr_is_unit_cl(glb* g, gptr cl, int ruleflag, int len);
int wr_isunit_cl(glb* g, gptr cl);
int wr_count_cl_nonans_atoms(glb* g, gptr cl);

void wr_set_stratlimits_cl(glb* g, gptr cl, int ruleflag, int len, int* posok, int* negok, int* nonanslen);
int wr_order_resolvable_atom(glb* g, int negflag,
      int negok, int posok, int negadded, int posadded,
      int hardness, int max_neg_hardness, int max_pos_hardness);
int wr_initial_select_active_cl(glb* g, gptr cl);

int wr_calc_clause_hardnesses(glb* g, gptr cl, 
      int* max_pos_hardness, int* max_neg_hardness);
int wr_calc_atom_hardness(glb* g, int polarity, gint atom);
int wr_calc_atom_hardness_aux(glb* g, gint x, int depth, int pos,
      atom_hardnesscalc* hptr, int polarity, int haveextdb);

int wr_order_eqterms(glb* g, gint a, gint b, gptr vb);

void wr_clear_countedvarlist(glb* g, cvec varlist);
void wr_record_varocc(glb* g, gint x, cvec varlist);
void wr_show_countedvarlist(glb* g, cvec varlist);
int wr_countedvarlist_is_subset(glb* g, cvec xlist, cvec ylist);

//static int wr_order_eqterms_lex_smaller(glb* g, gint x, gint y, gptr vb);

#endif
