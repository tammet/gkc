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

/*
#define DEFAULT_GUIDE2(...) #__VA_ARGS__
  {"aa":2,
   "bb" 3};
*/

#define DEFAULT_GUIDE "{\"print\":1, " \
  "\"print_level\": 10, " \
  "\"strategy\":\"negative_pref\" } " \

  

/* ======= Private protos ================ */


char* create_monitor(void);
char *create_monitor_with_helpers(void);
int supports_full_hd(const char * const monitor);

/* ====== Functions ============== */


cJSON* wr_parse_guide_file(int argc, char **argv, char** guidebuf) { 
  char* filename=NULL;  
  char* buf=NULL;
  FILE* fp=NULL;  
  cJSON *guide=NULL;
  int len;

  //printf("\nto wr_parse_guide_file %s\n",argv[3]);

  if (argc<4) {
    // default case
    len=strlen(DEFAULT_GUIDE);
    buf = (char*)malloc(len + 10);
    *guidebuf=buf;
    if (!buf) {
      wr_errprint("failed to allocate memory for the built guide str");
      return NULL;
    }
    strncpy(buf,DEFAULT_GUIDE,len); 
    guide=wr_parse_guide_str(buf);
    return guide;
  } 
  // file case
  
  filename=argv[3];
#ifdef _WIN32
  if(fopen_s(&fp, filename, "rb")) {
#else
  if(!(fp = fopen(filename, "rb"))) {
#endif    
    wr_errprint2("cannot open file", filename);
    return NULL;
  }     
  // get the length
  fseek(fp, 0, SEEK_END);
  len = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  // read and proceed
  buf = (char*)malloc(len + 10);
  *guidebuf=buf;
  if (!buf) {
    wr_errprint2("failed to allocate memory for the guide file", filename);
    return NULL;
  }
  if (fread(buf, 1, len, fp)<len) {
    fclose(fp);
    wr_errprint2("cannot read the guide file", filename);
    return NULL;
  }
  buf[len] = '\0';
  if (fp!=NULL) fclose(fp);
  guide=wr_parse_guide_str(buf);    
  return guide; 
}


cJSON* wr_parse_guide_str(char* buf) {
  cJSON *guide=NULL;
  char *errorptr=NULL;

  //printf("\nbuf:\n%s\n",buf);
  guide=cJSON_Parse(buf);
  if (guide==NULL) {
    errorptr=(char*)cJSON_GetErrorPtr();
    if (errorptr!=NULL) {      
      wr_errprint2("Incorrect json in guide before ",errorptr);
      return NULL;
    }
    wr_errprint("Empty guide");
    return NULL;
  }
  return guide;
}

int wr_parse_guide_section(glb* g, cJSON *guide, int runnr) {  
  cJSON *elem=NULL, *run=NULL;
  char *key, *errstr, *valuestr;
  int valueint,i,tmp;
  int runcount=0, runfound=0;

  if (!json_isobject(guide)) {    
    errstr=cJSON_Print(guide);
    wr_warn2(g,"misunderstood section in the guide:\n ", errstr);
    if (errstr) free(errstr);
    return -1;
  }
  elem=guide->child;
  while(elem) {
    key=json_key(elem);        
    if (key==NULL) { elem=elem->next; continue; }
    //printf("\nkey is %s\n",elem->string); 

    if (!strcmp(key,"print")) {
      //printf("print %d\n", json_valueint(elem));
      (g->print_flag)=json_valueint(elem);
    } else if (!strcmp(key,"print_level")) {
      //printf("print_level %d\n", json_valueint(elem));
      (g->print_level_flag)=json_valueint(elem);
    }     

    else if (!strcmp(key,"max_seconds")) {
      //printf("\nmax_run_seconds %d\n", json_valueint(elem));
      (g->max_run_seconds)=json_valueint(elem);
    } else if (!strcmp(key,"total_seconds")) {
      //printf("\nmax_seconds %d\n", json_valueint(elem));
      (g->max_seconds)=json_valueint(elem);
    } 

    else if (!strcmp(key,"given_queue_ratio")) {
      //printf("\nmax_seconds %d\n", json_valueint(elem));
      (g->pick_given_queue_ratio)=json_valueint(elem);
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
    } else if (!strcmp(key,"query_preference")) {
      //printf("query_preference %d\n", json_valueint(elem));
      (g->cl_pick_queue_strategy)=json_valueint(elem);
    } else if (!strcmp(key,"strategy")) {    
      wr_parse_guide_strategy(g,elem);
    } 

    else if (!strcmp(key,"runs")) {
      //printf("\nmax_seconds %d\n", json_valueint(elem));
      if (runnr<0) {
        wr_warn(g,"do not use nested runs sections in guide");
      } else {  
        if (json_isnumber(elem)) {
          elem=elem->next;
          continue;
        }
        if (!json_isarray(elem)) {        
          wr_warn(g,"runs value in guide must be an array or 0");        
          return -1;
        }
        // loop over runs, find and use the runnr section
        run=elem->child;        
        for(i=0; run; i++, run=run->next) {
          if (i==runnr) {
            runfound=1;
            tmp=wr_parse_guide_section(g,run,-1);
            if (tmp<0) return -1;            
          } 
        }
        runcount=i;          
      }  
    } 

    else {
      wr_warn2(g,"unknown setting in the guide: ", key);
    }
    
    elem=elem->next;
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
  } else if (!strcmp(stratname,"unit")) {
    (g->res_shortarglen_limit)=1;  
  } else if (!strcmp(stratname,"double")) {
    (g->res_shortarglen_limit)=2;      
  } else if (!strcmp(stratname,"triple")) {
    (g->res_shortarglen_limit)=3;      
  } else if (!strcmp(stratname,"query_focus")) {
    (g->queryfocus_strat)=1;   
  } else {
    wr_warn2(g,"unknown strategy in the guide: ", stratname);
  }

  return 0;
}




/* -------
  
  testing

------------*/

/*
void cjson_test(glb* g) {
  // json test start 
  char* jstr=create_monitor_with_helpers();
  printf("\njstr %s\n",jstr);
  int jres=supports_full_hd(jstr);
  printf("\njres %d\n",jres);  
  // json test end
}
*/

//create a monitor with a list of supported resolutions
//NOTE: Returns a heap allocated string, you are required to free it after use.
char* create_monitor(void)
{
    const unsigned int resolution_numbers[3][2] = {
        {1280, 720},
        {1920, 1080},
        {3840, 2160}
    };
    char *string = NULL;
    cJSON *name = NULL;
    cJSON *resolutions = NULL;
    cJSON *resolution = NULL;
    cJSON *width = NULL;
    cJSON *height = NULL;
    size_t index = 0;

    cJSON *monitor = cJSON_CreateObject();
    if (monitor == NULL)
    {
        goto end;
    }

    name = cJSON_CreateString("Awesome 4K");
    if (name == NULL)
    {
        goto end;
    }
    /* after creation was successful, immediately add it to the monitor,
     * thereby transfering ownership of the pointer to it */
    cJSON_AddItemToObject(monitor, "name", name);

    resolutions = cJSON_CreateArray();
    if (resolutions == NULL)
    {
        goto end;
    }
    cJSON_AddItemToObject(monitor, "resolutions", resolutions);

    for (index = 0; index < (sizeof(resolution_numbers) / (2 * sizeof(int))); ++index)
    {
        resolution = cJSON_CreateObject();
        if (resolution == NULL)
        {
            goto end;
        }
        cJSON_AddItemToArray(resolutions, resolution);

        width = cJSON_CreateNumber(resolution_numbers[index][0]);
        if (width == NULL)
        {
            goto end;
        }
        cJSON_AddItemToObject(resolution, "width", width);

        height = cJSON_CreateNumber(resolution_numbers[index][1]);
        if (height == NULL)
        {
            goto end;
        }
        cJSON_AddItemToObject(resolution, "height", height);
    }

    string = cJSON_Print(monitor);
    if (string == NULL)
    {
        fprintf(stderr, "Failed to print monitor.\n");
    }

end:
    cJSON_Delete(monitor);
    return string;
}

//NOTE: Returns a heap allocated string, you are required to free it after use.
char *create_monitor_with_helpers(void)
{
    const unsigned int resolution_numbers[3][2] = {
        {1280, 720},
        {1920, 1080},
        {3840, 2160}
    };
    char *string = NULL;
    cJSON *resolutions = NULL;
    size_t index = 0;

    cJSON *monitor = cJSON_CreateObject();

    if (cJSON_AddStringToObject(monitor, "name", "Awesome 4K") == NULL)
    {
        goto end;
    }

    resolutions = cJSON_AddArrayToObject(monitor, "resolutions");
    if (resolutions == NULL)
    {
        goto end;
    }

    for (index = 0; index < (sizeof(resolution_numbers) / (2 * sizeof(int))); ++index)
    {
        cJSON *resolution = cJSON_CreateObject();

        if (cJSON_AddNumberToObject(resolution, "width", resolution_numbers[index][0]) == NULL)
        {
            goto end;
        }

        if(cJSON_AddNumberToObject(resolution, "height", resolution_numbers[index][1]) == NULL)
        {
            goto end;
        }

        cJSON_AddItemToArray(resolutions, resolution);
    }

    string = cJSON_Print(monitor);
    if (string == NULL) {
        fprintf(stderr, "Failed to print monitor.\n");
    }

end:
    cJSON_Delete(monitor);
    return string;
}

/* return 1 if the monitor supports full hd, 0 otherwise */
int supports_full_hd(const char * const monitor)
{
    const cJSON *resolution = NULL;
    const cJSON *resolutions = NULL;
    const cJSON *name = NULL;
    int status = 0;
    cJSON *monitor_json = cJSON_Parse(monitor);
    if (monitor_json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        status = 0;
        goto end;
    }

    name = cJSON_GetObjectItemCaseSensitive(monitor_json, "name");
    if (cJSON_IsString(name) && (name->valuestring != NULL))
    {
        printf("Checking monitor \"%s\"\n", name->valuestring);
    }

    resolutions = cJSON_GetObjectItemCaseSensitive(monitor_json, "resolutions");
    cJSON_ArrayForEach(resolution, resolutions)
    {
        cJSON *width = cJSON_GetObjectItemCaseSensitive(resolution, "width");
        cJSON *height = cJSON_GetObjectItemCaseSensitive(resolution, "height");

        if (!cJSON_IsNumber(width) || !cJSON_IsNumber(height))
        {
            status = 0;
            goto end;
        }

        if ((width->valuedouble == 1920) && (height->valuedouble == 1080))
        {
            status = 1;
            goto end;
        }
    }

end:
    cJSON_Delete(monitor_json);
    return status;
}



#ifdef __cplusplus
}
#endif
