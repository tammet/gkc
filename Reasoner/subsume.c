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

 /** @file subsume.c
 *  Subsumption functions.
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
 

//#define DEBUG  
#undef DEBUG

//#define BACKDEBUG  
#undef BACKDEBUG
  
/* ====== Private headers and defs ======== */


/* ======= Private protos ================ */

static int wr_matchable_lit_meta(glb* g, gint mx, gint my);


/* ====== Functions ============== */

  /* ------------------------------------------------------ 

   clause-to-clause-list subsumption

   ------------------------------------------------------
*/


int wr_given_cl_subsumed(glb* g, gptr given_cl, gptr given_cl_metablock) {
  void* db=g->db;
  gptr cl;
  int iactive;
  gptr actptr;
  gint iactivelimit;
  int sres;
  int i,cllen;
  gint meta, lit;
  int dbused;

#ifdef DEBUG
  printf("wr_given_cl_is_subsumed is called with \n");
  wr_print_clause(g,given_cl); 
  printf("\n"); 
#endif  
  UNUSED(db);
  // first try to subsume with active unit clauses  
  if (!wg_rec_is_rule_clause(db,given_cl)) {
    if (wr_active_subsumed_lit(g,rpto(g,given_cl),0)) {
#ifdef DEBUG
      printf("\n fact cl is subsumed by active unit by wr_active_subsumed_lit\n");
#endif      
      (g->stat_clsubs_fact_groundunit_found)++;
      return 1;
    } 
  } else {
    // here a rule clause
    cllen=wg_count_clause_atoms(db,given_cl);     
    for(i=0;i<cllen;i++) {
      meta=wg_get_rule_clause_atom_meta(db,given_cl,i);
      if (!wg_atom_meta_is_ground(db,meta)) break; // only nonground will come later
      lit=wg_get_rule_clause_atom(db,given_cl,i);
      if (wr_active_subsumed_lit(g,lit,wg_atom_meta_is_neg(db,meta))) {
#ifdef DEBUG
        printf("\n rule cl literal is subsumed by active unit by wr_active_subsumed_lit\n");
#endif           
        (g->stat_clsubs_rule_groundunit_found)++;
        return 1;
      }  
      //if (!wg_atom_meta_is_neg(db,meta) && wr_equal_term(g,lit1,lit2,uniquestrflag)) return 1;        
    }
  }  

#ifdef DEBUG
  printf("wr_given_cl_is_subsumed ground unit subsumption loop did not find a subsumer \n");
#endif     

  // next try to subsume with active clauses in lists

  // loop over local active subslist (dbused==0) and external subslist (dbused==1)

  for(dbused=0; dbused<2; dbused++) {
    if (dbused==0) {
      actptr=rotp(g,g->clactivesubsume);   
#ifdef DEBUG
      printf("\n ** dbused %d trying local clactivesubsume\n",dbused);
#endif          
    } else {
#ifdef DEBUG
        printf("\n ** dbused %d \n",dbused);        
#endif        
      if (r_kb_g(g)) {        
        actptr=rotp(g,(r_kb_g(g))->clactivesubsume); 
#ifdef DEBUG
        printf("\n ** dbused %d trying external clactivesubsume\n",dbused);        
#endif          
      }  
      else break;      
    }
    if(1) {    
      //actptr=rotp(g,g->clactivesubsume);
      iactivelimit=CVEC_NEXT(actptr);
      for(iactive=CVEC_START; iactive<iactivelimit; iactive+=CLMETABLOCK_ELS) {
        if (given_cl_metablock!=NULL) {
  #ifdef DEBUG          
            
            printf("\ngeneral iactive %d :",iactive);    
            wr_print_clause(g,rotp(g,(actptr[iactive+CLMETABLOCK_CL_POS])));
            printf("\n");
            wr_print_record(g,rotp(g,(actptr[iactive+CLMETABLOCK_CL_POS])));    
            printf("\nmeta addr %d\n",(int)actptr+iactive);
            printf("\nmetablock1 %d %d %d %d \n",*(actptr+iactive),*(actptr+iactive+1),*(actptr+iactive+2),*(actptr+iactive+3));
            printf("\n lengths   ");
            wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_LENGTHS_POS]);
            printf("\n real length %d\n",
              (*(actptr+iactive) & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
            printf("\n sizes     ");
            wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_SIZES_POS]);
            printf("\n pref1bits ");
            wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_PREF1BITS_POS]);
            printf("\n pref2bits ");
            wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_PREF2BITS_POS]);
            printf("\n pref3bits ");
            wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_PREF3BITS_POS]);
            printf("\n");

            printf("\n specific given_cl:");    
            wr_print_clause(g,given_cl);
            printf("\n");
            wr_print_record(g,given_cl);   
            
            printf("\nmeta addr %d\n",(int)given_cl_metablock); 
            printf("\nmetablock2 %d %d %d %d \n",*(given_cl_metablock),*(given_cl_metablock+1),*(given_cl_metablock+2),*(given_cl_metablock+3));
            printf("\n lengths   ");
            wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_LENGTHS_POS]);
            printf("\n real length %d\n",
              (*(given_cl_metablock) & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
            printf("\n sizes     ");
            wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_SIZES_POS]);
            printf("\n pref1bits ");
            wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_PREF1BITS_POS]);
            printf("\n pref2bits ");
            wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_PREF2BITS_POS]);
            printf("\n pref3bits ");
            wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_PREF3BITS_POS]);
            printf("\n");
            
  #endif       
          (g->stat_clsubs_top_meta_attempted)++;
          if (!wr_clmetablock_can_subsume(g,actptr+iactive,given_cl_metablock)) {
            (g->stat_clsubs_top_meta_failed)++;
  #ifdef DEBUG          
            printf("\n meta-detected cannot subsume \n");            
            wr_print_clause(g,rotp(g,(actptr[iactive+CLMETABLOCK_CL_POS])));
            printf("\n");
            wr_print_clause(g,given_cl);
            printf("\n");            
  #endif    
                  
            continue;
          }
  #ifdef DEBUG        
          printf("\n meta-detected CAN subsume \n");          
          wr_print_clause(g,rotp(g,(actptr[iactive+CLMETABLOCK_CL_POS])));
          printf("\n");
          wr_print_clause(g,given_cl);
          printf("\n");          
  #endif        
        }        
        cl=rotp(g,(actptr[iactive+CLMETABLOCK_CL_POS]));
        if (cl!=NULL) {      
  #ifdef DEBUG
          printf(" *** about to try subsuming with active clause at pos %d nr %d:\n",
            iactive+CLMETABLOCK_CL_POS,iactive/CLMETABLOCK_ELS); 
          wr_print_clause(g,cl);
          printf("\n"); 
  #endif            
          // try to subsume
          sres=wr_subsume_cl(g,cl,given_cl,1);
          if (sres) {
  #ifdef DEBUG
            printf(" * subsumer was found! \n");         
  #endif           
            return 1;
          }  
        }	      
      }
  #ifdef DEBUG
      printf(" *** no subsumer was found from this list of active clauses\n");         
  #endif         
    }  
  }
