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


 /** @file rmain.h
 *  Reasoner top level: initialisation and startup.
 *
 */


#ifndef DEFINED_RMAIN_H
#define DEFINED_RMAIN_H


#include "glb.h" 

/*
#define PROOF_FOUND_SIGNAL SIGUSR1
void sig_usr(int signo, siginfo_t *sinfo, void *context);
*/

int wg_run_reasoner(void *db, char* inputname, char* stratfile, int informat,  
                    char* outfilename, char* guidestr);
int wg_run_converter(void *db, char* inputname, char* stratfile, int informat, 
                     char* outfilename, char* guidestr);                    
int wr_print_all_clauses(glb* g, void* child_db);
/*
int wg_run_reasoner_onestrat(
  void* db, int pid, char* outfilename, int iter, cJSON *guide, int givenguide, char *guidebuf, char* filename,
  void* kb_db, void *child_db, glb *kb_g, glb *rglb, glb* analyze_g, int* guideres);
*/
int wg_import_otter_file(void *db, char* filename, int iskb, int* informat);
int wg_import_otter_string(void *db, char* instr, int iskb, int* informat);
int wg_import_prolog_file(void *db, char* filename);
glb* wg_init_reasoner(void *db, int argc, char **argv);
int wr_init_active_passive_lists_from_one(glb* g, void* db, void* kb_db);

void wr_show_stats(glb* g, int show_local_complex);
void wr_set_detailed_printout(glb* g);
void wr_set_detailed_plus_printout(glb* g);

void print_datastructs(glb* g);
void wr_print_active_clauses(glb* g);

#endif
