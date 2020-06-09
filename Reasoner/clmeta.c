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

#define STORE_TERMMETA

#define PENALIZE_DEFS

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
  gint xatom=0;  
  int i;
  int w,weight,is_rewrite;
  int max_ground_weight=0;
  int atomdepth;
  int vc_tmp;
  int hasvars;
  int eqtermorder,atomlen;
  gint a,b; //,atype,btype;
  gptr tptr;

#ifdef DEBUG  
  wr_printf("wr_calc_clause_weight called for:\n"); 
  wr_print_clause(g,xptr);
  wr_printf("\n");
#endif
  db=g->db; 
  *size=0;
  *depth=0;
  *length=0;
  ruleflag=wg_rec_is_rule_clause(db,xptr);  
#ifdef WEIGHT_VARS  
  vc_tmp=*(g->tmp_unify_vc);
#endif
  if (!ruleflag) {
    xatomnr=1;
    *length=1;
    xatom=encode_datarec_offset(pto(db,xptr));
    hasvars=0;
    if (wr_answer_lit(g,xatom)) w=1; // !! special answer-lit weight
    else w=wr_calc_term_weight(g,xatom,0,size,depth,0,&hasvars);    
    weight=w;   
    if (!hasvars) {
      if (weight>max_ground_weight) max_ground_weight=weight;
    }      
  } else {        
    w=0;
    // loop over clause elems
    xatomnr=wg_count_clause_atoms(db,xptr); 
    max_ground_weight=0;   
    for(i=0;i<xatomnr;i++) {
      //xmeta=wg_get_rule_clause_atom_meta(db,xptr,i);
      xatom=wg_get_rule_clause_atom(db,xptr,i);
      if (wr_answer_lit(g,xatom)) w=w+1;
      else {
        atomdepth=0;
        hasvars=0;
        weight=wr_calc_term_weight(g,xatom,0,size,&atomdepth,0,&hasvars);
        w=w+weight;
        if (g->atom_poseq_penalty) {
          if (wr_equality_atom(g,xatom) && 
              !wg_atom_meta_is_neg(db,wg_get_rule_clause_atom_meta(db,xptr,i))) {
            w=w+(g->atom_poseq_penalty);
          }
        }  
        (*length)++;
        if (atomdepth > (*depth)) *depth=atomdepth;
        if (!hasvars) {
          if (weight>max_ground_weight) max_ground_weight=weight;
        }  
      }  
    }   
  }
#ifdef WEIGHT_VARS 
  if (vc_tmp!=*(g->tmp_unify_vc)) {
    wr_clear_varstack_topslice(g,g->varstack,vc_tmp);
  }  
#endif  
  // now check if rewrite rule
  is_rewrite=0;
  if (// TESTING: normally not blocked by 0
      xatomnr==1 &&
      (g->use_equality) && (g->use_rewrite_terms_strat) && 
      wr_equality_atom(g,xatom) &&
      ruleflag && 
      ! wg_atom_meta_is_neg(db,wg_get_rule_clause_atom_meta(db,xptr,0)) ) {
    tptr=rotp(g,xatom);
    atomlen=get_record_len(tptr);
    if (atomlen>=(g->unify_firstuseterm)+3) {
      // take equality args (two):
      a=tptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
      b=tptr[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];     
      //atype=wg_get_encoded_type(db,a);
      //btype=wg_get_encoded_type(db,b);
      eqtermorder=wr_order_eqterms(g,a,b,NULL);
      // 1: a bigger than b (prohibits b)
      // 2: b bigger than a (prohibits a)
      if (((eqtermorder==1) && wg_get_encoded_type(db,a)!=WG_VARTYPE) || 
          ((eqtermorder==2) && wg_get_encoded_type(db,b)!=WG_VARTYPE) ) {
        // rewrite rule  
        //printf("\nfound rewrite rule\n");
        //wr_print_clause(g,xptr);
        is_rewrite=1;
        w=(int)((float)w/2.0); // NORMAL /2.0 TESTING *0.8
        //w=(int)((float)w*0.9);
      }
    }
  }      
  if ((g->use_max_ground_weight) && max_ground_weight) {
    w=max_ground_weight+(((*length)-1)*(g->cl_length_penalty))+(((*depth)-1)*(g->cl_depth_penalty));
    if (is_rewrite) w=(int)((float)w/2.0); // NORMAL /2.0 TESTING *0.8
  } else {
    w=w+(((*length)-1)*(g->cl_length_penalty))+(((*depth)-1)*(g->cl_depth_penalty));
  }  

  //if (is_rewrite) w=(int)((float)w/3.0); // NORMAL /2.0 TESTING *0.8