#ifdef DEBUG
  printf(" *** no subsumer was found from all active clauses\n");         
#endif     
  return 0;    
}  

/*

back subsumption

*/

int wr_given_cl_backsubsume(glb* g, gptr given_cl, gptr given_cl_metablock) {
  void* db=g->db;
  gptr cl;
  int iactive;
  gptr actptr;
  gint iactivelimit;
  int sres;
  int subsumedcount;

#ifdef BACKDEBUG
  printf("wr_given_cl_backsubsume is called with \n");
  wr_print_clause(g,given_cl); 
  printf("\n"); 
#endif  
 
  UNUSED(db);
  // next try to subsume with active clauses in a list
  subsumedcount=0;
  if(1) {    
    actptr=rotp(g,g->clactivesubsume);
    iactivelimit=CVEC_NEXT(actptr);
    for(iactive=CVEC_START; iactive<iactivelimit; iactive+=CLMETABLOCK_ELS) {
      if (given_cl_metablock!=NULL) {
        
#ifdef BACKDEBUG          
          printf("\nspecific iactive %d :",iactive);    
          wr_print_clause(g,(gptr)(actptr[iactive+CLMETABLOCK_CL_POS]));
          printf("\n");
          wr_print_record(g,(gptr)(gptr)(actptr[iactive+CLMETABLOCK_CL_POS]));    
          printf("\nmeta addr %d\n",(int)actptr+iactive);
          printf("\nmetablock1 %d %d %d %d \n",*(actptr+iactive),*(actptr+iactive+1),*(actptr+iactive+2),*(actptr+iactive+3));
          printf("\n lengths   ");
          wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_LENGTHS_POS]);
          printf("\n real length %d\n",
            (*(actptr+iactive) & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
          printf("\n sizes     ");
          wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_SIZES_POS]);
          printf("\n pref1bits ");
          wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_PREF1BITS_POS]);
          printf("\n pref2bits ");
          wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_PREF2BITS_POS]);
          printf("\n pref3bits ");
          wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_PREF3BITS_POS]);
          printf("\n");

          printf("\n general given_cl:");    
          wr_print_clause(g,given_cl);
          printf("\n");
          wr_print_record(g,given_cl);   
          printf("\nmeta addr %d\n",(int)given_cl_metablock); 
          printf("\nmetablock2 %d %d %d %d \n",*(given_cl_metablock),*(given_cl_metablock+1),*(given_cl_metablock+2),*(given_cl_metablock+3));
          printf("\n lengths   ");
          wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_LENGTHS_POS]);
          printf("\n real length %d\n",
            (*(given_cl_metablock) & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
          printf("\n sizes     ");
          wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_SIZES_POS]);
          printf("\n pref1bits ");
          wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_PREF1BITS_POS]);
          printf("\n pref2bits ");
          wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_PREF2BITS_POS]);
          printf("\n pref3bits ");
          wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_PREF3BITS_POS]);
          printf("\n");
#endif         

        (g->stat_clsubs_top_meta_attempted)++;
        if (!wr_clmetablock_can_subsume(g,given_cl_metablock,actptr+iactive)) {
          (g->stat_clsubs_top_meta_failed)++;
#ifdef BACKDEBUG          
          printf("\n meta-detected cannot subsume \n");
          wr_print_clause(g,(gptr)(actptr[iactive+CLMETABLOCK_CL_POS]));
          printf("\n");
          wr_print_clause(g,given_cl);
          printf("\n");
#endif    
                 
          continue;
        }
#ifdef BACKDEBUG        
        printf("\n meta-detected CAN subsume \n");
        wr_print_clause(g,(gptr)(actptr[iactive+CLMETABLOCK_CL_POS]));
        printf("\n");
        wr_print_clause(g,given_cl);
        printf("\n");
