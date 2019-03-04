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

 /** @file clmeta.c
 * Clause and term metainfo functions. 
 */

/* ====== Includes =============== */


#ifdef __cplusplus
extern "C" {
#endif

#include "rincludes.h"  
 

/* ====== Private defs =========== */

//#define DEBUG
#undef DEBUG  

#undef XDEBUG
//#define XDEBUG
//#define DEBUGHASH

#define GINT_HASHMASK_LENGTH 30

#define WEIGHT_VARS

/* ====== Private headers ======== */
  
static void wr_inssort_cl(glb* g, gptr cl, int len);  
static int limit_byte(int nr);

/* ====== Functions ============== */



/* ============

 calc clause/atom/term weight (normally should use a more general metainfo calc)

 =============== */

/*

Calculate clause weight, size, depth, length
ignoring ans literals

*/

int  wr_calc_clause_weight(glb* g, gptr xptr, int* size, int* depth, int* length) {
  void* db;
  int ruleflag;       
  gint xatomnr; 
  gint xatom;  
  int i;
  int w;
  int atomdepth;
  int vc_tmp;
  
  //printf("wr_calc_clause_weight called for:\n"); 
  //wr_print_clause(g,xptr);
  //printf("\n");

  db=g->db; 
  *size=0;
  *depth=0;
  *length=0;
  ruleflag=wg_rec_is_rule_clause(db,xptr);  
  /*
  vc_tmp=2;
  *((g->varstack)+1)=vc_tmp; // zero varstack pointer  
  g->tmp_unify_vc=((gptr)(g->varstack))+1;
  */
#ifdef WEIGHT_VARS  
  vc_tmp=*(g->tmp_unify_vc);
#endif
  if (!ruleflag) {
    *length=1;
    xatom=encode_datarec_offset(pto(db,xptr));
    if (wr_answer_lit(g,xatom)) w=1; // !! special answer-lit weight
    else w=wr_calc_term_weight(g,xatom,0,size,depth);    
    //return w;        
  } else {        
    w=0;
    // loop over clause elems
    xatomnr=wg_count_clause_atoms(db,xptr);
    for(i=0;i<xatomnr;i++) {
      //xmeta=wg_get_rule_clause_atom_meta(db,xptr,i);
      xatom=wg_get_rule_clause_atom(db,xptr,i);
      if (wr_answer_lit(g,xatom)) w=w+1;
      else {
        atomdepth=0;
        w=w+wr_calc_term_weight(g,xatom,0,size,&atomdepth);   
        (*length)++;
        if (atomdepth > (*depth)) *depth=atomdepth;
      }  
    }   
  }
#ifdef WEIGHT_VARS 
  if (vc_tmp!=*(g->tmp_unify_vc)) {
    wr_clear_varstack_topslice(g,g->varstack,vc_tmp);
  }  
#endif  
  /*
  if (vc_tmp!=*((g->varstack)+1)) {
    //printf("\n clearing varstack\n");
    //wr_clear_varstack(g,g->varstack);
    wr_clear_varstack_topslice(g,g->varstack,vc_tmp);
  } 
  */
  w=w+(((*length)-1)*(g->cl_length_penalty))+(((*depth)-1)*(g->cl_depth_penalty));
  //printf("\nwr_calc_clause_weight returns weight %d and size %d depth %d length %d\n",
  //    w,*size,*depth,*length); 
  return w;
} 

int wr_calc_term_weight(glb* g, gint x, int depth, int* size, int* maxdepth) {
  void* db;
  gptr xptr;
  int i, start, end;  
  int w;
    
  //printf("wr_calc_term_weight called with x %d type %d depth %d size %d maxdepth %d\n",
  //     x,wg_get_encoded_type(g->db,x),depth,*size,*maxdepth);

  if (!isdatarec(x)) {
    // now we have a simple value  
    (*size)++;
    if (!isvar(x)) return 10;
    // here x is a var
#ifdef WEIGHT_VARS    
    if (VARVAL_DIRECT(x,(g->varbanks))==UNASSIGNED) {
      // a new var 
      SETVAR(x,encode_smallint(1),g->varbanks,g->varstack,g->tmp_unify_vc);
      //printf("\n new var\n");
      return 5;
    } else {
      // a var seen before
      //printf("\n old var\n");
      return 7;
    }    
#else
    return 5;    
#endif      
   
  }   
  // now we have a datarec
  db=g->db;
  xptr=decode_record(db,x);
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,xptr);
  w=0;    
  depth++;
  if (depth>(*maxdepth)) *maxdepth=depth;
  for(i=start;i<end;i++) {
    w=w+wr_calc_term_weight(g,xptr[i],depth,size,maxdepth);      
  }   
  return w;
}  