#ifdef DEBUG  
  wr_printf("\nwr_calc_clause_weight returns weight %d and size %d depth %d length %d\n",
      w,*size,*depth,*length); 
#endif  
  return w;
} 

int wr_calc_term_weight(glb* g, gint x, int depth, int* size, int* maxdepth, int pos, int* hasvars) {
  void* db;
  gptr xptr;
  int i, start, end;  
  int w;
#ifdef PENALIZE_DEFS
  char* str;
#endif  

#ifdef DEBUG    
  wr_printf("wr_calc_term_weight called with x %d type %d depth %d size %d maxdepth %d pos %d hasvars %d\n",
       x,wg_get_encoded_type(g->db,x),depth,*size,*maxdepth,pos,*hasvars);
#endif
  if (!isdatarec(x)) {
    // now we have a simple value  
    (*size)++;
    if (!isvar(x)) {
#ifdef PENALIZE_DEFS
      //db=g->db;
      //if (wg_get_encoded_type(db,x)==WG_URITYPE) {   
      if (!pos && depth<2 && islongstr(x)) {
        str = wg_decode_uri(g->db,x);    
        if (str[0]=='$' && str[1]=='d' && str[2]=='f') {
          return 50;
        } else {
          return 10;
        }  
      } else return 10;  
#else
      *hasvars=1;
      return 10;
#endif               
  }  

    // here x is a var
#ifdef WEIGHT_VARS    
    if (VARVAL_DIRECT(x,(g->varbanks))==UNASSIGNED) {
      // a new var 
      SETVAR(x,encode_smallint(1),g->varbanks,g->varstack,g->tmp_unify_vc);
      return (g->var_weight); // TESTING normal 5, testing 1
    } else {
      // a var seen before
      return (g->repeat_var_weight); // TESTING normal 7, testing 1
    }    
#else
    return (g->var_weight);  // TESTING normal 5, testing 1   
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
    w=w+wr_calc_term_weight(g,xptr[i],depth,size,maxdepth,i-start,hasvars);      
  }   
  return w;
}  


int wr_term_has_vars(glb* g, gint x) {
  void* db;
  gptr xptr;
  int i, start, end;  

  if (!isdatarec(x)) {
    // now we have a simple value  
    return isvar(x);
  }     
  // now we have a datarec
  db=g->db;
  xptr=decode_record(db,x);
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,xptr);
  for(i=start;i<end;i++) {
    if (wr_term_has_vars(g,xptr[i])) return 1;         
  }   
  return 0;
}  

/* ============

 calc clause/atom/term metainfo: ground, weight, depth, hash, etc

 =============== */


void wr_print_term_metacalc(glb* g, term_metacalc* metaptr) {
  wr_printf("\nterm_metacalc values:\n");
  wr_printf("  isground %d\n", metaptr->hasvars); // 1 iff ground
  wr_printf("  depth %d\n",metaptr->depth);  // clause depth: 1 is predicate without nesting
  wr_printf("  size %d\n",metaptr->size);   // clause size: count of all pred/fun/const/var occurrences
  wr_printf("  weight %d\n",metaptr->weight);  // weighted size
  wr_printf("   preflen %d\n",metaptr->preflen); // length of ground prefix: for ground lit it is size
  wr_printf("   topvars %d\n",metaptr->topvars);  // nr of var occurrences on top lit level
  wr_printf("   prefhash %d\n",metaptr->prefhash);  // hash of ground prefix
  wr_printf("   pref1hash %d\n",metaptr->pref1hash);
  wr_printf("   pref2hash %d\n",metaptr->pref2hash);
  wr_printf("   pref3hash %d\n",metaptr->pref3hash);
  wr_printf("   groundhash %d\n",metaptr->groundhash); // not used yet
}

