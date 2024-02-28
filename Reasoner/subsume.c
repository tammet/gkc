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
//define DEBUG1 
//#undef DEBUG

//#define BACKDEBUG  
//#undef BACKDEBUG

//#define SCUTDEBUG
  
/* ====== Private headers and defs ======== */

#define ANS_IGNORE

#define GLOBAL_UNITS

#define SHARED_DERIVED

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
  wr_printf("wr_given_cl_is_subsumed is called with \n");
  wr_print_clause(g,given_cl); 
  wr_printf("\n"); 
#endif  
  UNUSED(db);
  // first try to subsume with active unit clauses  
  if (!wg_rec_is_rule_clause(db,given_cl)) {
    if (wr_active_subsumed_lit(g,rpto(g,given_cl),0)) {
#ifdef DEBUG1
      wr_printf("\n fact cl is subsumed by active unit by wr_active_subsumed_lit\n");
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
#ifdef DEBUG1
        wr_printf("\n rule cl literal is subsumed by active unit by wr_active_subsumed_lit\n");
#endif           
        (g->stat_clsubs_rule_groundunit_found)++;
        return 1;
      }  
      //if (!wg_atom_meta_is_neg(db,meta) && wr_equal_term(g,lit1,lit2,uniquestrflag)) return 1;        
    }
  }  

#ifdef DEBUG
  wr_printf("wr_given_cl_is_subsumed ground unit subsumption loop did not find a subsumer \n");
#endif     

  // next try to subsume with active clauses in lists

  //printf("\n** clactivesubsume data from external:\n"); 
  //wr_show_clactivesubsume(r_kb_g(g));

  // loop over local active subslist (dbused==0) and external subslist (dbused==1)
  for(dbused=0; dbused<2; dbused++) {
    if (dbused==0) {
      actptr=rotp(g,g->clactivesubsume);   
#ifdef DEBUG
      wr_printf("\n ** dbused %d trying local clactivesubsume\n",dbused);
#endif          
    } else {
#ifdef DEBUG
        wr_printf("\n ** dbused %d \n",dbused);        
#endif        
      if (r_kb_g(g)) {        
        actptr=rotp(g,(r_kb_g(g))->clactivesubsume); 
#ifdef DEBUG
        wr_printf("\n ** dbused %d trying external clactivesubsume\n",dbused);        
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
            
            wr_printf("\ngeneral iactive %d :",iactive);    
            wr_print_clause(g,rotp(g,(actptr[iactive+CLMETABLOCK_CL_POS])));
            wr_printf("\n");
            wr_print_record(g,rotp(g,(actptr[iactive+CLMETABLOCK_CL_POS])));    
            wr_printf("\nmeta addr %d\n",(int)actptr+iactive);
            wr_printf("\nmetablock1 %d %d %d %d \n",*(actptr+iactive),*(actptr+iactive+1),*(actptr+iactive+2),*(actptr+iactive+3));
            wr_printf("\n lengths   ");
            wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_LENGTHS_POS]);
            wr_printf("\n real length %d\n",
              (*(actptr+iactive) & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
            wr_printf("\n sizes     ");
            wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_SIZES_POS]);
            wr_printf("\n pref1bits ");
            wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_PREF1BITS_POS]);
            wr_printf("\n pref2bits ");
            wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_PREF2BITS_POS]);
            wr_printf("\n pref3bits ");
            wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_PREF3BITS_POS]);
            wr_printf("\n");

            wr_printf("\n specific given_cl:");    
            wr_print_clause(g,given_cl);
            wr_printf("\n");
            wr_print_record(g,given_cl);   
            
            wr_printf("\nmeta addr %d\n",(int)given_cl_metablock); 
            wr_printf("\nmetablock2 %d %d %d %d \n",*(given_cl_metablock),*(given_cl_metablock+1),*(given_cl_metablock+2),*(given_cl_metablock+3));
            wr_printf("\n lengths   ");
            wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_LENGTHS_POS]);
            wr_printf("\n real length %d\n",
              (*(given_cl_metablock) & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
            wr_printf("\n sizes     ");
            wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_SIZES_POS]);
            wr_printf("\n pref1bits ");
            wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_PREF1BITS_POS]);
            wr_printf("\n pref2bits ");
            wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_PREF2BITS_POS]);
            wr_printf("\n pref3bits ");
            wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_PREF3BITS_POS]);
            wr_printf("\n");
            
  #endif       
            //wr_printf("\ngeneral iactive %d :",iactive);    
            //wr_print_clause(g,rotp(g,(actptr[iactive+CLMETABLOCK_CL_POS])));
            //wr_printf("\n");
            //wr_printf("\n specific given_cl:");    
            //wr_print_clause(g,given_cl);
            //wr_printf("\n");
            //wr_print_record(g,given_cl);   

          (g->stat_clsubs_top_meta_attempted)++;
          if (!wr_clmetablock_can_subsume(g,actptr+iactive,given_cl_metablock)) {
            (g->stat_clsubs_top_meta_failed)++;
            //wr_printf("\n meta-detected cannot subsume \n");  
  #ifdef DEBUG          
            wr_printf("\n meta-detected cannot subsume \n");            
            wr_print_clause(g,rotp(g,(actptr[iactive+CLMETABLOCK_CL_POS])));
            wr_printf("\n");
            wr_print_clause(g,given_cl);
            wr_printf("\n");            
  #endif    
                  
            continue;
          }
          //wr_printf("\n meta-detected CAN subsume \n");
  #ifdef DEBUG        
          wr_printf("\n meta-detected CAN subsume \n");          
          wr_print_clause(g,rotp(g,(actptr[iactive+CLMETABLOCK_CL_POS])));
          wr_printf("\n");
          wr_print_clause(g,given_cl);
          wr_printf("\n");          
  #endif        
        }       
        cl=rotp(g,(actptr[iactive+CLMETABLOCK_CL_POS]));
        if (cl!=NULL) {      
  #ifdef DEBUG
          wr_printf(" *** about to try subsuming with active clause at pos %d nr %d:\n",
            iactive+CLMETABLOCK_CL_POS,iactive/CLMETABLOCK_ELS); 
          wr_print_clause(g,cl);
          wr_printf("\n"); 
  #endif            
          // try to subsume
          sres=wr_subsume_cl(g,cl,given_cl,1);
          if (sres) {
  #ifdef DEBUG1
            wr_printf(" * subsumer was found! \n"); 
            wr_print_clause(g,cl);
            wr_printf("\n");        
  #endif           
            return 1;
          }            
        }	      
      }
  #ifdef DEBUG
      wr_printf(" *** no subsumer was found from this list of active clauses\n");         
  #endif         
    }  
  }