#endif        
      }        
 
      cl=(gptr)(actptr[iactive+CLMETABLOCK_CL_POS]);
      if (cl!=NULL) {  
   
#ifdef BACKDEBUG
        printf(" *** about to try back-subsuming with active clause at pos %d nr %d:\n",
          iactive+CLMETABLOCK_CL_POS,iactive/CLMETABLOCK_ELS); 
        wr_print_clause(g,cl);
        printf("\n"); 
#endif           
        // try to subsume
        sres=wr_subsume_cl(g,given_cl,cl,1);
        if (sres) {
          (g->stat_backward_subsumed)++;
          subsumedcount++;
          wr_mark_clause_blocked(g,cl);
          /*
          printf("\n * back-subsumed: \n");
          wr_print_clause(g,cl);
          printf("\n"); 
          */
#ifdef BACKDEBUG
          printf(" * back-subsumer was found! \n");         
#endif                     
        }  
      }	      
    }
#ifdef BACKDEBUG
    printf(" *** back-subsumed: %d\n",subsumedcount);         
#endif     
    return 0;
  }    
}  


int wr_clmetablock_can_subsume(glb* g, gptr genblock, gptr specblock) {
  gint gel, sel;

  gel=genblock[CLMETABLOCK_LENGTHS_POS];
  sel=specblock[CLMETABLOCK_LENGTHS_POS];
  (g->stat_clsubs_top_meta_nonpref_attempted)++;
  /*
  printf("\ngel %d sel %d",gel,sel);
  printf("\n gel bits:\n");
  wr_print_cl_meta(g, gel);
  printf(" sel bits:\n");
  wr_print_cl_meta(g, sel);
  printf("\n");
  
  printf("gel isground %d\n",(gel & (1<<CLMETABLOCK_ISGROUND_SHIFT))>>CLMETABLOCK_ISGROUND_SHIFT);
  printf("sel isground %d\n",(sel & (1<<CLMETABLOCK_ISGROUND_SHIFT))>>CLMETABLOCK_ISGROUND_SHIFT);
  printf("gel length %d\n",(gel & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
  printf("sel length %d\n",(sel & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
  printf("gel neglength %d\n",(gel & (255<<CLMETABLOCK_NEGLENGTH_SHIFT))>>CLMETABLOCK_NEGLENGTH_SHIFT);
  printf("sel neglength %d\n",(sel & (255<<CLMETABLOCK_NEGLENGTH_SHIFT))>>CLMETABLOCK_NEGLENGTH_SHIFT);
  printf("gel groundlength %d\n",(gel & (255<<CLMETABLOCK_GROUNDLENGTH_SHIFT))>>CLMETABLOCK_GROUNDLENGTH_SHIFT);
  printf("sel groundlength %d\n",(sel & (255<<CLMETABLOCK_GROUNDLENGTH_SHIFT))>>CLMETABLOCK_GROUNDLENGTH_SHIFT);
  //printf("gel length %d\n",(gel & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
  //printf("sel length %d\n",(sel & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
  */
  if (gel>sel) return 0;
  if ((gel & (255<<CLMETABLOCK_LENGTH_SHIFT)) > 
      (sel & (255<<CLMETABLOCK_LENGTH_SHIFT))) return 0;
  if ((gel & (255<<CLMETABLOCK_NEGLENGTH_SHIFT)) > 
      (sel & (255<<CLMETABLOCK_NEGLENGTH_SHIFT))) return 0; 
  if ((gel & (255<<CLMETABLOCK_GROUNDLENGTH_SHIFT)) > 
      (sel & (255<<CLMETABLOCK_GROUNDLENGTH_SHIFT))) return 0;       
  if ((gel & (255<<CLMETABLOCK_LENGTH_SHIFT)) == 
      (sel & (255<<CLMETABLOCK_LENGTH_SHIFT)) &&  
      ((sel & (255<<CLMETABLOCK_LENGTH_SHIFT))<255) &&
      ((gel & (255<<CLMETABLOCK_NEGLENGTH_SHIFT)) != 
       (sel & (255<<CLMETABLOCK_NEGLENGTH_SHIFT)) )) return 0;     
  gel=genblock[CLMETABLOCK_SIZES_POS];
  sel=specblock[CLMETABLOCK_SIZES_POS];
  if (gel>sel) return 0;
  if ((gel & (255<<CLMETABLOCK_SIZE_SHIFT)) > 
      (sel & (255<<CLMETABLOCK_SIZE_SHIFT))) return 0;
  if ((gel & (255<<CLMETABLOCK_PREFLEN_SHIFT)) > 
      (sel & (255<<CLMETABLOCK_PREFLEN_SHIFT))) return 0;   
  (g->stat_clsubs_top_meta_nonpref_succeeded)++;  
  (g->stat_clsubs_top_meta_pref_attempted)++;   
  gel=genblock[CLMETABLOCK_PREF1BITS_POS];
  sel=specblock[CLMETABLOCK_PREF1BITS_POS];
  if ((gel & sel) != gel) return 0;
  (g->stat_clsubs_top_meta_pref1_succeeded)++;
  gel=genblock[CLMETABLOCK_PREF2BITS_POS];
  sel=specblock[CLMETABLOCK_PREF2BITS_POS];
  if ((gel & sel) != gel) return 0;
  (g->stat_clsubs_top_meta_pref2_succeeded)++;
  gel=genblock[CLMETABLOCK_PREF3BITS_POS]; 
  sel=specblock[CLMETABLOCK_PREF3BITS_POS];
  if ((gel & sel) != gel) return 0;
  (g->stat_clsubs_top_meta_pref3_succeeded)++;
  (g->stat_clsubs_top_meta_pref_succeeded)++;
  return 1;
}


/*

  return 0 if no subsumption
  return 1 if subsumed

*/

