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

 /** @file guide.c
 *  Conf and strategy selection parameters, parsing guide file.
 *
 */

/* ====== Includes =============== */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "rincludes.h"
 
// #include "../cjson/cjson.h"

/* ====== Private headers and defs ======== */

//#define DEBUG
#undef DEBUG
#define QUIET
//#undef QUIET

#define json_get(obj,name) cJSON_GetObjectItemCaseSensitive(obj,name)
#define json_isstring(name) cJSON_IsString(name)
#define json_isnumber(name) cJSON_IsNumber(name)
#define json_isarray(name) cJSON_IsArray(name)
#define json_isobject(name) cJSON_IsObject(name)
#define json_key(obj) ((obj)->string)
#define json_valuestring(obj) ((obj)->valuestring)
#define json_valueint(obj) ((obj)->valueint)


#define DEFAULT_GUIDE "{\"print\":1, " \
  "\"print_level\": 15, " \
  "\"strategy\":\"negative_pref\" } " \

  

/* ======= Private protos ================ */


/* ====== Functions ============== */


cJSON* wr_parse_guide_file(char* stratfile, char** guidebuf) { 
  char* filename=NULL;  
  char* buf=NULL;
  FILE* fp=NULL;  
  cJSON *guide=NULL;
  int len;

  if (!stratfile) {
    // default case: no guide file

    len=strlen(DEFAULT_GUIDE);
    buf = (char*)sys_malloc(len+10);
    *guidebuf=buf;
    if (!buf) {
      wr_errprint("failed to allocate memory for the built guide str");
      return NULL;
    }
    //strncpy(buf,DEFAULT_GUIDE,len+10);     
    strcpy(buf,DEFAULT_GUIDE);
    guide=wr_parse_guide_str(buf);
    printf("Using default strategy.");
    return guide;
  } 
  // file case
  filename=stratfile;  
#ifdef _WIN32
  if(fopen_s(&fp, filename, "rb")) {
#else
  if(!(fp = fopen(filename, "rb"))) {
#endif    
    wr_errprint2("cannot open strategy file", filename);
    return NULL;
  }   
  // get the length
  fseek(fp, 0, SEEK_END);
  len = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  // read and proceed
  buf = (char*)sys_malloc(len + 10);
  *guidebuf=buf;
  if (!buf) {
    wr_errprint2("failed to allocate memory for the strategy file", filename);
    return NULL;
  }
  if (fread(buf, 1, len, fp)<len) {
    fclose(fp);
    wr_errprint2("cannot read the strategy file", filename);
    return NULL;
  }
  buf[len] = '\0';
  if (fp!=NULL) fclose(fp);
  guide=wr_parse_guide_str(buf);
  //printf("Using strategy from %s.\n",filename);
  return guide; 
}


cJSON* wr_parse_guide_str(char* buf) {
  cJSON *guide=NULL;
  char *errorptr=NULL;
  int i;

  guide=cJSON_Parse(buf);
  if (guide==NULL) {
    errorptr=(char*)cJSON_GetErrorPtr();
    if (errorptr!=NULL) {
      for(i=0;errorptr[i]!=0;i++) {
        if (errorptr[i]=='"') errorptr[i]='\'';
      }
    }  
    if (errorptr!=NULL) {      
      wr_errprint2("Incorrect json in strategy before",errorptr);
      return NULL;
    }    
    wr_errprint("Empty guide");
    return NULL;
  }  
  return guide;
}

int wr_parse_guide_section(glb* g, cJSON *guide, int runnr, char** outstr) {  
  cJSON *elem=NULL, *run=NULL;
  char *key, *errstr; //, *valuestr;
  int i,tmp; // valueint
  int runcount=0, runfound=0;
  char* out;

  if (!json_isobject(guide)) {    
    errstr=cJSON_Print(guide);
    wr_warn2(g,"misunderstood section in the strategy:\n ", errstr);
    if (errstr) wr_free(g,errstr);
    return -1;
  }
  elem=guide->child;
  while(elem) {
    key=json_key(elem);        
    if (key==NULL) { elem=elem->next; continue; }
    //printf("\nkey is %s\n",elem->string); 

    if (!strncmp(key,"print",5) && strlen(key)==5) {
      //printf("print %d\n", json_valueint(elem));
      (g->print_flag)=json_valueint(elem);
    } else if (!strcmp(key,"print_json")) {
      //printf("print_level %d\n", json_valueint(elem));
      (g->print_json)=json_valueint(elem);     
      if (json_valueint(elem)) {
        (g->print_clauses_json)=1;
      }
    } else if (!strcmp(key,"print_tptp")) {
      //printf("print_tptp %d\n", json_valueint(elem));
      (g->print_tptp)=json_valueint(elem);   
      if (json_valueint(elem)==0) {
        // remove fof source storage and printout
        (g->print_fof_conversion_proof)=0;
        (g->store_fof_source)=0;
        (g->store_fof_skolem_steps)=0;
        (g->print_clauses_tptp)=0;
        (g->print_proof_tptp)=0;
        (g->use_comp_arithmetic)=1;
      } else {
        (g->print_fof_conversion_proof)=1;
        (g->store_fof_source)=1;
        (g->store_fof_skolem_steps)=1;
        (g->print_clauses_tptp)=1;
        (g->print_proof_tptp)=1;
        (g->use_comp_arithmetic)=0;
      }
    } else if (!strcmp(key,"print_level")) {
      //printf("print_level %d\n", json_valueint(elem));
      (g->print_level_flag)=json_valueint(elem);

    } else if (!strcmp(key,"max_answers")) {
      //printf("print_level %d\n", json_valueint(elem));
      (g->required_answer_nr)=json_valueint(elem);
    }   

    else if (!strcmp(key,"max_seconds")) {
      //printf("\nmax_run_seconds %d\n", json_valueint(elem));
      (g->max_run_seconds)=json_valueint(elem);

      // TESTING: commented out. NORMALLY: active
      if ((g->max_run_seconds)<2) {
        (g->use_strong_unit_cutoff)=1;
      } else {
        (g->use_strong_unit_cutoff)=0;
      }      

      //(g->use_strong_unit_cutoff)=1;

    } else if (!strcmp(key,"total_seconds")) {
      //printf("\nmax_seconds %d\n", json_valueint(elem));
      (g->max_seconds)=json_valueint(elem);
    } 
    /*
    else if (!strcmp(key,"max_forks")) {
      //printf("\nmax_forks %d\n", json_valueint(elem));
      (g->max_forks)=json_valueint(elem);
    }
    */
    else if (!strcmp(key,"given_queue_ratio")) {
      //printf("\nmax_seconds %d\n", json_valueint(elem));
      (g->pick_given_queue_ratio)=json_valueint(elem);
    } else if (!strcmp(key,"sine")) {
      //printf("\nsine_strat %d\n", json_valueint(elem));
      (g->sine_strat)=json_valueint(elem);
    }


    else if (!strcmp(key,"max_weight")) {
      //printf("\nmax_weight %d\n", json_valueint(elem));
      (g->cl_maxkeep_weightlimit)=json_valueint(elem);
    } else if (!strcmp(key,"max_size")) {
      //printf("\nmax_size %d\n", json_valueint(elem));
      (g->cl_maxkeep_sizelimit)=json_valueint(elem);
    } else if (!strcmp(key,"max_depth")) {
      //printf("\nmax_depth %d\n", json_valueint(elem));
      (g->cl_maxkeep_depthlimit)=json_valueint(elem);
    } else if (!strcmp(key,"max_length")) {
      //printf("max_length %d\n", json_valueint(elem));
      (g->cl_maxkeep_lengthlimit)=json_valueint(elem);

    } else if (!strcmp(key,"depth_penalty")) {
      //printf("depth_penalty %d\n", json_valueint(elem));
      (g->cl_depth_penalty)=json_valueint(elem);
    } else if (!strcmp(key,"length_penalty")) {
      //printf("length_penalty %d\n", json_valueint(elem));
      (g->cl_length_penalty)=json_valueint(elem);  
    } else if (!strcmp(key,"var_weight")) {
      //printf("var_weight %d\n", json_valueint(elem));
      (g->var_weight)=json_valueint(elem);   
    } else if (!strcmp(key,"repeat_var_weight")) {
      //printf("repeat_var_weight %d\n", json_valueint(elem));
      (g->repeat_var_weight)=json_valueint(elem);     
  
      


    } else if (!strcmp(key,"equality")) {
      //printf("equality %d\n", json_valueint(elem));
      (g->use_equality_strat)=json_valueint(elem);  
    } else if (!strcmp(key,"rewrite")) {
      //printf("rewrite %d\n", json_valueint(elem));
      (g->use_rewrite_terms_strat)=json_valueint(elem);    
    } else if (!strcmp(key,"weight_select_ratio")) {
      //printf("weight_select_ratio %d\n", json_valueint(elem));
      (g->pick_given_queue_ratio)=json_valueint(elem);          
    } else if (!strcmp(key,"reverse_clauselist")) {
      //printf("weight_select_ratio %d\n", json_valueint(elem));
      (g->reverse_clauselist_strat)=json_valueint(elem);  
    } else if (!strcmp(key,"query_preference")) {
      //printf("query_preference %d\n", json_valueint(elem));
      (g->cl_pick_queue_strategy)=json_valueint(elem);
    } else if (!strcmp(key,"instgen")) {
      //printf("query_preference %d\n", json_valueint(elem));
      (g->instgen_strat)=json_valueint(elem);  
    } else if (!strcmp(key,"propgen")) {
      //printf("query_preference %d\n", json_valueint(elem));
      (g->propgen_strat)=json_valueint(elem);  
    } else if (!strcmp(key,"strategy")) {    
      wr_parse_guide_strategy(g,elem);
    } 

    else if (!strcmp(key,"runs")) {
      //printf("\nmax_seconds %d\n", json_valueint(elem));
      if (runnr<0) {
        wr_warn(g,"do not use nested runs sections in strategy");
      } else {  
        if (json_isnumber(elem)) {
          elem=elem->next;
          continue;
        }
        if (!json_isarray(elem)) {        
          wr_warn(g,"runs value in strategy must be an array or 0");        
          return -1;
        }
        // loop over runs, find and use the runnr section
        run=elem->child;        
        for(i=0; run; i++, run=run->next) {
          if (i==runnr) {
            runfound=1;
            tmp=wr_parse_guide_section(g,run,-1,outstr);
            //printf("\n wr_parse_guide_section: \n");
            //out=cJSON_Print(run);            
            out=cJSON_PrintUnformatted(run);
            *outstr=out;
            if (tmp<0) return -1;            
          } 
        }
        runcount=i;          
      }  
    } else {
      wr_warn2(g,"unknown setting in the strategy: ", key);
    }    
    elem=elem->next;
  }

  //printf("\n runfound %d runnr %d guide not null: %d \n",runfound,runnr,guide!=NULL);  
  if (runfound && runnr<0 && guide!=NULL) {
    out=NULL; 
    *outstr=NULL;    
    return -1;
  } else if (!runfound && runnr<0 && guide!=NULL) {
    out=cJSON_Print(guide); 
    *outstr=out;    
  } else if (!runfound && runnr>=0 && guide!=NULL) {
    out=NULL; //cJSON_Print(guide); 
    *outstr=NULL;    
    //return -1;
  }

  return runcount;
}

int wr_parse_guide_strategy(glb* g, cJSON *obj) {
  cJSON *elem=NULL;

  if (json_isstring(obj) && json_valuestring(obj)!=NULL) {
    //printf("\nstrat %s\n",json_valuestring(obj));
    wr_parse_guide_strategy_set(g,json_valuestring(obj));
  } else if (json_isarray(obj)) {
    cJSON_ArrayForEach(elem, obj) {
      if (json_isstring(elem) && json_valuestring(elem)!=NULL) {
        //printf("\nstrat elem %s\n",json_valuestring(elem));
        wr_parse_guide_strategy_set(g,json_valuestring(elem));
      }  
    }  
  }
  return 0;
}

int wr_parse_guide_strategy_set(glb* g, char* stratname) {
  
  if (!strcmp(stratname,"negative_pref")) {
    (g->negpref_strat)=1;
  } else if (!strcmp(stratname,"positive_pref")) {
    (g->pospref_strat)=1;
  } else if (!strcmp(stratname,"hyper")) {
    (g->hyperres_strat)=1;  
  } else if (!strcmp(stratname,"unit")) {
    (g->res_shortarglen_limit)=1;  
  } else if (!strcmp(stratname,"double")) {
    (g->res_shortarglen_limit)=2;      
  } else if (!strcmp(stratname,"triple")) {
    (g->res_shortarglen_limit)=3;   
  } else if (!strcmp(stratname,"hardness_pref")) {
    (g->hardnesspref_strat)=1;       
  } else if (!strcmp(stratname,"query_focus")) {
    (g->queryfocus_strat)=1;   
  } else if (!strcmp(stratname,"knuthbendix_pref")) {
    (g->knuthbendixpref_strat)=1;     
  } else if (!strcmp(stratname,"prohibit_nested_para")) {
    (g->prohibit_nested_para)=1;    
  } else if (!strcmp(stratname,"posunitpara")) {
    (g->posunitpara_strat)=1;     
  } else if (!strcmp(stratname,"max_ground_weight")) {
    (g->use_max_ground_weight)=1;  
  } else if (!strcmp(stratname,"pure_unit")) {
    (g->res_arglen_limit)=1;
    (g->res_strict_arglen_limit)=1;



  } else {
    wr_warn2(g,"unknown strategy: ", stratname);
  }

  return 0;
}





#ifdef __cplusplus
}
#endif