#ifdef DEBUG
  wr_printf(" *** no subsumer was found from all active clauses\n");         
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
  wr_printf("wr_given_cl_backsubsume is called with \n");
  wr_print_clause(g,given_cl); 
  wr_printf("\n"); 
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
/*       
          wr_printf("\nactptr %ld specific iactive %d :",actptr,iactive);   
          wr_print_clause(g,rotp(g,actptr[iactive+CLMETABLOCK_CL_POS]));
          wr_printf("\n");
          wr_print_record(g,rotp(g,actptr[iactive+CLMETABLOCK_CL_POS]));    
          wr_printf("\nmeta addr %d\n",(int)actptr+iactive);
          wr_printf("\nmetablock1 %d %d %d %d \n",*(actptr+iactive),*(actptr+iactive+1),*(actptr+iactive+2),*(actptr+iactive+3));
          wr_printf("\n lengths   ");
          wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_LENGTHS_POS]);
          wr_printf("\n real length %d\n",
            (*(actptr+iactive) & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
          wr_printf("\n sizes     ");
          wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_SIZES_POS]);
          wr_printf("\n pref1bits ");
          wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_PREF1BITS_POS]);
          wr_printf("\n pref2bits ");
          wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_PREF2BITS_POS]);
          wr_printf("\n pref3bits ");
          wr_print_gint_hashmask(g, actptr[iactive+CLMETABLOCK_PREF3BITS_POS]);
          wr_printf("\n");

          wr_printf("\n general given_cl:");    
          wr_print_clause(g,given_cl);
          wr_printf("\n");
          wr_print_record(g,given_cl);   
          wr_printf("\nmeta addr %d\n",(int)given_cl_metablock); 
          wr_printf("\nmetablock2 %d %d %d %d \n",*(given_cl_metablock),*(given_cl_metablock+1),*(given_cl_metablock+2),*(given_cl_metablock+3));
          wr_printf("\n lengths   ");
          wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_LENGTHS_POS]);
          wr_printf("\n real length %d\n",
            (*(given_cl_metablock) & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
          wr_printf("\n sizes     ");
          wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_SIZES_POS]);
          wr_printf("\n pref1bits ");
          wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_PREF1BITS_POS]);
          wr_printf("\n pref2bits ");
          wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_PREF2BITS_POS]);
          wr_printf("\n pref3bits ");
          wr_print_gint_hashmask(g,given_cl_metablock[CLMETABLOCK_PREF3BITS_POS]);
          wr_printf("\n");
*/          
#endif         

        (g->stat_clsubs_top_meta_attempted)++;
        if (!wr_clmetablock_can_subsume(g,given_cl_metablock,actptr+iactive)) {
          (g->stat_clsubs_top_meta_failed)++;
#ifdef BACKDEBUG      
/*    
          wr_printf("\n meta-detected cannot subsume \n");
          wr_print_clause(g,rotp(g,actptr[iactive+CLMETABLOCK_CL_POS]));
          wr_printf("\n");
          wr_print_clause(g,given_cl);
          wr_printf("\n");
*/          
#endif    
                 
          continue;
        }
#ifdef BACKDEBUG     
/*   
        wr_printf("\n meta-detected CAN subsume \n");
        wr_print_clause(g,rotp(g,actptr[iactive+CLMETABLOCK_CL_POS]));
        wr_printf("\n");
        wr_print_clause(g,given_cl);
        wr_printf("\n");
*/        
#endif        
      }        
 
      cl=rotp(g,(actptr[iactive+CLMETABLOCK_CL_POS]));
      if (cl!=NULL) {  
   
#ifdef BACKDEBUG
/*
        wr_printf(" *** about to try back-subsuming with active clause at pos %d nr %d:\n",
          iactive+CLMETABLOCK_CL_POS,iactive/CLMETABLOCK_ELS); 
        wr_print_clause(g,cl);
        wr_printf("\n"); 
*/        
#endif           
        // try to subsume
        sres=wr_subsume_cl(g,given_cl,cl,1);
        if (sres) {
          (g->stat_backward_subsumed)++;
          subsumedcount++;
          wr_set_cl_backsubsumed(g,cl);
          /*
          wr_printf("\n * back-subsumed: \n");
          wr_print_clause(g,cl);
          wr_printf("\n"); 
          */
#ifdef BACKDEBUG
          wr_printf(" * back-subsumer was found! \n");         
#endif                     
        }  
      }	      
    }
#ifdef BACKDEBUG
    wr_printf(" *** back-subsumed: %d\n",subsumedcount);         
#endif     
    return 0;
  }    
}  


void wr_set_cl_backsubsumed(glb* g,gptr cl) {
  gint clid;

  clid=wr_get_clid(g,cl);
  if (clid>=(g->backsubsume_bytes)) return;
  (g->backsubsume_values)[clid]=(char)1;
}

int wr_get_cl_backsubsumed(glb* g,gptr cl) {
  gint clid;

  clid=wr_get_clid(g,cl);
  if (clid>=(g->backsubsume_bytes)) return 0;
  return (int)((g->backsubsume_values)[clid]);
}