/* ============

 calc clause/atom/term metainfo: ground, weight, depth, hash, etc

 =============== */


void wr_print_term_metacalc(glb* g, term_metacalc* metaptr) {
  printf("\nterm_metacalc values:\n");
  printf("  isground %d\n", metaptr->hasvars); // 1 iff ground
  printf("  depth %d\n",metaptr->depth);  // clause depth: 1 is predicate without nesting
  printf("  size %d\n",metaptr->size);   // clause size: count of all pred/fun/const/var occurrences
  printf("  weight %d\n",metaptr->weight);  // weighted size
  printf("   preflen %d\n",metaptr->preflen); // length of ground prefix: for ground lit it is size
  printf("   topvars %d\n",metaptr->topvars);  // nr of var occurrences on top lit level
  printf("   prefhash %d\n",metaptr->prefhash);  // hash of ground prefix
  printf("   pref1hash %d\n",metaptr->pref1hash);
  printf("   pref2hash %d\n",metaptr->pref2hash);
  printf("   pref3hash %d\n",metaptr->pref3hash);
  printf("   groundhash %d\n",metaptr->groundhash); // not used yet
}

void wr_print_cl_metacalc(glb* g, cl_metacalc* metaptr) {
  printf("\ncl_metacalc values:\n");
  printf("  length    %10d\n", metaptr->length); // 1, 2, ...
  printf("  neglength %10d\n", metaptr->neglength); // 1, 2, ...
  printf("  groundlen %10d\n", metaptr->groundlength); // 1, 2, ...
  printf("  isground  %10d\n", metaptr->hasvars); // 1 iff ground
  printf("  depth     %10d\n",metaptr->depth);  // clause depth: 1 is predicate without nesting
  printf("  size      %10d\n",metaptr->size);   // clause size: count of all pred/fun/const/var occurrences
  printf("  weight    %10d\n",metaptr->weight);  // weighted size
  printf("   preflen  %10d\n",metaptr->preflen); // length of ground prefix: for ground lit it is size
  printf("   topvars  %10d\n",metaptr->topvars);  // nr of var occurrences on top lit level
  printf("   prefhash %10d\n",metaptr->prefhash);  // hash of ground prefix

  printf("   pref1bits %10d ",(int)(metaptr->pref1bits)); // pref len 1 (pred symbs with sign)
  wr_print_gint_hashmask(g,metaptr->pref1bits);
  printf("\n");
  printf("   pref2bits %10d ",(int)(metaptr->pref2bits)); // pref len 2 (pred symbs with sign + immediate args
  wr_print_gint_hashmask(g,metaptr->pref2bits);
  printf("\n");
  printf("   pref3bits %10d ",(int)(metaptr->pref3bits)); // pref len 3
  wr_print_gint_hashmask(g,metaptr->pref3bits);
  printf("\n"); 
  printf("   groundbits %10d\n",(int)(metaptr->groundbits)); // one bit for each full-ground lit (from ground lit hash)
  wr_print_gint_hashmask(g,metaptr->groundbits);
  printf("\n");

  printf("   pref1hash %d\n",metaptr->pref1hash);
  printf("   pref2hash %d\n",metaptr->pref2hash);
  printf("   pref3hash %d\n",metaptr->pref3hash);
  printf("   groundhash %d\n",metaptr->groundhash);  
}


