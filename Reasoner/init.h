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

 /** @file init.h
 * Initialization of reasoning-specific data structures before search
 */


#ifndef DEFINED_INIT_H
#define DEFINED_INIT_H

/* ==== Includes ==== */

#include "types.h"
#include "glb.h"
#include "clterm.h"

/* ==== Global defines ==== */



/* ==== Protos ==== */

int init_shared_database(void* db, char* guidefilename);

void wr_show_database_details(glb* g,void* db, char* desc);
void wr_show_database_headers(void* db);
void wr_show_database_details_shared (glb* passedg,void* db, char* desc);

#endif
