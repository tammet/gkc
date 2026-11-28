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

#include <stdint.h>
#include <string.h>
#include "rincludes.h"  
 

/* ====== Private defs =========== */

//#define DEBUG
//#undef DEBUG  

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
  wr_printf("\npushing to clpickstack pos %d\n",(int)((rotp(g,g->clpickstack))[1]));  
#endif  
  (g->clpickstack)=rpto(g,wr_cvec_push(g,rotp(g,(g->clpickstack)),(gint)cl));
  //wr_show_clpickstack(g);
}


void wr_show_clpickstack(glb* g) {
  int i;
  
  for(i=2;i<(rotp(g,g->clpickstack))[1];i++) {
    wr_printf("\nclpickstack nr %d :",i);
    wr_print_record(g,(gptr)((rotp(g,g->clpickstack))[i]));    
  } 
}


/*

store a clause in a passive queue

*/

void wr_push_clqueue_cl(glb* g, gptr cl) {

#ifdef DEBUG  
  wr_printf("\npushing to clqueue pos %d\n",(int)((rotp(g,g->clqueue))[1]));  
#endif  
  (g->clqueue)=rpto(g,wr_cvec_push(g,rotp(g,(g->clqueue)),(gint)cl));
  //wr_show_clqueue(g);
}


void wr_show_clqueue(glb* g) {
  int i;
  
  for(i=2;i<(rotp(g,g->clqueue))[1];i++) {
    wr_printf("\nclqueue nr %d :",i);
    wr_print_record(g,(gptr)((rotp(g,g->clqueue))[i]));
    wr_printf(" ");
    if (wg_rec_is_rule_clause(g->db, (gptr)((rotp(g,g->clqueue))[i]))) {
      wr_printf("rule");
    } else {
      wr_printf("fact");
    }
  } 
  wr_printf("\n");
}

/*

make a clause active

*/


void wr_show_clactive(glb* g) {
  int i;
  
  for(i=2;i<(rotp(g,g->clactive))[1];i++) {
    wr_printf("\nclactive nr %d :",i);
    wr_print_record(g,(gptr)((rotp(g,g->clactive))[i]));    
  } 
}

