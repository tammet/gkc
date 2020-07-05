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

 /** @file printerrutils.h
 *   Headers for printing and err handling utils.
 *
 */


#ifndef DEFINED_PRINTERRUTILS_H
#define DEFINED_PRINTERRUTILS_H

/* ==== Includes ==== */


/* ==== Global defines ==== */

#define db_printf(...) printf(FIRST(__VA_ARGS__)  REST(__VA_ARGS__))
#define wr_printf(...) if(g->print_flag) printf(FIRST(__VA_ARGS__)  REST(__VA_ARGS__))
/* expands to the first argument */
#define FIRST(...) FIRST_HELPER(__VA_ARGS__, throwaway)
#define FIRST_HELPER(first, ...) first

/*
 * if there's only one argument, expands to nothing.  if there is more
 * than one argument, expands to a comma followed by everything but
 * the first argument.  only supports up to 9 arguments but can be
 * trivially expanded.
 */
#define REST(...) REST_HELPER(NUM(__VA_ARGS__), __VA_ARGS__)
#define REST_HELPER(qty, ...) REST_HELPER2(qty, __VA_ARGS__)
#define REST_HELPER2(qty, ...) REST_HELPER_##qty(__VA_ARGS__)
#define REST_HELPER_ONE(first)
#define REST_HELPER_TWOORMORE(first, ...) , __VA_ARGS__
#define NUM(...) \
    SELECT_10TH(__VA_ARGS__, TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE,\
                TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE, ONE, throwaway)
#define SELECT_10TH(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, ...) a10


#define DPRINTF

#ifdef DPRINTF
#define dprintf(fmt, ...) printf(fmt, ##__VA_ARGS__);
#define dp(fmt, ...) { printf(fmt, ##__VA_ARGS__);  printf(" %s %d\n",__FILE__, __LINE__);  }
#else
#define dprintf(fmt,...) ;
#define dp(fmt,...) ;
#endif

#define CP0 printf("CP0 %s %d\n",__FILE__, __LINE__);
#define CP1 printf("CP1 %s %d\n",__FILE__, __LINE__);
#define CP2 printf("CP2 %s %d\n",__FILE__, __LINE__);
#define CP3 printf("CP3 %s %d\n",__FILE__, __LINE__);
#define CP4 printf("CP4 %s %d\n",__FILE__, __LINE__);
#define CP5 printf("CP5 %s %d\n",__FILE__, __LINE__);
#define CP6 printf("CP6 %s %d\n",__FILE__, __LINE__);
#define CP7 printf("CP7 %s %d\n",__FILE__, __LINE__);
#define CP8 printf("CP8 %s %d\n",__FILE__, __LINE__);
#define CP9 printf("CP9 %s %d\n",__FILE__, __LINE__);

#define CP10 printf("CP10 %s %d\n",__FILE__, __LINE__);
#define CP11 printf("CP11 %s %d\n",__FILE__, __LINE__);
#define CP12 printf("CP12 %s %d\n",__FILE__, __LINE__);
#define CP13 printf("CP13 %s %d\n",__FILE__, __LINE__);
#define CP14 printf("CP14 %s %d\n",__FILE__, __LINE__);
#define CP15 printf("CP15 %s %d\n",__FILE__, __LINE__);
#define CP16 printf("CP16 %s %d\n",__FILE__, __LINE__);
#define CP17 printf("CP17 %s %d\n",__FILE__, __LINE__);
#define CP18 printf("CP18 %s %d\n",__FILE__, __LINE__);
#define CP19 printf("CP19 %s %d\n",__FILE__, __LINE__);

/*

#define CP0 printf("CP0\n");
#define CP1 printf("CP1\n");
#define CP2 printf("CP2\n");
#define CP3 printf("CP3\n");
#define CP4 printf("CP4\n");
#define CP5 printf("CP5\n");
#define CP6 printf("CP6\n");
#define CP7 printf("CP7\n");
#define CP8 printf("CP8\n");
#define CP9 printf("CP9\n");

#define CP10 printf("CP10\n");
#define CP11 printf("CP11\n");
#define CP12 printf("CP12\n");
#define CP13 printf("CP13\n");
#define CP14 printf("CP14\n");
#define CP15 printf("CP15\n");
#define CP16 printf("CP16\n");
#define CP17 printf("CP17\n");
#define CP18 printf("CP18\n");
#define CP19 printf("CP19\n");

*/

#define PRINT_LIMITS

/* ==== Protos ==== */

void* wr_warn(glb* g, char* errstr);
void* wr_warn2(glb* g, char* errstr1, char* errstr2); 

void* wr_errprint(char* errstr);
void* wr_errprint2(char* errstr1, char* errstr2);

void* wr_alloc_err(glb* g, char* errstr);
void* wr_alloc_err2(glb* g, char* errstr1, char* errstr2);
void* wr_alloc_err2int(glb* g, char* errstr, int n);
void wr_sys_exiterr(glb* g, char* errstr);
void wr_sys_exiterr2(glb* g, char* errstr1, char* errstr2);
void wr_sys_exiterr2int(glb* g, char* errstr, int n);

void wr_exiterr(glb* g, char* errstr1);
void wr_exiterr2(glb* g, char* errstr1, char* errstr2);
void wr_exiterr2int(glb* g, char* errstr, int n);

void wg_show_db_error(void* db);

#endif
