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

 /** @file dbprologparse.h
 *  Special defs and headers for prolog parser
 *
 */

#ifndef DEFINED_PROLOGPARSE_H
#define DEFINED_PROLOGPARSE_H

#include "dbgenparse.h"

int     wg_yyprologlex(YYSTYPE *, void *);
int     wg_yyprologlex_init(void **);
int     wg_yyprologlex_destroy(void *);
void    wg_yyprologset_extra(YY_EXTRA_TYPE, void *);
int     wg_yyprologparse(parse_parm *, void *);   

void wg_yyprologerror (parse_parm* parm, void* scanner, char* msg);


#endif