void wr_show_clactivesubsume(glb* g) {
  int i;
  
  if (!(g->clactivesubsume)) {
    wr_printf("\n g->clactivesubsume is NULL\n");
    return;
  }
  for(i=2;i<(rotp(g,g->clactivesubsume))[1];i+=CLMETABLOCK_ELS) {
    wr_printf("\nclactivesubsume nr %d :",i);       
    wr_printf("\n lengths   ");
    wr_print_gint_hashmask(g, (rotp(g,g->clactivesubsume))[i+CLMETABLOCK_LENGTHS_POS]);
    wr_printf("\n sizes     ");
    wr_print_gint_hashmask(g, (rotp(g,g->clactivesubsume))[i+CLMETABLOCK_SIZES_POS]);
    wr_printf("\n pref1bits ");
    wr_print_gint_hashmask(g, (rotp(g,g->clactivesubsume))[i+CLMETABLOCK_PREF1BITS_POS]);
    wr_printf("\n pref2bits ");
    wr_print_gint_hashmask(g, (rotp(g,g->clactivesubsume))[i+CLMETABLOCK_PREF2BITS_POS]);
    wr_printf("\n pref3bits ");
    wr_print_gint_hashmask(g, (rotp(g,g->clactivesubsume))[i+CLMETABLOCK_PREF3BITS_POS]);
    wr_printf("\n clause    ");
    wr_print_clause(g,rotp(g,(rotp(g,g->clactivesubsume))[i+CLMETABLOCK_CL_POS]));
    wr_printf("\n record    ");
    wg_print_record(g->db,rotp(g,(rotp(g,g->clactivesubsume))[i+CLMETABLOCK_CL_POS]));

    wr_printf("\n"); 
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


int wr_cl_store_res_terms(glb* g, gptr cl, cvec resolvability) {    
  void* db=g->db;
  int i,j;
  int len;      
  int ruleflag; // 0 if not rule
  gint meta;
  gint atom;  
  int negflag; // 1 if negative
  //int termflag; // 1 if complex atom  
  gint hash=0;
  vec hashvec, hashvec_unit;
  int tmp;
  int preflen;
  gint prefhashes[ATOM_PREFHASH_MAXLEN+1];
  
#ifdef DEBUG
  wr_printf("cl_store_res_terms called on cl: "); 
  wr_print_clause(g,cl);
#endif  

  // get clause data for input clause
       
  ruleflag=wg_rec_is_rule_clause(db,cl);
  if (ruleflag) len = wg_count_clause_atoms(db, cl);
  else len=1;
 
#ifdef DEBUG
  wr_printf("ruleflag %d len %d \n",
          ruleflag,len);
#endif  
  // loop over literals
  for(i=0; i<len; i++) {  
    if (!resolvability[i+1] && !(g->queryfocus_strat)) continue;
    negflag=0;
    if (!ruleflag) {
      atom=encode_record(db,cl);      
    } else {       
      meta=wg_get_rule_clause_atom_meta(db,cl,i);
      if (wg_atom_meta_is_neg(db,meta)) negflag=1;              
      atom=wg_get_rule_clause_atom(db,cl,i);
      if (wg_get_encoded_type(db,atom)!=WG_RECORDTYPE) continue;   
    }
    if (1) {
#ifdef XDEBUG 
      wr_printf("adding to hash g->hash_pos_atoms or g->hash_neg_atoms in wr_cl_store_res_terms\n");
#endif       

#ifdef SIMPLE_ACTIVE_SEARCH_HASH
      hash=wr_atom_funhash(g,atom);
      preflen=1;
#else
      preflen=wr_atom_calc_prefhashes(g,atom,prefhashes);        
#endif
      if (negflag) hashvec=rotp(g,g->hash_neg_atoms);
      else hashvec=rotp(g,g->hash_pos_atoms);
      if (0) { // (g->use_strong_unit_cutoff) {
        if (negflag) hashvec_unit=rotp(g,g->hash_neg_units);
        else hashvec_unit=rotp(g,g->hash_pos_units);        
      } 
#ifdef XDEBUG 
      wr_printf("before adding to hash negflag: %d hash: %d\n",negflag,(int)hash);
#endif     
      for(j=0;j<preflen;j++) {
#ifdef SIMPLE_ACTIVE_SEARCH_HASH
#else        
        hash=WR_HASH_NORM(WR_HASH_ADD(preflen-1,prefhashes[j]),NROF_CLTERM_HASHVEC_ELS);
#endif    
        //printf("\n storing preflen %d j %d prefhashes[j] %d hash %d \n",preflen,j,prefhashes[j],hash);    
#ifdef DEBUG        
        wr_printf("\n storing preflen %d j %d prefhashes[j] %d hash %d \n",preflen,j,prefhashes[j],hash);
#endif        
        tmp=wr_clterm_add_hashlist(g,hashvec,hash,atom,cl);  
#ifdef XDEBUG 
        wr_printf("adding to hash ended with %d\n",tmp);
#endif       
        if (tmp) {
          wr_sys_exiterr2int(g,"adding term to hashlist in cl_store_res_terms, code ",tmp);
          return 1;        
        } 
        // add to unit hash if unit and strong strat set
        if (0) { // (len==1 && (g->use_strong_unit_cutoff)) {          
          tmp=wr_clterm_add_hashlist(g,hashvec_unit,hash,atom,cl);         
          if (tmp) {
            wr_sys_exiterr2int(g,"adding term to hashlist_unit in cl_store_res_terms, code ",tmp);
            return 1;        
          }
        }         
      }  
#ifdef DEBUGHASH    
      wr_printf("\nhash_pos_atoms after adding:");      
      wr_clterm_hashlist_print(g,rotp(g,g->hash_pos_atoms));
      wr_printf("\nhash_neg_atoms after adding:");      
      wr_clterm_hashlist_print(g,rotp(g,g->hash_neg_atoms)); 
      if (0) { // (g->use_strong_unit_cutoff) {          
        wr_printf("\nhash_pos_units after adding:");      
        wr_clterm_hashlist_print(g,rotp(g,g->hash_pos_units));
        wr_printf("\nhash_neg_units after adding:");      
        wr_clterm_hashlist_print(g,rotp(g,g->hash_neg_units));
      }    
#endif      
    }  
  }     
#ifdef DEBUG
  wr_printf("cl_store_res_terms finished\n"); 
#endif      
  return 0;
}


int wr_cl_store_res_units(glb* g, gptr cl) {    
  void* db=g->db;
  int i,j;
  int len;      
  int ruleflag; // 0 if not rule
  gint meta;
  gint atom;  
  int negflag; // 1 if negative
  //int termflag; // 1 if complex atom  
  gint hash=0;
  vec hashvec_unit;
  int tmp;
  int preflen;
  gint prefhashes[ATOM_PREFHASH_MAXLEN+1];
  
#ifdef DEBUG
  wr_printf("cl_store_res_units called on cl: "); 
  wr_print_clause(g,cl);
#endif  

  // get clause data for input clause
       
  ruleflag=wg_rec_is_rule_clause(db,cl);
  if (ruleflag) len = wg_count_clause_atoms(db, cl);
  else len=1;
  if (len!=1) return 0;
  
#ifdef DEBUG
  wr_printf("ruleflag %d len %d \n",
          ruleflag,len);
#endif  
  // loop over literals
  for(i=0; i<len; i++) {   
    negflag=0;
    if (!ruleflag) {
      atom=encode_record(db,cl);      
    } else {       
      meta=wg_get_rule_clause_atom_meta(db,cl,i);
      if (wg_atom_meta_is_neg(db,meta)) negflag=1;              
      atom=wg_get_rule_clause_atom(db,cl,i);
      if (wg_get_encoded_type(db,atom)!=WG_RECORDTYPE) continue;   
    }
    if (1) {
#ifdef XDEBUG 
      wr_printf("adding to hash g->hash_pos_units or g->hash_neg_units in wr_cl_store_res_units\n");
#endif       
#ifdef DEBUGHASH               
      wr_printf("\nhash_pos_units before adding:");      
      wr_clterm_hashlist_print(g,rotp(g,g->hash_pos_units));
      wr_printf("\nhash_neg_units before adding:");      
      wr_clterm_hashlist_print(g,rotp(g,g->hash_neg_units));          
#endif         
#ifdef SIMPLE_ACTIVE_SEARCH_HASH
      hash=wr_atom_funhash(g,atom);
      preflen=1;
#else
      preflen=wr_atom_calc_prefhashes(g,atom,prefhashes);        
#endif
      if (negflag) hashvec_unit=rotp(g,g->hash_neg_units);
      else hashvec_unit=rotp(g,g->hash_pos_units);              
#ifdef XDEBUG 
      wr_printf("before adding to hash negflag: %d hash: %d\n",negflag,(int)hash);
#endif     
      for(j=0;j<preflen;j++) {
#ifdef SIMPLE_ACTIVE_SEARCH_HASH
#else        
        hash=WR_HASH_NORM(WR_HASH_ADD(preflen-1,prefhashes[j]),NROF_CLTERM_HASHVEC_ELS);
#endif    
        //printf("\n storing preflen %d j %d prefhashes[j] %d hash %d \n",preflen,j,prefhashes[j],hash);    
#ifdef DEBUG        
        wr_printf("\n storing preflen %d j %d prefhashes[j] %d hash %d \n",preflen,j,prefhashes[j],hash);
#endif        
        tmp=wr_clterm_add_hashlist(g,hashvec_unit,hash,atom,cl);  
#ifdef XDEBUG 
        wr_printf("adding to hash ended with %d\n",tmp);
#endif       
        if (tmp) {
          wr_sys_exiterr2int(g,"adding term to hashlist in cl_store_res_units, code ",tmp);
          return 1;        
        }          
      }  
#if defined(USE_SCUT_INDEX) && !defined(SIMPLE_ACTIVE_SEARCH_HASH)
      wr_sci_insert(g,atom,cl,negflag,preflen);
#endif
#ifdef DEBUGHASH               
      wr_printf("\nhash_pos_units after adding:");      
      wr_clterm_hashlist_print(g,rotp(g,g->hash_pos_units));
      wr_printf("\nhash_neg_units after adding:");      
      wr_clterm_hashlist_print(g,rotp(g,g->hash_neg_units));          
#endif      
    }  
  }     
#ifdef DEBUG
  wr_printf("cl_store_res_units finished\n"); 
#endif      
  return 0;
}


/*
   store all these for the clause cl:

   * subterms into a hash structure for para-from later
   * equality args into a hash structure for para-into later
   * equality as demodulator if orientable

*/

int wr_cl_store_para_terms(glb* g, gptr cl, cvec resolvability) {    
  void* db=g->db;
  int i;
  int len;      
  int ruleflag; // 0 if not rule
  gint meta;
  gint atom;  
  int negflag; // 1 if negative
  int termpath=0;
  gptr tptr;
  gint a,b,atype,btype;
  int tlen;
  int eqtermorder;
  
#ifdef DEBUG
  wr_printf("cl_store_para_terms called on cl: "); 
  wr_print_clause(g,cl);
#endif 
  // get clause data for input clause
  ruleflag=wg_rec_is_rule_clause(db,cl);
  if (ruleflag) len = wg_count_clause_atoms(db, cl);
  else len=1;
  if ((g->hyperres_strat) && !(g->relaxed_hyperres_strat) && !wr_hyperres_satellite_cl(g,cl)) return 0;
#ifdef DEBUG
  wr_printf("ruleflag %d len %d \n",
          ruleflag,len);
#endif  
  // loop over literals
  for(i=0; i<len; i++) {  
    if (!resolvability[i+1] && !(g->queryfocus_strat)) continue;
    negflag=0;
    atom=0;
    termpath=0;
    if (!ruleflag) {
      atom=encode_record(db,cl);      
    } else {      
      meta=wg_get_rule_clause_atom_meta(db,cl,i);
      if (wg_atom_meta_is_neg(db,meta)) negflag=1;               
      atom=wg_get_rule_clause_atom(db,cl,i);    
    }
    if (!atom) continue;
    // store subterms into a hash structure for para-from later
    if (!(g->rewrite_only_strat)) wr_cl_store_para_subterms(g,cl,atom,0,i,&termpath);    
    // cannot store anything if len>1 and g->posunitpara_strat is on
    if ((g->posunitpara_strat) && len!=1) continue;
    // store equality args into a hash structure for para-into later
    if (!negflag && wr_equality_atom(g,atom)) {
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
      if ((g->prohibit_unordered_para) && (!eqtermorder || eqtermorder==3)) continue;
      //printf("\n in wr_cl_store_para_terms eqtermorder:%d ",eqtermorder);
      if (!(g->posunitpara_strat) || len==1) { 
        if ((eqtermorder==1 || eqtermorder==3) && atype!=WG_VARTYPE) {
            //(atype==WG_RECORDTYPE || atype==WG_URITYPE || atype==WG_ANONCONSTTYPE)) {
          // ok to para from left
          if (!(g->rewrite_only_strat)) wr_cl_store_eq_arg(g,cl,a,atype,i,1);
        } else if (eqtermorder==3 && atype==WG_VARTYPE) {
          // a variable side (X = t with X not in t, or X = Y): its own list
          if (!(g->rewrite_only_strat)) wr_cl_store_varside(g,cl,a,b,i,1);
        }
        if ((eqtermorder==2 || eqtermorder==3) && btype!=WG_VARTYPE) {
            //(btype==WG_RECORDTYPE || btype==WG_URITYPE || btype==WG_ANONCONSTTYPE)) {
          // ok to para from right
          if (!(g->rewrite_only_strat)) wr_cl_store_eq_arg(g,cl,b,btype,i,0);
        } else if (eqtermorder==3 && btype==WG_VARTYPE) {
          if (!(g->rewrite_only_strat)) wr_cl_store_varside(g,cl,b,a,i,0);
        }   
      }    
      // store equality as demodulator if everything ok
      if (g->demod_skip_active) {
        // demod_source 2: the picked clause was filed when it was kept
      } else if (len==1 && eqtermorder==1 && (g->use_rewrite_terms_strat) && atype!=WG_VARTYPE) {
        // a will rewrite to b
        wr_cl_store_term_rewriter(g,cl,a,atype,i,1);
        (g->have_rewrite_terms)=1;
      } else if (len==1 && eqtermorder==2 && (g->use_rewrite_terms_strat) && btype!=WG_VARTYPE) {
        // b will rewrite to a
        wr_cl_store_term_rewriter(g,cl,b,btype,i,0);
        (g->have_rewrite_terms)=1;
      } else if (len==1 && eqtermorder==3 && (g->demod_unoriented) && (g->use_rewrite_terms_strat) &&
                 !(g->queryfocus_strat)) {
        // demod_unoriented: both sides, each rewrites with its oriented instances
        wr_cl_store_unoriented_rewriter(g,cl,a,atype,b,btype,i);
      }
    }
  }  
  return 0;
}


/* demod_source 2: file a kept clause as a demodulator when it is a positive
   unit equation that the ordering orients, with the tests of the
   activation in wr_cl_store_para_terms (no demodulators with query_focus;
   a clause of a partial hyperresolution is not filed). The clause record
   stays in queue_termbuf (or in the database for an input clause) for the
   whole run: no rollback of the buffer happens after a clause is pushed to
   the queues. */

void wr_demod_file_kept(glb* g, gptr cl) {
  void* db=g->db;
  gint atom, a, b, meta;
  gptr tptr;
  int atype, btype, eqtermorder;
  if ((g->queryfocus_strat) || !(g->use_rewrite_terms_strat) || (g->inkb)) return;
  if (wg_in_attached_kb(cl)) return; // a KB clause: the KB keeps its own demodulators
  if ((g->hyperres_strat) && !(g->relaxed_hyperres_strat) && !wr_hyperres_satellite_cl(g,cl)) return;
  if (wg_rec_is_rule_clause(db,cl)) {
    if (wg_count_clause_atoms(db,cl)!=1) return;
    meta=wg_get_rule_clause_atom_meta(db,cl,0);
    if (wg_atom_meta_is_neg(db,meta)) return;
    atom=wg_get_rule_clause_atom(db,cl,0);
  } else {
    atom=encode_record(db,cl);
  }
  if (!atom || !wr_equality_atom(g,atom)) return;
  tptr=rotp(g,atom);
  if (get_record_len(tptr)<(g->unify_firstuseterm)+3) return;
  a=tptr[RECORD_HEADER_GINTS+(g->unify_funarg1pos)];
  b=tptr[RECORD_HEADER_GINTS+(g->unify_funarg2pos)];
  atype=wg_get_encoded_type(db,a);
  btype=wg_get_encoded_type(db,b);
  eqtermorder=wr_order_eqterms(g,a,b,NULL);
  if (eqtermorder==1 && atype!=WG_VARTYPE) {
    wr_cl_store_term_rewriter(g,cl,a,atype,0,1);
  } else if (eqtermorder==2 && btype!=WG_VARTYPE) {
    wr_cl_store_term_rewriter(g,cl,b,btype,0,0);
  } else if (eqtermorder==3 && (g->demod_unoriented)) {
    // demod_unoriented: both sides, each rewrites with its oriented instances
    wr_cl_store_unoriented_rewriter(g,cl,a,atype,b,btype,0);
    return;
  } else {
    return;
  }
  (g->have_rewrite_terms)=1;
  (g->stat_demod_filed_kept)++;
}

/* demod_unoriented: the sides of an unorientable positive unit equation:
   each side that is not a variable gets a node in hash_rewrite_terms_u (a
   list that only gives the clause and the side to the rewriting and the
   proof) and an entry in the tree dmu_index, with the other side as its
   right side; a side whose other side has a variable it does not have is
   not filed (the rewriting could not bind that variable) */

void wr_cl_store_unoriented_rewriter(glb* g, gptr cl, gint a, int atype, gint b, int btype, int litnr) {
#ifdef USE_DEMOD_INDEX
  gint fun, hash, side;
  int k, leftflag, stype;
  vec hashvec;
  gptr tptr;
  if ((g->inkb) || (g->dmu_index)==(void*)1 || (g->dmi_index)==(void*)1) return;
  if (!(g->hash_rewrite_terms_u)) {
    gptr hv=wr_vec_new_zero(g,NROF_CLTERM_HASHVEC_ELS);
    if (!hv) return;
    (g->hash_rewrite_terms_u)=rpto(g,hv);
  }
  hashvec=rotp(g,g->hash_rewrite_terms_u);
  for(k=0;k<2;k++) {
    side=(k ? b : a); stype=(k ? btype : atype); leftflag=(k ? 0 : 1);
    if (stype==WG_VARTYPE) continue;
    if (stype==WG_RECORDTYPE) {
      tptr=rotp(g,side);
      fun=tptr[RECORD_HEADER_GINTS+(g->unify_funpos)];
    } else {
      fun=side;
    }
    hash=wr_term_basehash(g,fun);
    if (wr_clterm_add_hashlist_withpath(g,hashvec,hash,side,cl,wr_encode_para_termpath(g,litnr,leftflag))) return;
    // the new node is the first one of its list
    if (wr_dmu_insert(g,side,wr_clterm_hashlist_start(g,hashvec,hash))) (g->have_rewrite_terms)=1;
  }
#else
  UNUSED(g); UNUSED(cl); UNUSED(a); UNUSED(atype); UNUSED(b); UNUSED(btype); UNUSED(litnr);
#endif
}

/* a variable side of a positive equality literal of an active clause, for
   paramodulation into the given clause: a variable unifies with every term,
   so the side has no bucket of its own top symbol in hash_eq_terms; it goes
   to the one bucket of hash_eq_varsides. Left out (counted) when the
   variable occurs in another literal of the clause: the condition of
   superposition from a variable in Vampire (checkSuperpositionFromVariable).
   With the strategy negative_pref, Test/ordering/s1 and s4 end without a
   proof by this condition; without it, equations like X = a | X = b
   (frequent in the GEO and SEV problems) make millions of steps. */

static int wr_varside_occurs(glb* g, gint var, gint t) {
  void* db=g->db; gptr tp; int i, start, end;
  if (t==var) return 1;
  if (!isdatarec(t)) return 0;
  tp=decode_record(db,t);
  start=wr_term_unify_startpos(g); end=wr_term_unify_endpos(g,tp);
  for(i=start;i<end;i++) if (wr_varside_occurs(g,var,tp[i])) return 1;
  return 0;
}

/* 1 if the variable var occurs in a literal of cl other than the literal litnr */

int wr_varside_in_other_lits(glb* g, gptr cl, int litnr, gint var) {
  void* db=g->db; int i, n;
  (void)db; // the clause macros do not use it
  if (!wg_rec_is_rule_clause(db,cl)) return 0;
  n=wg_count_clause_atoms(db,cl);
  for(i=0;i<n;i++) {
    if (i==litnr) continue;
    if (wr_varside_occurs(g,var,wg_get_rule_clause_atom(db,cl,i))) return 1;
  }
  return 0;
}

void wr_cl_store_varside(glb* g, gptr cl, gint var, gint other, int litnr, int leftflag) {
  vec hashvec;
  // query_focus gives the ordering 3 to every equation: the real ordering decides
  if (wr_order_eqterms(g,var,other,NULL)!=3) return;
  if (wr_varside_in_other_lits(g,cl,litnr,var)) { (g->stat_varside_skip_lit)++; return; }
  if (!(g->hash_eq_varsides)) {
    gptr hv=wr_vec_new_zero(g,4);
    if (!hv) return;
    (g->hash_eq_varsides)=rpto(g,hv);
  }
  hashvec=rotp(g,g->hash_eq_varsides);
  if (wr_clterm_add_hashlist_withpath(g,hashvec,1,var,cl,wr_encode_para_termpath(g,litnr,leftflag))) return;
  (g->stat_varside_filed)++;
}

/* the bucket number hash of hash_para_terms gets its first entry: note it, so
   that a variable side of a given clause walks only the buckets with entries */

void wr_para_bucket_note(glb* g, gint hash) {
  cvec v;
  if (!(g->para_terms_buckets)) {
    v=wr_cvec_new(g,1024);
    if (!v) return;
    (g->para_terms_buckets)=rpto(g,v);
  }
  v=wr_cvec_push(g,rotp(g,g->para_terms_buckets),hash);
  if (v) (g->para_terms_buckets)=rpto(g,v);
}

int wr_cl_store_eq_arg(glb* g, gptr cl, gint term, int termtype, int litnr, int leftflag) {    
  gint fun, hash;
  int tmp, path;
  vec hashvec;

  gptr tptr;

#ifdef XDEBUG 
  wr_printf("\nwr_cl_store_eq_arg starts\n");
  wr_printf("term \n");
  wr_print_term(g,term);
  wr_printf("clause \n");
  wr_print_clause(g,cl);
  wr_printf("\nwtermtype %d litnr %d leftflag %d\n",termtype,litnr,leftflag);
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
  wr_printf("\nadding to eq_hash ended with %d\n",tmp);
#endif       
  if (tmp) {
    wr_sys_exiterr2int(g,"adding term to hashlist in  wr_cl_store_eq_arg, code ",tmp);
    return 1;        
  }  
#ifdef DEBUGHASH      
  wr_printf("\nhash_eq_terms after adding:");      
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
  wr_printf("\n*** wr_cl_store_term_rewriter starts \n");
  wr_printf("rewriter ");
  wr_print_term(g,term);
  wr_printf(" --> ");
  //wr_print_term(g,toterm);
  wr_printf("\n clause \n");
  wr_print_clause(g,cl);
  wr_printf("\nwtermtype %d litnr %d leftflag %d\n",termtype,litnr,leftflag);
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
#ifdef USE_DEMOD_INDEX
  // the new node is the first one of its list
  if (!tmp) wr_dmi_insert(g,term,wr_clterm_hashlist_start(g,hashvec,hash));
#endif
#ifdef XDEBUG 
  wr_printf("\nadding to rewrite_hash ended with %d\n",tmp);
#endif       
  if (tmp) {
    wr_sys_exiterr2int(g,"adding term to hashlist in  wr_cl_store_term_rewriter, code ",tmp);
    return 1;        
  }  
  (g->stat_made_rewriters)++;
  if (g->print_derived_cl) {
    wr_printf("\n+ rewriter kept lf %d: ",leftflag);
    wr_print_clause(g,cl);
  }  
#ifdef DEBUGHASH      
  wr_printf("\nhash_rewrite_terms after adding:");      
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
  wr_printf("\ncl_store_para_subterms called at depth %d on term and cl: ",depth); 
  wr_print_term(g,term);
  wr_printf("\n");
  wr_print_clause(g,cl);
  wr_printf("\n");
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

#ifdef PARA_INTO_FUNSYMB
      (*termpath)++;
      wr_cl_store_para_subterms(g,cl,yi,depth+1,litnr,termpath);
#else
      // do not recurse into atomic funsymb
      if (!(i==istart && !isdatarec(yi))) {
        (*termpath)++;
        wr_cl_store_para_subterms(g,cl,yi,depth+1,litnr,termpath);
        /*
        if (!(g->prohibit_deep_para) ||
             ((g->prohibit_deep_para) && depth<1)) {
          wr_cl_store_para_subterms(g,cl,yi,depth+1,litnr,termpath);
        } 
        */ 
      }
#endif
      
    }
  }  
#ifdef PARA_INTO_ATOM  
#else
  // do not put into hash on the atom level  
  if (depth<1) return 1;  
#endif  

  if ((g->prohibit_deep_para) && (depth>2)) return 1;

  // put term into the hash table
  hash=wr_term_basehash(g,fun);
#ifdef XDEBUG 
  wr_printf("\nbefore adding to g->hash_para_terms fun hash is: %d litnr %d origtermpath %d\n",
    (int)hash,litnr,origtermpath);
#endif      
  hashvec=rotp(g,g->hash_para_terms);  
  if (hash>0 && hash<VEC_LEN(hashvec) && !hashvec[hash]) wr_para_bucket_note(g,hash);
  path=wr_encode_para_termpath(g,litnr,origtermpath);
  tmp=wr_clterm_add_hashlist_withpath(g,hashvec,hash,term,cl,path);
#ifdef XDEBUG 
  wr_printf("\nadding to hash ended with %d\n",tmp);
#endif       
  if (tmp) {
    wr_sys_exiterr2int(g,"adding term to hashlist in cl_store_para_terms, code ",tmp);
    return 1;        
  }  
#ifdef DEBUGHASH      
  wr_printf("\nhash_para_terms after adding:");      
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
  wr_printf("\n*** wr_create_clpick_queues called with count %d ****\n",count);
#endif

  queues=wr_vec_new_zero(g,NROF_CLPICK_QUEUES*CLPICK_QUEUE_BLOCKGINTS); // use as vec
  clpick_queues=rpto(g,queues); // return offset
  limit=queues[0]; 
  // do big block-size steps over array
  for(i=1; i<limit && i<(NROF_CLPICK_QUEUES*CLPICK_QUEUE_BLOCKGINTS); i=i+CLPICK_QUEUE_BLOCKGINTS) {
#ifdef QADDDEBUG    
    wr_printf("\n-- queue nr %d ---\n",i);
#endif    
    queues[i+CLPICK_QUEUE_GIVEN_POS]=2; // 0 contains len and 1 contains next free,  first real is 2
    queues[i+CLPICK_PRIORQUEUE_RATIO]=(g->pick_given_queue_ratio); // 4 is normal
    queues[i+CLPICK_PRIORQUEUE_RATIO_COUNTER]=0;
    // v41 testcode follows:
    /*
    if (i==1) {
      // goalassumption
      if (g->cl_pick_queue_strategy) queues[i+CLPICK_THIS_QUEUE_RATIO]=3; // 6
      else queues[i+CLPICK_THIS_QUEUE_RATIO]=1;
    } else if (i==11) {
      // goal
      if (g->cl_pick_queue_strategy) queues[i+CLPICK_THIS_QUEUE_RATIO]=3; // 4
      else queues[i+CLPICK_THIS_QUEUE_RATIO]=1;  // 1
    } else if (i==21) {
      // assumption      
      queues[i+CLPICK_THIS_QUEUE_RATIO]=2;  
    } else {
      queues[i+CLPICK_THIS_QUEUE_RATIO]=1;
    }   
    */   
    // v40 code follows (was in may 2020) before testing
    
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
         
    // assumption experiment code follows  TESTING
    /*
    if (i==1) {
      // goalassumption
      if (g->cl_pick_queue_strategy) queues[i+CLPICK_THIS_QUEUE_RATIO]=10; // 6, 3
      else queues[i+CLPICK_THIS_QUEUE_RATIO]=1;
    } else if (i==11) {
      // goal
      if (g->cl_pick_queue_strategy) queues[i+CLPICK_THIS_QUEUE_RATIO]=1; // 4, 3
      else queues[i+CLPICK_THIS_QUEUE_RATIO]=1;  // 1
    } else if (i==21) {
      // assumption      
      //queues[i+CLPICK_THIS_QUEUE_RATIO]=1;  
      if (g->cl_pick_queue_strategy) queues[i+CLPICK_THIS_QUEUE_RATIO]=10; // 4, 2
      else queues[i+CLPICK_THIS_QUEUE_RATIO]=1;
    } else {
      queues[i+CLPICK_THIS_QUEUE_RATIO]=1;
    } 
    */
    // assumption experiment code ends   
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
  
  //printf("\n push to hyperqueue\n");
  clo=rpto(g,cl);
  newqueue=wr_cvec_push(g,queue,clo);  
  if (newqueue==NULL) return 0;
  if (queue!=newqueue) {
    (g->hyper_queue)=newqueue;
  }
  /* 
  printf("\n after push queue[0] %ld\n",queue[0]);
  printf("\n after push queue[1] %ld\n",queue[1]);
  printf("\n after push queue[2] %ld\n",queue[2]); 
  int i;
  for(i=3;i<newqueue[1];i++) {
    printf("\n hyperqueue %d ",i);
    wr_print_clause(g,rotp(g,newqueue[i]));
    printf("\n");
  }
  */
  return 1;
}


/*

  push to queues

*/


/*
  The start position of the queue block of a clause in the clpick queues
  (GOAL_ASSUMPTIONS_QUEUE_POS, ...): the block that wr_push_cl_clpick_queues
  puts the clause into.
*/

int wr_clpick_queue_start(glb* g, gptr cl) {
  void *db=g->db;
  int qstart;
  gint history;
  gptr historyptr=NULL;
  gint decprior, priority=0;

  if (!(g->cl_pick_queue_strategy)) return AXIOMS_QUEUE_POS;
  // query strat: push to a selected queue depending on priority
  history=wr_get_history(g,cl); 
  historyptr=otp(db,history);
  priority=wr_get_history_record_field(db,historyptr,HISTORY_PRIORITY_POS);
  decprior=wg_decode_int(db,priority);
   
  // modify decpriors according to strat

#ifdef QADDDEBUG
   wr_printf("\ndecprior originally %d\n",decprior);
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
   wr_printf("\ndecprior changed to %d\n",decprior);
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
   wr_printf("\nqstart %d\n",qstart);
#endif

  return qstart;
}

void wr_push_cl_clpick_queues(glb* g, gint queues_offset, gptr cl, int weight) {
  void *db=g->db;
  UNUSED(db);
  int qstart; //,i;
  gint simplequeue_offset, priorqueue_offset;
  gptr queues, simplequeue, priorqueue;

  if ((g->unit_propagate) || (g->unit_backsubsume)) wr_up_register(g,cl); // unit propagation


#ifdef QADDDEBUG
  wr_printf("\nwr_push_cl_clpick_queues called with queryfocus_strat %d (g->cl_pick_queue_strategy) %d weight %d and cl: ",
         (g->queryfocus_strat),(g->cl_pick_queue_strategy),weight);
  wr_print_clause(g,cl);
#endif

  // start pushing

  queues=rotp(g,queues_offset);
  if (g->cl_pick_queue_strategy) { // any strategy value not zero
    qstart=wr_clpick_queue_start(g,cl);
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


/* =============================================

  Experimental weight limit per queue block (strategy key
  queue_weight_limit).

  wr_qwl_compute estimates how far the coming picks reach in each queue
  block and sets g->qwl_weight and g->qwl_fifo_reach. It reads the queues
  and copies of their counters; it changes nothing in them. wr_qwl_drop
  tells the late weight check whether a new clause lies beyond both.

  The pick rule of wr_pick_from_clpick_queues: the blocks take turns; a
  block with the ratio R gives ceil((R+1)/2) picks per turn (the pick and
  the block loop both increase its counter); inside a block, P picks from
  the priority queue (P: CLPICK_PRIORQUEUE_RATIO) and then one from the
  first-in list. The given marks are not used, so the two streams of a
  block are independent.

================================================ */

/*
  budget: the estimated number of picks that remain in this run.
*/

void wr_qwl_compute(glb* g, gptr queues, double budget) {
  int b, k, qs, active[NROF_CLPICK_QUEUES], nactive, changed;
  double turn[NROF_CLPICK_QUEUES], cap[NROF_CLPICK_QUEUES], n[NROF_CLPICK_QUEUES];
  double total=0, remaining, sumturn, share;
  gint np[NROF_CLPICK_QUEUES], nf[NROF_CLPICK_QUEUES], fpos[NROF_CLPICK_QUEUES];
  gint nprior, nfifo, ratio, counter, cum, max_used;
  gptr simplequeue, priorqueue;
  int elstart, prior;

  (g->qwl_active)=0;
  for(b=0;b<NROF_CLPICK_QUEUES;b++) {
    qs=1+(b*CLPICK_QUEUE_BLOCKGINTS);
    np[b]=0; nf[b]=0; fpos[b]=0;
    if (queues[qs+CLPICK_PRIORQUEUE_POS]) {
      priorqueue=rotp(g,queues[qs+CLPICK_PRIORQUEUE_POS]);
      max_used=priorqueue[PRIORQUEUE_MAX_USED_PRIOR_POS];
      for(prior=0;prior<=max_used;prior++) {
        elstart=(prior+1)*PRIORQUEUE_NODE_GINT_NR;
        if (priorqueue[elstart+PRIORQUEUE_ARR_LEN_POS])
          np[b]+=priorqueue[elstart+PRIORQUEUE_NEXT_FREE_INDEX_POS]-
                 priorqueue[elstart+PRIORQUEUE_NEXT_PICK_INDEX_POS];
      }
    }
    if (queues[qs+CLPICK_QUEUE_POS]) {
      simplequeue=rotp(g,queues[qs+CLPICK_QUEUE_POS]);
      fpos[b]=queues[qs+CLPICK_QUEUE_GIVEN_POS];
      nf[b]=CVEC_NEXT(simplequeue)-fpos[b];
      if (nf[b]<0) nf[b]=0;
    }
    cap[b]=(double)(np[b]+nf[b]);
    total+=cap[b];
    turn[b]=(double)((queues[qs+CLPICK_THIS_QUEUE_RATIO]+2)/2);
    if (turn[b]<1) turn[b]=1;
    n[b]=0;
    active[b]=(cap[b]>0);
  }
  if (budget>=total) return; // every clause in the queues is reached: no limit
  // share the budget by the turn lengths; a block that runs empty passes
  // its share on to the others
  remaining=budget;
  for(;;) {
    sumturn=0; nactive=0;
    for(b=0;b<NROF_CLPICK_QUEUES;b++) if (active[b]) { sumturn+=turn[b]; nactive++; }
    if (!nactive || remaining<=0) break;
    changed=0;
    for(b=0;b<NROF_CLPICK_QUEUES;b++) {
      if (!active[b]) continue;
      share=remaining*turn[b]/sumturn;
      if (n[b]+share>=cap[b]) {
        remaining-=cap[b]-n[b];
        n[b]=cap[b];
        active[b]=0;
        changed=1;
      }
    }
    if (changed) continue;
    for(b=0;b<NROF_CLPICK_QUEUES;b++) if (active[b]) n[b]+=remaining*turn[b]/sumturn;
    break;
  }
  // the limits of each block
  for(b=0;b<NROF_CLPICK_QUEUES;b++) {
    qs=1+(b*CLPICK_QUEUE_BLOCKGINTS);
    (g->qwl_weight)[b]=-1;
    (g->qwl_fifo_reach)[b]=fpos[b]+nf[b]+1; // no limit
    if (cap[b]<=0) continue;
    ratio=queues[qs+CLPICK_PRIORQUEUE_RATIO];
    counter=queues[qs+CLPICK_PRIORQUEUE_RATIO_COUNTER];
    if (counter>ratio) counter=ratio;
    // position j of the coming picks of the block is a first-in pick when
    // (counter+j) mod (ratio+1) == ratio
    nfifo=(gint)(((gint)n[b]+counter)/(ratio+1));
    if (nfifo>nf[b]) nfifo=nf[b];
    nprior=(gint)n[b]-nfifo;
    if (nprior>np[b]) {
      nprior=np[b];
      nfifo=(gint)n[b]-np[b];
      if (nfifo>nf[b]) nfifo=nf[b];
    }
    (g->qwl_fifo_reach)[b]=fpos[b]+nfifo;
    // the weight of the first priority-queue clause that is not reached
    if (queues[qs+CLPICK_PRIORQUEUE_POS]) {
      priorqueue=rotp(g,queues[qs+CLPICK_PRIORQUEUE_POS]);
      max_used=priorqueue[PRIORQUEUE_MAX_USED_PRIOR_POS];
      cum=0;
      for(prior=0;prior<=max_used;prior++) {
        elstart=(prior+1)*PRIORQUEUE_NODE_GINT_NR;
        if (!priorqueue[elstart+PRIORQUEUE_ARR_LEN_POS]) continue;
        k=priorqueue[elstart+PRIORQUEUE_NEXT_FREE_INDEX_POS]-
          priorqueue[elstart+PRIORQUEUE_NEXT_PICK_INDEX_POS];
        if (k<=0) continue;
        cum+=k;
        if (cum>nprior) { (g->qwl_weight)[b]=prior; break; }
      }
    }
  }
  (g->qwl_active)=1;
}

/*
  1 if the new clause cl of the given weight lies beyond the limits of its
  queue block: its weight is above the weight limit, and the first-in list
  does not reach its position. A clause at the limit is kept.
*/

int wr_qwl_drop(glb* g, gptr cl, int weight) {
  void *db=g->db;
  UNUSED(db);
  int qstart, b;
  gptr queues, simplequeue;

  if (!(g->qwl_active) || weight<0) return 0;
  // value 3 keeps unit clauses; value 4 also keeps two-literal clauses,
  // which the double cut reads
  if ((g->queue_weight_limit)>=3 &&
      (!wg_rec_is_rule_clause(db,cl) || wg_count_clause_atoms(db,cl)==1)) return 0;
  if ((g->queue_weight_limit)==4 && wg_count_clause_atoms(db,cl)==2) return 0;
  qstart=wr_clpick_queue_start(g,cl);
  b=(qstart-1)/CLPICK_QUEUE_BLOCKGINTS;
  if (b<0 || b>=NROF_CLPICK_QUEUES || (g->qwl_weight)[b]<0) return 0;
  if (weight>MAX_CLPRIOR) weight=MAX_CLPRIOR; // as wr_push_priorqueue
  if (weight<=(g->qwl_weight)[b]) return 0;
  queues=rotp(g,g->clpick_queues);
  if (!queues[qstart+CLPICK_QUEUE_POS]) return 0;
  simplequeue=rotp(g,queues[qstart+CLPICK_QUEUE_POS]);
  if (CVEC_NEXT(simplequeue)<(g->qwl_fifo_reach)[b]) return 0;
  return 1;
}


/* =============================================

  Experimental lower bound of the weight of a derived clause before its
  build (strategy key queue_weight_bound, only with queue_weight_limit).

  The bound sums the weights of the parent literals that stay in the
  derived clause, as wr_calc_lits_weight sums the literals of a clause,
  with every variable at its smaller weight. A substitution replaces a
  variable by a term that weighs at least as much, so the bound is not
  above the weight of the derived clause, unless a merge or a cut removes
  a literal. The literal weights of a parent clause are kept in a cache
  (g->qwb_cache), keyed by the clause; the given clause in given_termbuf
  is computed again for each pick.

================================================ */

typedef struct { gptr p; gint gen; gint off; gint n; } wr_qwb_ent;
typedef struct { wr_qwb_ent* tab; gint cap, cnt; gint* data; gint dcap, dlen; } wr_qwb_cache_t;

#define QWB_LITINFO 4 // per literal: weight, weight with penalty, depth, flags
#define QWB_ANSWER 1
#define QWB_POSITIVE 2
#define QWB_EQUALITY 4

void wr_qwb_free(glb* g) {
  wr_qwb_cache_t* c=(wr_qwb_cache_t*)(g->qwb_cache);
  if (!c) return;
  if (c->tab) sys_free(c->tab);
  if (c->data) sys_free(c->data);
  sys_free(c);
  (g->qwb_cache)=NULL;
}

static wr_qwb_cache_t* wr_qwb_get(glb* g) {
  wr_qwb_cache_t* c=(wr_qwb_cache_t*)(g->qwb_cache);
  if (c) return c;
  c=sys_malloc(sizeof(wr_qwb_cache_t));
  if (!c) return NULL;
  c->cap=1<<14; c->cnt=0;
  c->tab=sys_malloc(sizeof(wr_qwb_ent)*c->cap);
  c->dcap=1<<16; c->dlen=0;
  c->data=sys_malloc(sizeof(gint)*c->dcap);
  if (!c->tab || !c->data) {
    if (c->tab) sys_free(c->tab);
    if (c->data) sys_free(c->data);
    sys_free(c);
    return NULL;
  }
  memset(c->tab,0,sizeof(wr_qwb_ent)*c->cap);
  (g->qwb_cache)=c;
  return c;
}

static gint wr_qwb_slot(wr_qwb_cache_t* c, gptr p) {
  uint64_t x=(uint64_t)(uintptr_t)p;
  x^=x>>33; x*=UINT64_C(0xff51afd7ed558ccd); x^=x>>29;
  return (gint)(x&(uint64_t)(c->cap-1));
}

/* the literal information of cl: QWB_LITINFO gints per literal, or NULL */

static gint* wr_qwb_litinfo(glb* g, gptr cl, gint* n) {
  void* db=g->db;
  wr_qwb_cache_t* c=wr_qwb_get(g);
  gint gen, k, i, len, atom, meta, *d;
  int size, depth, hasvars, w, ingiven;

  if (!c) return NULL;
  ingiven=((g->given_termbuf) && cl>=(g->given_termbuf) &&
           cl<(g->given_termbuf)+CVEC_LEN(g->given_termbuf));
  gen=ingiven ? (gint)(g->stat_given_candidates)+1 : 0;
  k=wr_qwb_slot(c,cl);
  while(c->tab[k].p) {
    if (c->tab[k].p==cl) {
      if (c->tab[k].gen==gen) { *n=c->tab[k].n; return c->data+c->tab[k].off; }
      break;
    }
    k=(k+1)&(c->cap-1);
  }
  if (wg_rec_is_rule_clause(db,cl)) len=wg_count_clause_atoms(db,cl);
  else len=1;
  if (c->dlen+QWB_LITINFO*len>c->dcap) {
    gint ncap=c->dcap; gint* nd;
    while(c->dlen+QWB_LITINFO*len>ncap) ncap*=2;
    nd=sys_malloc(sizeof(gint)*ncap);
    if (!nd) return NULL;
    memcpy(nd,c->data,sizeof(gint)*c->dlen);
    sys_free(c->data);
    c->data=nd; c->dcap=ncap;
  }
  if (!(c->tab[k].p)) {
    if (2*(c->cnt+1)>c->cap) {
      // grow the table and find the slot again
      wr_qwb_ent* ot=c->tab; gint oc=c->cap, j, m;
      wr_qwb_ent* nt=sys_malloc(sizeof(wr_qwb_ent)*oc*2);
      if (!nt) return NULL;
      memset(nt,0,sizeof(wr_qwb_ent)*oc*2);
      c->tab=nt; c->cap=oc*2;
      for(j=0;j<oc;j++) if (ot[j].p) {
        m=wr_qwb_slot(c,ot[j].p);
        while(nt[m].p) m=(m+1)&(c->cap-1);
        nt[m]=ot[j];
      }
      sys_free(ot);
      k=wr_qwb_slot(c,cl);
      while(c->tab[k].p) k=(k+1)&(c->cap-1);
    }
    c->tab[k].p=cl;
    (c->cnt)++;
  }
  c->tab[k].gen=gen;
  c->tab[k].off=c->dlen;
  c->tab[k].n=len;
  d=c->data+c->dlen;
  for(i=0;i<len;i++) {
    if (wg_rec_is_rule_clause(db,cl)) {
      atom=wg_get_rule_clause_atom(db,cl,i);
      meta=wg_get_rule_clause_atom_meta(db,cl,i);
    } else {
      atom=encode_datarec_offset(pto(db,cl));
      meta=0;
    }
    d[i*QWB_LITINFO+3]=0;
    if (!wg_atom_meta_is_neg(db,meta)) d[i*QWB_LITINFO+3]|=QWB_POSITIVE;
    if (wr_answer_lit(g,atom)) {
      d[i*QWB_LITINFO+0]=1;
      d[i*QWB_LITINFO+1]=1;
      d[i*QWB_LITINFO+2]=0;
      d[i*QWB_LITINFO+3]|=QWB_ANSWER;
      continue;
    }
    size=0; depth=0; hasvars=0;
    w=wr_calc_term_weight_novars(g,atom,0,&size,&depth,0,&hasvars);
    d[i*QWB_LITINFO+0]=w;
    if (wr_equality_atom(g,atom)) d[i*QWB_LITINFO+3]|=QWB_EQUALITY;
    if ((g->atom_poseq_penalty) && (d[i*QWB_LITINFO+3]&QWB_EQUALITY) &&
        (d[i*QWB_LITINFO+3]&QWB_POSITIVE))
      w+=(g->atom_poseq_penalty);
    d[i*QWB_LITINFO+1]=w;
    d[i*QWB_LITINFO+2]=depth;
  }
  c->dlen+=QWB_LITINFO*len;
  *n=len;
  return d;
}

/* sums over the literals of cl without one literal: the first with the atom
   skipatom (if not 0) or the one at index skipidx (if not -1) */

static int wr_qwb_part(glb* g, gptr cl, gint skipatom, int skipidx,
                       int* sum, int* maxw, int* maxdepth, int* nlen, int* nlits, int* flags) {
  void* db=g->db;
  gint* d; gint n, i;
  int skipped=0;

  if (!cl) return 1;
  d=wr_qwb_litinfo(g,cl,&n);
  if (!d) return 0;
  for(i=0;i<n;i++) {
    if (!skipped) {
      if (skipatom) {
        gint a=wg_rec_is_rule_clause(db,cl) ? wg_get_rule_clause_atom(db,cl,i) : encode_datarec_offset(pto(db,cl));
        if (a==skipatom) { skipped=1; continue; }
      } else if (i==skipidx) { skipped=1; continue; }
    }
    (*nlits)++;
    *flags|=(int)d[i*QWB_LITINFO+3];
    if (d[i*QWB_LITINFO+3]&QWB_ANSWER) continue;
    (*nlen)++;
    *sum+=(int)d[i*QWB_LITINFO+1];
    if (d[i*QWB_LITINFO+0]>*maxw) *maxw=(int)d[i*QWB_LITINFO+0];
    if (d[i*QWB_LITINFO+2]>*maxdepth) *maxdepth=(int)d[i*QWB_LITINFO+2];
  }
  return 1;
}

/*
  1 if the derived clause of the parent literals that stay (cl1 without one
  literal, cl2 without one literal) would be dropped by the queue limit
  already by its lower bound. h1, h2: the parents whose history records
  give the queue block, as wr_calc_history_priority2 combines them.
*/

/*
  The queue block start of a new clause from its two parents h1 and h2, as
  wr_clpick_queue_start finds it for the strategies 0, 1 and 4; -1 for the
  strategies 2 and 3, where the form of the new clause counts too.
*/

int wr_qwl_block_from_parents(glb* g, gptr h1, gptr h2) {
  gint decprior;

  if (!(g->cl_pick_queue_strategy) || (g->cl_pick_queue_strategy)==4) return AXIOMS_QUEUE_POS;
  if ((g->cl_pick_queue_strategy)==2 || (g->cl_pick_queue_strategy)==3) return -1;
  decprior=wg_decode_int(g->db,wr_calc_history_priority2(g,h1,h2));
  if (decprior==WR_HISTORY_GOAL_ROLENR) return GOAL_QUEUE_POS;
  if (decprior==WR_HISTORY_ASSUMPTION_ROLENR) return ASSUMPTIONS_QUEUE_POS;
  if (decprior==WR_HISTORY_FROMGOAL_ROLENR) return GOAL_QUEUE_POS;
  if (decprior==WR_HISTORY_FROMGOALASSUMPTION_ROLENR) return GOAL_ASSUMPTIONS_QUEUE_POS;
  if (decprior==WR_HISTORY_FROMASSUMPTION_ROLENR) return ASSUMPTIONS_QUEUE_POS;
  return AXIOMS_QUEUE_POS;
}

/*
  1 if a clause of nlits literals and the given weight in the block that
  starts at qstart lies beyond the queue limit, as wr_qwl_drop decides it.
*/

int wr_qwl_drop_weight(glb* g, int qstart, int weight, int nlits) {
  int b;
  gptr queues, simplequeue;

  if (!(g->qwl_active) || weight<0 || qstart<0) return 0;
  if ((g->queue_weight_limit)>=3 && nlits<=1) return 0;
  if ((g->queue_weight_limit)==4 && nlits<=2) return 0;
  b=(qstart-1)/CLPICK_QUEUE_BLOCKGINTS;
  if ((g->qwl_weight)[b]<0) return 0;
  if (weight>MAX_CLPRIOR) weight=MAX_CLPRIOR;
  if (weight<=(g->qwl_weight)[b]) return 0;
  queues=rotp(g,g->clpick_queues);
  if (!queues[qstart+CLPICK_QUEUE_POS]) return 0;
  simplequeue=rotp(g,queues[qstart+CLPICK_QUEUE_POS]);
  if (CVEC_NEXT(simplequeue)<(g->qwl_fifo_reach)[b]) return 0;
  return 1;
}

int wr_qwb_drop(glb* g, gptr cl1, gint skipatom1, int skipidx1,
                gptr cl2, gint skipatom2, int skipidx2, gptr h1, gptr h2) {
  int sum=0, maxw=0, maxdepth=0, nlen=0, nlits=0, flags=0, bound, qstart, rewrite;
  int minvarw;

  if (!(g->qwl_active) || (g->use_hornpref) || (g->hyperres_strat) ||
      (g->print_derived_cl) || (g->print_derived)) return 0;
  minvarw=((g->var_weight)<(g->repeat_var_weight)) ? (g->var_weight) : (g->repeat_var_weight);
  if (minvarw>10) return 0; // a substitution could lower the weight
  if (!wr_qwb_part(g,cl1,skipatom1,skipidx1,&sum,&maxw,&maxdepth,&nlen,&nlits,&flags)) return 0;
  if (!wr_qwb_part(g,cl2,skipatom2,skipidx2,&sum,&maxw,&maxdepth,&nlen,&nlits,&flags)) return 0;
  if (flags&QWB_ANSWER) return 0; // the final steps may register an answer
  if (nlen<=0) return 0;
  if ((g->queue_weight_limit)>=3 && nlen<=1) return 0;
  if ((g->queue_weight_limit)==4 && nlen<=2) return 0;
  // a positive unit equation can become a rewrite rule: wr_calc_lits_weight
  // then halves the sum, and halves the result with the penalties again
  rewrite=(nlits==1 && (flags&QWB_EQUALITY) && (flags&QWB_POSITIVE) &&
           (g->use_equality) && (g->use_rewrite_terms_strat));
  if (rewrite) sum=(int)((float)sum/2.0);
  if (((g->use_max_ground_weight) && maxw) || (g->use_max_weight))
    bound=maxw+((nlen-1)*(g->cl_length_penalty))+((maxdepth-1)*(g->cl_depth_penalty));
  else
    bound=sum+((nlen-1)*(g->cl_length_penalty))+((maxdepth-1)*(g->cl_depth_penalty));
  if (rewrite) bound=(int)((float)bound/2.0);
  // the queue block from the parents, and the queue limit
  qstart=wr_qwl_block_from_parents(g,h1,h2);
  return wr_qwl_drop_weight(g,qstart,bound,nlen);
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
  /* 
  printf("\n before pick queue[0] %ld\n",queue[0]);
  printf("\n before pick queue[1] %ld\n",queue[1]);
  printf("\n before pick queue[2] %ld\n",queue[2]);
  */
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
  /*
  printf("\n after pick queue[0] %ld\n",queue[0]);
  printf("\n after pick queue[1] %ld\n",queue[1]);
  printf("\n after pick queue[2] %ld\n",queue[2]);
  */
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
  wr_printf("\n!!!! wr_pick_from_clpick_queues starts with queuenr %d \n",(g->next_pick_given_queue_block_nr));
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
    wr_printf("\n  queue loop iteration starts loopflag %d queuenr %d first_queuenr %d\n",loopflag,queuenr,first_queuenr);
#endif             
    if (loopflag && queuenr==first_queuenr) {
      // full loop done, failed to find any clauses to pick
#ifdef QPICKDEBUG
      wr_printf("\n  full queue loop done, no clauses found \n");
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
    wr_printf("\n  no cl found for queuenr %d \n",queuenr);
#endif     
    
    // here we did not get a clause, need to try next until full cycle
    queuenr++;
    if (queuenr>=NROF_CLPICK_QUEUES || blockstart>=limit) {
      // pushed over the edge, loop back
#ifdef QPICKDEBUG
      wr_printf("\n  queuenr to loop back \n");
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
  wr_printf("\n\n --- pick cl with queuenr %d limit %d NROF_CLPICK_QUEUES %d ---\n",
         queuenr,(int)(queues[0]),NROF_CLPICK_QUEUES); 
  //printf("\n queues are: \n");       
  //wr_print_clpick_queues(g,rotp(g,g->clpick_queues));       
  //printf("\n starting to pick: \n");  
#endif  

  
  i=1+(queuenr*CLPICK_QUEUE_BLOCKGINTS); // i is block start in queues vector

#ifdef QPICKDEBUG    
    
  wr_printf("\n   --  queue nr %d at pos %d --\n",queuenr,i);
    
  wr_printf("\n   CLPICK_QUEUE_GIVEN_POS: %d",(int)(queues[i+CLPICK_QUEUE_GIVEN_POS]));
  wr_printf("\n   CLPICK_PRIORQUEUE_RATIO: %d",(int)(queues[i+CLPICK_PRIORQUEUE_RATIO]));
  wr_printf("\n   CLPICK_PRIORQUEUE_RATIO_COUNTER: %d",(int)(queues[i+CLPICK_PRIORQUEUE_RATIO_COUNTER]));
  wr_printf("\n   CLPICK_THIS_QUEUE_RATIO: %d",(int)(queues[i+CLPICK_THIS_QUEUE_RATIO]));
  wr_printf("\n   CLPICK_THIS_QUEUE_RATIO_COUNTER: %d",(int)(queues[i+CLPICK_THIS_QUEUE_RATIO_COUNTER]));
  
#endif

  // check if this queue is over ratio
  /*
  if (queues[i+CLPICK_THIS_QUEUE_RATIO_COUNTER] >= queues[i+CLPICK_THIS_QUEUE_RATIO]) {
    // over ratio!
    wr_printf("\nthis whole queue block is over ratio setting to 0\n");
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
        wr_printf("\n\n  picking from queue nr %d priority queue the clause: ",queuenr);
        wr_print_clause(g,cl);
        wr_printf("\n");
        //wr_print_priorqueue(g,queue); 
      }  
#endif
      if (cl!=NULL) {
        (queues[i+CLPICK_PRIORQUEUE_RATIO_COUNTER])++;
        (queues[i+CLPICK_THIS_QUEUE_RATIO_COUNTER])++;
#ifdef QPICKEDDEBUG
        wr_printf("\npicked cl from queue nr %d priority queue",queuenr);
#endif        
#ifdef QPICKDEBUG                   
        wr_printf("\n  priority queue with max priority %d, max used priority %d \n",
                (int)(queue[PRIORQUEUE_ARR_LEN_POS]-2),
                (int)(queue[PRIORQUEUE_MAX_USED_PRIOR_POS]));  
#endif
          
        wr_cl_mark_given(g,cl);
        if (!wg_in_attached_kb(cl)) { // kb clause metas were stored at build time
          wr_calc_clause_meta(g,cl,given_cl_metablock);
        }
        return cl;   
      }     
    }    
  } else {
    queues[i+CLPICK_PRIORQUEUE_RATIO_COUNTER]=0;
#ifdef QPICKDEBUG
    wr_printf("\n  not from priority queue; setting ratio_counter to 0\n");
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
      wr_printf("\n  simplequeue max len %d count %d, elems:\n",(int)(simplequeue[0]),(int)(simplequeue[1]));
      /*
      wr_printf("\n  simplequeue max len %d count %d, elems:\n",(int)(simplequeue[0]),(int)(simplequeue[1]));
      for(j=2;j<simplequeue[0] && j<simplequeue[1];j++) {
        wr_printf("\n %d: ",j);
        wr_print_clause(g,simplequeue[j]);        
      }          
      wr_printf("\n  next %d simplequeue_given_pos %d\n",next,simplequeue_given_pos);
      */
#endif
      if (next>2 && next>simplequeue_given_pos) {
        // there is a clause to take (next is initially 2 when no clauses have been added)
        cl=(gptr)(simplequeue[simplequeue_given_pos]);           
        ++(queues[i+CLPICK_QUEUE_GIVEN_POS]); // next time take the next clause
        (queues[i+CLPICK_THIS_QUEUE_RATIO_COUNTER])++;
        if (cl!=NULL && !wr_cl_ismarked_given(g,cl)) {
#ifdef QPICKEDDEBUG
          wr_printf("\npicked cl from queue nr %d simple queue",queuenr);
#endif           
  #ifdef QPICKDEBUG       
          wr_printf("\n\n  picking from queue nr %d simple queue the clause: ",queuenr);
          wr_print_clause(g,cl); 
          wr_printf("\n");
          /*
          wr_printf("\n  new next %d queues[i+CLPICK_QUEUE_GIVEN_POS] \n",next,queues[i+CLPICK_QUEUE_GIVEN_POS]);
          */
  #endif            
          wr_cl_mark_given(g,cl);
          if (!wg_in_attached_kb(cl)) { // kb clause metas were stored at build time
            wr_calc_clause_meta(g,cl,given_cl_metablock);
          }
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
  wr_printf("\n checking if marked given:");
  wr_print_clause(g,cl);
  wr_printf("\n");
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
    wr_printf("\n WARNING: history empty, cannot mark given\n");
  }
  /*
  wr_printf("\n");
  wr_printf("\n is marked given?: %d\n",wr_cl_ismarked_given(g,cl));
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
    wr_printf("\ni %d queues[i] %d ",i,queues[i]);
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
  wr_vec_free(g,queues); // free the top-level queues vector itself
}


/*
  print all queues
*/


void wr_print_clpick_queues(glb* g, gint* queues) {
  int i,j;
  gint limit;
  gint simplequeue_offset;
  gptr simplequeue;

  wr_printf("\n===== g->clpick_queues content =====\n");
  if (!queues) {
    wr_printf("\nqueues is NULL\n");
    return;    
  }   
  
  wr_printf("\ng->clpick_given: %d",(int)(g->clpick_given));

  limit=queues[0];
  wr_printf("\ng->clpick_given vec len (el 0): %d",(int)limit);
  wr_printf("\nNROF_CLPICK_QUEUES: %d\n",NROF_CLPICK_QUEUES);
  // do big block-size steps over array
  for(i=1; i<limit && i<(NROF_CLPICK_QUEUES*CLPICK_QUEUE_BLOCKGINTS); i=i+CLPICK_QUEUE_BLOCKGINTS) {
    wr_printf("\n-- queue nr %d ",i);
    if (i==GOAL_ASSUMPTIONS_QUEUE_POS) { 
      wr_printf("GOAL_ASSUMPTIONS");
    } else if (i==GOAL_QUEUE_POS) { 
      wr_printf("GOAL");
    } else if (i==ASSUMPTIONS_QUEUE_POS) {
      wr_printf("ASSUMPTIONS");
    } else if (i==AXIOMS_QUEUE_POS) {
      wr_printf("AXIOMS");
    }
    wr_printf(" --\n");
    wr_printf("\nCLPICK_PRIORQUEUE_RATIO: %d",(int)(queues[i+CLPICK_PRIORQUEUE_RATIO]));
    wr_printf("\nCLPICK_PRIORQUEUE_RATIO_COUNTER: %d",(int)(queues[i+CLPICK_PRIORQUEUE_RATIO_COUNTER]));
    wr_printf("\nCLPICK_THIS_QUEUE_RATIO: %d",(int)(queues[i+CLPICK_THIS_QUEUE_RATIO]));
    wr_printf("\nCLPICK_THIS_QUEUE_RATIO_COUNTER: %d",(int)(queues[i+CLPICK_THIS_QUEUE_RATIO_COUNTER]));
    if (queues[i+CLPICK_QUEUE_POS]!=0) {

      simplequeue_offset=queues[i+CLPICK_QUEUE_POS];
      simplequeue=rotp(g,simplequeue_offset);
      //printf("\nlen of queue: %d",(int)(rotp(g,queues[i+CLPICK_QUEUE_POS])[0]));
      if ((int)(simplequeue[1])<=2) {
        wr_printf("\n\nsimplequeue empty");
      } else {  
        wr_printf("\n\nsimplequeue max len %d count %d, elems:\n",(int)(simplequeue[0]),(int)(simplequeue[1]));
        for(j=2;j<simplequeue[0] && j<simplequeue[1];j++) {
          wr_printf("\n %d: ",j);
          wr_print_clause(g,(gptr)(simplequeue[j]));        
        } 
      }  
    } else {
      wr_printf("\nsimplequeue is 0");  
    }
    if (queues[i+CLPICK_PRIORQUEUE_POS]!=0) {
      wr_print_priorqueue(g,rotp(g,queues[i+CLPICK_PRIORQUEUE_POS]));
    } else {
      wr_printf("\npriorqueue is 0");    
    }  
  }    
   wr_printf("\n===== g->clpick_queues content ends =====\n");
}



void wr_print_clpick_queue_sizes(glb* g, gint* queues) {
  int i;
  gint limit;
  gint simplequeue_offset;
  gptr simplequeue;

  wr_printf("\n*** g->clpick_queues sizes ****\n");
  if (!queues) {
    wr_printf("\nqueues is NULL\n");
    return;    
  }  
  
  wr_printf("\ng->clpick_given: %d",(int)(g->clpick_given));

  limit=queues[0];
  wr_printf("\ng->clpick_given vec len (el 0): %d",(int)limit);
  wr_printf("\nNROF_CLPICK_QUEUES: %d",NROF_CLPICK_QUEUES);
  // do big block-size steps over array
  for(i=1; i<limit && i<(NROF_CLPICK_QUEUES*CLPICK_QUEUE_BLOCKGINTS); i=i+CLPICK_QUEUE_BLOCKGINTS) {
    wr_printf("\n-- queue nr %d ---\n",i);
    wr_printf("\nCLPICK_QUEUE_GIVEN_POS: %d",(int)(queues[i+CLPICK_QUEUE_GIVEN_POS]));
    wr_printf("\nCLPICK_PRIORQUEUE_RATIO: %d",(int)(queues[i+CLPICK_PRIORQUEUE_RATIO]));
    wr_printf("\nCLPICK_PRIORQUEUE_RATIO_COUNTER: %d",(int)(queues[i+CLPICK_PRIORQUEUE_RATIO_COUNTER]));
    wr_printf("\nCLPICK_THIS_QUEUE_RATIO: %d",(int)(queues[i+CLPICK_THIS_QUEUE_RATIO]));
    wr_printf("\nCLPICK_THIS_QUEUE_RATIO_COUNTER: %d",(int)(queues[i+CLPICK_THIS_QUEUE_RATIO_COUNTER]));

    if (queues[i+CLPICK_QUEUE_POS]!=0) {

      simplequeue_offset=queues[i+CLPICK_QUEUE_POS];
      simplequeue=rotp(g,simplequeue_offset);
      //printf("\nlen of queue: %d",(int)(rotp(g,queues[i+CLPICK_QUEUE_POS])[0])); 
      wr_printf("\nsimplequeue max len %d count %d\n",(int)(simplequeue[0]),(int)(simplequeue[1]));

      //for(j=2;j<simplequeue[0] && j<simplequeue[1];j++) {
      //  wr_printf("\n %d: ",j);
      //  wr_print_clause(g,(gptr)(simplequeue[j]));        
      //} 

    } else {
      wr_printf("\nsimplequeue is 0");  
    }
    if (queues[i+CLPICK_PRIORQUEUE_POS]!=0) {
      wr_print_priorqueue_sizes(g,rotp(g,queues[i+CLPICK_PRIORQUEUE_POS]));
    } else {
      wr_printf("\npriorqueue is 0");    
    }  
  }    
   wr_printf("\n* g->clpick_queues content ends *\n");
}


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
        wr_printf("\n wr_pick_priorqueue found cl using %d and %d and %d\n",
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
  if (!max_used_prior) {
    wr_printf("\npriority queue empty\n");
    return;
  }
  wr_printf("\n\npriority queue with max priority %d, max used priority %d: \n\n",
    (int)(queue[PRIORQUEUE_ARR_LEN_POS]-2),
    max_used_prior
  );  
  for(prior=0;prior<=max_used_prior;prior++) {  
    elstart=(prior+1)*PRIORQUEUE_NODE_GINT_NR; // each prior has a block of N gints    
    if (queue[elstart+PRIORQUEUE_ARR_OFFSET_POS]) {
      wr_printf("clauses with priority %d bucket size %d next free %d next pick %d\n",
        prior,
        (int)(queue[elstart+PRIORQUEUE_ARR_LEN_POS]),
        (int)(queue[elstart+PRIORQUEUE_NEXT_FREE_INDEX_POS]),
        (int)(queue[elstart+PRIORQUEUE_NEXT_PICK_INDEX_POS])
      );
      bucket=rotp(g,queue[elstart+PRIORQUEUE_ARR_OFFSET_POS]);      
      for(i=0;i<queue[elstart+PRIORQUEUE_NEXT_FREE_INDEX_POS];i++) {
        cl=rotp(g,bucket[i]);
        if (i==queue[elstart+PRIORQUEUE_NEXT_PICK_INDEX_POS]) {
          wr_printf("%d [pick] ",i);
        } else {
          wr_printf("%d: ",i);
        }
        wr_print_clause(g,cl);
        wr_printf("\n");
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
  wr_printf("\n");
  wr_printf("priority queue with max priority %d, max used priority %d: \n",
    (int)(queue[PRIORQUEUE_ARR_LEN_POS]-2),
    max_used_prior
  );  
  for(prior=0;prior<=max_used_prior;prior++) {  
    elstart=(prior+1)*PRIORQUEUE_NODE_GINT_NR; // each prior has a block of N gints    
    if (queue[elstart+PRIORQUEUE_ARR_OFFSET_POS]) {
      wr_printf("clauses with priority %d bucket size %d next free %d next pick %d\n",
        prior,
        (int)(queue[elstart+PRIORQUEUE_ARR_LEN_POS]),
        (int)(queue[elstart+PRIORQUEUE_NEXT_FREE_INDEX_POS]),
        (int)(queue[elstart+PRIORQUEUE_NEXT_PICK_INDEX_POS])
      );
      
      bucket=rotp(g,queue[elstart+PRIORQUEUE_ARR_OFFSET_POS]);      
      for(i=0;i<queue[elstart+PRIORQUEUE_NEXT_FREE_INDEX_POS] && i<2;i++) {
        cl=rotp(g,bucket[i]);
        if (i==queue[elstart+PRIORQUEUE_NEXT_PICK_INDEX_POS]) {
          wr_printf("%d [pick] ",i);
        } else {
          wr_printf("%d: ",i);
        }
        wr_print_clause(g,cl);
        wr_printf("\n");
      }
      
    }
  }
}


#ifdef __cplusplus
}
#endif