void wr_print_cl_metacalc(glb* g, cl_metacalc* metaptr) {
  wr_printf("\ncl_metacalc values:\n");
  wr_printf("  length    %10d\n", metaptr->length); // 1, 2, ...
  wr_printf("  neglength %10d\n", metaptr->neglength); // 1, 2, ...
  wr_printf("  groundlen %10d\n", metaptr->groundlength); // 1, 2, ...
  wr_printf("  isground  %10d\n", metaptr->hasvars); // 1 iff ground
  wr_printf("  depth     %10d\n",metaptr->depth);  // clause depth: 1 is predicate without nesting
  wr_printf("  size      %10d\n",metaptr->size);   // clause size: count of all pred/fun/const/var occurrences
  wr_printf("  weight    %10d\n",metaptr->weight);  // weighted size
  wr_printf("   preflen  %10d\n",metaptr->preflen); // length of ground prefix: for ground lit it is size
  wr_printf("   topvars  %10d\n",metaptr->topvars);  // nr of var occurrences on top lit level
  wr_printf("   prefhash %10d\n",metaptr->prefhash);  // hash of ground prefix

  wr_printf("   pref1bits %10d ",(int)(metaptr->pref1bits)); // pref len 1 (pred symbs with sign)
  wr_print_gint_hashmask(g,metaptr->pref1bits);
  wr_printf("\n");
  wr_printf("   pref2bits %10d ",(int)(metaptr->pref2bits)); // pref len 2 (pred symbs with sign + immediate args
  wr_print_gint_hashmask(g,metaptr->pref2bits);
  wr_printf("\n");
  wr_printf("   pref3bits %10d ",(int)(metaptr->pref3bits)); // pref len 3
  wr_print_gint_hashmask(g,metaptr->pref3bits);
  wr_printf("\n"); 
  wr_printf("   groundbits %10d\n",(int)(metaptr->groundbits)); // one bit for each full-ground lit (from ground lit hash)
  wr_print_gint_hashmask(g,metaptr->groundbits);
  wr_printf("\n");

  wr_printf("   pref1hash %d\n",metaptr->pref1hash);
  wr_printf("   pref2hash %d\n",metaptr->pref2hash);
  wr_printf("   pref3hash %d\n",metaptr->pref3hash);
  wr_printf("   groundhash %d\n",metaptr->groundhash);  
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
  void* db=g->db;
  term_metacalc litmeta = {0,0,0,0,0,0,0,0,0,0,0};
  cl_metacalc clmeta = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  
  int ruleflag, negflag;       
  gint xatomnr; 
  gint xmeta, xatom; 
  gint litmetagint=0;
  //gint clmetagint=0; 
  int i,tmp;

#ifdef DEBUG   
  wr_printf("\nwr_calc_clause_meta called on \n"); 
  wr_print_clause(g,xptr);
  wr_printf("\n");
#endif  
  UNUSED(db);
  ruleflag=wg_rec_is_rule_clause(db,xptr);  
  if (!ruleflag) {
    negflag=0;
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
    litmetagint=wr_litmeta_to_gint(g,&litmeta);     
  } else {            
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
      
      if (wg_atom_meta_is_neg(db,litmetagint)!=wg_atom_meta_is_neg(db,xmeta)) {
        wr_printf("\n *?*?*? wrong %ld %ld %ld %ld\n",
          litmetagint,xmeta,wg_atom_meta_is_neg(db,litmetagint),wg_atom_meta_is_neg(db,xmeta));
        wr_print_term(g,xatom);
        wr_print_clause(g,xptr);
        wr_print_term_metacalc(g,&litmeta);
      }       
      wg_set_rule_clause_atom_meta(db,xptr,i,litmetagint); // !!!
      if (xatomnr<2) {
        //clmetagint=wr_litmeta_to_gint(g,&litmeta);
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
#ifdef DEBUG
  wr_printf("wr_calc_clause_meta returns for the clause:\n"); 
  wr_print_cl_metacalc(g,&clmeta);
  
  wr_printf("\n in details:");            
  wr_printf("\nmeta addr %d\n",(int)cl_metablock); 
  wr_printf("\nmetablock2 %d %d %d %d \n",*(cl_metablock),*(cl_metablock+1),*(cl_metablock+2),*(cl_metablock+3));
  wr_printf("\n lengths   ");
  wr_print_gint_hashmask(g,cl_metablock[CLMETABLOCK_LENGTHS_POS]);
  wr_printf("\n real length %d\n",
    (*(cl_metablock) & (255<<CLMETABLOCK_LENGTH_SHIFT))>>CLMETABLOCK_LENGTH_SHIFT);
  wr_printf("\n sizes     ");
  wr_print_gint_hashmask(g,cl_metablock[CLMETABLOCK_SIZES_POS]);
  wr_printf("\n pref1bits ");
  wr_print_gint_hashmask(g,cl_metablock[CLMETABLOCK_PREF1BITS_POS]);
  wr_printf("\n pref2bits ");
  wr_print_gint_hashmask(g,cl_metablock[CLMETABLOCK_PREF2BITS_POS]);
  wr_printf("\n pref3bits ");
  wr_print_gint_hashmask(g,cl_metablock[CLMETABLOCK_PREF3BITS_POS]);
  wr_printf("\n");
#endif        

  return litmetagint;
} 

static int limit_byte(int nr) {
  if (nr<255 && nr>=0) return nr;
  if (nr>255) return 255;
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

/*

  Recursively calculates term metainf (ground / size) and stores to the term
  meta pos: 
  do not apply to not-rule-clauses since these should have the history at the
  term meta pos

*/


gint wr_calc_term_meta(glb* g, gint x, int depth, int pos, term_metacalc* metaptr) {
  void* db;
  gptr xptr;
  int start,end,i;
  gint hash, hashtemp;  
  gint tmeta=0;
#ifdef USE_TERM_META 
  int hasvars;
#else
  gint smeta;    
#endif    

  UNUSED(end);  
  UNUSED(smeta);
#ifdef DEBUG  
  wr_printf("wr_calc_term_meta called with x %d type %d\n",x,wg_get_encoded_type(g->db,x));
#endif  
  if (!isdatarec(x)) {
    // now we have a simple value  
    (metaptr->size)++;
    if (!isvar(x)) {
      (metaptr->weight)+=CONSTANT_WEIGHT;
      tmeta=1 | TERMMETA_GROUND_MASK;   
    } else {
      (metaptr->weight)+=(g->var_weight);
      (metaptr->hasvars)=1;
      if (depth==1) (metaptr->topvars)++;
      tmeta=1;
    }
    if (!(metaptr->hasvars)) {  
      hash=wr_term_basehash(g,x);
      hashtemp=(metaptr->prefhash)+hash; // !! needs bitshifting improvement !!       
      //hashtemp=hashtemp%256;
      if (hashtemp<0) hashtemp=0-hashtemp;
      (metaptr->prefhash)=hashtemp;    
      (metaptr->preflen)++;
      if (metaptr->preflen==1) (metaptr->pref1hash)=(metaptr->prefhash); 
      else if (metaptr->preflen==2) (metaptr->pref2hash)=(metaptr->prefhash);
      else if (metaptr->preflen==3) (metaptr->pref3hash)=(metaptr->prefhash);     
    } 
    return tmeta;  
  }   
  // now we have a datarec
  db=g->db;
  xptr=decode_record(db,x);
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,xptr);
  if ((metaptr->depth)<(depth+1)) (metaptr->depth)=depth+1;

// new starts
#ifdef USE_TERM_META  
  tmeta=0;
  hasvars=0;
#endif  
  for(i=start;i<end;i++) {
    smeta=wr_calc_term_meta(g,xptr[i],depth+1,i-start,metaptr);  
#ifdef USE_TERM_META        
    tmeta=tmeta+(smeta & TERMMETA_SIZE_MASK);
    if (tmeta>TERMMETA_SIZE_MASK) tmeta=TERMMETA_SIZE_MASK;
    if (!(smeta & TERMMETA_GROUND_MASK)) hasvars=1;
#endif    
  }   
#ifdef USE_TERM_META    
  if (!hasvars) tmeta=tmeta | TERMMETA_GROUND_MASK;
#ifdef STORE_TERMMETA
  xptr[start-TERMMETA_POS_DIFF]=encode_smallint(tmeta);
#ifdef DEBUG
  if (xptr[start-TERMMETA_POS_DIFF]==0 || !isdatarec(xptr[start-TERMMETA_POS_DIFF])) {    
    wr_printf("\nOK %ld %ld\n",xptr[start-TERMMETA_POS_DIFF],tmeta);
    wg_print_record(db,xptr);
    wr_printf(" end\n");
    xptr[start-TERMMETA_POS_DIFF]=encode_smallint(tmeta);
    
  } else {
    wr_printf("\nPROBLEM %ld \n",xptr[start-TERMMETA_POS_DIFF]);
    wr_print_term(g,rpto(g,xptr));
    wr_printf("\n");
    wg_print_record(db,xptr);
    wr_printf("\n");
  } 
#endif
#endif  
#endif  
  return tmeta;
}


void wr_print_cl_meta(glb* g, gint meta1) {
  unsigned int size = 28;
  unsigned int bit,mask;
  int i,num;

  wr_printf(" GROUND %d SIZE %d DEPTH %d PREFLEN %d DEPTHCANSUBS %d PREFLENCANSUBS %d ",        
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
  wr_printf("\n");      
  wr_printf("; int      29       24              16               8             1|\n");
  wr_printf("          |r|s|c|e|i|l|n|g|  size   | depth   | pln | prfhs         |\n");
  wr_printf("           ");
  num=meta1>>SMALLINTSHFT;
  for(i=size; i>=0; --i){
    mask=1<<i;
    bit=num & mask;
    if (bit) {
      wr_printf("1 ");      
    } else {
      wr_printf("0 ");
    }   
  }  
  wr_printf("\n");
}


void wr_print_cl_literals_meta(glb* g, gptr cl) {
   int i, len;
   gint imeta, iatom;

   wr_printf("\nmetas for clause with \n");
   wr_print_clause(g,cl);
   if (!wg_rec_is_rule_clause(g->db,cl)) {
     wr_printf("\n not a rule clause");
     return;
   }
   len=wg_count_clause_atoms(db,cl);

   for(i=0; i<len; i++) {
    wr_printf("\n atom %d ",i);
    imeta=(gint)(wg_get_rule_clause_atom_meta(db,cl,i));
    iatom=(gint)(wg_get_rule_clause_atom(db,cl,i));
    wr_print_term(g,iatom);
    wr_printf("\n");   
    wr_print_cl_meta(g,imeta);
    wr_printf("\n");
   }  
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
      wr_printf("1");      
    } else {
      wr_printf("0");
    }   
  }    
}


/* -----------------------------

  Clause sorting by meta 

-------------------------------- */

 
void wr_sort_cl(glb* g, gptr cl) {
  void* db;
  int len;       
#ifdef DEBUG
  dp("\nwr_sort_cl called with:\n"); 
  wr_print_clause(g,cl);
  wr_printf("\n");
  wg_print_record(g->db,cl);
  wr_printf("\n");  
  wr_printf("field 0: %d field 1: %d\n",get_field(cl,0),get_field(cl,1));
  wr_print_cl_meta(g,get_field(cl,0));
  wr_printf("\n");
#endif
  db=g->db; 
  UNUSED(db);
  if (!wg_rec_is_rule_clause(db,cl)) return; // no sorting for fact clauses
  len=wg_count_clause_atoms(db,cl);
  if (len<2) return; // no sorting for unit clauses
  // now we have a rule clause of len at least 2: sort!
  wr_inssort_cl(g,cl,len);
#ifdef DEBUG
  dp("\nwr_sort_cl returns with sorted:\n"); 
  wr_print_clause(g,cl);
  wr_printf("\n");
  wg_print_record(g->db,cl);
#endif
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

  UNUSED(db);
  for(i=1; i<len; i++) {
    imeta=(gint)(wg_get_rule_clause_atom_meta(db,cl,i));
    iatom=(gptr)(wg_get_rule_clause_atom(db,cl,i));        
    j=i-1;
    jmeta=wg_get_rule_clause_atom_meta(db,cl,j);
    
    while(j>=0 && !wr_sort_meta_bigger(jmeta, imeta)) {
      wg_set_rule_clause_atom_meta(db,cl,j+1,(gint)jmeta);
      jatom=(gptr)(wg_get_rule_clause_atom(db,cl,j));
      wg_set_rule_clause_atom(db,cl,j+1,(gptr)jatom);
      j--;
      jmeta=wg_get_rule_clause_atom_meta(db,cl,j);
    }
    wg_set_rule_clause_atom_meta(db,cl,j+1,(gint)imeta);
    wg_set_rule_clause_atom(db,cl,j+1,(gptr)iatom);   
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
  quicksort for a list of meta+rule vector

  NB! low and high are from 0,... with increment 1:
  mapping to actual vec address is done later, (i*2)+1
*/

void wr_qsort_metaclvec(glb* g, gptr vec, gint low, gint high) { 
  gint i, j;
  gint tmpm, tmpc, pivot;

  if (low<high) { 
    // partition
    i = low, j = high;
    pivot=vec[(((i+j)/2)*2)+1];
    for(;;) {
      while(vec[(i*2)+1] < pivot) i++;
      while(pivot < vec[(j*2)+1]) j--;
      if(i >= j) break;
      // swap two elems with metas
      tmpm = vec[(i*2)+1]; 
      tmpc = vec[(i*2)+2]; 
      vec[(i*2)+1] = vec[(j*2)+1]; 
      vec[(i*2)+2] = vec[(j*2)+2];
      vec[(j*2)+1] = tmpm;
      vec[(j*2)+2] = tmpc;
      i++; j--;
    }
    // separately sort parts
    wr_qsort_metaclvec(g,vec,low,i-1); 
    wr_qsort_metaclvec(g,vec,j+1,high); 
  }
}



#ifdef __cplusplus
}
#endif



