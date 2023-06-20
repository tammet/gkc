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

/** @file guide.h
 *  Conf and strategy selection parameters, parsing guide file.
 *
 */

#ifndef DEFINED_GUIDE_H
#define DEFINED_GUIDE_H

#include "glb.h"
#include "../cjson/cjson.h"

cJSON* wr_parse_guide_file(char* stratfile, char** guidebuf);
cJSON* wr_parse_guide_str(char* buf);
int wr_parse_guide_strategy_set(glb* g, char* stratname);
int wr_parse_guide_section(glb* g, cJSON *guide, int runnr, char** outstr, int seqnr);
int wr_parse_guide_strategy(glb* g, cJSON *guide);

#endif
