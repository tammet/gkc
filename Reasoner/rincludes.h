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

 /** @file rincludes.h
 *   standard includes for reasoner c files
 */


#ifndef DEFINED_RINCLUDES_H
#define DEFINED_RINCLUDES_H

/* ==== Includes ==== */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include "../config-w32.h"
#else
#include "../config.h"
#endif

#define UNUSED(x) (void)(x);

#include "mem.h" 
#include "glb.h"
#include "clterm.h"
#include "unify.h"
#include "build.h"
#include "clstore.h"
#include "clmeta.h"
#include "history.h"
#include "prob.h"
#include "hash.h"
#include "subsume.h"
#include "propagate.h"
#include "derive.h"
#include "simp.h"
#include "strat.h"
#include "rewrite.h"
#include "resolve.h"
#include "rgenloop.h"
#include "rmain.h"
#include "printerrutils.h"
#include "guide.h"
#include "init.h"
#include "analyze.h"
#include "chains.h"
#include "prop.h"
#include "../cjson/cjson.h"
#include "../Db/dbutil.h"
#include "../Printer/dbotterprint.h"
//#include "../Test/dbtest.h"

/* ==== Global defines ==== */



/* ==== Protos ==== */

#endif