gint wr_litmeta_to_gint(glb* g,term_metacalc* metaptr) {
  gint res=0,tmp;
  
  tmp=(metaptr->hasvars);
  if (!tmp) tmp=1;
  else tmp=0;
  res=res|(tmp<<ATOM_META_GROUND_SHIFT);   
  tmp=(metaptr->size);
  if (tmp>ATOM_META_SIZE_MASK) tmp=ATOM_META_SIZE_MASK;
  res=res|(tmp<<ATOM_META_SIZE_SHIFT);
  tmp=(metaptr->depth);
  if (tmp>ATOM_META_DEPTH_MASK) tmp=ATOM_META_DEPTH_MASK;
  res=res|(tmp<<ATOM_META_DEPTH_SHIFT);
  tmp=(metaptr->preflen);
  if (tmp>ATOM_META_PREFLEN_MASK) tmp=ATOM_META_PREFLEN_MASK;
  res=res|(tmp<<ATOM_META_PREFLEN_SHIFT);
  tmp=(metaptr->prefhash);
  if (tmp>ATOM_META_PREFHASH_MASK) 
    tmp=(tmp % ATOM_META_PREFHASH_MASK)&ATOM_META_PREFHASH_MASK;
  res=res|(tmp<<ATOM_META_PREFHASH_SHIFT);  
  //*/
  return encode_smallint(res);  
}