int wr_active_subsumed_lit(glb* g, gint atom, int negflag) {
  gint hash;
  cvec bucket;
  gptr atomptr;

#ifdef DEBUG
  printf("\nwr_active_subsumed_lit is called \n");
  wr_print_term(g,atom);  
  printf("\n and negflag %d\n",negflag);
#endif

  hash=wr_lit_hash(g,atom); 
  atomptr=rotp(g,atom);
  if (negflag) {
    bucket=wr_find_termhash(g,rotp(g,g->hash_neg_active_groundunits),atomptr,hash);      
    if (bucket!=NULL) {
      return 1;
    } else if (r_kb_g(g)) {
      bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_neg_active_groundunits),atomptr,hash);
      if (bucket!=NULL) return 1;
      else return 0;        
    } else {
      return 0;
    }   
  } else {
    bucket=wr_find_termhash(g,rotp(g,g->hash_pos_active_groundunits),atomptr,hash);
    if (bucket!=NULL) {
      return 1;
    } else if (r_kb_g(g)) {
      bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_pos_active_groundunits),atomptr,hash);
      if (bucket!=NULL) return 1;
      else return 0;        
    } else {
      return 0;
    }   
  }
}



/*

  return -1 if subsumed
  return 0 if no subsumption, no cuts
  return a nr of cuts (more than 0) if any cuts

*/

int wr_derived_cl_cut_and_subsume(glb* g, gptr rptr, int rpos) {

  int i,tmp;
  gint hash,xatom,xatommeta;
  gptr xatomptr;
  cvec bucket;
  int cuts=0;

#ifdef DEBUG
  printf("\nwr_derived_cl_cut_and_subsume is called \n");
#endif

  (g->stat_forwardsubs_attempted)++;
  (g->cut_clvec)[1]=(gint)NULL;
  for(i=0;i<rpos;i++) { 
    tmp=i*LIT_WIDTH;
    xatom=rptr[tmp+LIT_ATOM_POS];
    xatommeta=rptr[tmp+LIT_META_POS];   
#ifdef DEBUG     
    printf("\n i %d xatom %d xatommeta %d atom:\n",i,xatom,xatommeta);
    wr_print_term(g,xatom);    
#endif    
    xatomptr=rotp(g,xatom);
    hash=wr_lit_hash(g,xatom); 
#ifdef DEBUG    
    printf("\ncalculated hash %d \n",hash);
#endif    
    if (wg_atom_meta_is_neg(db,xatommeta)) {
#ifdef DEBUG      
      printf("\nneg\n");
#endif      
      bucket=wr_find_termhash(g,rotp(g,g->hash_neg_groundunits),xatomptr,hash);      
      //printf("\ncpneg\n");
      if (bucket!=NULL) {
#ifdef DEBUG        
        printf("\nsubsumed by ground hash as neg!\n"); 
        wr_print_term(g,xatomptr);
        printf("\n bucket:\n");
        wr_print_clause(g,bucket); 
#endif        
        (g->stat_lit_hash_subsume_ok)++;
        (g->stat_forward_subsumed)++;
        return -1;
      }
      if (r_kb_g(g)) {
#ifdef DEBUG
        printf("\nexternal kb r_kb_g(g) present");
#endif        
        bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_neg_groundunits),xatomptr,hash);     
        if (bucket!=NULL) {
  #ifdef DEBUG        
          printf("\nsubsumed by kb ground hash as neg!\n"); 
          wr_print_term(g,xatomptr);
          printf("\n bucket:\n");
          wr_print_clause(g,bucket); 
  #endif        
          (g->stat_lit_hash_subsume_ok)++;
          (g->stat_forward_subsumed)++;
          return -1;
        }
      }  

      bucket=wr_find_termhash(g,rotp(g,g->hash_pos_groundunits),xatomptr,hash);      
      if (bucket!=NULL) {         
#ifdef DEBUG        
        printf("\ncutoff by ground hash as neg!\n"); 
        wr_print_term(g,xatom);
        printf("\n bucket:\n");
        wr_print_clause(g,bucket);        
#endif
        cuts++;
        rptr[tmp+LIT_META_POS]=0; // mark cutoff lit metas as 0
        if ((g->cut_clvec)[0]>cuts+1) {
          (g->cut_clvec)[cuts]=(gint)bucket;
          (g->cut_clvec)[cuts+1]=(gint)NULL;
        }  
        (g->stat_lit_hash_cut_ok)++;   
        //return 0;
      }
      if (r_kb_g(g)) {
#ifdef DEBUG
        printf("\nexternal kb r_kb_g(g) present");
#endif          
        bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_pos_groundunits),xatomptr,hash);      
        if (bucket!=NULL) {         
  #ifdef DEBUG        
          printf("\ncutoff by kb ground hash as neg!\n"); 
          wr_print_term(g,xatom);
          printf("\n bucket:\n");
          wr_print_clause(g,bucket);        
  #endif        
          cuts++;
          rptr[tmp+LIT_META_POS]=0; // mark cutoff lit metas as 0
          if ((g->cut_clvec)[0]>cuts+1) {
            (g->cut_clvec)[cuts]=(gint)bucket;
            (g->cut_clvec)[cuts+1]=(gint)NULL;
          }  
          (g->stat_lit_hash_cut_ok)++;   
          //return 0;
        }
      }  

    } else {
#ifdef DEBUG      
      printf("\npos\n");
#endif
      bucket=wr_find_termhash(g,rotp(g,g->hash_pos_groundunits),xatomptr,hash);
      //printf("\ncppos\n");
      if (bucket!=NULL) {
#ifdef DEBUG        
        printf("\nsubsumed by ground hash as pos!\n");
        wr_print_term(g,xatom);;
        printf("\n bucket:\n");
        wr_print_clause(g,bucket);
#endif        
        (g->stat_lit_hash_subsume_ok)++;
        (g->stat_forward_subsumed)++;
        return -1;
      }
      if (r_kb_g(g)) {
#ifdef DEBUG
        printf("\nexternal kb r_kb_g(g) present");
#endif          
        bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_pos_groundunits),xatomptr,hash);      
        if (bucket!=NULL) {
  #ifdef DEBUG        
          printf("\nsubsumed by kb ground hash as pos!\n"); 
          wr_print_term(g,xatomptr);
          printf("\n bucket:\n");
          wr_print_clause(g,bucket); 
  #endif        
          (g->stat_lit_hash_subsume_ok)++;
          (g->stat_forward_subsumed)++;
          return -1;
        }
      }  
      bucket=wr_find_termhash(g,rotp(g,g->hash_neg_groundunits),xatomptr,hash);      
      if (bucket!=NULL) {            
#ifdef DEBUG        
        printf("\ncutoff by ground hash as pos!\n"); 
        wr_print_term(g,xatom);
        printf("\n bucket:\n");
        wr_print_clause(g,bucket);    
#endif        
        cuts++;
        rptr[tmp+LIT_META_POS]=0; // mark cutoff lit metas as 0
        if ((g->cut_clvec)[0]>cuts+1) {
          (g->cut_clvec)[cuts]=(gint)bucket;
          (g->cut_clvec)[cuts+1]=(gint)NULL;
        }  
        (g->stat_lit_hash_cut_ok)++;       
        //return 0;
      }
      if (r_kb_g(g)) {
#ifdef DEBUG
        printf("\nexternal kb r_kb_g(g) present");
#endif          
        bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_neg_groundunits),xatomptr,hash);      
        if (bucket!=NULL) {         
  #ifdef DEBUG        
          printf("\ncutoff by kb ground hash as pos!\n"); 
          wr_print_term(g,xatom);
          printf("\n bucket:\n");
          wr_print_clause(g,bucket);        
  #endif        
          cuts++;
          rptr[tmp+LIT_META_POS]=0; // mark cutoff lit metas as 0
          if ((g->cut_clvec)[0]>cuts+1) {
            (g->cut_clvec)[cuts]=(gint)bucket;
            (g->cut_clvec)[cuts+1]=(gint)NULL;
          }  
          (g->stat_lit_hash_cut_ok)++;   
          //return 0;
        }
      }  
    } 
    //blt=wr_build_calc_term(g,rptr[tmp+LIT_ATOM_POS]);       
  }