int wr_clmetablock_can_subsume(glb* g, gptr genblock, gptr specblock) {
  gint gel, sel;

  gel=genblock[CLMETABLOCK_LENGTHS_POS];
  sel=specblock[CLMETABLOCK_LENGTHS_POS];
  (g->stat_clsubs_top_meta_nonpref_attempted)++;
  /*
  wr_printf("\ngel %d sel %d",gel,sel);
  wr_printf("\n gel bits:\n");
  wr_print_cl_meta(g, gel);
  wr_printf(" sel bits:\n");
  wr_print_cl_meta(g, sel);
  wr_printf("\n");
  
  wr_printf("gel isground %d\n",(gel & (1<<CLMETABLOCK_ISGROUND_SHIFT))>>CLMETABLOCK_ISGROUND_SHIFT);
  wr_printf("sel isground %d\n",(sel & (1<<CLMETABLOCK_ISGROUND_SHIFT))>>CLMETABLOCK_ISGROUND_SHIFT);
  wr_printf("gel length %d\n",(gel & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
  wr_printf("sel length %d\n",(sel & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
  wr_printf("gel neglength %d\n",(gel & (255<<CLMETABLOCK_NEGLENGTH_SHIFT))>>CLMETABLOCK_NEGLENGTH_SHIFT);
  wr_printf("sel neglength %d\n",(sel & (255<<CLMETABLOCK_NEGLENGTH_SHIFT))>>CLMETABLOCK_NEGLENGTH_SHIFT);
  wr_printf("gel groundlength %d\n",(gel & (255<<CLMETABLOCK_GROUNDLENGTH_SHIFT))>>CLMETABLOCK_GROUNDLENGTH_SHIFT);
  wr_printf("sel groundlength %d\n",(sel & (255<<CLMETABLOCK_GROUNDLENGTH_SHIFT))>>CLMETABLOCK_GROUNDLENGTH_SHIFT);
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
  if ((g->instgen_strat) && 
      ((gel & (255<<CLMETABLOCK_LENGTH_SHIFT)) == 
       (sel & (255<<CLMETABLOCK_LENGTH_SHIFT)) ) &&
      ((sel & (255<<CLMETABLOCK_LENGTH_SHIFT)) < (255<<CLMETABLOCK_LENGTH_SHIFT)) &&
      ((gel!=sel) || (genblock[CLMETABLOCK_SIZES_POS]!=specblock[CLMETABLOCK_SIZES_POS]) ||
       (genblock[CLMETABLOCK_PREF1BITS_POS]!=specblock[CLMETABLOCK_PREF1BITS_POS]) ||
       (genblock[CLMETABLOCK_PREF2BITS_POS]!=specblock[CLMETABLOCK_PREF2BITS_POS]) ))
    return 0; // only shorter or exactly equal may subsume for instgen
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
  wr_printf("\nwr_active_subsumed_lit is called \n");
  wr_print_term(g,atom);  
  wr_printf("\n and negflag %d\n",negflag);
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


int wr_derived_cl_cut_and_subsume(glb* g, gptr rptr, int rpos, gptr clhashptr) {

  int i,tmp;
  gint hash,xatom,xatommeta;
  gptr xatomptr;
  cvec bucket;
  int cuts=0, oldcuts;
  gint clhash;
  gint lock;

#ifdef DEBUG
  wr_printf("\nwr_derived_cl_cut_and_subsume is called \n");
#endif  
  (g->stat_forwardsubs_attempted)++;
  (g->cut_clvec)[1]=(gint)NULL;
  clhash=0;  
  for(i=0;i<rpos;i++) { 
    tmp=i*LIT_WIDTH;
    xatom=rptr[tmp+LIT_ATOM_POS];
    xatommeta=rptr[tmp+LIT_META_POS];   
    oldcuts=cuts;
#ifdef DEBUG     
    wr_printf("\n i %d xatom %d xatommeta %d atom:\n",i,xatom,xatommeta);
    wr_print_term(g,xatom);    
#endif    
    xatomptr=rotp(g,xatom);
    hash=wr_lit_hash(g,xatom);    
#ifdef DEBUG    
    wr_printf("\ncalculated hash %d \n",hash);
#endif    
    if (wg_atom_meta_is_neg(db,xatommeta)) {
#ifdef DEBUG      
      wr_printf("\nneg\n");
#endif 
      if (g->forward_subsume_derived)      
        bucket=wr_find_termhash(g,rotp(g,g->hash_neg_groundunits),xatomptr,hash);      
      else
        bucket=NULL;       
      //printf("\ncpneg\n");
      if (bucket!=NULL) {
#ifdef DEBUG        
        wr_printf("\nsubsumed by ground hash as neg!\n"); 
        wr_print_term(g,xatomptr);
        wr_printf("\n bucket:\n");
        wr_print_clause(g,bucket); 
#endif        
        (g->stat_lit_hash_subsume_ok)++;
        (g->stat_forward_subsumed)++;
        return -1;
      }
      if (r_kb_g(g)) {
#ifdef DEBUG
        wr_printf("\nexternal kb r_kb_g(g) present");
#endif  
        if (g->forward_subsume_derived)      
          bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_neg_groundunits),xatomptr,hash);     
        else
          bucket=NULL;  
        if (bucket!=NULL) {
  #ifdef DEBUG        
          wr_printf("\nsubsumed by kb ground hash as neg!\n"); 
          wr_print_term(g,xatomptr);
          wr_printf("\n bucket:\n");
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
        wr_printf("\ncutoff by ground hash as neg!\n"); 
        wr_print_term(g,xatom);
        wr_printf("\n bucket:\n");
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
      if (r_kb_g(g) && rptr[tmp+LIT_META_POS]) {
#ifdef DEBUG
        wr_printf("\nexternal kb r_kb_g(g) present");
#endif          
        bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_pos_groundunits),xatomptr,hash);      
        if (bucket!=NULL) {         
  #ifdef DEBUG        
          wr_printf("\ncutoff by kb ground hash as neg!\n"); 
          wr_print_term(g,xatom);
          wr_printf("\n bucket:\n");
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

#ifdef GLOBAL_UNITS
      if (r_kb_g(g) && rptr[tmp+LIT_META_POS]) {
#ifdef DEBUG
        wr_printf("\nexternal kb r_kb_g(g) present");
#endif  
        if (r_kb_g(g) && (g->shared_units)) {
          lock=g_lock_shared_read(g);
          bucket=wr_find_shared_offset_termhash(g,rotp(g,(r_kb_g(g))->shared_hash_pos_groundunits),xatomptr,hash,0,NULL);      
          g_free_shared_read(g,lock);
        }   
        if (bucket!=NULL) {         
          //wr_printf("\ncut by shared ground hash as neg: "); 
          //wr_print_term(g,xatom);
          //wr_printf("\n");
          //printf("!");
          /*
          printf("\ncutoff by kb ground hash as neg!\n"); 
          wr_print_term(g,xatom);
          printf("\n bucket:\n");
          wr_print_clause(g,bucket);     
          */
  #ifdef DEBUG        
          wr_printf("\ncutoff by kb ground hash as neg!\n"); 
          wr_print_term(g,xatom);
          wr_printf("\n bucket:\n");
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
#endif

    } else {
#ifdef DEBUG      
      wr_printf("\npos\n");
#endif
      if (g->forward_subsume_derived)   
        bucket=wr_find_termhash(g,rotp(g,g->hash_pos_groundunits),xatomptr,hash);
      else
        bucket=NULL;  
      //printf("\ncppos\n");
      if (bucket!=NULL) {
#ifdef DEBUG        
        wr_printf("\nsubsumed by ground hash as pos!\n");
        wr_print_term(g,xatom);;
        wr_printf("\n bucket:\n");
        wr_print_clause(g,bucket);
#endif        
        (g->stat_lit_hash_subsume_ok)++;
        (g->stat_forward_subsumed)++;
        return -1;
      }
      if (r_kb_g(g)) {
#ifdef DEBUG
        wr_printf("\nexternal kb r_kb_g(g) present");
#endif          
        if (g->forward_subsume_derived)   
          bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_pos_groundunits),xatomptr,hash);      
        else
          bucket=NULL;  
        if (bucket!=NULL) {
  #ifdef DEBUG        
          wr_printf("\nsubsumed by kb ground hash as pos!\n"); 
          wr_print_term(g,xatomptr);
          wr_printf("\n bucket:\n");
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
        wr_printf("\ncutoff by ground hash as pos!\n"); 
        wr_print_term(g,xatom);
        wr_printf("\n bucket:\n");
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
      if (r_kb_g(g) && rptr[tmp+LIT_META_POS]) {
#ifdef DEBUG
        wr_printf("\nexternal kb r_kb_g(g) present");
#endif          
        bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_neg_groundunits),xatomptr,hash);      
        if (bucket!=NULL) {         
  #ifdef DEBUG        
          wr_printf("\ncutoff by kb ground hash as pos!\n"); 
          wr_print_term(g,xatom);
          wr_printf("\n bucket:\n");
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

#ifdef GLOBAL_UNITS
      if (r_kb_g(g) && rptr[tmp+LIT_META_POS]) {
#ifdef DEBUG
        wr_printf("\nexternal kb r_kb_g(g) present");
#endif  
        if (r_kb_g(g) && (g->shared_units)) {
          lock=g_lock_shared_read(g);
          bucket=wr_find_shared_offset_termhash(g,rotp(g,(r_kb_g(g))->shared_hash_neg_groundunits),xatomptr,hash,0,NULL);      
          g_free_shared_read(g,lock);
        }
        if (bucket!=NULL) {       
          //wr_printf("\ncut by shared ground hash as pos: "); 
          //wr_print_term(g,xatom);
          //wr_printf("\n");
          //printf("!");
          /*
          printf("\ncutoff by kb ground hash as pos!\n"); 
          wr_print_term(g,xatom);
          printf("\n bucket:\n");
          wr_print_clause(g,bucket);  
          */
  #ifdef DEBUG        
          wr_printf("\ncutoff by kb ground hash as pos!\n"); 
          wr_print_term(g,xatom);
          wr_printf("\n bucket:\n");
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
#endif

    } 
    if (clhashptr!=NULL && (oldcuts==cuts)) {
      // no cut during this iteration
      clhash+=hash;
      if (wg_atom_meta_is_neg(db,xatommeta)) clhash+=10;      
      //if (wg_atom_meta_is_neg(db,xatommeta)) hash+=10;
      //clhash = hash + (clhash << 6) + (clhash << 16) - clhash;
      if (clhash<0) clhash=0-clhash;
      clhash=(gint)(0+(clhash%(NROF_CLTERM_HASHVEC_ELS-2)));
      if (clhash<0) clhash=0-clhash;
    }     
    //blt=wr_build_calc_term(g,rptr[tmp+LIT_ATOM_POS]);       
  }
  if (clhashptr!=NULL) {
    *clhashptr=clhash;
    //printf("\nabout to call wr_find_prop_clausehash with clhash %d and rpos %d\n",clhash,rpos);
    if (wr_find_prop_clausehash(g,rotp(g,g->prop_hash_clauses),rptr,rpos,clhash)) {
      //printf("\nwr_find_prop_clausehash returned 1\n");
      return -1;
    }
  }
#ifdef DEBUG  
  wr_printf("\n! wr_derived_cl_cut_and_subsume is returning %d \n",cuts);
#endif  
  return cuts;  
}