gint wr_calc_clause_meta(glb* g, gptr xptr, gptr cl_metablock) {
  term_metacalc litmeta = {0,0,0,0,0,0,0,0,0,0,0};
  cl_metacalc clmeta = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  void* db;
  int ruleflag, negflag;       
  gint xatomnr; 
  gint xmeta, xatom; 
  gint litmetagint=0;
  gint clmetagint=0; 
  int i,tmp;
  /*
  printf("\nwr_calc_clause_meta called on \n"); 
  wr_print_clause(g,xptr);
  */
  db=g->db;
  ruleflag=wg_rec_is_rule_clause(db,xptr);  
  if (!ruleflag) {
    //printf("\nnot a ruleclause\n");
    negflag=0;
    wr_calc_term_meta(g,encode_datarec_offset(pto(db,xptr)),0,0,&litmeta);
    clmeta.length=1;
    clmeta.neglength=0;
    clmeta.groundlength=!litmeta.hasvars;
    clmeta.hasvars=litmeta.hasvars;
    clmeta.depth=litmeta.depth;
    clmeta.size=litmeta.size;
    clmeta.weight=litmeta.weight;
    clmeta.preflen=litmeta.preflen;
    clmeta.topvars=litmeta.topvars;
    clmeta.prefhash=litmeta.prefhash;
    if (litmeta.pref1hash) 
      clmeta.pref1bits=wr_signed_lithash_to_bitmask(g,litmeta.pref1hash,negflag);    
    if (litmeta.pref2hash) 
      clmeta.pref2bits=wr_lithash_to_bitmask(g,litmeta.pref2hash);    
    if (litmeta.pref3hash) 
      clmeta.pref3bits=wr_lithash_to_bitmask(g,litmeta.pref3hash);
    //clmeta.=litmeta.;
    litmetagint=wr_litmeta_to_gint(g,&litmeta);
    //clmetagint=clmetagint|RECORD_META_FACT_CLAUSE; // ??
    //xptr[RECORD_META_POS]=clmetagint;
    //wr_print_term_metacalc(g,&litmeta);      
  } else {        
    //printf("\nis a ruleclause\n");
    // loop over clause elems
    xatomnr=wg_count_clause_atoms(db,xptr);
    clmeta.length=xatomnr;    
    for(i=0;i<xatomnr;i++) {
      xmeta=wg_get_rule_clause_atom_meta(db,xptr,i);  
      xatom=wg_get_rule_clause_atom(db,xptr,i);            
      wr_calc_term_meta(g,xatom,0,i,&litmeta);  
      litmetagint=wr_litmeta_to_gint(g,&litmeta); // !!!! must fix term/cl_metacalc
      if (wg_atom_meta_is_neg(db,xmeta)) {
        negflag=1;
        litmetagint=litmetagint|ATOM_META_NEG_RAWMASK;
      } else negflag=0;

      /*
      if (wg_atom_meta_is_neg(db,litmetagint)!=wg_atom_meta_is_neg(db,xmeta)) {
        printf("\n *?*?*? wrong %d %d %d %d\n",
          litmetagint,xmeta,wg_atom_meta_is_neg(db,litmetagint),wg_atom_meta_is_neg(db,xmeta));
        wr_print_term(g,xatom);
        wr_print_clause(g,xptr);
        wr_print_term_metacalc(g,&litmeta);
      } 
      */ 
      //if (wg_atom_meta_is_neg(db,xmeta)) litmetagint=ATOM_META_NEG;
      //else litmetagint=0;
      wg_set_rule_clause_atom_meta(db,xptr,i,litmetagint); // !!!
      /*
      wr_print_term_metacalc(g,&litmeta);
      printf("\nmeta gint %d\n",wr_litmeta_to_gint(g,&litmeta));
      */
      if (xatomnr<2) {
        clmetagint=wr_litmeta_to_gint(g,&litmeta);
      } else {
        // xatomnr>1
      }  
      if (negflag) clmeta.neglength++;
      if (litmeta.hasvars) clmeta.hasvars=1;
      else clmeta.groundlength++;
      if (litmeta.depth>clmeta.depth) clmeta.depth=litmeta.depth;     
      if (litmeta.preflen>clmeta.preflen) clmeta.preflen=litmeta.preflen;
      clmeta.size+=litmeta.size;
      clmeta.weight+=litmeta.weight;
      if (litmeta.pref1hash) 
        clmeta.pref1bits=clmeta.pref1bits | 
          wr_signed_lithash_to_bitmask(g,litmeta.pref1hash,negflag);      
      if (litmeta.pref2hash) 
        clmeta.pref2bits=clmeta.pref2bits | 
          wr_lithash_to_bitmask(g,litmeta.pref2hash);      
      if (litmeta.pref3hash) 
        clmeta.pref3bits=clmeta.pref3bits | 
          wr_lithash_to_bitmask(g,litmeta.pref3hash);      

      if (i+1<xatomnr) {
        litmeta.hasvars=0; 
        litmeta.depth=0;
        litmeta.size=0;
        litmeta.weight=0;
        litmeta.preflen=0;
        litmeta.topvars=0;
        litmeta.prefhash=0;
        litmeta.groundhash=0;
      }       
    }  
    if (!clmetagint) clmetagint=wr_litmeta_to_gint(g,&clmeta);  
  }
  // pack clmeta struct to cl_metablock
  if (cl_metablock!=NULL) {
    tmp=(!clmeta.hasvars)<<CLMETABLOCK_ISGROUND_SHIFT |
        limit_byte(clmeta.length)<<CLMETABLOCK_LENGTH_SHIFT  | 
        limit_byte(clmeta.neglength) <<CLMETABLOCK_NEGLENGTH_SHIFT | 
        limit_byte(clmeta.groundlength) <<CLMETABLOCK_GROUNDLENGTH_SHIFT;
    cl_metablock[CLMETABLOCK_LENGTHS_POS]=tmp;
    tmp=limit_byte(clmeta.depth) << CLMETABLOCK_DEPTH_SHIFT |
        limit_byte(clmeta.size) << CLMETABLOCK_SIZE_SHIFT | 
        limit_byte(clmeta.preflen) << CLMETABLOCK_PREFLEN_SHIFT;
    cl_metablock[CLMETABLOCK_SIZES_POS]=tmp;
    cl_metablock[CLMETABLOCK_PREF1BITS_POS]=clmeta.pref1bits;
    cl_metablock[CLMETABLOCK_PREF2BITS_POS]=clmeta.pref2bits;
    cl_metablock[CLMETABLOCK_PREF3BITS_POS]=clmeta.pref3bits;
  }

  //finished
  /*
  printf("wr_calc_clause_meta returns for the clause:\n"); 
  wr_print_cl_metacalc(g,&clmeta);
  
   printf("\n in details:");            
          printf("\nmeta addr %d\n",(int)cl_metablock); 
          printf("\nmetablock2 %d %d %d %d \n",*(cl_metablock),*(cl_metablock+1),*(cl_metablock+2),*(cl_metablock+3));
          printf("\n lengths   ");
          wr_print_gint_hashmask(g,cl_metablock[CLMETABLOCK_LENGTHS_POS]);
          printf("\n real length %d\n",
            (*(cl_metablock) & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
          printf("\n sizes     ");
          wr_print_gint_hashmask(g,cl_metablock[CLMETABLOCK_SIZES_POS]);
          printf("\n pref1bits ");
          wr_print_gint_hashmask(g,cl_metablock[CLMETABLOCK_PREF1BITS_POS]);
          printf("\n pref2bits ");
          wr_print_gint_hashmask(g,cl_metablock[CLMETABLOCK_PREF2BITS_POS]);
          printf("\n pref3bits ");
          wr_print_gint_hashmask(g,cl_metablock[CLMETABLOCK_PREF3BITS_POS]);
          printf("\n");
  */        
  //clmetagint=wr_litmeta_to_gint(g,&clmeta);

  return litmetagint;
} 

