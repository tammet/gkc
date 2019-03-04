/*
* $Id:  $
* $Version: $
*
* Copyright (c) Tanel Tammet 2004,2005,2006,2007,2008,2009,2010
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

 /** @file dbclausify.h
 *  Top level/generic headers and defs for the clausifier
 *
 */

#ifndef DEFINED_DBCLAUSIFY_H
#define DEFINED_DBCLAUSIFY_H

#include "../Db/dballoc.h"
#include "../Reasoner/mem.h"
#include "../Reasoner/glb.h" 

#define NEWVAR_PREFIX "X"

void* wr_clausify_formula(glb* g, void* mpool, void* frm);
void* wr_clausify_negpush(glb* g, void* mpool, void* frm, int sign);
void* wr_clausify_copy(glb* g, void* mpool, void* term);
void* wr_clausify_skolemize(glb* g, void* mpool, void* frm, void* vars, int* varnr);
void* wr_clausify_append_vars
        (glb* g, void* mpool, void* assoc, void* varlist, int allflag, int *varnr);
void wr_clausify_replace_vars(glb* g, void* mpool, void* assoc, void* term);
void* wr_clausify_distribute(glb* g, void* mpool, void* frm, void **defs);
int wr_clausify_isliteral(glb* g, void* frm);
int wr_clausify_collect_vars(glb* g, void* term, gptr* vars, int *count, int max);
void* wr_clausify_makedef(glb* g, void* mpool, void* frm, void **defs); 

void* wr_flatten_logclause(glb* g,void* mpool, void* cl);
int wr_is_logtruth(glb* g, void* cl);
int wr_is_logconst(glb* g, void* cl);
void* wr_flatten_logclause_or(glb* g,void* mpool, void* cl);

//void* wr_clausify_clauselist(glb* g,void* mpool,cvec clvec,void* clauselist);

#endif