/*


  if (tmp==0 && !doublecut_found && len>1) {
    // attempt double cut
    tmp3=wr_atom_doublecut(g,yatom,xmeta,xcl,i,&foundbucket2); 
  } else {
    tmp3=2;
  }


*/


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
  gint lock;

#ifdef DEBUG
  wr_printf("\n!!!!! wr_atom_cut_and_subsume is called \n");    
  wr_printf("\n xatom %d xatommeta %d atom:\n",xatom,xatommeta);
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
        wr_printf("\nsubsumed by ground hash as neg!\n"); 
        wr_print_term(g,xatom);
        wr_printf("\n bucket:\n");
        wr_print_clause(g,bucket); 
  #endif    
        return -1;
      }      
      if (r_kb_g(g)) {
        bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_neg_active_groundunits),xatomptr,hash);
        if (bucket!=NULL) {
    #ifdef DEBUG
          wr_printf("\nsubsumed by ground hash as neg!\n"); 
          wr_print_term(g,xatom);
          wr_printf("\n bucket:\n");
          wr_print_clause(g,bucket); 
    #endif    
          return -1;
        }
      }                   
    }  
    bucket=wr_find_termhash(g,rotp(g,g->hash_pos_groundunits),xatomptr,hash);      
    if (bucket!=NULL) {         
#ifdef DEBUG            
      wr_printf("\ncutoff by ground hash as neg!\n"); 
      wr_print_term(g,xatom);
      wr_printf("\n bucket:\n");
      wr_print_clause(g,bucket);        
#endif
      *foundbucket=bucket;      
      return 1;
    }
    if (r_kb_g(g)) {
      bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_pos_active_groundunits),xatomptr,hash);
      if (bucket!=NULL) {
  #ifdef DEBUG
        wr_printf("\ncutoff by ground hash as neg!\n"); 
        wr_print_term(g,xatom);
        wr_printf("\n bucket:\n");
        wr_print_clause(g,bucket);
  #endif    
        *foundbucket=bucket;
        return 1;
      }
    }     
#ifdef GLOBAL_UNITS
    if (r_kb_g(g) && (g->shared_units)) {
      lock=g_lock_shared_read(g);
      bucket=wr_find_shared_offset_termhash(g,rotp(g,(r_kb_g(g))->shared_hash_pos_groundunits),xatomptr,hash,0,NULL);
      g_free_shared_read(g,lock);
      if (bucket!=NULL) {
        //wr_printf("\ncut by shared ground hash as neg: "); 
        //wr_print_term(g,xatom);
        //wr_printf("\n");
        //printf("!");

  #ifdef DEBUG
        wr_printf("\ncutoff by shared ground hash as neg!\n"); 
        wr_print_term(g,xatom);
        wr_printf("\n bucket:\n");
        wr_print_clause(g,bucket);
  #endif    
        *foundbucket=bucket;
        return 1;
      }
    }
#endif
  } else {
    //printf("\npos\n");
    if (subsflag) {
      bucket=wr_find_termhash(g,rotp(g,g->hash_pos_groundunits),xatomptr,hash);
      //printf("\ncppos\n");
      if (bucket!=NULL) {
  #ifdef DEBUG
        wr_printf("\nsubsumed by ground hash as pos!\n");
        wr_print_term(g,xatom);
        wr_printf("\n bucket:\n");
        wr_print_clause(g,bucket);
  #endif
        return -1;
      }
      if (r_kb_g(g)) {
        bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_pos_active_groundunits),xatomptr,hash);
        if (bucket!=NULL) {
    #ifdef DEBUG
          wr_printf("\nsubsumed by ground hash as pos!\n"); 
          wr_print_term(g,xatom);
          wr_printf("\n bucket:\n");
          wr_print_clause(g,bucket); 
    #endif    
          return -1;
        }
      }       
    }  
    bucket=wr_find_termhash(g,rotp(g,g->hash_neg_groundunits),xatomptr,hash);      
    if (bucket!=NULL) {            
#ifdef DEBUG       
      wr_printf("\ncutoff by ground hash as pos!\n"); 
      wr_print_term(g,xatom);
      wr_printf("\n bucket:\n");
      wr_print_clause(g,bucket);    
#endif
      *foundbucket=bucket;      
      return 1;
    }
    if (r_kb_g(g)) {
      bucket=wr_find_termhash(g,rotp(g,(r_kb_g(g))->hash_neg_active_groundunits),xatomptr,hash);
      if (bucket!=NULL) {
  #ifdef DEBUG
        wr_printf("\ncutoff by ground hash as pos!\n"); 
        wr_print_term(g,xatom);
        wr_printf("\n bucket:\n");
        wr_print_clause(g,bucket);
  #endif    
        *foundbucket=bucket;
        return 1;
      }
    }     
#ifdef GLOBAL_UNITS
    if (r_kb_g(g) && (g->shared_units)) {
      lock=g_lock_shared_read(g);
      bucket=wr_find_shared_offset_termhash(g,rotp(g,(r_kb_g(g))->shared_hash_neg_groundunits),xatomptr,hash,0,NULL);
      g_free_shared_read(g,lock);
      if (bucket!=NULL) {
        //wr_printf("\ncut by shared ground hash as pos: "); 
        //wr_print_term(g,xatom);
        //wr_printf("\n");
        //wr_printf("\n bucket:\n");
        //wr_print_clause(g,bucket);
        //printf("!");

  #ifdef DEBUG
        wr_printf("\ncutoff by shared ground hash as neg!\n"); 
        wr_print_term(g,xatom);
        wr_printf("\n bucket:\n");
        wr_print_clause(g,bucket);
  #endif    
        *foundbucket=bucket;
        return 1;
      }
    }
#endif    
  }   
#ifdef DEBUG
  wr_printf("\n!!!!! wr_atom_cut_and_subsume found no subs or cuts \n");
#endif     
  return 0;  
}