static int limit_byte(int nr) {
  if (nr<255 && nr>=0) return nr;
  if (nr>255) return 255;
  //if (nr<0) 
  return 0;
}

gint wr_signed_lithash_to_bitmask(glb* g, int hash, int negflag) {
  int pos;

  pos=(hash % (GINT_HASHMASK_LENGTH / 2))+1;  
  if (negflag) return 1<<(pos+(GINT_HASHMASK_LENGTH / 2));
  else return 1<<pos;
}

gint wr_lithash_to_bitmask(glb* g, int hash) {
  int pos;

  pos=(hash % GINT_HASHMASK_LENGTH)+1;  
  return 1<<pos;
}

void wr_calc_term_meta(glb* g, gint x, int depth, int pos, term_metacalc* metaptr) {
  void* db;
  gptr xptr;
  int i, start, end;
  gint hash;
    
  //printf("wr_calc_term_meta called with x %d type %d\n",x,wg_get_encoded_type(g->db,x));
  if (!isdatarec(x)) {
    // now we have a simple value  
    (metaptr->size)++;
    if (!isvar(x)) {
      (metaptr->weight)+=CONSTANT_WEIGHT;    
    } else {
      (metaptr->weight)+=VAR_WEIGHT;
      (metaptr->hasvars)=1;
      if (depth==1) (metaptr->topvars)++;
    }
    if (!(metaptr->hasvars)) {  
      hash=wr_term_basehash(g,x);
      (metaptr->prefhash)+=hash;  // !! needs bitshifting improvement !!
      (metaptr->preflen)++;
      if (metaptr->preflen==1) (metaptr->pref1hash)=(metaptr->prefhash); 
      else if (metaptr->preflen==2) (metaptr->pref2hash)=(metaptr->prefhash);
      else if (metaptr->preflen==3) (metaptr->pref3hash)=(metaptr->prefhash);
    } 
    return;  
  }   
  // now we have a datarec
  db=g->db;
  xptr=decode_record(db,x);
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,xptr);
  if ((metaptr->depth)<(depth+1)) (metaptr->depth)=depth+1;
  for(i=start;i<end;i++) {
    wr_calc_term_meta(g,xptr[i],depth+1,i-start,metaptr);      
  }   
  return;
}  





void wr_print_cl_meta(glb* g, gint meta1) {
  unsigned int size = 28;
  unsigned int bit,mask;
  int i,num;

  //meta1=5<<SMALLINTSHFT;

  printf(" GROUND %d SIZE %d DEPTH %d PREFLEN %d DEPTHCANSUBS %d PREFLENCANSUBS %d ",        
        (int)((meta1&((ATOM_META_GROUND_MASK<<ATOM_META_GROUND_SHIFT)<<SMALLINTSHFT))
          >>ATOM_META_GROUND_SHIFT)>>SMALLINTSHFT,        
        (int)((meta1&((ATOM_META_SIZE_MASK<<ATOM_META_SIZE_SHIFT)<<SMALLINTSHFT))
          >>ATOM_META_SIZE_SHIFT)>>SMALLINTSHFT, 
        (int)((meta1&((ATOM_META_DEPTH_MASK<<ATOM_META_DEPTH_SHIFT)<<SMALLINTSHFT))
          >>ATOM_META_DEPTH_SHIFT)>>SMALLINTSHFT,
        (int)((meta1&((ATOM_META_PREFLEN_MASK<<ATOM_META_PREFLEN_SHIFT)<<SMALLINTSHFT))
          >>ATOM_META_PREFLEN_SHIFT)>>SMALLINTSHFT,
        (int)(meta1&(ATOM_META_DEPTHCANSUBS_MASK<<SMALLINTSHFT)),
        (int)(meta1&(ATOM_META_PREFLENCANSUBS_MASK<<SMALLINTSHFT)) );
  printf("\n");      
  printf("; int      29       24              16               8             1|\n");
  printf("          |r|s|c|e|i|l|n|g|  size   | depth   | pln | prfhs         |\n");
  printf("           ");
  num=meta1>>SMALLINTSHFT;
  for(i=size; i>=0; --i){
    mask=1<<i;
    bit=num & mask;
    if (bit) {
      printf("1 ");      
    } else {
      printf("0 ");
    }   
  }  
  printf("\n");
}