#ifdef DEBUG  
  printf("\n! wr_derived_cl_cut_and_subsume is returning %d \n",cuts);
#endif  
  return cuts;  
}


/*

  return -1 if subsumed
  return 0 if no subsumption, no cuts
  return 1 if any cuts

  //skipcl is a clause which should be always skipped when comparing (set to NULL if not used)
  if subsflag==0 then no subsumption done

*/

int wr_atom_cut_and_subsume(glb* g, gint xatom, gint xatommeta, cvec* foundbucket, int subsflag) {
  gptr xatomptr;
  cvec bucket;
  gint hash;

#ifdef DEBUG
  printf("\n!!!!! wr_atom_cut_and_subsume is called \n");    
  printf("\n xatom %d xatommeta %d atom:\n",xatom,xatommeta);
  wr_print_term(g,xatom);  
#endif     
  xatomptr=rotp(g,xatom);
  hash=wr_lit_hash(g,xatom); 
  if (wg_atom_meta_is_neg(db,xatommeta)) {
    //printf("\nneg\n");
    if (subsflag) {
      bucket=wr_find_termhash(g,rotp(g,g->hash_neg_groundunits),xatomptr,hash);      
      //printf("\ncpneg\n");
      if (bucket!=NULL) {
  #ifdef DEBUG
        printf("\nsubsumed by ground hash as neg!\n"); 
        wr_print_term(g,xatom);
        printf("\n bucket:\n");
        wr_print_clause(g,bucket); 
  #endif    
        return -1;
      }      
      if (r_kb_g(g)) {
        bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_neg_active_groundunits),xatomptr,hash);
        if (bucket!=NULL) {
    #ifdef DEBUG
          printf("\nsubsumed by ground hash as neg!\n"); 
          wr_print_term(g,xatom);
          printf("\n bucket:\n");
          wr_print_clause(g,bucket); 
    #endif    
          return -1;
        }
      }                   
    }  
    bucket=wr_find_termhash(g,rotp(g,g->hash_pos_groundunits),xatomptr,hash);      
    if (bucket!=NULL) {         
#ifdef DEBUG            
      printf("\ncutoff by ground hash as neg!\n"); 
      wr_print_term(g,xatom);
      printf("\n bucket:\n");
      wr_print_clause(g,bucket);        
#endif
      *foundbucket=bucket;      
      return 1;
    }
    if (r_kb_g(g)) {
      bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_pos_active_groundunits),xatomptr,hash);
      if (bucket!=NULL) {
  #ifdef DEBUG
        printf("\ncutoff by ground hash as neg!\n"); 
        wr_print_term(g,xatom);
        printf("\n bucket:\n");
        wr_print_clause(g,bucket);
  #endif    
        *foundbucket=bucket;
        return 1;
      }
    }     

  } else {
    //printf("\npos\n");
    if (subsflag) {
      bucket=wr_find_termhash(g,rotp(g,g->hash_pos_groundunits),xatomptr,hash);
      //printf("\ncppos\n");
      if (bucket!=NULL) {
  #ifdef DEBUG
        printf("\nsubsumed by ground hash as pos!\n");
        wr_print_term(g,xatom);
        printf("\n bucket:\n");
        wr_print_clause(g,bucket);
  #endif
        return -1;
      }
      if (r_kb_g(g)) {
        bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_pos_active_groundunits),xatomptr,hash);
        if (bucket!=NULL) {
    #ifdef DEBUG
          printf("\nsubsumed by ground hash as pos!\n"); 
          wr_print_term(g,xatom);
          printf("\n bucket:\n");
          wr_print_clause(g,bucket); 
    #endif    
          return -1;
        }
      }       
    }  
    bucket=wr_find_termhash(g,rotp(g,g->hash_neg_groundunits),xatomptr,hash);      
    if (bucket!=NULL) {            
#ifdef DEBUG       
      printf("\ncutoff by ground hash as pos!\n"); 
      wr_print_term(g,xatom);
      printf("\n bucket:\n");
      wr_print_clause(g,bucket);    
#endif
      *foundbucket=bucket;      
      return 1;
    }
    if (r_kb_g(g)) {
      bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_neg_active_groundunits),xatomptr,hash);
      if (bucket!=NULL) {
  #ifdef DEBUG
        printf("\ncutoff by ground hash as pos!\n"); 
        wr_print_term(g,xatom);
        printf("\n bucket:\n");
        wr_print_clause(g,bucket);
  #endif    
        *foundbucket=bucket;
        return 1;
      }
    }     
  }   