/*

  return -1 if subsumed
  return 0 if no subsumption, no cuts
  return 1 if any cuts

  //skipcl is a clause which should be always skipped when comparing (set to NULL if not used)
  if subsflag==0 then no subsumption done

*/

int wr_atom_doublecut(glb* g, gint xatom, gint xatommeta, gptr xcl, int pos, cvec* foundbucket) {
  gptr xatomptr;
  //cvec bucket;
  gint bucket;
  gint hash;
  gptr bucket_asp;
  int j, shared;
  gint lock;
  int locktaken=0;

  /*
  printf("\n** wr_atom_doublecut is called \n");    
  printf("\n xatom %d xatommeta %d atom:\n",xatom,xatommeta);
  wr_print_term(g,xatom);  
  printf("\nclause\n");
  wr_print_clause(g,xcl);  
  printf("\n");
  */
  
  if (!(r_kb_g(g))) return 0;
  
#ifdef DEBUG
  wr_printf("\nwr_atom_doublecut is called \n");    
  wr_printf("\n xatom %d xatommeta %d atom:\n",xatom,xatommeta);
  wr_print_term(g,xatom);  
#endif     
  
  xatomptr=rotp(g,xatom);
  hash=wr_lit_hash(g,xatom); 
  if (wg_atom_meta_is_neg(db,xatommeta)) {
    //printf("\nneg atom\n");   
    //bucket=wr_find_termhash(g,rotp(g,g->hash_pos_grounddoubles),xatomptr,hash);      
    //--- new part starts
    shared=0;
    bucket=wr_find_offset_termbucket(g,rotp(g,g->hash_pos_grounddoubles),xatomptr,hash);
#ifdef SHARED_DERIVED
    if (!bucket && (r_kb_g(g)) && (g->shared_doubles)) {  
      lock=g_lock_shared_read(g);    
      locktaken=1;
      bucket=wr_find_shared_offset_termbucket(g,rotp(g,(r_kb_g(g))->shared_hash_pos_grounddoubles),xatomptr,hash);      
      //if (bucket) printf("B-");
      shared=1;
    }
#endif    
    if (bucket) {      
      bucket_asp=rotp(g,bucket);
      //printf("\nbucket found\n");
      //printf("bucket for hash %d size %d next free %d\n",hash,bucket_asp[0],bucket_asp[1]);
      for(j=2;j<bucket_asp[0] && j<bucket_asp[1]; j=j+2) {       
        if (wr_atom_doublecut_aux(g,xatom,xatommeta,xcl,pos,rotp(g,bucket_asp[j+1]))) {         
          if (shared) printf("+");
          else printf("*");
          *foundbucket=rotp(g,bucket_asp[j+1]);   
          if (locktaken) g_free_shared_read(g,lock);   
          return 1;
        }         
      }
      if (locktaken) g_free_shared_read(g,lock); 
      if (!shared && (r_kb_g(g)) && (g->shared_doubles)) {
        lock=g_lock_shared_read(g);
        locktaken=1;
        bucket=wr_find_shared_offset_termbucket(g,rotp(g,(r_kb_g(g))->shared_hash_pos_grounddoubles),xatomptr,hash);                
        if (!bucket) {
          if (locktaken) g_free_shared_read(g,lock); 
          return -1;
        }  
        //printf("G-");
        bucket_asp=rotp(g,bucket);
        for(j=2;j<bucket_asp[0] && j<bucket_asp[1]; j=j+2) {       
          if (wr_atom_doublecut_aux(g,xatom,xatommeta,xcl,pos,rotp(g,bucket_asp[j+1]))) {         
            printf("+");            
            *foundbucket=rotp(g,bucket_asp[j+1]);      
            if (locktaken) g_free_shared_read(g,lock); 
            return 1;
          }         
        }
        if (locktaken) g_free_shared_read(g,lock); 
      }
    } else {
      if (locktaken) g_free_shared_read(g,lock); 
    }
  } else {
    //printf("\npos atom\n");    
    //bucket=wr_find_termhash(g,rotp(g,g->hash_pos_grounddoubles),xatomptr,hash);  
    
    //--- new part starts
    shared=0;
    bucket=wr_find_offset_termbucket(g,rotp(g,g->hash_neg_grounddoubles),xatomptr,hash);
#ifdef SHARED_DERIVED
    if (!bucket && (r_kb_g(g)) && (g->shared_doubles)) {
      //CP10
      lock=g_lock_shared_read(g);
      locktaken=1;
      bucket=wr_find_shared_offset_termbucket(g,rotp(g,(r_kb_g(g))->shared_hash_neg_grounddoubles),xatomptr,hash);      
      //if (bucket) printf("B+");
      //CP11
      shared=1;
    }
#endif       
    if (bucket) {      
      bucket_asp=rotp(g,bucket);
      //printf("\nbucket found\n");
      //printf("bucket for hash %d size %d next free %d\n",hash,bucket_asp[0],bucket_asp[1]);
      for(j=2;j<bucket_asp[0] && j<bucket_asp[1]; j=j+2) {
        
        /*
        printf("j %d meaning %d term ",j,(j-2)/2);
        //wr_print_term(g,rpto(g,bucket_asp[j]));
        wr_print_term(g,bucket_asp[j]);
        printf(" in cl \n");
        wr_print_clause(g,rotp(g,bucket_asp[j+1]));
        printf("\n");
        */
        //CP12
        if (wr_atom_doublecut_aux(g,xatom,xatommeta,xcl,pos,rotp(g,bucket_asp[j+1]))) {
          //CP13
          //CP10
          /*
          printf("\nmatch found for\n");
          wr_print_clause(g,xcl);
          printf("\nwith double\n");
          wr_print_clause(g,rotp(g,bucket_asp[j+1]));
          printf("\n");
          */
          if (shared) printf("+");
          else printf("*");
          *foundbucket=rotp(g,bucket_asp[j+1]);      
          if (locktaken) g_free_shared_read(g,lock); 
          return 1;
        }
      }   
      if (locktaken) g_free_shared_read(g,lock); 
      if (!shared && (r_kb_g(g)) && (g->shared_doubles)) {  
        lock=g_lock_shared_read(g); 
        locktaken=1;  
        bucket=wr_find_shared_offset_termbucket(g,rotp(g,(r_kb_g(g))->shared_hash_pos_grounddoubles),xatomptr,hash);        
        if (!bucket) {
          if (locktaken) g_free_shared_read(g,lock); 
          return -1;
        }  
        //printf("G+");
        bucket_asp=rotp(g,bucket);
        for(j=2;j<bucket_asp[0] && j<bucket_asp[1]; j=j+2) {       
          if (wr_atom_doublecut_aux(g,xatom,xatommeta,xcl,pos,rotp(g,bucket_asp[j+1]))) {         
            printf("+");            
            *foundbucket=rotp(g,bucket_asp[j+1]); 
            if (locktaken) g_free_shared_read(g,lock);     
            return 1;
          }            
        }
        if (locktaken) g_free_shared_read(g,lock); 
      }  
    } else {
      if (locktaken) g_free_shared_read(g,lock); 
    }
  }   
#ifdef DEBUG
  wr_printf("\n!!!!! wr_atom_cut_and_subsume found no subs or cuts \n");
#endif     
  return 0;  
}