void wr_print_gint_hashmask(glb* g, gint meta) {
  unsigned int size = GINT_HASHMASK_LENGTH;
  unsigned int bit,mask;
  int i,num;

  num=meta;
  for(i=size; i>=0; --i){
    mask=1<<i;
    bit=num & mask;
    if (bit) {
      printf("1");      
    } else {
      printf("0");
    }   
  }    
}

/* ---------------------------------------------------------------------- 
 
 term decoration

------------------------------------------------------------------------ */

/*

  Modify literals in the clause with meta

*/

gint wr_decorate_clause(glb* g, gptr xptr) {
  term_metacalc litmeta = {0,0,0,0,0,0,0,0};
  term_metacalc clmeta;
  void* db;
  int ruleflag;       
  gint xatomnr; 
  gint xmeta, xatom; 
  gint litmetagint=0;
  gint clmetagint=0; 
  int i;
  
  printf("\nwr_decorate_term called on \n"); 
  wr_print_clause(g,xptr);

  db=g->db;
  ruleflag=wg_rec_is_rule_clause(db,xptr);
  if (!ruleflag) {
    //printf("\nnot a ruleclause\n");
    wr_decorate_term(g,encode_datarec_offset(pto(db,xptr)),0,0,&litmeta);
    clmetagint=wr_litmeta_to_gint(g,&litmeta);
    litmetagint=clmetagint;
    //clmetagint=clmetagint|RECORD_META_FACT_CLAUSE; // ??
    //xptr[RECORD_META_POS]=clmetagint;
    //wr_print_term_metacalc(g,&litmeta);      
  } else {        
    //printf("\nis a ruleclause\n");
    // loop over clause elems
    xatomnr=wg_count_clause_atoms(db,xptr);
    if (xatomnr>1) {
      clmeta.hasvars=0; 
      clmeta.depth=0;
      clmeta.size=0;
      clmeta.weight=0;
    }
    for(i=0;i<xatomnr;i++) {
      xmeta=wg_get_rule_clause_atom_meta(db,xptr,i);  
      xatom=wg_get_rule_clause_atom(db,xptr,i);            
      wr_calc_term_meta(g,xatom,0,i,&litmeta);  
      litmetagint=wr_litmeta_to_gint(g,&litmeta);
      if (wg_atom_meta_is_neg(db,xmeta)) litmetagint=litmetagint|ATOM_META_NEG_RAWMASK;
      /*
      if (wg_atom_meta_is_neg(db,litmetagint)!=wg_atom_meta_is_neg(db,xmeta)) {
        printf("\n *?*?*? wrong %d %d %d %d\n",
          litmetagint,xmeta,wg_atom_meta_is_neg(db,litmetagint),wg_atom_meta_is_neg(db,xmeta));
        wr_print_term(g,xatom);
        wr_print_clause(g,xptr);
        wr_print_term_metacalc(g,&litmeta);
      } 
      */ 
      //if (wg_atom_meta_is_neg(db,xmeta)) litmetagint=ATOM_META_NEG;
      //else litmetagint=0;
      wg_set_rule_clause_atom_meta(db,xptr,i,litmetagint); // !!!
      
      //wr_print_term_metacalc(g,&litmeta);
      //printf("\nmeta gint %d\n",wr_litmeta_to_gint(g,&litmeta));
      if (xatomnr<2) {
        clmetagint=wr_litmeta_to_gint(g,&litmeta);
      } else {
        // xatomnr>1
        if (litmeta.hasvars) clmeta.hasvars=1;
        if (litmeta.depth>clmeta.depth) clmeta.depth=litmeta.depth;
        clmeta.size+=litmeta.size;
        clmeta.weight+=litmeta.weight;
        if (i+1<xatomnr) {
          litmeta.hasvars=0; 
          litmeta.depth=0;
          litmeta.size=0;
          litmeta.weight=0;
          litmeta.preflen=0;
          litmeta.topvars=0;
          litmeta.prefhash=0;
          litmeta.groundhash=0;
        }  
      }      
    }  
    if (!clmetagint) clmetagint=wr_litmeta_to_gint(g,&clmeta);  
  }
  
  //printf("wr_calc_clause_meta returns for the clause:\n"); 
  //wr_print_term_metacalc(g,&clmeta);
  //clmetagint=wr_litmeta_to_gint(g,&clmeta);
  return litmetagint;
} 