#ifdef DEBUG
  printf("\n!!!!! wr_atom_cut_and_subsume found no subs or cuts \n");
#endif     
  return 0;  
}


/* ------------------------------------------------------ 

   clause-to-clause subsumption

   ------------------------------------------------------
*/   
  
/*
  Each gcl literal must match a different scl literal.


  Take first gcl literal glit
    Loop over all scl literals     
      if match found glit<->slit and
         recursive subsumption without glit and slit present is OK
      then return OK
      else restore variable settings before last match attempt
    If match found during internal loop 

    otherwise 

  Loop over all gcl literals
    Loop over all scl literals     
      if match found glit<->slit then stop internal loop
      else restore variable settings before last match attempt


*/


gint wr_subsume_cl(glb* g, gptr cl1, gptr cl2, int uniquestrflag) {
  void* db=g->db;
  int cllen1,cllen2;
  int i2;
  gint meta1,meta2;
  gint lit1,lit2;
  int vc_tmp;
  int mres;
  
#ifdef DEBUG  
  printf("\nwr_subsume_cl called on clauses %d %d :\n",(int)cl1,(int)cl2);
  wr_print_clause(g,cl1);
  printf("\n");
  wr_print_clause(g,cl2);
  printf("\n");
#endif

  ++(g->stat_clsubs_attempted);  

  // check fact clause cases first
  if (!wg_rec_is_rule_clause(db,cl1)) {
    if (!wg_rec_is_rule_clause(db,cl2)) {
#ifdef DEBUG  
       printf("both clauses are facts \n");
#endif            
      ++(g->stat_clsubs_unit_attempted);      
      if (wr_equal_term(g,encode_record(db,cl1),encode_record(db,cl2),uniquestrflag))
        return 1;
      else
        return 0;
    } else {
      // fact clause subsuming a rule clause
      cllen2=wg_count_clause_atoms(db,cl2);
      lit1=encode_record(db,cl1);      
      ++(g->stat_clsubs_unit_attempted);
      for(i2=0;i2<cllen2;i2++) {
        meta2=wg_get_rule_clause_atom_meta(db,cl2,i2);
        if (!wg_atom_meta_is_ground(db,meta2)) return 0; // only nonground will come later
        lit2=wg_get_rule_clause_atom(db,cl2,i2);
        if (!wg_atom_meta_is_neg(db,meta2) && wr_equal_term(g,lit1,lit2,uniquestrflag)) return 1;        
      }  
      return 0;
    }      
  } 
  // now cl1 is a rule clause
  cllen1=wg_count_clause_atoms(db,cl1);
  
  if (!wg_rec_is_rule_clause(db,cl2)) {
    // unit rule clause subsuming a unit fact clause case
    ++(g->stat_clsubs_unit_attempted);    
    cllen2=1;
    if (cllen1>1) return 0;
    meta1=wg_get_rule_clause_atom_meta(db,cl1,0);
    if (wg_atom_meta_is_neg(d,meta1)) return 0;
    lit1=wg_get_rule_clause_atom(db,cl1,0);    
    lit2=rpto(g,cl2);
    vc_tmp=2;
    mres=wr_match_term(g,lit1,lit2,uniquestrflag);
    if (vc_tmp!=*((g->varstack)+1)) wr_clear_varstack(g,g->varstack);
    return mres;      
  } else {
    // cl2 is a rule clause
    cllen2=wg_count_clause_atoms(db,cl2); 
  }    
  // now both clauses are rule clauses   

#ifdef DEBUG  
  printf("cllen1 %d cllen2 %d\n",cllen1,cllen2);
#endif   
  // check unit rule clause case
  if (cllen1==1) { 
#ifdef DEBUG  
    printf("unit clause subsumption case \n");
#endif        
    //++(g->stat_clsubs_unit_attempted);    // ?? why double??
    ++(g->stat_clsubs_unit_attempted);
    meta1=wg_get_rule_clause_atom_meta(db,cl1,0);
    lit1=wg_get_rule_clause_atom(db,cl1,0);
    vc_tmp=2;
    *((g->varstack)+1)=vc_tmp; // zero varstack pointer 
    for(i2=0;i2<cllen2;i2++) {
        meta2=wg_get_rule_clause_atom_meta(db,cl2,i2);        
        if (wr_sort_meta_bigger(meta1,meta2)) {
          // lit1 cannot subsume any literal in cl2vec here or coming after, like:
          // general  s(b)
          // general  l(X,Y)
          // specific p(a,b) |  s(b) |  m(X,Y) | r(X)
          (g->stat_clsubs_meta_attempted)++;
          (g->stat_clsubs_meta_failed)++;
          return 0;
        } else if (!litmeta_negpolarities(meta1,meta2) && wr_matchable_lit_meta(g,meta1,meta2)) {  
          lit2=wg_get_rule_clause_atom(db,cl2,i2);
          mres=wr_match_term(g,lit1,lit2,uniquestrflag);                     
          if (vc_tmp!=*((g->varstack)+1)) wr_clear_varstack(g,g->varstack);
          if (mres) return 1;
        }                    
    }  
    return 0;
  }    
  if (cllen1>cllen2) return 0;
  
  // now both clauses are nonunit rule clauses and we do full subsumption
  // prepare for subsumption: set globals etc 
#ifdef DEBUG  
  printf("general subsumption case \n");
#endif  
  g->tmp_unify_vc=(g->varstack)+1;  // var counter for varstack   
  // clear lit information vector (0 pos holds vec len)
  for(i2=1;i2<=cllen2;i2++) (g->tmp_litinf_vec)=wr_vec_store(g,g->tmp_litinf_vec,i2,0);
  ++(g->stat_clsubs_full_attempted);
  mres=wr_subsume_cl_aux(g,cl1,cl2,
                   cl1+RECORD_HEADER_GINTS+CLAUSE_EXTRAHEADERLEN,
                   cl2+RECORD_HEADER_GINTS+CLAUSE_EXTRAHEADERLEN,
                   0,0,
                   cllen1,cllen2,
                   uniquestrflag);
  wr_clear_varstack(g,g->varstack);
  return mres;  
}   