int wr_atom_doublecut_aux(glb* g, gint xatom, gint xatommeta, gptr xcl, int pos, cvec bucket) {
  int i, j, xcllen, posmatch, negmatch;
  gptr ycl;
  gint yclatom, xclatom, xclmeta,yclmeta;
  int iused,jused;

  /*
  printf("\nwr_atom_doublecut_aux called, pos %d\n",pos);
  printf("\nxatom ");
  wr_print_term(g,xatom);
  printf("\nxcl ");
  wr_print_clause(g,xcl); 
  printf("\nbucket ");
  wr_print_clause(g,bucket); 
  */

  posmatch=0;
  negmatch=0;
  iused=-1;
  jused=-1;
  ycl=bucket; 
  xcllen=wg_count_clause_atoms(db,xcl);
  for(i=0;i<2;i++) {
    yclmeta=wg_get_rule_clause_atom_meta(db,ycl,i);
    yclatom=wg_get_rule_clause_atom(db,ycl,i); 
    //printf("\ni %d ",i); 
    //wr_print_term(g,yclatom);
    //printf("\n");   
    for(j=0;j<xcllen;j++) {
      //if (i==iused || j==jused) continue;
      //printf("\nj %d ",j);   
      //if (j==posused || j==negused) continue;
      xclmeta=wg_get_rule_clause_atom_meta(db,xcl,j);      
      xclatom=wg_get_rule_clause_atom(db,xcl,j); 
      //wr_print_term(g,xclatom);
      //printf("\n"); 
      if (xatom!=xclatom && 
          ( (wg_atom_meta_is_neg(db,yclmeta) && wg_atom_meta_is_neg(db,xclmeta)) ||
            (!wg_atom_meta_is_neg(db,yclmeta) && !wg_atom_meta_is_neg(db,xclmeta)) )) {
        // same polarities and atom is not the same as cutoff atom
        if (jused!=j && iused!=i &&
            ((yclmeta&(ATOM_META_CANSUBS_MASK<<SMALLINTSHFT))==(xclmeta&(ATOM_META_CANSUBS_MASK<<SMALLINTSHFT))) &&
            wr_equal_term(g,yclatom,xclatom,1)) {
          //printf("\nposmatch found\n");    
          //wr_print_term(g,yclatom);
          //wr_print_term(g,xclatom);
          posmatch=1;
          //posused=yclatom;
          //xclatomused=xclatom;
          //yclatomused=yclatom;
          iused=i;
          jused=j;
          if (negmatch) {
            //printf("\n!!! same found\n"); 
            return 1;
          }
        }
      } else {
        // different polarities and atom is the same as cutoff atom
        if (xatom==xclatom && xatommeta==xclmeta &&
            jused!=j && iused!=i &&
            ( (wg_atom_meta_is_neg(db,yclmeta) && !wg_atom_meta_is_neg(db,xclmeta)) ||
              (!wg_atom_meta_is_neg(db,yclmeta) && wg_atom_meta_is_neg(db,xclmeta))) && 
            ((yclmeta&(ATOM_META_CANSUBS_MASK<<SMALLINTSHFT))==(xclmeta&(ATOM_META_CANSUBS_MASK<<SMALLINTSHFT))) && 
            wr_equal_term(g,yclatom,xclatom,1)) {
          //printf("\nnegmatch found\n");     
          //wr_print_term(g,yclatom);
          //wr_print_term(g,xclatom);
          negmatch=1;
          //negused=yclatom;
          //xclatomused=xclatom;
          iused=i;
          jused=j;
          if (posmatch) {
            //printf("\n!!! diff found\n");
            //CP12; 
            return 1;
          }
        }
      }
    }    
  }

  return 0;
}

/*

  return -1 if subsumed
  return 0 if no subsumption, no cuts
  return a nr of cuts (more than 0) if any cuts

*/

int wr_derived_cl_strong_cut(glb* g, gptr rptr, int rpos, int prevcuts) {

  int i,tmp;
  gint xatom,xatommeta;
  int cuts=0;
  //gint clhash;
  gptr cutter;
  int storecount,nongroundcount;


#ifdef DEBUG
  wr_printf("\nwr_derived_cl_strong_cut is called \n");
#endif          
  storecount=0;        
  nongroundcount=0;
  for(i=0; i < rpos; i++) { 
    tmp=i*LIT_WIDTH;    
    xatommeta=rptr[tmp+LIT_META_POS];    
    if (xatommeta!=0) storecount++;    
  }
  // now storecount is the length uncut
  if (storecount>1) {
    // here check whether there are more than one non-ground
    for(i=0; i < rpos; i++) { 
      tmp=i*LIT_WIDTH;    
      xatommeta=rptr[tmp+LIT_META_POS];    
      if (!xatommeta) continue;    
      xatom=rptr[tmp+LIT_ATOM_POS];
      if (wr_term_has_vars(g,xatom)) {
        nongroundcount++;
      }   
      //printf("\n pos %d nongroundcount %d ",i,nongroundcount);
      //wr_print_term(g,xatom);
      //printf("\n");
      if (nongroundcount>1) break;
    }
  }

  /* 
    clause length 1: full cutoff, no var checking
    clause contains a single non-ground: full cutoff, no var checking
    clause contains several non-ground: use atom-to-atom subsumption, not unification

   */

#ifdef DEBUG     
    wr_printf("\n storecount %d \n",storecount);    
#endif   

  for(i=0;i<rpos;i++) { 
    tmp=i*LIT_WIDTH;    
    xatommeta=rptr[tmp+LIT_META_POS];   
    if (xatommeta==0) continue;
    xatom=rptr[tmp+LIT_ATOM_POS];
    //oldcuts=cuts;
#ifdef SCUTDEBUG     
    printf("\n i %d xatom %d xatommeta %d atom:\n",i,xatom,xatommeta);
    wr_print_term(g,xatom);    
#endif       
    if (wg_atom_meta_is_neg(db,xatommeta)) {
#ifdef SCUTDEBUG      
      printf("\nneg\n");
#endif                
      cutter=strongly_cut_atom(g,xatom,rotp(g,g->hash_pos_units),(nongroundcount>1));
/*
#ifdef SHARED_DERIVED
      if (!cutter) {
        cutter=strongly_cut_atom(g,xatom,rotp(g,g->shared_hash_pos_units),(nongroundcount>1));
      }  
#endif      
*/
      if (cutter!=NULL) {         
#ifdef SCUTDEBUG        
        printf("\ncutoff as neg!\n"); 
        wr_print_term(g,xatom);
        printf("\n cutter:\n");
        wr_print_clause(g,cutter);        
#endif
        cuts++;
        rptr[tmp+LIT_META_POS]=0; // mark cutoff lit metas as 0
        if ((g->cut_clvec)[0]>prevcuts+cuts+1) {
          (g->cut_clvec)[prevcuts+cuts]=(gint)cutter;
          (g->cut_clvec)[prevcuts+cuts+1]=(gint)NULL;
        }  
        (g->stat_lit_strong_cut_ok)++; 
      }      
    } else {
#ifdef SCUTDEBUG      
      printf("\npos\n");
#endif 
      cutter=strongly_cut_atom(g,xatom,rotp(g,g->hash_neg_units),(nongroundcount>1));
/*      
#ifdef SHARED_DERIVED
      if (!cutter) {
        cutter=strongly_cut_atom(g,xatom,rotp(g,g->shared_hash_pos_units),(nongroundcount>1));
      }  
#endif        
*/
      if (cutter!=NULL) {              
#ifdef SCUTDEBUG        
        printf("\ncutoff  as pos!\n"); 
        wr_print_term(g,xatom);
        wr_printf("\n bucket:\n");
        wr_print_clause(g,cutter);    
#endif        
        cuts++;
        rptr[tmp+LIT_META_POS]=0; // mark cutoff lit metas as 0
        if ((g->cut_clvec)[0]>prevcuts+cuts+1) {
          (g->cut_clvec)[prevcuts+cuts]=(gint)cutter;
          (g->cut_clvec)[prevcuts+cuts+1]=(gint)NULL;
        }  
        (g->stat_lit_strong_cut_ok)++;       
      }
    }     
  }
 
#ifdef SCUTDEBUG  
  wr_printf("\n! wr_derived_cl_strong_cut is returning %d \n",cuts);
#endif  
  return cuts;  
}