/*

  change term by replacing the first el of each record with metainfo

*/


void wr_decorate_term(glb* g, gint x, int depth, int pos, term_metacalc* metaptr) { 
  void* db;
  gptr xptr;
  int i, start, end;  
    
  printf("wr_decorate_term called with x %d type %d\n",(int)x,(int)wg_get_encoded_type(g->db,x));

  if (!isdatarec(x)) {
    // now we have a simple value  
    (metaptr->size)++;
    if (!isvar(x)) {
      (metaptr->weight)+=CONSTANT_WEIGHT;    
    } else {
      (metaptr->weight)+=VAR_WEIGHT;
      (metaptr->hasvars)=1;
      if (depth==1) (metaptr->topvars)++;
    }
    if (!(metaptr->hasvars)) {
      (metaptr->prefhash)+=wr_term_basehash(g,x); // !! needs bitshifting improvement !!
      (metaptr->preflen)++;
    } 
    return;  
  }   
  // now we have a datarec
  db=g->db;
  xptr=decode_record(db,x);
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,xptr);
  if ((metaptr->depth)<(depth+1)) (metaptr->depth)=depth+1;
  for(i=start;i<end;i++) {
    wr_calc_term_meta(g,xptr[i],depth+1,i-start,metaptr);      
  }   

  return;
}   


/* -----------------------------

  Clause sorting

-------------------------------- */

 
void wr_sort_cl(glb* g, gptr cl) {
  void* db;
  int len;       
  /*
  dp("\nwr_sort_cl called with:\n"); 
  wr_print_clause(g,cl);
  printf("\n");
  wg_print_record(g->db,cl);
  printf("\n");  
  printf("field 0: %d field 1: %d\n",get_field(cl,0),get_field(cl,1));
  wr_print_cl_meta(g,get_field(cl,0));
  printf("\n");
  */
  db=g->db; 
  if (!wg_rec_is_rule_clause(db,cl)) return; // no sorting for fact clauses
  len=wg_count_clause_atoms(db,cl);
  if (len<2) return; // no sorting for unit clauses
  // now we have a rule clause of len at least 2: sort!
  wr_inssort_cl(g,cl,len);
  /*
  dp("\nwr_sort_cl returns with sorted:\n"); 
  wr_print_clause(g,cl);
  printf("\n");
  wg_print_record(g->db,cl);
  */
  return;
} 

/* 
  insertion sort for rule clauses
  
  put larger atoms to the beginning and smaller ones to the end
  ground are larger than non-ground
  physically larger or deeper are larger than physically smaller
*/

