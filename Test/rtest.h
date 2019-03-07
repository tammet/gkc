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

 /** @file rtest.h
 *  Reasoner testing functions.
 *
 */


#ifndef DEFINED_RTEST_H
#define DEFINED_RTEST_H

#include "../Reasoner/glb.h"

int wg_test_reasoner(int argc, char **argv);
static int wr_test_subsume_cl(glb* g, cvec clvec, char* clstr, int expres, int p);

#endif