gptr strongly_cut_atom(glb* g, gint xatom, vec hashvec, int usematch) {
  void* db=g->db;
  gint hash;
  int hlen;
  gint node;
  gint yatom=0;
  gptr ycl;
  int ures;
  int preflen, plen, iseq=0, atomlen;
  gptr tptr;
  gint a=0,b=0,ay,by;
  gint prefhashes[ATOM_PREFHASH_MAXLEN+1];
  /*
  if (wr_equality_atom(g,xatom)) {    
    tptr=rotp(g,xatom);
    atomlen=get_record_len(tptr);
    if (atomlen>=(g->unify_firstuseterm)+3) {
      iseq=1;
      // take equality args (two):
      a=tptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
      b=tptr[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];
      //atype=wg_get_encoded_type(db,a);
      //btype=wg_get_encoded_type(db,b);
    }  
  }
  */
  preflen=wr_atom_calc_prefhashes(g,xatom,prefhashes);
  for(plen=0;plen<ATOM_PREFHASH_MAXLEN;plen++) {   
    if (preflen==1) {      
      hash=WR_HASH_NORM(WR_HASH_ADD(plen,prefhashes[0]),NROF_CLTERM_HASHVEC_ELS);
    } else if (preflen==2) {      
      if (plen==0) // hash=prefhashes[0]+plen;
        hash=WR_HASH_NORM(WR_HASH_ADD(plen,prefhashes[0]),NROF_CLTERM_HASHVEC_ELS);
      else // hash=prefhashes[1]+plen;
        hash=WR_HASH_NORM(WR_HASH_ADD(plen,prefhashes[1]),NROF_CLTERM_HASHVEC_ELS);
    } else {      
      hash=WR_HASH_NORM(WR_HASH_ADD(plen,prefhashes[plen]),NROF_CLTERM_HASHVEC_ELS);
    }                     
#ifdef SCUTDEBUG
    wr_printf("\n looking for hash %ld for plen %d preflen %d\n",hash,plen,preflen);
#endif

    //wr_clterm_hashlist_print(g,hashvec);       
    hlen=wr_clterm_hashlist_len(g,hashvec,hash);
    if (hlen==0) {
#ifdef SCUTDEBUG        
      wr_printf("no matching atoms in hash\n");
#endif        
      continue;
    }     
    node=wr_clterm_hashlist_start(g,hashvec,hash);
    if (!node)  {
      wr_sys_exiterr(g,"apparently wrong hash given to wr_clterm_hashlist_start");
      return NULL;
    }      
    while(node!=0) {    
      yatom=(otp(db,node))[CLTERM_HASHNODE_TERM_POS];
      
      /*
      wr_printf("after while(node!=0): \n");
      wr_printf("\nxatom ");
      wr_print_term(g,xatom);     
      wr_printf("\nyatom ");
      wr_print_term(g,yatom);  
      printf("\nyclause ") ;
      wr_print_clause(g,otp(db,(otp(db,node))[CLTERM_HASHNODE_CL_POS]));   
      printf("\nat pos %ld\n",(otp(db,node))[CLTERM_HASHNODE_PATH_POS]);
      */            
#ifdef SCUTDEBUG        
      wr_printf("\nxatom ");
      wr_print_term(g,xatom);     
      wr_printf("\nyatom ");
      wr_print_term(g,yatom);  
      printf("\nyclause ") ;
      wr_print_clause(g,otp(db,(otp(db,node))[CLTERM_HASHNODE_CL_POS]));   
      printf("\nat pos %ld\n",(otp(db,node))[CLTERM_HASHNODE_PATH_POS]);
      wr_print_vardata(g);
#endif    
      if (usematch) {
        ures=wr_match_term(g,yatom,xatom,1);
      } else {
        ures=wr_unify_term(g,xatom,yatom,1); // uniquestrflag=1            
      }  

      if (!ures && iseq && wr_equality_atom(g,yatom)) {       
        tptr=rotp(g,yatom);
        atomlen=get_record_len(tptr);
        if (atomlen>=(g->unify_firstuseterm)+3) {
          wr_clear_varstack(g,g->varstack);
          // take equality args (two):
          ay=tptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
          by=tptr[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];
          if (usematch) {
            ures=wr_match_term(g,a,by,1);
            if (ures) {
              ures=wr_match_term(g,b,ay,1);
            }
          } else {
            ures=wr_unify_term(g,a,by,1); // uniquestrflag=1                       
            if (ures) {
              ures=wr_unify_term(g,b,ay,1);
            }
          }
        }  
      }

#ifdef SCUTDEBUG        
      wr_printf("unification check res: %d\n",ures);
#endif        
      if (ures) {        
        ycl=otp(db,(otp(db,node))[CLTERM_HASHNODE_CL_POS]);

        /*
        wr_printf("\nin wr_resolve_binary_all_active after wr_process_resolve_result\n");
        wr_printf("\nxatom\n");
        wr_print_term(g,xatom);
        wr_printf("\nyatom\n");
        wr_print_term(g,yatom);
        wr_printf(" in ycl ");
        wr_print_clause(g,ycl);
        wr_printf("\n!!!!! wr_resolve_binary_all_active after wr_process_resolve_result queue is\n");
        wr_show_clqueue(g);
        wr_printf("\nqueue ended\n");
        */
        
#ifdef SCUTDEBUG                   
        wr_printf("\nin wr_resolve_binary_all_active after wr_process_resolve_result\n");
        wr_printf("\nxatom\n");
        wr_print_term(g,xatom);
        wr_printf("\nyatom\n");
        wr_print_term(g,yatom);
        wr_printf(" in ycl ");
        wr_print_clause(g,ycl);
        wr_printf("\n!!!!! wr_resolve_binary_all_active after wr_process_resolve_result queue is\n");
        wr_show_clqueue(g);
        wr_printf("\nqueue ended\n");
#endif             
        wr_clear_varstack(g,g->varstack); // !! added in june
        return ycl;                
      }
#ifdef SCUTDEBUG
      wr_printf("wr_resolve_binary_all_active before wr_clear_varstack queue is\n");
      wr_show_clqueue(g);
      wr_printf("\nqueue ended\n");
#endif         
      wr_clear_varstack(g,g->varstack);              
      //wr_print_vardata(g);      
      // get next node;
      node=wr_clterm_hashlist_next(g,hashvec,node);               
    } /* over one single hash vector */
#ifdef SCUTDEBUG      
    wr_printf("\nexiting node loop\n");      
#endif      
  } /* over different preflens of the given atom */
  
  return NULL;
}