/*
  Each gcl literal must match a different scl literal.


  Take first gcl literal glit
    Loop over all scl literals     
      if match found glit<->slit and
         recursive subsumption without glit and slit present is OK
      then return OK
      else restore variable settings before last match attempt
    If match found during internal loop 

    in other words

  Loop over all gcl literals
    Loop over all scl literals     
      if match found glit<->slit then stop internal loop
      else restore variable settings before last match attempt

*/

gint wr_subsume_cl_aux(glb* g,gptr cl1vec, gptr cl2vec, 
	                  gptr litpt1, gptr litpt2, 
	                  int litind1, int litind2, 
                    int cllen1, int cllen2,
                    int uniquestrflag) {
  int i1,i2; 
  gint lit1,lit2;
  gptr pt1,pt2;  
  gint meta1,meta2;	    
  //int foundflag;	    
  int vc_tmp;	    
  int nobackflag;	    

#ifdef DEBUG 
  printf("\nwr_subsume_cl_aux called with litind1 %d litind2 %d cllen1 %d cllen2 %d gen and specific literals:\n",
    litind1,litind2,cllen1,cllen2);
  wr_print_term(g,*(litpt1+LIT_ATOM_POS)); 
  printf("\n");
  wr_print_term(g,*(litpt2+LIT_ATOM_POS)); 
  printf("\n");
#endif            
  if(litind1<cllen1) {   
    i1=litind1;
    pt1=litpt1;
    meta1=*(pt1+LIT_META_POS); 
    lit1=*(pt1+LIT_ATOM_POS);    
    nobackflag=0; // backtracing will be prohibited if match found without vars set
    for(i2=0,pt2=litpt2; !nobackflag && i2<cllen2; i2++,pt2+=LIT_WIDTH) {
#ifdef DEBUG      
      printf("\n in wr_subsume_cl_aux i2 %d nobackflag %d cllen2 %d\n",i2,nobackflag,cllen2);
#endif      
      if ((g->tmp_litinf_vec)[i2+1]==0) {
	      // literal not bound by subsumption yet
        meta2=*(pt2+LIT_META_POS); 
        lit2=*(pt2+LIT_ATOM_POS);         
        //foundflag=0;          
#ifdef DEBUG
        printf("\n in wr_subsume_cl_aux gen lit1 and specific lit2:\n");
        wr_print_term(g,lit1);
        printf("\n");
        wr_print_term(g,lit2);
        printf("\n");
#endif        
        if (wr_sort_meta_bigger(meta1,meta2)) {
          // lit1 cannot subsume any literal in cl2vec here or coming after, like:
          // general  s(b)
          // general  l(X,Y)
          // specific p(a,b) |  s(b) |  m(X,Y) | r(X)
          (g->stat_clsubs_meta_attempted)++;
          (g->stat_clsubs_meta_failed)++;
          return 0;
        } else if (!litmeta_negpolarities(meta1,meta2) && wr_matchable_lit_meta(g,meta1,meta2)) {          
#ifdef DEBUG      
          printf("\n ok polarities and matchable lit meta\n");
#endif             
          vc_tmp=*(g->tmp_unify_vc); // store current value of varstack pointer ????????
          if (wr_match_term_aux(g,lit1,lit2,uniquestrflag)) {
#ifdef DEBUG      
            printf("\n wr_match_term_aux returned 1\n");
#endif                
        
            
#ifdef DEBUG             
	          printf("lit match ok with *(g->tmp_unify_vc): %d\n",*(g->tmp_unify_vc));
            wr_print_vardata(g);
#endif            
            // literals were successfully matched
	          (g->tmp_litinf_vec)[i2+1]=1; // mark as a bound literal
	          if (vc_tmp==*(g->tmp_unify_vc)) nobackflag=1; // no need to backtrack
            if ((i1+1>=cllen1) ||
	               wr_subsume_cl_aux(g,cl1vec,cl2vec,pt1+(LIT_WIDTH),litpt2,
	                             i1+1,i2,cllen1,cllen2,uniquestrflag)) {
	            // found a right match for current gcl lit
#ifdef DEBUG                                  
              printf("rest ok with *(g->tmp_unify_vc): %d\n",*(g->tmp_unify_vc));		
              wr_print_vardata(g);                                  
#endif                                 
	            return 1;	    
	          } 
#ifdef DEBUG             
	          printf("rest failed with *(g->tmp_unify_vc): %d\n",*(g->tmp_unify_vc));
            wr_print_vardata(g);
#endif            
	          if (vc_tmp!=*(g->tmp_unify_vc)) wr_clear_varstack_topslice(g,g->varstack,vc_tmp);
            (g->tmp_litinf_vec)[i2+1]=0; // clear as a bound literal	      
          } else {
   	        //printf("lit match failed with *(g->tmp_unify_vc): %d\n",*(g->tmp_unify_vc));
  	        if (vc_tmp!=*(g->tmp_unify_vc)) wr_clear_varstack_topslice(g,g->varstack,vc_tmp);	 
          }		  
        } 
      }		
    }
    // all literals checked, no way to subsume using current lit1
    return 0;        
  }
  // clause 
  printf("REASONER ERROR! something wrong in calling wr_subsume_cl_aux\n");
  //printf("litind1: %d cllen1: %d i1: %d \n", litind1,cllen1,i1);  
  return 0;  
}     