static void wr_inssort_cl(glb* g, gptr cl, int len) { 
  void* db=g->db;
  int i,j;
  gint imeta,jmeta;
  gptr iatom,jatom;

  for(i=1; i<len; i++) {
    imeta=wg_get_rule_clause_atom_meta(db,cl,i);
    iatom=wg_get_rule_clause_atom(db,cl,i);
    /*
    printf("\n wr_inssort_cl i: %d imeta: %d, iatom: %d metadetails:\n",i,(int)imeta,(int)iatom);
    wr_print_cl_meta(g,imeta);
    printf("\n");
    */
    j=i-1;
    jmeta=wg_get_rule_clause_atom_meta(db,cl,j);
    /*
    printf("\n j: %d, jmeta: %d, metadetails:\n",j,(int)jmeta);
    wr_print_cl_meta(g,jmeta);
    printf("\n");
    if (wr_sort_meta_bigger(jmeta, imeta)) printf("\njmeta is bigger\n");
    else printf("\njmeta is not bigger\n");
    */
    while(j>=0 && !wr_sort_meta_bigger(jmeta, imeta)) {
      wg_set_rule_clause_atom_meta(db,cl,j+1,jmeta);
      jatom=wg_get_rule_clause_atom(db,cl,j);
      wg_set_rule_clause_atom(db,cl,j+1,jatom);
      j--;
      jmeta=wg_get_rule_clause_atom_meta(db,cl,j);
    }
    wg_set_rule_clause_atom_meta(db,cl,j+1,imeta);
    wg_set_rule_clause_atom(db,cl,j+1,iatom);   
  }
}

/*

  sort comparison: jmeta > imeta or not?

  ground are larger than non-ground
  physically larger or deeper are larger than physically smaller
  
*/

int wr_sort_meta_bigger(gint jmeta,  gint imeta) {
  if ((jmeta&((ATOM_META_GROUND_MASK<<ATOM_META_GROUND_SHIFT)<<SMALLINTSHFT))) {
    // jmeta is ground
    if ((imeta&((ATOM_META_GROUND_MASK<<ATOM_META_GROUND_SHIFT)<<SMALLINTSHFT))) {
      // jmeta and imeta are both ground
      if (jmeta>imeta) return 1; // creates quite full meta-based ordering of ground literals
      else return 0;
    } else {
      // jmeta ground, imeta non-ground (here ground are bigger than non-ground)
      return 1;
    }
  } else {
    // jmeta is non-ground
    if ((imeta&((ATOM_META_GROUND_MASK<<ATOM_META_GROUND_SHIFT)<<SMALLINTSHFT))) {
      // jmeta non-ground, imeta is ground 
      return 0;     
    } else {
      // imeta and jmeta both non-ground
      if  ((jmeta&(ATOM_META_CANSUBS_MASK<<SMALLINTSHFT)) > 
           (imeta&(ATOM_META_CANSUBS_MASK<<SMALLINTSHFT))) return 1;       
      else return 0;
    }
  }
}


/* 
  quicksort for rule clauses
  
void wr_qsort_cl(glb* g, gptr cl, int low, int high) { 
  int pi; // partioning index
  if (low<high) { 
    pi = wr_partition_cl(g,cl,low,high); 
    // separately sort elements 
    wr_qsort_cl(g,cl,low,pi-1); 
    wr_qsort_cl(g,cl,pi+1,high); 
  }
}



  partioning helper for wr_qsort_cl
 
  take last element as pivot, place
  the pivot element at its correct position in sorted 
  array, place all smaller than pivot 
  to left of pivot and all greater elements to right 
  of pivot 


int wr_partition_cl(glb* g, gptr cl, int low, int high) { 
  void* db=g->db;  
  
  //int arr[high];    // pivot 
  //int pivot_atom=
  //int i=(low - 1);  // index of smaller element 

  int pivot_i, i, j;
  gint pivot_meta, pivot_atom, meta, atom;
  
  i=low-1;
  pivot_i=(low + high) / 2;
  pivot_meta=wg_get_rule_clause_atom_meta(db,cl,pivot_i);
  pivot_atom=wg_get_rule_clause_atom(db,xcl,pivot_i);


  for (j=low; j<=high-1; j++) { 
    // If current element is smaller than or 
    // equal to pivot 
    meta=wg_get_rule_clause_atom_meta(db,cl,j);
    if (meta <= pivot_meta) { 
      i++;  // increment index of smaller element 
      wr_swap_cl(&arr[i], &arr[j]); 
    } 
  } 
  wr_swap_cl(&arr[i + 1], &arr[high]); 
  return (i + 1); 
} 


  swapping helper for wr_qsort_cl / wr_partition_cl
 

 
void wr_swap_cl(int* a, int* b) { 
  int t = *a; 
  *a = *b; 
  *b = t; 
} 
*/


#ifdef __cplusplus
}
#endif