/* ------------------------------------------------------ 

   clause-to-clause subsumption

   ------------------------------------------------------
*/   
  
/*
  cl1 is gcl, cl2 is scl: 
  test if cl1 subsumes cl2

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
  wr_printf("\nwr_subsume_cl called on clauses %d %d :\n",(int)cl1,(int)cl2);
  wr_print_clause(g,cl1);
  wr_printf("\n");
  wr_print_clause(g,cl2);
  wr_printf("\n");
#endif

  ++(g->stat_clsubs_attempted);  

  // check fact clause cases first
  if (!wg_rec_is_rule_clause(db,cl1)) {
    if (!wg_rec_is_rule_clause(db,cl2)) {
#ifdef DEBUG  
       wr_printf("both clauses are facts \n");
#endif                 
      ++(g->stat_clsubs_unit_attempted);      
      if (wr_equal_term(g,encode_record(db,cl1),encode_record(db,cl2),uniquestrflag))
        return 1;
      else
        return 0;
    } else {
      // fact clause subsuming a rule clause
      cllen2=wg_count_clause_atoms(db,cl2);
      if ((g->instgen_strat) && cllen2<2) return 0; // only shorter may subsume for instgen
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
    if (g->instgen_strat) return 0; // only shorter may subsume for instgen
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
  wr_printf("cllen1 %d cllen2 %d\n",cllen1,cllen2);
#endif   
  // check unit rule clause case
  if (cllen1==1) { 
#ifdef DEBUG  
    wr_printf("unit clause subsumption case \n");
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
          if ((g->instgen_strat) && cllen2==1) {
            mres=wr_equal_mod_vars_term(g,lit1,lit2,uniquestrflag);
          } else {  
            mres=wr_match_term(g,lit1,lit2,uniquestrflag);                     
          }  
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
  wr_printf("general subsumption case \n");
#endif  
  /* 
  if ((g->instgen_strat) && cllen1==cllen2) {
    CP0;
    mres=wr_equal_instgen_clauses(g,cl1,cl2);
    printf("\nmres %d\n",mres);
    return mres;
  }  
  */
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
#ifdef ANS_IGNORE
  int lit1ans=0;
  int lit2ans=0;
#endif      

#ifdef DEBUG 
  wr_printf("\nwr_subsume_cl_aux called with litind1 %d litind2 %d cllen1 %d cllen2 %d gen and specific literals:\n",
    litind1,litind2,cllen1,cllen2);
  wr_print_term(g,*(litpt1+LIT_ATOM_POS)); 
  wr_printf("\n");
  wr_print_term(g,*(litpt2+LIT_ATOM_POS)); 
  wr_printf("\n");
#endif            
  if(litind1<cllen1) {   
    i1=litind1;
    pt1=litpt1;
    meta1=*(pt1+LIT_META_POS); 
    lit1=*(pt1+LIT_ATOM_POS);  
#ifdef ANS_IGNORE
    if ((g->required_answer_nr)==1 && wr_answer_lit(g,lit1)) lit1ans=1;
#endif       
    nobackflag=0; // backtracing will be prohibited if match found without vars set
    for(i2=0,pt2=litpt2; !nobackflag && i2<cllen2; i2++,pt2+=LIT_WIDTH) {
#ifdef DEBUG      
      wr_printf("\n in wr_subsume_cl_aux i2 %d nobackflag %d cllen2 %d\n",i2,nobackflag,cllen2);
#endif      
      if ((g->tmp_litinf_vec)[i2+1]==0) {
	      // literal not bound by subsumption yet
        meta2=*(pt2+LIT_META_POS); 
        lit2=*(pt2+LIT_ATOM_POS);       
#ifdef ANS_IGNORE
        if ((g->required_answer_nr)==1 && wr_answer_lit(g,lit2)) lit2ans=1;
        else lit2ans=0;
#endif
        //foundflag=0;          
#ifdef DEBUG
        wr_printf("\n in wr_subsume_cl_aux gen lit1 and specific lit2:\n");
        wr_print_term(g,lit1);
        wr_printf("\n");
        wr_print_term(g,lit2);
        wr_printf("\n");
#endif        
#ifdef ANS_IGNORE
        if (wr_sort_meta_bigger(meta1,meta2) && !lit1ans && !lit2ans) {
#else
        if (wr_sort_meta_bigger(meta1,meta2)) {
#endif          
          // lit1 cannot subsume any literal in cl2vec here or coming after, like:
          // general  s(b)
          // general  l(X,Y)
          // specific p(a,b) |  s(b) |  m(X,Y) | r(X)
          (g->stat_clsubs_meta_attempted)++;
          (g->stat_clsubs_meta_failed)++;
          return 0;
        } else if (!litmeta_negpolarities(meta1,meta2) 
#ifdef ANS_IGNORE 
                   && ((lit1ans && lit2ans) || wr_matchable_lit_meta(g,meta1,meta2))) {
#else        
                   && wr_matchable_lit_meta(g,meta1,meta2)) {          
#endif                     
#ifdef DEBUG      
          wr_printf("\n ok polarities and matchable lit meta\n");
#endif             
          vc_tmp=*(g->tmp_unify_vc); // store current value of varstack pointer ????????    
          if (((g->instgen_strat) && cllen1==cllen2) ?
               wr_eqmodvars_term_aux(g,lit1,lit2,uniquestrflag) :
#ifdef ANS_IGNORE
               (lit1ans && lit2ans) || wr_match_term_aux(g,lit1,lit2,uniquestrflag) ) {
#else               
               wr_match_term_aux(g,lit1,lit2,uniquestrflag) ) {
#endif                 
          //if (wr_match_term_aux(g,lit1,lit2,uniquestrflag)) { // not instgen case
#ifdef DEBUG      
            wr_printf("\n wr_match_term_aux returned 1\n");
#endif                
        
            
#ifdef DEBUG             
	          wr_printf("lit match ok with *(g->tmp_unify_vc): %d\n",*(g->tmp_unify_vc));
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
              wr_printf("rest ok with *(g->tmp_unify_vc): %d\n",*(g->tmp_unify_vc));		
              wr_print_vardata(g);                                  
#endif                                 
	            return 1;	    
	          } 
#ifdef DEBUG             
	          wr_printf("rest failed with *(g->tmp_unify_vc): %d\n",*(g->tmp_unify_vc));
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
  wr_printf("REASONER ERROR! something wrong in calling wr_subsume_cl_aux\n");
  //printf("litind1: %d cllen1: %d i1: %d \n", litind1,cllen1,i1);  
  return 0;  
}     

/** Match terms:  mx is the general term meta, my is a meta of an instance of x
    NB! we do not consider the polarities here, ie both pos/pos and neg/pos may be matched
*/

int wr_matchable_lit_meta(glb* g, gint mx, gint my) {

#ifdef DEBUG
  wr_printf("\nwr_matchable_lit_meta called on\n  general mx %d: ",mx);
  wr_print_cl_meta(g, mx);
  wr_printf("\n");
  wr_printf("  specific my %d: ",my);
  wr_print_cl_meta(g, my);
  wr_printf("\n");
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