/** Match terms:  mx is the general term meta, my is a meta of an instance of x
    NB! we do not consider the polarities here, ie both pos/pos and neg/pos may be matched
*/

int wr_matchable_lit_meta(glb* g, gint mx, gint my) {

#ifdef DEBUG
  printf("\nwr_matchable_lit_meta called on\n  general mx %d: ",mx);
  wr_print_cl_meta(g, mx);
  printf("\n");
  printf("  specific my %d: ",my);
  wr_print_cl_meta(g, my);
  printf("\n");
#endif  
  (g->stat_clsubs_meta_attempted)++;
  if ((mx&(ATOM_META_CANSUBS_MASK<<SMALLINTSHFT)) > (my&(ATOM_META_CANSUBS_MASK<<SMALLINTSHFT))) {
    (g->stat_clsubs_meta_failed)++;    
    //dp("\n initial ATOM_META_CANSUBS_MASK failed, returning 0\n");
    return 0;
  }
  if ((mx&((ATOM_META_GROUND_MASK<<ATOM_META_GROUND_SHIFT)<<SMALLINTSHFT))) {      
    // x and y are ground (y due to previous comparison): have to be equal to subsume
    /*
     if (   ((mx&(ATOM_META_DEPTHCANSUBS_MASK<<SMALLINTSHFT)) > (my&(ATOM_META_DEPTHCANSUBS_MASK<<SMALLINTSHFT))) 
      || ((mx&(ATOM_META_PREFLENCANSUBS_MASK<<SMALLINTSHFT)) > (my&(ATOM_META_PREFLENCANSUBS_MASK<<SMALLINTSHFT))) ) {
      (g->stat_clsubs_meta_failed)++;
      //printf("\n ground depth or preflen failed\n");
      return 0;
    } 
    return 1;
    */
    if ((mx&(ATOM_META_GROUNDCMP_MASK<<SMALLINTSHFT)) != (my&(ATOM_META_GROUNDCMP_MASK<<SMALLINTSHFT))) {
      (g->stat_clsubs_meta_failed)++;      
      //dp("\n GROUNDCMP failed, returning 0\n");
      return 0;
    } else {
       //dp("\n returning 1\n");
      return 1;
    }
  }
  //return 1;
  // here x and y are both non-ground
  //dp("\n here x and y are both non-ground\n");

  if (   ((mx&(ATOM_META_DEPTHCANSUBS_MASK<<SMALLINTSHFT)) > (my&(ATOM_META_DEPTHCANSUBS_MASK<<SMALLINTSHFT))) 
      || ((mx&(ATOM_META_PREFLENCANSUBS_MASK<<SMALLINTSHFT)) > (my&(ATOM_META_PREFLENCANSUBS_MASK<<SMALLINTSHFT))) ) {
    (g->stat_clsubs_meta_failed)++;
    //dp("\n nonground depth or pref failed, returning 0\n");
    //printf("\n nonground depth or pref failed\n");
    return 0;
  } 

  // if preflens are equal (checking that they are not max!), the prefhashes must be equal

  if (   ((mx&((ATOM_META_PREFLEN_MASK<<ATOM_META_PREFLEN_SHIFT)<<SMALLINTSHFT)) == 
          (my&((ATOM_META_PREFLEN_MASK<<ATOM_META_PREFLEN_SHIFT)<<SMALLINTSHFT)) &&
          
         (((mx&((ATOM_META_PREFLEN_MASK<<ATOM_META_PREFLEN_SHIFT)<<SMALLINTSHFT))
          >>ATOM_META_PREFLEN_SHIFT)>>SMALLINTSHFT)!=ATOM_META_PREFLEN_MASK)

      && ((mx&(ATOM_META_PREFHASH_MASK<<SMALLINTSHFT)) != (my&(ATOM_META_PREFHASH_MASK<<SMALLINTSHFT))) ) {
    //dp("\n hashes for same-len prefixes failed, returning 0\n");
    //printf("\n preflen %d mask %d\n",
    //   (((mx&((ATOM_META_PREFLEN_MASK<<ATOM_META_PREFLEN_SHIFT)<<SMALLINTSHFT))
    //      >>ATOM_META_PREFLEN_SHIFT)>>SMALLINTSHFT), ATOM_META_PREFLEN_MASK);
    (g->stat_clsubs_meta_failed)++;
    return 0;
  } 
  //dp("\n final case returning 1\n");  
  return 1;
}


#ifdef __cplusplus
}
#endif
