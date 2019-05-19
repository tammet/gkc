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

 /** @file clstore.c
 * Clause storage functions. 
 */

/* ====== Includes =============== */


#ifdef __cplusplus
extern "C" {
#endif

#include "rincludes.h"  
 

/* ====== Private defs =========== */

//#define DEBUG
#undef DEBUG  

//#define QPICKEDDEBUG // minimal info on where cl was chosen from
//#define QADDDEBUG // queue creation and adding debug
//#define QPICKDEBUG // queue picking debug


//#define PARADEBUG // storing para terms

//#define XDEBUG
//#undef XDEBUG
//#define DEBUGHASH

//#define SIMPLE_ACTIVE_SEARCH_HASH

/* ====== Private headers ======== */
  

  
/* ====== Functions ============== */


/*

store a clause in a passive stack

*/

void wr_push_clpickstack_cl(glb* g, gptr cl) {

#ifdef DEBUG  
  printf("\npushing to clpickstack pos %d\n",(int)((rotp(g,g->clpickstack))[1]));  
#endif  
  (g->clpickstack)=rpto(g,wr_cvec_push(g,rotp(g,(g->clpickstack)),(gint)cl));
  //wr_show_clpickstack(g);
}


void wr_show_clpickstack(glb* g) {
  int i;
  
  for(i=2;i<(rotp(g,g->clpickstack))[1];i++) {
    printf("\nclpickstack nr %d :",i);
    wr_print_record(g,(gptr)((rotp(g,g->clpickstack))[i]));    
  } 
}


/*

store a clause in a passive queue

*/

void wr_push_clqueue_cl(glb* g, gptr cl) {

#ifdef DEBUG  
  printf("\npushing to clqueue pos %d\n",(int)((rotp(g,g->clqueue))[1]));  
#endif  
  (g->clqueue)=rpto(g,wr_cvec_push(g,rotp(g,(g->clqueue)),(gint)cl));
  //wr_show_clqueue(g);
}


void wr_show_clqueue(glb* g) {
  int i;
  
  for(i=2;i<(rotp(g,g->clqueue))[1];i++) {
    printf("\nclqueue nr %d :",i);
    wr_print_record(g,(gptr)((rotp(g,g->clqueue))[i]));
    printf(" ");
    if (wg_rec_is_rule_clause(g->db, (gptr)((rotp(g,g->clqueue))[i]))) {
      printf("rule");
    } else {
      printf("fact");
    }
  } 
  printf("\n");
}

/*

make a clause active

*/


void wr_show_clactive(glb* g) {
  int i;
  
  for(i=2;i<(rotp(g,g->clactive))[1];i++) {
    printf("\nclactive nr %d :",i);
    wr_print_record(g,(gptr)((rotp(g,g->clactive))[i]));    
  } 
}

void wr_show_clactivesubsume(glb* g) {
  int i;
  
  if (!(g->clactivesubsume)) {
    printf("\n g->clactivesubsume is NULL\n");
    return;
  }
  for(i=2;i<(rotp(g,g->clactivesubsume))[1];i+=CLMETABLOCK_ELS) {
    printf("\nclactivesubsume nr %d :",i);       
    printf("\n lengths   ");
    wr_print_gint_hashmask(g, (rotp(g,g->clactivesubsume))[i+CLMETABLOCK_LENGTHS_POS]);
    printf("\n sizes     ");
    wr_print_gint_hashmask(g, (rotp(g,g->clactivesubsume))[i+CLMETABLOCK_SIZES_POS]);
    printf("\n pref1bits ");
    wr_print_gint_hashmask(g, (rotp(g,g->clactivesubsume))[i+CLMETABLOCK_PREF1BITS_POS]);
    printf("\n pref2bits ");
    wr_print_gint_hashmask(g, (rotp(g,g->clactivesubsume))[i+CLMETABLOCK_PREF2BITS_POS]);
    printf("\n pref3bits ");
    wr_print_gint_hashmask(g, (rotp(g,g->clactivesubsume))[i+CLMETABLOCK_PREF3BITS_POS]);
    printf("\n clause    ");
    wr_print_clause(g,rotp(g,(rotp(g,g->clactivesubsume))[i+CLMETABLOCK_CL_POS]));
    printf("\n record    ");
    wg_print_record(g->db,rotp(g,(rotp(g,g->clactivesubsume))[i+CLMETABLOCK_CL_POS]));
    //printf("\n cl offset stored in (g->clactivesubsume): %ld", 
    //          ((rotp(g,g->clactivesubsume))[i+CLMETABLOCK_CL_POS]));
    printf("\n"); 
  } 
}

/*

store resolvable literals/terms of a clause to fast resolvable-lit list
returns 0 iff ok

hash adding:
 
  - separately for pos and neg literals (g->hash_pos_atoms_bits and g->hash_neg_atoms_bits)
    - g->hash_pos_atoms_bits is a cvec with required hash pos combinations, 
      each as a gint of bits correponding to positions
    - g->hash_pos_atoms_vecs is a cvec with els pointing to hashvec-s of corresponding 
      bit/pos values

  - two hash systems: 
    
    - bit/pos for top-level-ground atoms
    - non-var top-level prefix for non-top-level-ground
  
  - unifiable atoms of each active clause are entered to hash for all given bit/pos values
    where all corresp subterms are non-var
  
  - usage of stored hash for unification candidates:

    - pick active, say -p(a,X) | -p(X,b) | r(a,b)
    - search the p(a hash (bits 11) for all matches and use them 
      like p(a,c), p(a,f(X)) ...
    - then search the p( hash (bits 1) for all matches and use them
      like p(X,c), p(X,X), ...
    - then search the univ list for all matches and use them
      like X(Y,Y), ...
      
    - easy for finding unifiable active ground atoms: 
      pick the hash with most bits nonvar: this is the
      best option and covers all unifiable ground atoms
      like for p(a,X) pick p(a, for p(a,c) pick this
      
    - suppose we search unifiable atoms for p(a,X)
     
      - we have p(a,Z): need p(a
      - we have p(Y,b): need p(
    
      - p(a,Z) comes up twice: as p(a and as p(
        - how to skip p( case? p( would be
          needed for finding p(Y,b) where we have
          var at hash bit/pos          
        - we could also mark handled cases

    - normally we search unifiable atoms for ground atoms?
    
  - idea: N-nonvar subcases

    0 nonvar: full list
    1 nonvar: hash over all nonvars
    2 nonvar: hash over all nonvars

    search:

    p(X,Y):

    use 1 nonvar hash
    use 0 nonvar hash

    p(a,Y)
    
    use 2 nonvar hash to find p(a,Z) and p(Z,a)
    use 1 nonvar hash to find p(Z,U)
    use 0 nonvar hash to find W(U,V)

    - 0-var: ground case, full bit/pos hash storage
       - search: just look for max bit/pos combo
    - 1-var: store in possible bit/pos hashes
       - search: 
    - ...
    - N/all-var: list 
    
    
    
    
    
    
    
  - idea: N-len ground prefixes
  
      suppose we search unifiable atoms for p(a,X)
  
      - we have p(a,Z): need p(a
      - we have p(Y,b): need p( 
      
      suppose we search unifiable atoms for p(U,V)
  
      - we have p(a,Z): need p(
      - we have p(Y,b): need p( 
      
       suppose we search unifiable atoms for W(U,V)
  
      - we have p(a,Z): need full list
      - we have p(Y,b): need full list
      
      
      NB! no overlap in ground prefix lists: each
          atom in exactly one      

      p(a,X) would be in 2-pref
      p(Y,X) would be in 1-pref
      U(Y,X) would be in 0-pref
      
      search unifiers for p(a,U):
        - search all hashes from 2-pref to lower
        should find 
        p(a,V) in 2-pref
        p(Y,b) in 1-pref
        X(Y,c) in 0-pref
        
      search unifiers for p(X,Y):
        should find 
        p(a,V) in 2-pref?? no, need to put p(a,V) to 1-pref as well
      
      --- two lists: pred hash and full ---      
      
      full contains everything
      full is used only by X(a,b) cases with var pred
      
      predhash contains all with pred
      predhash is used by all p(X,Y) cases?
      how to find X(U,V) then?
      
      --- just pred hash list
      
      search unifiers for p(X,Y): normal
      
      search unifiers for U(X,Y): scan all active clauses
            
      
*/


int wr_cl_store_res_terms(glb* g, gptr cl) {    
  void* db=g->db;
  int i,j;
  int len;      
  int ruleflag; // 0 if not rule
  int posok=1;  // default allow
  int negok=1;  // default allow 
  int nonanslen; // length not counting ans literals;
  gint meta;
  gint atom;  
  int negflag; // 1 if negative
  //int termflag; // 1 if complex atom  
  gint hash=0;
  int addflag=0;
  int negadded=0;
  int posadded=0;
  vec hashvec;
  int tmp;
  int preflen;
  gint prefhashes[ATOM_PREFHASH_MAXLEN+1];
  
#ifdef DEBUG
  printf("cl_store_res_terms called on cl: "); 
  wr_print_clause(g,cl);
#endif  

  // get clause data for input clause
       
  ruleflag=wg_rec_is_rule_clause(db,cl);
  if (ruleflag) len = wg_count_clause_atoms(db, cl);
  else len=1;
  // for negpref check out if negative literals present
  wr_set_stratlimits_cl(g,cl,ruleflag,len,&posok,&negok,&nonanslen);

#ifdef DEBUG
  printf("ruleflag %d len %d posok %d negok %d\n",
          ruleflag,len,posok,negok);
#endif  
  // loop over literals
#if 0
/* XXX: FIXME */
#ifdef USE_CHILD_DB
  if (ruleflag) parent = wg_get_rec_base_offset(db,cl);
#endif
#endif
  for(i=0; i<len; i++) {  
    negflag=0;
    //termflag=0;
    addflag=0;
    if (!ruleflag) {
      atom=encode_record(db,cl);      
      addflag=1;
    } else {       
      meta=wg_get_rule_clause_atom_meta(db,cl,i);
      if (wg_atom_meta_is_neg(db,meta)) negflag=1;
      if (!((g->negpref_strat) || (g->pospref_strat)) ||
          (negflag && (g->hyperres_strat)) ||
          (negok && negflag && !negadded) || 
          (posok && !negflag)) {            
        if (negflag) negadded++; 
        else posadded++;          
        atom=wg_get_rule_clause_atom(db,cl,i);
        if (wg_get_encoded_type(db,atom)==WG_RECORDTYPE) {
          //termflag=1;
#if 0
/* XXX: FIXME */
#ifdef USE_CHILD_DB
          if(parent) atom=wg_encode_parent_data(parent,atom);
#endif             
#endif
        }               
        addflag=1;
      }      
    }
    if (addflag) {
#ifdef XDEBUG 
      printf("adding to hash g->hash_pos_atoms or g->hash_neg_atoms in wr_cl_store_res_terms\n");
#endif       

#ifdef SIMPLE_ACTIVE_SEARCH_HASH
      hash=wr_atom_funhash(g,atom);
      preflen=1;
#else
      preflen=wr_atom_calc_prefhashes(g,atom,prefhashes);        
#endif
      if (negflag) hashvec=rotp(g,g->hash_neg_atoms);
      else hashvec=rotp(g,g->hash_pos_atoms);
#ifdef XDEBUG 
      printf("before adding to hash negflag: %d hash: %d\n",negflag,(int)hash);
#endif     
      for(j=0;j<preflen;j++) {
#ifdef SIMPLE_ACTIVE_SEARCH_HASH
#else        
        hash=WR_HASH_NORM(WR_HASH_ADD(preflen-1,prefhashes[j]),NROF_CLTERM_HASHVEC_ELS);
#endif    
        //printf("\n storing preflen %d j %d prefhashes[j] %d hash %d \n",preflen,j,prefhashes[j],hash);    
#ifdef DEBUG        
        printf("\n storing preflen %d j %d prefhashes[j] %d hash %d \n",preflen,j,prefhashes[j],hash);
#endif        
        tmp=wr_clterm_add_hashlist(g,hashvec,hash,atom,cl);  
#ifdef XDEBUG 
        printf("adding to hash ended with %d\n",tmp);
#endif       
        if (tmp) {
          wr_sys_exiterr2int(g,"adding term to hashlist in cl_store_res_terms, code ",tmp);
          return 1;        
        }  
      }  
#ifdef DEBUGHASH      
      //printf("\nhash table after adding:");      
      //wr_clterm_hashlist_print(g,hashvec);     
      printf("\nhash_pos_atoms after adding:");      
      wr_clterm_hashlist_print(g,rotp(g,g->hash_pos_atoms));
      printf("\nhash_neg_atoms after adding:");      
      wr_clterm_hashlist_print(g,rotp(g,g->hash_neg_atoms));  
#endif      
    }  
  }     
#ifdef DEBUG
  printf("cl_store_res_terms finished\n"); 
#endif      
  return 0;
}

/*
   store all these for the clause cl:

   * subterms into a hash structure for para-from later
   * equality args into a hash structure for para-into later
   * equality as demodulator if orientable

*/

int wr_cl_store_para_terms(glb* g, gptr cl) {    
  void* db=g->db;
  int i;
  int len;      
  int ruleflag; // 0 if not rule
  int poscount=0; // used only for pos/neg pref
  int negcount=0; // used only for pos/neg pref
  int posok=1;  // default allow
  int negok=1;  // default allow
  gint meta;
  gint atom;  
  int negflag; // 1 if negative
  int negadded=0;
  int posadded=0;
  int addflag=0;
  int termpath=0;
  gptr tptr;
  gint a,b,atype,btype;
  int tlen;
  int eqtermorder;
  
#ifdef DEBUG
  printf("cl_store_para_terms called on cl: "); 
  wr_print_clause(g,cl);
#endif 
  // get clause data for input clause
  ruleflag=wg_rec_is_rule_clause(db,cl);
  if (ruleflag) len = wg_count_clause_atoms(db, cl);
  else len=1;
  // for negpref check out if negative literals present
  if (1) {
    // prohibit pos or neg    
    if ((g->negpref_strat) || (g->pospref_strat)) {
      if (!ruleflag) {
        poscount=1;
        negcount=0;
      } else {         
        poscount=0;
        negcount=0;        
        for(i=0; i<len; i++) {          
          meta=wg_get_rule_clause_atom_meta(db,cl,i);
          if (wg_atom_meta_is_neg(db,meta)) negcount++;
          else poscount++; 
        }  
        // set neg/pospref!    
        if (g->negpref_strat) {
          if (poscount>0 && negcount>0) posok=0;
        }      
        if (g->pospref_strat) {
          if (poscount>0 && negcount>0) negok=0;
        }
      }
    }
  }
#ifdef DEBUG
  printf("ruleflag %d len %d poscount %d negcount %d posok %d negok %d\n",
          ruleflag,len,poscount,negcount,posok,negok);
#endif  
  // loop over literals
  for(i=0; i<len; i++) {  
    negflag=0;
    atom=0;
    //termflag=0;
    addflag=0;
    termpath=0;
    if (!ruleflag) {
      atom=encode_record(db,cl);      
      addflag=1;
    } else {      
      meta=wg_get_rule_clause_atom_meta(db,cl,i);
      if (wg_atom_meta_is_neg(db,meta)) negflag=1;
      if (!((g->negpref_strat) || (g->pospref_strat)) ||
          (negflag && (g->hyperres_strat)) ||
          (negok && negflag && !negadded) || 
          (posok && !negflag)) {            
        if (negflag) negadded++; 
        else posadded++;          
        atom=wg_get_rule_clause_atom(db,cl,i);
        if (wg_get_encoded_type(db,atom)==WG_RECORDTYPE) {
          //termflag=1;
        }               
        addflag=1;
      }      
    }
    if (!atom) continue;
    // store subterms into a hash structure for para-from later
    if (addflag) {
      wr_cl_store_para_subterms(g,cl,atom,0,i,&termpath);
    }
    // store equality args into a hash structure for para-into later
    if (addflag && !negflag && wr_equality_atom(g,atom)) {
      tptr=rotp(g,atom);
      tlen=get_record_len(tptr);
      if (tlen<(g->unify_firstuseterm)+3) continue;              
      a=tptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
      b=tptr[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];                 
      atype=wg_get_encoded_type(db,a);
      btype=wg_get_encoded_type(db,b); 
      if (g->queryfocus_strat) eqtermorder=3;
      else eqtermorder=wr_order_eqterms(g,a,b,NULL);
      // eqtermorder values:
      // 0: none bigger, neither ok for para
      // 1: a bigger than b (prohibits b)
      // 2: b bigger than a (prohibits a)
      // 3: none bigger, both ok for para

      //printf("\n in wr_cl_store_para_terms eqtermorder:%d ",eqtermorder);

      if ((eqtermorder==1 || eqtermorder==3) && atype!=WG_VARTYPE) {
          //(atype==WG_RECORDTYPE || atype==WG_URITYPE || atype==WG_ANONCONSTTYPE)) {
        // ok to para from left
        wr_cl_store_eq_arg(g,cl,a,atype,i,1);
      }
      if ((eqtermorder==2 || eqtermorder==3) && btype!=WG_VARTYPE) {
          //(btype==WG_RECORDTYPE || btype==WG_URITYPE || btype==WG_ANONCONSTTYPE)) {
        // ok to para from right
        wr_cl_store_eq_arg(g,cl,b,btype,i,0);
      }     
      // store equality as demodulator if everything ok
      if (len==1 && eqtermorder==1 && (g->use_rewrite_terms_strat) && atype!=WG_VARTYPE) {
        // a will rewrite to b
        wr_cl_store_term_rewriter(g,cl,a,atype,i,1);
        (g->have_rewrite_terms)=1;
      } else if (len==1 && eqtermorder==2 && (g->use_rewrite_terms_strat) && btype!=WG_VARTYPE) {
        // b will rewrite to a
        wr_cl_store_term_rewriter(g,cl,b,btype,i,0);
        (g->have_rewrite_terms)=1;
      }
    }
  }  
  return 0;
}


int wr_cl_store_eq_arg(glb* g, gptr cl, gint term, int termtype, int litnr, int leftflag) {    
  gint fun, hash;
  int tmp, path;
  vec hashvec;

  gptr tptr;

#ifdef XDEBUG 
  printf("\nwr_cl_store_eq_arg starts\n");
  printf("term \n");
  wr_print_term(g,term);
  printf("clause \n");
  wr_print_clause(g,cl);
  printf("\nwtermtype %d litnr %d leftflag %d\n",termtype,litnr,leftflag);
#endif  
  if (termtype==WG_RECORDTYPE) {
    tptr=rotp(g,term);
    fun=tptr[RECORD_HEADER_GINTS+(g->unify_funpos)];
  } else {
    fun=term;
  }  
  hash=wr_term_basehash(g,fun); 
  hashvec=rotp(g,g->hash_eq_terms);  
  path=wr_encode_para_termpath(g,litnr,leftflag);
  tmp=wr_clterm_add_hashlist_withpath(g,hashvec,hash,term,cl,path);
#ifdef XDEBUG 
  printf("\nadding to eq_hash ended with %d\n",tmp);
#endif       
  if (tmp) {
    wr_sys_exiterr2int(g,"adding term to hashlist in  wr_cl_store_eq_arg, code ",tmp);
    return 1;        
  }  
#ifdef DEBUGHASH      
  printf("\nhash_eq_terms after adding:");      
  wr_clterm_hashlist_print_para(g,hashvec);
#endif 
  return 1; 

}


int wr_cl_store_term_rewriter(glb* g, gptr cl, gint term, int termtype, int litnr, int leftflag) {    
  gint fun, hash;
  int tmp, path;
  vec hashvec;

  gptr tptr;

#ifdef XDEBUG 
  printf("\n*** wr_cl_store_term_rewriter starts \n");
  printf("rewriter ");
  wr_print_term(g,term);
  printf(" --> ");
  //wr_print_term(g,toterm);
  printf("\n clause \n");
  wr_print_clause(g,cl);
  printf("\nwtermtype %d litnr %d leftflag %d\n",termtype,litnr,leftflag);
#endif  
  if (termtype==WG_RECORDTYPE) {
    tptr=rotp(g,term);
    fun=tptr[RECORD_HEADER_GINTS+(g->unify_funpos)];
  } else {
    fun=term;
  }  
  hash=wr_term_basehash(g,fun); 
  hashvec=rotp(g,g->hash_rewrite_terms);  
  path=wr_encode_para_termpath(g,litnr,leftflag);
  tmp=wr_clterm_add_hashlist_withpath(g,hashvec,hash,term,cl,path);
#ifdef XDEBUG 
  printf("\nadding to rewrite_hash ended with %d\n",tmp);
#endif       
  if (tmp) {
    wr_sys_exiterr2int(g,"adding term to hashlist in  wr_cl_store_term_rewriter, code ",tmp);
    return 1;        
  }  
  printf("\nwr_cl_store_term_rewriter adds term \n");
  wr_print_term(g,term);
  printf("\nresulting with\n");
  wr_clterm_hashlist_print_para(g,hashvec);
#ifdef DEBUGHASH      
  printf("\nhash_rewrite_terms after adding:");      
  wr_clterm_hashlist_print_para(g,hashvec);
#endif 
  return 1; 

}


int wr_cl_store_para_subterms(glb* g, gptr cl, gint term, int depth, int litnr, int* termpath) {    
  void* db=g->db;
  gptr ptr;
  gint fun, yi, hash;
  int len, istart, ilimit, i, tmp, path, origtermpath;
  vec hashvec;
  
#ifdef PARADEBUG
  printf("\ncl_store_para_subterms called at depth %d on term and cl: ",depth); 
  wr_print_term(g,term);
  printf("\n");
  wr_print_clause(g,cl);
  printf("\n");
#endif  
  if (litnr>PARA_TERMPATH_MAXLITNR || (*termpath)>PARA_TERMPATH_MAXPOS) {
    (g->stat_internlimit_discarded_para)++;
    return 0;
  }
  origtermpath=(*termpath);
  if (!isdatarec(term)) {
    // term is a primitive
    if (isvar(term)) return 1;
    // term is a constant
    fun=term;    
  } else {
    // complex term   
    ptr=decode_record(db,term);  
    fun=get_field(ptr,(g->unify_funpos)); 
    // recurse into arguments
    len=get_record_len(ptr);    
    if (g->unify_maxuseterms) {
      if (((g->unify_maxuseterms)+(g->unify_firstuseterm))<len) 
        len=(g->unify_firstuseterm)+(g->unify_maxuseterms);
    }    
    istart=RECORD_HEADER_GINTS+(g->unify_firstuseterm);
    ilimit=RECORD_HEADER_GINTS+len;
    for(i=istart; i<ilimit; i++) {   
      yi=*(ptr+i);

      //printf("\ndepth %d i %d istart %d ilimit %d \n",depth,i,istart,ilimit);
      //wr_print_term(g,yi);  
      //printf("\n");
#ifdef PARA_INTO_FUNSYMB
      (*termpath)++;
      wr_cl_store_para_subterms(g,cl,yi,depth+1,litnr,termpath);
#else
      // do not recurse into atomic funsymb
      if (!(i==istart && !isdatarec(yi))) {
        (*termpath)++;
        wr_cl_store_para_subterms(g,cl,yi,depth+1,litnr,termpath);
      }
#endif
      
    }
  }  
#ifdef PARA_INTO_ATOM  
#else
  // do not put into hash on the atom level  
  if (depth<1) return 1;  
#endif  

  // put term into the hash table
  hash=wr_term_basehash(g,fun);
#ifdef XDEBUG 
  printf("\nbefore adding to g->hash_para_terms fun hash is: %d litnr %d origtermpath %d\n",
    (int)hash,litnr,origtermpath);
#endif      
  hashvec=rotp(g,g->hash_para_terms);  
  path=wr_encode_para_termpath(g,litnr,origtermpath);
  tmp=wr_clterm_add_hashlist_withpath(g,hashvec,hash,term,cl,path);
#ifdef XDEBUG 
  printf("\nadding to hash ended with %d\n",tmp);
#endif       
  if (tmp) {
    wr_sys_exiterr2int(g,"adding term to hashlist in cl_store_para_terms, code ",tmp);
    return 1;        
  }  
#ifdef DEBUGHASH      
  printf("\nhash_para_terms after adding:");      
  wr_clterm_hashlist_print(g,hashvec);
#endif 
  return 1;     
}

int wr_encode_para_termpath(glb* g, int litnr, int pos) {
  return (litnr<<PARA_TERMPATH_LITNRSHIFT) | pos;
}

int wr_decode_para_termpath_litnr(glb* g, int n) {
  return ((n & PARA_TERMPATH_LITNRMASK) >> PARA_TERMPATH_LITNRSHIFT);
}

int wr_decode_para_termpath_pos(glb* g, int n) {
  return (n & PARA_TERMPATH_POSMASK);
}

int wr_decode_para_termpath_leftflag(glb* g, int n) {
  return (n & PARA_TERMPATH_LEFTMASK);
}


/*
int wr_cl_store_res_terms_new (glb* g, gptr cl) {    
  void* db=g->db;
  int i;
  int len;      
  int ruleflag; // 0 if not rule
  int poscount=0; // used only for pos/neg pref
  int negcount=0; // used only for pos/neg pref
  int posok=1;  // default allow
  int negok=1;  // default allow
  gint meta;
  gint atom;  
  int negflag; // 1 if negative
  //int termflag; // 1 if complex atom  
  //gint hash;
  int addflag=0;
  int negadded=0;
  int posadded=0;
  vec hashvec;
  //void* hashdata;
  int tmp;
  //int hashposbits;
  
#ifdef DEBUG
  printf("cl_store_res_terms called on cl: "); 
  wr_print_clause(g,cl);
#endif  

  // get clause data for input clause
       
  ruleflag=wg_rec_is_rule_clause(db,cl);
  if (ruleflag) len = wg_count_clause_atoms(db, cl);
  else len=1;
  
  // for negpref check out if negative literals present
    
  if (1) {
    // prohibit pos or neg    
    if ((g->negpref_strat) || (g->pospref_strat)) {
      if (!ruleflag) {
        poscount=1;
        negcount=0;
      } else {         
        poscount=0;
        negcount=0;        
        for(i=0; i<len; i++) {          
          meta=wg_get_rule_clause_atom_meta(db,cl,i);
          if (wg_atom_meta_is_neg(db,meta)) negcount++;
          else poscount++; 
        }  
        // set neg/pospref!    
        if (g->negpref_strat) {
          if (poscount>0 && negcount>0) posok=0;
        }      
        if (g->pospref_strat) {
          if (poscount>0 && negcount>0) negok=0;
        }
      }
    }
  }

#ifdef DEBUG
  printf("ruleflag %d len %d poscount %d negcount %d posok %d negok %d\n",
          ruleflag,len,poscount,negcount,posok,negok);
#endif  
  // loop over literals
#if 0
#ifdef USE_CHILD_DB
  if (ruleflag) parent = wg_get_rec_base_offset(db,cl);
#endif
#endif
  for(i=0; i<len; i++) {  
    negflag=0;
    //termflag=0;
    addflag=0;
    if (!ruleflag) {
      atom=encode_record(db,cl);      
      addflag=1;
    } else {       
      meta=wg_get_rule_clause_atom_meta(db,cl,i);
      if (wg_atom_meta_is_neg(db,meta)) negflag=1;
      if (!((g->negpref_strat) || (g->pospref_strat)) ||
          (negflag && (g->hyperres_strat)) ||
          (negok && negflag && !negadded) || 
          (posok && !negflag)) {            
        if (negflag) negadded++; 
        else posadded++;          
        atom=wg_get_rule_clause_atom(db,cl,i);
        if (wg_get_encoded_type(db,atom)==WG_RECORDTYPE) {
          //termflag=1;
#if 0
#ifdef USE_CHILD_DB
          if(parent) atom=wg_encode_parent_data(parent,atom);
#endif             
#endif
        }               
        addflag=1;
      }      
    }
    if (addflag) {      
#ifdef XDEBUG 
      printf("before adding to hash negflag: %d\n",negflag);
#endif           
      if (negflag) hashvec=rotp(g,g->hash_neg_atoms);
      else hashvec=rotp(g,g->hash_pos_atoms);
#ifdef XDEBUG 
      printf("adding to hash g->hash_neg_atoms or g->hash_pos_atoms\n");
#endif       
      tmp=wr_term_hashstore(g,hashvec,atom,cl);    
      if (tmp) {
        wr_sys_exiterr2int(g,"adding term to hashlist in cl_store_res_terms, code ",tmp);
        return 1;        
      }  
#ifdef DEBUGHASH      
      printf("\nhash table after adding:");      
      wr_clterm_hashlist_print(g,hashvec);     
      printf("\npos hash table after adding:");      
      wr_clterm_hashdata_print(g,rotp(g,g->hash_pos_atoms));
      printf("\nneg hash table after adding:");      
      wr_clterm_hashdata_print(g,rotp(g,g->hash_neg_atoms));  
#endif      
    }  
  }     
#ifdef DEBUG
  printf("cl_store_res_terms finished\n"); 
#endif      
  return 0;
}
*/

/* =============================================

  queues

  we have g->clpick_queues being offset of pickqueue_vec 

  pickqueue_vec for a given clause selection contains 
  several priority queues along with standard queues

  queues for query strat:

    goal_assumptions queue: each parent either has a goal or an assumption as ancestor
    goal gueue: not in queue above, one parent has a goal as an ancestor
    assumptions gueue: non in queue above, one parent has an assumption as an ancestor
    axioms queue: any clauses not above

  queues for small eq problems:
    
    resolve queue: clause not derived by paramodulation
    para queue: clause derived by paramodulation
  

  pickqueue_vec contains blocks for each queue, with this block structure:

  0: vec len

  1...9 goal_assumptions block:

  1: queue (nonpriority queue)         CLPICK_QUEUE_POS
  2: priorqueue (priority queue)       CLPICK_PRIORQUEUE_POS
  3: queue_given (index of next clause to be taken from nonpriority clqueue)                 CLPICK_QUEUE_GIVEN_POS
  4: priorqueue_ratio (how many times prioritiqueue pick before one nonpriority queue pick)  CLPICK_PRIORQUEUE_RATIO
  5: priorqueue_ratio_counter (how many times in a row have we picked prioritiqueue)         CLPICK_PRIORQUEUE_RATIO_COUNTER
  6: this_queue_ratio (how many times this whole queue should be picked before others)       CLPICK_THIS_QUEUE_RATIO
  7: this_queue_ratio_counter (how many times this whole queue should be picked before others) CLPICK_THIS_QUEUE_RATIO_COUNTER
  8: reserved
  9: reserved

  10...19 goal block

  20...29 assumptions block
   
  30...39 axioms block


  
  the overall picking algorithm:
  
  - loop over all queues

    the queue clqueue field in queue with nr g->clpick_given is not null and
    this_queue_ratio_counter<this_queue_ratio:
        
        1) use this queue as decribed in the next loop step
        
    if this_queue_ratio_counter<this_queue_ratio:     

        take a next queue:

        increase g->clpick_given 
        continue if a queue clqueue field is NULL (no queue used or just empty)
        if a queue this_queue_ratio_counter>=this_queue_ratio then zero this_queue_ratio_counter 
           and continue to next queue
        here we can use the queue:
        if a priorqueue_ratio_counter>=priorqueue_ratio 
        then zero this priorqueue_ratio_counter and pick from nonpriorqueue
        else pick from priorqueue

================================================ */

/*
  
  create all selection queues


*/


gint wr_create_clpick_queues(glb* g, int count) {
  int i;
  gint limit;
  gptr queues;
  gint clpick_queues;
  gptr tmpvec;

#ifdef QADDDEBUG
  printf("\n*** wr_create_clpick_queues called with count %d ****\n",count);
#endif

  queues=wr_vec_new_zero(g,NROF_CLPICK_QUEUES*CLPICK_QUEUE_BLOCKGINTS); // use as vec
  clpick_queues=rpto(g,queues); // return offset
  limit=queues[0]; 
  // do big block-size steps over array
  for(i=1; i<limit && i<(NROF_CLPICK_QUEUES*CLPICK_QUEUE_BLOCKGINTS); i=i+CLPICK_QUEUE_BLOCKGINTS) {
#ifdef QADDDEBUG    
    printf("\n-- queue nr %d ---\n",i);
#endif    
    queues[i+CLPICK_QUEUE_GIVEN_POS]=2; // 0 contains len and 1 contains next free,  first real is 2
    queues[i+CLPICK_PRIORQUEUE_RATIO]=(g->pick_given_queue_ratio); // 4 is normal
    queues[i+CLPICK_PRIORQUEUE_RATIO_COUNTER]=0;
    if (i==1) {
      // goalassumption
      if (g->cl_pick_queue_strategy) queues[i+CLPICK_THIS_QUEUE_RATIO]=6; // 6
      else queues[i+CLPICK_THIS_QUEUE_RATIO]=1;
    } else if (i==11) {
      // goal
      if (g->cl_pick_queue_strategy) queues[i+CLPICK_THIS_QUEUE_RATIO]=4; // 4
      else queues[i+CLPICK_THIS_QUEUE_RATIO]=1;  // 1
    } else if (i==21) {
      // assumption      
      queues[i+CLPICK_THIS_QUEUE_RATIO]=1;  
    } else {
      queues[i+CLPICK_THIS_QUEUE_RATIO]=1;
    }  
    queues[i+CLPICK_THIS_QUEUE_RATIO_COUNTER]=0;   
    tmpvec=wr_cvec_new(g,NROF_DYNALLOCINITIAL_ELS);
    if (!tmpvec) return 0;
    queues[i+CLPICK_QUEUE_POS]=rpto(g,tmpvec);
    tmpvec=wr_make_priorqueue(g,MAX_CLPRIOR);
    if (!tmpvec) return 0;
    queues[i+CLPICK_PRIORQUEUE_POS]=rpto(g,tmpvec);
  } 
  return clpick_queues;
}

/*
 
  push to hyper-queues

*/

int wr_push_cl_hyper_queue(glb* g, cvec queue, gptr cl, int weight) {
  //int qlen;
  gint clo;
  cvec newqueue;
  
  clo=rpto(g,cl);
  newqueue=wr_cvec_push(g,queue,clo);  
  if (newqueue==NULL) return 0;
  if (queue!=newqueue) {
    (g->hyper_queue)=newqueue;
  }
  return 1;
}


/*

  push to queues

*/


void wr_push_cl_clpick_queues(glb* g, gint queues_offset, gptr cl, int weight) {
  void *db=g->db;
  int qstart; //,i;
  gint simplequeue_offset, priorqueue_offset;
  gptr queues, simplequeue, priorqueue;
  gint history;
  gptr historyptr=NULL;
  gint decprior, priority=0;   


#ifdef QADDDEBUG
  printf("\nwr_push_cl_clpick_queues called with queryfocus_strat %d (g->cl_pick_queue_strategy) %d weight %d and cl: ",
         (g->queryfocus_strat),(g->cl_pick_queue_strategy),weight);
  wr_print_clause(g,cl);
#endif

  // start pushing

  queues=rotp(g,queues_offset);
  if (g->cl_pick_queue_strategy) { // any strategy value not zero
    // query strat: push to a selected queue depending on priority
    history=wr_get_history(g,cl); 
    historyptr=otp(db,history);
    priority=wr_get_history_record_field(db,historyptr,HISTORY_PRIORITY_POS);
    decprior=wg_decode_int(db,priority);


    // modify decpriors according to strat

#ifdef QADDDEBUG
     printf("\ndecprior originally %d\n",decprior);
#endif

    if ((g->cl_pick_queue_strategy)==2) {
      // make non-included axioms assumptions and positive conjecture part assumptions
      if (decprior==WR_HISTORY_GOAL_ROLENR && wr_is_positive_unit_cl(g,cl)) 
        decprior=WR_HISTORY_ASSUMPTION_ROLENR;        
      else if (decprior==WR_HISTORY_AXIOM_ROLENR) // && (g->parse_is_included_file))
        decprior=WR_HISTORY_ASSUMPTION_ROLENR;       
    } else if ((g->cl_pick_queue_strategy)==3) {
      // only fully negative clauses of goal made goal and no assumptions (ie made axioms)
      if (decprior==WR_HISTORY_GOAL_ROLENR && !wr_is_negative_cl(g,cl))
        decprior=WR_HISTORY_AXIOM_ROLENR;  
      else if (decprior==WR_HISTORY_ASSUMPTION_ROLENR)  
        decprior=WR_HISTORY_AXIOM_ROLENR; // !! had no effect before
    } else if ((g->cl_pick_queue_strategy)==4) {
      // everything in one, axiom queue, regardless of polarity or role
      decprior=WR_HISTORY_AXIOM_ROLENR;   
    }   

#ifdef QADDDEBUG
     printf("\ndecprior changed to %d\n",decprior);
#endif

    // select queue
  
    if (decprior==WR_HISTORY_GOAL_ROLENR) qstart=GOAL_QUEUE_POS;
    else if (decprior==WR_HISTORY_ASSUMPTION_ROLENR) qstart=ASSUMPTIONS_QUEUE_POS;
    else if (decprior==WR_HISTORY_FROMGOAL_ROLENR) qstart=GOAL_QUEUE_POS;
    else if (decprior==WR_HISTORY_FROMGOALASSUMPTION_ROLENR) qstart=GOAL_ASSUMPTIONS_QUEUE_POS;
    else if (decprior==WR_HISTORY_FROMASSUMPTION_ROLENR) qstart=ASSUMPTIONS_QUEUE_POS;
    else if (decprior==WR_HISTORY_AXIOM_ROLENR) qstart=AXIOMS_QUEUE_POS;
    else if (decprior==WR_HISTORY_EXTAXIOM_ROLENR) qstart=AXIOMS_QUEUE_POS;
    else if (decprior==WR_HISTORY_FROMAXIOM_ROLENR) qstart=AXIOMS_QUEUE_POS;
    else qstart=AXIOMS_QUEUE_POS;   

#ifdef QADDDEBUG
     printf("\nqstart %d\n",qstart);
#endif

    // push to simple queue  
    simplequeue_offset=queues[qstart+CLPICK_QUEUE_POS];
    simplequeue=rotp(g,simplequeue_offset);
    queues[qstart+CLPICK_QUEUE_POS]=rpto(g,wr_cvec_push(g,simplequeue,(gint)cl));
    // push to priority queue
    priorqueue_offset=queues[qstart+CLPICK_PRIORQUEUE_POS];
    priorqueue=rotp(g,priorqueue_offset);
    if (weight>=0) {
      wr_push_priorqueue(g,priorqueue,cl,weight);
    }     
  } else {
    // not a query strat: push to a single queue
    qstart=AXIOMS_QUEUE_POS;
    // push to simple queue  
    simplequeue_offset=queues[qstart+CLPICK_QUEUE_POS];
    simplequeue=rotp(g,simplequeue_offset);
    queues[qstart+CLPICK_QUEUE_POS]=rpto(g,wr_cvec_push(g,simplequeue,(gint)cl));
    // push to priority queue
    priorqueue_offset=queues[qstart+CLPICK_PRIORQUEUE_POS];
    priorqueue=rotp(g,priorqueue_offset);
    if (weight>=0) {
      wr_push_priorqueue(g,priorqueue,cl,weight);
    } 
  }
  //wr_print_clpick_queues(g,queues);
}


int wr_is_positive_unit_cl(glb* g, gptr cl) {
  void *db=g->db;
  int cllen,i;
  gint meta;

  UNUSED(db);
  if (!wg_rec_is_rule_clause(db,cl)) return 1;
  cllen=wg_count_clause_atoms(db,cl);     
  if (cllen>1) return 0;
  for(i=0;i<cllen;i++) {
    meta=wg_get_rule_clause_atom_meta(db,cl,i);
    if (wg_atom_meta_is_neg(db,meta)) return 0;
  }
  return 1;  
}

int wr_is_negative_cl(glb* g, gptr cl) {
  void *db=g->db;
  int cllen,i;
  gint meta;
  gint xatom;

  UNUSED(db);
  if (!wg_rec_is_rule_clause(db,cl)) return 0;
  cllen=wg_count_clause_atoms(db,cl);     
  if (cllen>1) return 0;
  for(i=0;i<cllen;i++) {
    meta=wg_get_rule_clause_atom_meta(db,cl,i);
    xatom=wg_get_rule_clause_atom(db,cl,i); 
    if (!wr_answer_lit(g,xatom) && !wg_atom_meta_is_neg(db,meta)) return 0;
  }
  return 1;  
}

/*
  
  Pick given cl from hyper queue and detect/reset queue if nothing left

  queue[0] - queue array length
  queue[1] - next free pos in the queue (initially for empty queue 3)
  queue[2] - next pos to pick for given (initially for empty queue 3)

*/

gptr wr_pick_from_hyper_queue(glb* g, gptr queue, gptr given_cl_metablock)  {
  gint pos;
  gint res;

  if (!queue) return NULL;
  pos=queue[2];
  if (queue[1]<=3 || pos>=queue[1]) return NULL;
  // now there is something to pick
  res=queue[pos];
  pos++;
  if (pos>=queue[1]) {
    // everything is picked: set queue to initial situation
    queue[1]=3;
    queue[2]=3;
  } else {
    queue[2]=pos;
  }
  return rotp(g,res);
}
  


gptr wr_pick_from_clpick_queues(glb* g, gptr queues, gptr given_cl_metablock) { 

  int queuenr;
  int first_queuenr; 
  gptr cl;
  gint limit;
  int blockstart; 
  int loopflag=0;

#ifdef QPICKDEBUG
  //int qn=0;
  printf("\n!!!! wr_pick_from_clpick_queues starts with queuenr %d \n",(g->next_pick_given_queue_block_nr));
  //while(1) {
  //  blockstart=1+(queuenr*CLPICK_QUEUE_BLOCKGINTS);    
  //  qn++;
  //}  
#endif 

  //wr_print_clpick_queues(g,rotp(g,g->clpick_queues));
  //wr_print_clpick_queue_sizes(g,rotp(g,g->clpick_queues));

  // first try to take from the designated next_pick_given_queue_block_nr
  queuenr=(g->next_pick_given_queue_block_nr);
  if (queuenr>=NROF_CLPICK_QUEUES) {
    queuenr=0;
    (g->next_pick_given_queue_block_nr)=0;
  }  
  first_queuenr=queuenr;
  limit=queues[0]; // nr of elems in the queue blocks vector
  while(1) {    
#ifdef QPICKDEBUG
    printf("\n  queue loop iteration starts loopflag %d queuenr %d first_queuenr %d\n",loopflag,queuenr,first_queuenr);
#endif             
    if (loopflag && queuenr==first_queuenr) {
      // full loop done, failed to find any clauses to pick
#ifdef QPICKDEBUG
      printf("\n  full queue loop done, no clauses found \n");
#endif          
      return NULL;
    }

    cl=wr_pick_from_clpick_queues_aux(g,queues,queuenr,given_cl_metablock);

    blockstart=1+(queuenr*CLPICK_QUEUE_BLOCKGINTS);    
    if (cl!=NULL) {
      if (queues[blockstart+CLPICK_THIS_QUEUE_RATIO_COUNTER] >= 
          queues[blockstart+CLPICK_THIS_QUEUE_RATIO]) {
        // over ratio!
        //printf("\nthis whole queue block is over ratio setting to 0\n");
        queues[blockstart+CLPICK_THIS_QUEUE_RATIO_COUNTER]=0;
        (g->next_pick_given_queue_block_nr)++;
      } else {
        // not over ratio, continue with the same block next time
        (queues[blockstart+CLPICK_THIS_QUEUE_RATIO_COUNTER])++;
      }
      //(g->next_pick_given_queue_block_nr)++;
      return cl;
    }  
#ifdef QPICKDEBUG
    printf("\n  no cl found for queuenr %d \n",queuenr);
#endif     
    
    // here we did not get a clause, need to try next until full cycle
    queuenr++;
    if (queuenr>=NROF_CLPICK_QUEUES || blockstart>=limit) {
      // pushed over the edge, loop back
#ifdef QPICKDEBUG
      printf("\n  queuenr to loop back \n");
#endif           
      loopflag=1;
      queuenr=0;
    }
    // set the designated next_pick_given_queue_block_nr to a new block
    (g->next_pick_given_queue_block_nr)=queuenr;
  }
}    


gptr wr_pick_from_clpick_queues_aux(glb* g, gptr queues, int queuenr, gptr given_cl_metablock) { 
  
  int i; //,j;
  //gint limit;
  gint simplequeue_offset;
  gptr simplequeue;
  int simplequeue_given_pos;
  //int max_used_prior;
  gint* queue;

  gptr cl;
  int next;

#ifdef QPICKDEBUG
  printf("\n\n --- pick cl with queuenr %d limit %d NROF_CLPICK_QUEUES %d ---\n",
         queuenr,(int)(queues[0]),NROF_CLPICK_QUEUES); 
  //printf("\n queues are: \n");       
  //wr_print_clpick_queues(g,rotp(g,g->clpick_queues));       
  //printf("\n starting to pick: \n");  
#endif  

  
  i=1+(queuenr*CLPICK_QUEUE_BLOCKGINTS); // i is block start in queues vector

#ifdef QPICKDEBUG    
    
  printf("\n   --  queue nr %d at pos %d --\n",queuenr,i);
    
  printf("\n   CLPICK_QUEUE_GIVEN_POS: %d",(int)(queues[i+CLPICK_QUEUE_GIVEN_POS]));
  printf("\n   CLPICK_PRIORQUEUE_RATIO: %d",(int)(queues[i+CLPICK_PRIORQUEUE_RATIO]));
  printf("\n   CLPICK_PRIORQUEUE_RATIO_COUNTER: %d",(int)(queues[i+CLPICK_PRIORQUEUE_RATIO_COUNTER]));
  printf("\n   CLPICK_THIS_QUEUE_RATIO: %d",(int)(queues[i+CLPICK_THIS_QUEUE_RATIO]));
  printf("\n   CLPICK_THIS_QUEUE_RATIO_COUNTER: %d",(int)(queues[i+CLPICK_THIS_QUEUE_RATIO_COUNTER]));
  
#endif

  // check if this queue is over ratio
  /*
  if (queues[i+CLPICK_THIS_QUEUE_RATIO_COUNTER] >= queues[i+CLPICK_THIS_QUEUE_RATIO]) {
    // over ratio!
    printf("\nthis whole queue block is over ratio setting to 0\n");
    queues[i+CLPICK_THIS_QUEUE_RATIO_COUNTER]=0;
    return NULL;
  }
  */
  // from here not over ratio

  if (queues[i+CLPICK_PRIORQUEUE_RATIO_COUNTER] < queues[i+CLPICK_PRIORQUEUE_RATIO]) {

    // selecting from priority queue

    if (queues[i+CLPICK_PRIORQUEUE_POS]!=0) {
      queue=rotp(g,queues[i+CLPICK_PRIORQUEUE_POS]);
      cl=wr_pick_priorqueue(g,queue); // it gives only previously unpicked clauses

#ifdef QPICKDEBUG   
      if (cl!=NULL) {
        printf("\n\n  picking from queue nr %d priority queue the clause: ",queuenr);
        wr_print_clause(g,cl);
        printf("\n");
        //wr_print_priorqueue(g,queue); 
      }  
#endif
      if (cl!=NULL) {
        (queues[i+CLPICK_PRIORQUEUE_RATIO_COUNTER])++;
        (queues[i+CLPICK_THIS_QUEUE_RATIO_COUNTER])++;
#ifdef QPICKEDDEBUG
        printf("\npicked cl from queue nr %d priority queue",queuenr);
#endif        
#ifdef QPICKDEBUG                   
        printf("\n  priority queue with max priority %d, max used priority %d \n",
                (int)(queue[PRIORQUEUE_ARR_LEN_POS]-2),
                (int)(queue[PRIORQUEUE_MAX_USED_PRIOR_POS]));  
#endif
          
        wr_cl_mark_given(g,cl);
        wr_calc_clause_meta(g,cl,given_cl_metablock);
        return cl;   
      }     
    }    
  } else {
    queues[i+CLPICK_PRIORQUEUE_RATIO_COUNTER]=0;
#ifdef QPICKDEBUG
    printf("\n  not from priority queue; setting ratio_counter to 0\n");
#endif    
  }  

  // selecting from simple queue

  if (queues[i+CLPICK_QUEUE_POS]!=0) {
    simplequeue_offset=queues[i+CLPICK_QUEUE_POS]; // actual flat queue offs with len at 0 and nextfree at 1
    simplequeue=rotp(g,simplequeue_offset); 

    // loop to avoid already picked clauses

    while(1) {
      simplequeue_given_pos=queues[i+CLPICK_QUEUE_GIVEN_POS]; // take the next cl from this pos at queue
      next=CVEC_NEXT(simplequeue);
#ifdef QPICKDEBUG       
      printf("\n  simplequeue max len %d count %d, elems:\n",(int)(simplequeue[0]),(int)(simplequeue[1]));
      /*
      printf("\n  simplequeue max len %d count %d, elems:\n",(int)(simplequeue[0]),(int)(simplequeue[1]));
      for(j=2;j<simplequeue[0] && j<simplequeue[1];j++) {
        printf("\n %d: ",j);
        wr_print_clause(g,simplequeue[j]);        
      }          
      printf("\n  next %d simplequeue_given_pos %d\n",next,simplequeue_given_pos);
      */
#endif
      if (next>2 && next>simplequeue_given_pos) {
        // there is a clause to take (next is initially 2 when no clauses have been added)
        cl=(gptr)(simplequeue[simplequeue_given_pos]);           
        ++(queues[i+CLPICK_QUEUE_GIVEN_POS]); // next time take the next clause
        (queues[i+CLPICK_THIS_QUEUE_RATIO_COUNTER])++;
        if (cl!=NULL && !wr_cl_ismarked_given(g,cl)) {
#ifdef QPICKEDDEBUG
          printf("\npicked cl from queue nr %d simple queue",queuenr);
#endif           
  #ifdef QPICKDEBUG       
          printf("\n\n  picking from queue nr %d simple queue the clause: ",queuenr);
          wr_print_clause(g,cl); 
          printf("\n");
          /*
          printf("\n  new next %d queues[i+CLPICK_QUEUE_GIVEN_POS] \n",next,queues[i+CLPICK_QUEUE_GIVEN_POS]);
          */
  #endif            
          wr_cl_mark_given(g,cl);
          wr_calc_clause_meta(g,cl,given_cl_metablock);
          return cl;
        }  
      } else {
        break; // no pickable clauses
      }      
    }  
  } 
  return NULL;  
}


// WR_HISTORY_USED_BIT_POS in history priority pos is 0 indicates "not given yet", 1 "given"

int wr_cl_ismarked_given(glb* g, gptr cl) {
  gint hist, prior, decprior;
  gptr histptr;

  return 0;
  /*
  printf("\n checking if marked given:");
  wr_print_clause(g,cl);
  printf("\n");
  */
  hist=wr_get_history(g,cl);
  if (hist) {
    histptr=rotp(g,hist);
    prior=wr_get_history_record_field(g,histptr,HISTORY_PRIORITY_POS);
    if (!prior) return 0;
    decprior=wg_decode_int(g->db,prior);
    //printf("\n decprior in ismarked? %d\n",decprior);
    if (decprior & (gint)(1<<WR_HISTORY_USED_BIT_POS)) {
      //printf("\n marked given\n");
      return 1;
    } else {
      //printf("\n not marked given\n");
      return 0;
    }
  } else {
    //printf("\n no hist found \n");
    return 0;
  }  
}

// set WR_HISTORY_USED_BIT_POS to 1 indicating "given already"

void wr_cl_mark_given(glb* g, gptr cl) {
  void *db=g->db;
  gint hist, prior, decprior;
  gptr histptr;

  //printf("\n marking given:");
  //wr_print_clause(g,cl);

  return;

  hist=wr_get_history(g,cl);
  if (hist) {
    histptr=rotp(g,hist);
    prior=wr_get_history_record_field(g,histptr,HISTORY_PRIORITY_POS);    
    decprior=wg_decode_int(db,prior);
    decprior=decprior | (gint)(1<<WR_HISTORY_USED_BIT_POS); 
    //printf("\n decprior in mark_given %d\n",decprior);
    prior=wg_encode_int(db,decprior);
    wr_set_history_record_field(g,histptr,HISTORY_PRIORITY_POS,prior);
  } else {
    printf("\n WARNING: history empty, cannot mark given\n");
  }
  /*
  printf("\n");
  printf("\n is marked given?: %d\n",wr_cl_ismarked_given(g,cl));
  */
}


/*

free all cl selection queues

*/


void wr_free_clpick_queues(glb* g, gint* queues) {
  int i;
  gint limit;

  if (queues==NULL) return;
  limit=queues[0];

  /*   
  for(i=0; i<limit; i++) {
    printf("\ni %d queues[i] %d ",i,queues[i]);
  } 
  return; 
  */
  // do big block-size steps over array
  for(i=1; i<limit && i<(NROF_CLPICK_QUEUES*CLPICK_QUEUE_BLOCKGINTS); i=i+CLPICK_QUEUE_BLOCKGINTS) {
    //printf("\ni : %d",i);   
    //printf("\nqueues[i+CLPICK_QUEUE_POS] :%d ",queues[i+CLPICK_QUEUE_POS]);
    //continue;
    if (queues[i+CLPICK_QUEUE_POS]!=0) {
      //printf("\n!1 i+CLPICK_QUEUE_POS %d queues[i+CLPICK_QUEUE_POS] %d",i+CLPICK_QUEUE_POS,queues[i+CLPICK_QUEUE_POS]);
      wr_vec_free(g,rotp(g,queues[i+CLPICK_QUEUE_POS])); // free plain vector
    }
    if (queues[i+CLPICK_PRIORQUEUE_POS]!=0) {
      //printf("\n!2 i+CLPICK_PRIORQUEUE_POS %d queues[i+CLPICK_PRIORQUEUE_POS] %d",i+CLPICK_PRIORQUEUE_POS,queues[i+CLPICK_PRIORQUEUE_POS]);
      wr_free_priorqueue(g,rotp(g,queues[i+CLPICK_PRIORQUEUE_POS])); // free the complex priorqueue structure
    }  
  }
}  


/*
  print all queues
*/


void wr_print_clpick_queues(glb* g, gint* queues) {
  int i,j;
  gint limit;
  gint simplequeue_offset;
  gptr simplequeue;

  printf("\n*** g->clpick_queues content ****\n");
  if (!queues) {
    printf("\nqueues is NULL\n");
    return;    
  }  
  
  //limit=queues[0];
  //for(i=0; i<limit; i++) {
  //  printf("\ni %d queues[i] %d ",i,queues[i]);
  //} 
  //return; 
  
  printf("\ng->clpick_given: %d",(int)(g->clpick_given));

  limit=queues[0];
  printf("\ng->clpick_given vec len (el 0): %d",(int)limit);
  printf("\nNROF_CLPICK_QUEUES: %d",NROF_CLPICK_QUEUES);
  // do big block-size steps over array
  for(i=1; i<limit && i<(NROF_CLPICK_QUEUES*CLPICK_QUEUE_BLOCKGINTS); i=i+CLPICK_QUEUE_BLOCKGINTS) {
    printf("\n-- queue nr %d ---\n",i);
    printf("\nCLPICK_QUEUE_GIVEN_POS: %d",(int)(queues[i+CLPICK_QUEUE_GIVEN_POS]));
    printf("\nCLPICK_PRIORQUEUE_RATIO: %d",(int)(queues[i+CLPICK_PRIORQUEUE_RATIO]));
    printf("\nCLPICK_PRIORQUEUE_RATIO_COUNTER: %d",(int)(queues[i+CLPICK_PRIORQUEUE_RATIO_COUNTER]));
    printf("\nCLPICK_THIS_QUEUE_RATIO: %d",(int)(queues[i+CLPICK_THIS_QUEUE_RATIO]));
    printf("\nCLPICK_THIS_QUEUE_RATIO_COUNTER: %d",(int)(queues[i+CLPICK_THIS_QUEUE_RATIO_COUNTER]));

    if (queues[i+CLPICK_QUEUE_POS]!=0) {

      simplequeue_offset=queues[i+CLPICK_QUEUE_POS];
      simplequeue=rotp(g,simplequeue_offset);
      //printf("\nlen of queue: %d",(int)(rotp(g,queues[i+CLPICK_QUEUE_POS])[0])); 
      printf("\nsimplequeue max len %d count %d, elems:\n",(int)(simplequeue[0]),(int)(simplequeue[1]));
      for(j=2;j<simplequeue[0] && j<simplequeue[1];j++) {
        printf("\n %d: ",j);
        wr_print_clause(g,(gptr)(simplequeue[j]));        
      } 
    } else {
      printf("\nsimplequeue is 0");  
    }
    if (queues[i+CLPICK_PRIORQUEUE_POS]!=0) {
      wr_print_priorqueue(g,rotp(g,queues[i+CLPICK_PRIORQUEUE_POS]));
    } else {
      printf("\npriorqueue is 0");    
    }  
  }    
   printf("\n* g->clpick_queues content ends *\n");
}



void wr_print_clpick_queue_sizes(glb* g, gint* queues) {
  int i;
  gint limit;
  gint simplequeue_offset;
  gptr simplequeue;

  printf("\n*** g->clpick_queues sizes ****\n");
  if (!queues) {
    printf("\nqueues is NULL\n");
    return;    
  }  
  
  //limit=queues[0];
  //for(i=0; i<limit; i++) {
  //  printf("\ni %d queues[i] %d ",i,queues[i]);
  //} 
  //return; 
  
  printf("\ng->clpick_given: %d",(int)(g->clpick_given));

  limit=queues[0];
  printf("\ng->clpick_given vec len (el 0): %d",(int)limit);
  printf("\nNROF_CLPICK_QUEUES: %d",NROF_CLPICK_QUEUES);
  // do big block-size steps over array
  for(i=1; i<limit && i<(NROF_CLPICK_QUEUES*CLPICK_QUEUE_BLOCKGINTS); i=i+CLPICK_QUEUE_BLOCKGINTS) {
    printf("\n-- queue nr %d ---\n",i);
    printf("\nCLPICK_QUEUE_GIVEN_POS: %d",(int)(queues[i+CLPICK_QUEUE_GIVEN_POS]));
    printf("\nCLPICK_PRIORQUEUE_RATIO: %d",(int)(queues[i+CLPICK_PRIORQUEUE_RATIO]));
    printf("\nCLPICK_PRIORQUEUE_RATIO_COUNTER: %d",(int)(queues[i+CLPICK_PRIORQUEUE_RATIO_COUNTER]));
    printf("\nCLPICK_THIS_QUEUE_RATIO: %d",(int)(queues[i+CLPICK_THIS_QUEUE_RATIO]));
    printf("\nCLPICK_THIS_QUEUE_RATIO_COUNTER: %d",(int)(queues[i+CLPICK_THIS_QUEUE_RATIO_COUNTER]));

    if (queues[i+CLPICK_QUEUE_POS]!=0) {

      simplequeue_offset=queues[i+CLPICK_QUEUE_POS];
      simplequeue=rotp(g,simplequeue_offset);
      //printf("\nlen of queue: %d",(int)(rotp(g,queues[i+CLPICK_QUEUE_POS])[0])); 
      printf("\nsimplequeue max len %d count %d\n",(int)(simplequeue[0]),(int)(simplequeue[1]));

      //for(j=2;j<simplequeue[0] && j<simplequeue[1];j++) {
      //  printf("\n %d: ",j);
      //  wr_print_clause(g,(gptr)(simplequeue[j]));        
      //} 

    } else {
      printf("\nsimplequeue is 0");  
    }
    if (queues[i+CLPICK_PRIORQUEUE_POS]!=0) {
      wr_print_priorqueue_sizes(g,rotp(g,queues[i+CLPICK_PRIORQUEUE_POS]));
    } else {
      printf("\npriorqueue is 0");    
    }  
  }    
   printf("\n* g->clpick_queues content ends *\n");
}

/*
void OLD___wr_print_clpick_queue_sizes(glb* g, gint* queues) {
  int i; //,j;
  gint limit;
  gint simplequeue_offset;
  gptr simplequeue;
  int prior,max_used_prior,elstart;
  gint* queue;

  printf("\n*** g->clpick_queues sizes ****\n");
  if (!queues) {
    printf("\nqueues is NULL\n");
    return;    
  }  
  
  //limit=queues[0];
  //for(i=0; i<limit; i++) {
  //  printf("\ni %d queues[i] %d ",i,queues[i]);
  //} 
  //return; 
  
  printf("\ng->clpick_given: %d",(int)(g->clpick_given));

  limit=queues[0];
  printf("\ng->clpick_given vec len (el 0): %d",(int)limit);
  printf("\nNROF_CLPICK_QUEUES: %d",NROF_CLPICK_QUEUES);
  // do big block-size steps over array
  for(i=1; i<limit && i<(NROF_CLPICK_QUEUES*CLPICK_QUEUE_BLOCKGINTS); i=i+CLPICK_QUEUE_BLOCKGINTS) {
    printf("\n-- queue nr %d ---\n",i);
    printf("\nCLPICK_QUEUE_GIVEN_POS: %d",(int)(queues[i+CLPICK_QUEUE_GIVEN_POS]));
    printf("\nCLPICK_PRIORQUEUE_RATIO: %d",(int)(queues[i+CLPICK_PRIORQUEUE_RATIO]));
    printf("\nCLPICK_PRIORQUEUE_RATIO_COUNTER: %d",(int)(queues[i+CLPICK_PRIORQUEUE_RATIO_COUNTER]));
    printf("\nCLPICK_THIS_QUEUE_RATIO: %d",(int)(queues[i+CLPICK_THIS_QUEUE_RATIO]));
    printf("\nCLPICK_THIS_QUEUE_RATIO_COUNTER: %d",(int)(queues[i+CLPICK_THIS_QUEUE_RATIO_COUNTER]));

    if (queues[i+CLPICK_QUEUE_POS]!=0) {

      simplequeue_offset=queues[i+CLPICK_QUEUE_POS];
      simplequeue=rotp(g,simplequeue_offset);
      //printf("\nlen of queue: %d",(int)(rotp(g,queues[i+CLPICK_QUEUE_POS])[0])); 
      printf("\nsimplequeue max len %d count %d, elems:\n",(int)(simplequeue[0]),(int)(simplequeue[1]));
      
      //for(j=2;j<simplequeue[0] && j<simplequeue[1];j++) {
      //  printf("\n %d: ",j);
      //  wr_print_clause(g,(gptr)(simplequeue[j]));        
      //} 
      
    } else {
      printf("\nsimplequeue is 0");  
    }
    if (queues[i+CLPICK_PRIORQUEUE_POS]!=0) {
      queue=rotp(g,queues[i+CLPICK_PRIORQUEUE_POS]);
      max_used_prior=queue[PRIORQUEUE_MAX_USED_PRIOR_POS];
      printf("\n");
      printf("priority queue with max priority %d, max used priority %d: \n",
        (int)(queue[PRIORQUEUE_ARR_LEN_POS]-2),
        max_used_prior
      );  

      for(prior=0;prior<=max_used_prior;prior++) {  
        queue=rotp(g,queues[i+CLPICK_PRIORQUEUE_POS]);
        elstart=(prior+1)*PRIORQUEUE_NODE_GINT_NR; // each prior has a block of N gints    
        if (queue[elstart+PRIORQUEUE_ARR_OFFSET_POS]) {
          printf("clauses with priority %d bucket size %d next free %d next pick %d\n",
            prior,
            (int)(queue[elstart+PRIORQUEUE_ARR_LEN_POS]),
            (int)(queue[elstart+PRIORQUEUE_NEXT_FREE_INDEX_POS]),
            (int)(queue[elstart+PRIORQUEUE_NEXT_PICK_INDEX_POS])
          );
          
          //bucket=rotp(g,queue[elstart+PRIORQUEUE_ARR_OFFSET_POS]);      
          //for(i=0;i<queue[elstart+PRIORQUEUE_NEXT_FREE_INDEX_POS];i++) {
          //  cl=rotp(g,bucket[i]);
          //  if (i==queue[elstart+PRIORQUEUE_NEXT_PICK_INDEX_POS]) {
          //    printf("%d [pick] ",i);
          //  } else {
          //    printf("%d: ",i);
          //  }
          //  wr_print_clause(g,cl);
          //  printf("\n");
          //}
          
        }
      }

      wr_print_priorqueue(g,rotp(g,queues[i+CLPICK_PRIORQUEUE_POS]));
    } else {
      printf("\npriorqueue is 0");    
    }  
  }    
  printf("\n* g->clpick_queues content ends *\n");
}
*/

/* =====================================================

  priority queue for given clause selection
  
  priorities are 0... with smaller being better
  prior queue length in gints: 4*(max_priority+2)
  prior queue is a list of a four-el header 

    max_priority: larger ones are added to the last
    min_pickable_priority: updated when new clause added or existing picked (not used yet)
    max_used_priority: updated when new clause added
    max_keep_priority: els with larger priority are not added if space limited
  
  and N els, each el a sequence of four gints:
    
    arr_length
    next_free_index
    next_to_pick_index: zero if cannot pick
    offset_to_arr: arr is an array of offsets of clauses  
      if no more space to add a new el, array is realloced


====================================================== */

/*   
  
make a priority queue  

  maxprior is the maximal allowed priority 

*/

gint* wr_make_priorqueue(glb* g, int maxprior) {
  gint* queue;
  int i,gsize;

  gsize=PRIORQUEUE_NODE_GINT_NR*(maxprior+2);
  queue=wr_malloc(g,sizeof(gint)*gsize);
  if (queue==NULL) {
    (g->alloc_err)=1;    
    wr_alloc_err2int(g,"Cannot allocate memory for a priority queue with size %d",gsize);    
    return NULL;
  }
  for(i=0;i<gsize;i++) queue[i]=0;  
  queue[PRIORQUEUE_MAX_PRIOR_POS]=maxprior;
  /*
  queue[PRIORQUEUE_MIN_PICKABLE_PRIOR_POS]=0;
  queue[PRIORQUEUE_MAX_USED_PRIOR_POS]=0;
  queue[PRIORQUEUE_MAX_KEEP_PRIOR_POS]=0;
  */
  return queue;  
}


/*

store a clause in a priority queue

*/

gint* wr_push_priorqueue(glb* g, gint* queue, gptr cl, int prior) {
  int elstart; // start of a bucket el in the queue
  gint* bucket;
  int arrsize;

  // negative priors not ok
  if (prior<0) return queue;
  // too big priors may be discarded
  if (queue[PRIORQUEUE_MAX_KEEP_PRIOR_POS] && prior>queue[PRIORQUEUE_MAX_KEEP_PRIOR_POS]) {
    return queue;
  }  
  // priors over max allowed are all stored in the last bucket
  if (prior>queue[PRIORQUEUE_MAX_PRIOR_POS]) prior=queue[PRIORQUEUE_MAX_PRIOR_POS];
  elstart=(prior+1)*PRIORQUEUE_NODE_GINT_NR; // first el block is a header
  if (!queue[elstart+PRIORQUEUE_ARR_LEN_POS]) {
    // el is empty: create a new bucket array
    arrsize=PRIORQUEUE_INITIAL_BUCKETELS;
    bucket=wr_malloc(g, sizeof(gint)*arrsize);
    if (bucket==NULL) {
      (g->alloc_err)=1;    
      wr_alloc_err2int(g,"Cannot allocate memory for a priority queue bucket with length",arrsize);
      return NULL;
    }
    bucket[0]=rpto(g,cl);
    queue[elstart+PRIORQUEUE_ARR_LEN_POS]=arrsize;
    queue[elstart+PRIORQUEUE_NEXT_FREE_INDEX_POS]=1;
    queue[elstart+PRIORQUEUE_NEXT_PICK_INDEX_POS]=0;
    queue[elstart+PRIORQUEUE_ARR_OFFSET_POS]=rpto(g,bucket);
    // if greater than max prior of a really allocated bucket, update the max  indicator
    if (prior>queue[PRIORQUEUE_MAX_USED_PRIOR_POS]) queue[PRIORQUEUE_MAX_USED_PRIOR_POS]=prior;
  } else if (queue[elstart+PRIORQUEUE_NEXT_FREE_INDEX_POS]>=queue[elstart+PRIORQUEUE_ARR_LEN_POS]){
    // bucket array is full: realloc and then add
    arrsize=queue[elstart+PRIORQUEUE_ARR_LEN_POS]*2;
    bucket=wr_realloc(g,rotp(g,queue[elstart+PRIORQUEUE_ARR_OFFSET_POS]),sizeof(gint)*arrsize);
    if (bucket==NULL) {
      (g->alloc_err)=1;    
      wr_alloc_err2int(g,"Cannot reallocate memory for a priority queue bucket with length",arrsize);
      return NULL;
    }
    bucket[queue[elstart+PRIORQUEUE_NEXT_FREE_INDEX_POS]]=rpto(g,cl);
    queue[elstart+PRIORQUEUE_ARR_LEN_POS]=arrsize;
    queue[elstart+PRIORQUEUE_NEXT_FREE_INDEX_POS]++;    
    queue[elstart+PRIORQUEUE_ARR_OFFSET_POS]=rpto(g,bucket);
  } else {
    // bucket array has enough space
    bucket=rotp(g,queue[elstart+PRIORQUEUE_ARR_OFFSET_POS]);
    bucket[queue[elstart+PRIORQUEUE_NEXT_FREE_INDEX_POS]]=rpto(g,cl);
    queue[elstart+PRIORQUEUE_NEXT_FREE_INDEX_POS]++;
  } 
  return queue;
}

/*

pick a clause from a priority queue

*/

gint* wr_pick_priorqueue(glb* g, gint* queue) {
  int prior, max_used_prior;
  int elstart; // start of a bucket el in the queue
  int pickel; // which el to pick in the bucket
  gint* bucket;  
  gint* cl;
  
  max_used_prior=queue[PRIORQUEUE_MAX_USED_PRIOR_POS];
  for(prior=0;prior<=max_used_prior;prior++) {
    // loop to avoid marked-as-used clauses
    while(1) {
      elstart=(prior+1)*PRIORQUEUE_NODE_GINT_NR; // each prior has a block of N gints
      if (queue[elstart+PRIORQUEUE_NEXT_PICK_INDEX_POS]<queue[elstart+PRIORQUEUE_NEXT_FREE_INDEX_POS] &&
          queue[elstart+PRIORQUEUE_NEXT_PICK_INDEX_POS]<queue[elstart+PRIORQUEUE_ARR_LEN_POS] &&       
          queue[elstart+PRIORQUEUE_ARR_LEN_POS]) {
        /*    
        printf("\n wr_pick_priorqueue found cl using %d and %d and %d\n",
          queue[elstart+PRIORQUEUE_NEXT_FREE_INDEX_POS],
          queue[elstart+PRIORQUEUE_NEXT_PICK_INDEX_POS],
          queue[elstart+PRIORQUEUE_ARR_LEN_POS]
        );
        */    
        pickel=queue[elstart+PRIORQUEUE_NEXT_PICK_INDEX_POS];
        bucket=rotp(g,queue[elstart+PRIORQUEUE_ARR_OFFSET_POS]);      
        cl=rotp(g,bucket[pickel]);
        queue[elstart+PRIORQUEUE_NEXT_PICK_INDEX_POS]++; // next time pick next   
        if (cl!=NULL && !wr_cl_ismarked_given(g,cl)) { 
          return cl;
        }  
      } else {
        break; // no pickable clauses found
      }  
    }
  }
  // if nothing found, return NULL
  return NULL;
}


/*

free a priority queue

*/

void wr_free_priorqueue(glb* g, gint* queue) {
  int max_used_prior, prior;
  int elstart; // start of a bucket el in the queue  
  
  //printf("\nwr_free_priorqueue called\n");
  max_used_prior=queue[PRIORQUEUE_MAX_USED_PRIOR_POS];
  for(prior=0;prior<=max_used_prior;prior++) {
    //printf("\nprior %d\n",prior);
    elstart=(prior+1)*PRIORQUEUE_NODE_GINT_NR; // each prior has a block of N gints    
    //printf("\n elstart %d queue[elstart+PRIORQUEUE_ARR_OFFSET_POS] %d ",
    //   elstart,queue[elstart+PRIORQUEUE_ARR_OFFSET_POS]);
    if (queue[elstart+PRIORQUEUE_ARR_OFFSET_POS]) {
      wr_free(g,rotp(g,queue[elstart+PRIORQUEUE_ARR_OFFSET_POS]));
    }
  }
  wr_free(g,queue);
}



/*

print a priority queue

*/

void wr_print_priorqueue(glb* g, gint* queue) {
  int max_used_prior;
  int elstart; // start of a bucket el in the queue 
  gint* bucket;
  gint* cl;
  int prior,i;
  
  max_used_prior=queue[PRIORQUEUE_MAX_USED_PRIOR_POS];
  printf("\n");
  printf("priority queue with max priority %d, max used priority %d: \n",
    (int)(queue[PRIORQUEUE_ARR_LEN_POS]-2),
    max_used_prior
  );  
  for(prior=0;prior<=max_used_prior;prior++) {  
    elstart=(prior+1)*PRIORQUEUE_NODE_GINT_NR; // each prior has a block of N gints    
    if (queue[elstart+PRIORQUEUE_ARR_OFFSET_POS]) {
      printf("clauses with priority %d bucket size %d next free %d next pick %d\n",
        prior,
        (int)(queue[elstart+PRIORQUEUE_ARR_LEN_POS]),
        (int)(queue[elstart+PRIORQUEUE_NEXT_FREE_INDEX_POS]),
        (int)(queue[elstart+PRIORQUEUE_NEXT_PICK_INDEX_POS])
      );
      bucket=rotp(g,queue[elstart+PRIORQUEUE_ARR_OFFSET_POS]);      
      for(i=0;i<queue[elstart+PRIORQUEUE_NEXT_FREE_INDEX_POS];i++) {
        cl=rotp(g,bucket[i]);
        if (i==queue[elstart+PRIORQUEUE_NEXT_PICK_INDEX_POS]) {
          printf("%d [pick] ",i);
        } else {
          printf("%d: ",i);
        }
        wr_print_clause(g,cl);
        printf("\n");
      }
    }
  }
}



void wr_print_priorqueue_sizes(glb* g, gint* queue) {
  int max_used_prior;
  int elstart; // start of a bucket el in the queue 
  gint* bucket;
  gint* cl;
  int prior,i;
  
  max_used_prior=queue[PRIORQUEUE_MAX_USED_PRIOR_POS];
  printf("\n");
  printf("priority queue with max priority %d, max used priority %d: \n",
    (int)(queue[PRIORQUEUE_ARR_LEN_POS]-2),
    max_used_prior
  );  
  for(prior=0;prior<=max_used_prior;prior++) {  
    elstart=(prior+1)*PRIORQUEUE_NODE_GINT_NR; // each prior has a block of N gints    
    if (queue[elstart+PRIORQUEUE_ARR_OFFSET_POS]) {
      printf("clauses with priority %d bucket size %d next free %d next pick %d\n",
        prior,
        (int)(queue[elstart+PRIORQUEUE_ARR_LEN_POS]),
        (int)(queue[elstart+PRIORQUEUE_NEXT_FREE_INDEX_POS]),
        (int)(queue[elstart+PRIORQUEUE_NEXT_PICK_INDEX_POS])
      );
      
      bucket=rotp(g,queue[elstart+PRIORQUEUE_ARR_OFFSET_POS]);      
      for(i=0;i<queue[elstart+PRIORQUEUE_NEXT_FREE_INDEX_POS] && i<2;i++) {
        cl=rotp(g,bucket[i]);
        if (i==queue[elstart+PRIORQUEUE_NEXT_PICK_INDEX_POS]) {
          printf("%d [pick] ",i);
        } else {
          printf("%d: ",i);
        }
        wr_print_clause(g,cl);
        printf("\n");
      }
      
    }
  }
}


#ifdef __cplusplus
}
#endif



