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

 /** @file prop.c
 * Propositional reasoning functions.  
 */

/* ====== Includes =============== */


#ifdef __cplusplus
extern "C" {
#endif

#include "rincludes.h"  


/* ====== Private defs =========== */

#define PROP_CL_VARSTART RECORD_HEADER_GINTS+CLAUSE_EXTRAHEADERLEN

//#define DEBUG


/* ====== Private headers ======== */

/* ====== Functions ============== */


/*
  
  create all (one or more) prop instances (integer vecs) of cl 
  and store them in

  return:
     -1 : error: cannot allocate
     0: all ok
     1: was discarded as tautology or subsumed
     2: contradiction found
 */

int wr_cl_create_propinst(glb* g, gptr cl) {
  int len;
  gint atom;
  gint meta;
  void* db;  
  int i,j,negflag,storepos,length,noadd, cuts, sign;
  gint propvar, abspropvar, history, value, cutter;
  gptr rec, store, tmp_build_buffer;
  cvec varvals, vecptr, tmp;
    
  if (!(g->instgen_strat) && !(g->propgen_strat)) return 0;
  db=g->db;   
  UNUSED(db);
  
  if ((g->print_flag) && (g->print_level_flag)>=40) {
    printf("\ncreating propinst for\n");
    wr_print_clause(g,cl);
    printf("\n");
  }  
  //printf("\n sin and in max_const_ucount: %ld %ld \n ",(g->sin_max_const_ucount),(g->in_max_const_ucount));
  if (g->in_max_const_ucount) {
    (g->prop_constant)=(g->in_max_occ_const);   
  } else {
    (g->prop_constant)=encode_smallint(1);
  }
  (g->use_prop_constant)=1;
  
  if (wg_rec_is_fact_clause(db,cl)) len=1;
  else len=wg_count_clause_atoms(db,cl);

  /* 
  printf("\nwr_cl_create_propinst:\n");
  wr_print_clause(g,cl);
  printf("\nlen %d wg_rec_is_rule_clause %d\n",len,wg_rec_is_rule_clause(db,cl));
  printf("\n constant: ");
  wr_print_term(g,(g->prop_constant));
  printf("\n");
  */
  
  tmp_build_buffer=(g->build_buffer);
  (g->build_buffer)=(g->queue_termbuf);
  varvals=rotp(g,g->prop_varvals);
  (g->cut_clvec)[1]=(gint)NULL;
  cuts=0;
  storepos=1;
  for(i=0; i<len; i++) {
    if (len==1 && !(wg_rec_is_rule_clause(db,cl))) {
      atom=rpto(g,cl);
      negflag=0;
    } else {     
      meta=wg_get_rule_clause_atom_meta(db,cl,i);
      atom=wg_get_rule_clause_atom(db,cl,i);
      if (wg_atom_meta_is_neg(db,meta)) negflag=1;
      else negflag=0;     
    }     
    propvar=wr_make_get_propvar(g,atom);    
    if (!propvar) {
      // fail to create
      (g->use_prop_constant)=0;
      (g->build_buffer)=tmp_build_buffer;
      return -1;
    }
    abspropvar=propvar;
    value=varvals[(int)propvar];    
    if (negflag) { sign=0-1; propvar=0-propvar; }
    else { sign=1; }
    //printf("\n for atom in pos %d created propvar %ld\n",i,propvar);
    // check value
    if (value) {
      // variable already assigned
      if (value==sign) {
        // already true
        (g->use_prop_constant)=0;
        (g->build_buffer)=tmp_build_buffer;
        return 1;
      } else {
        // contradiction, cut off      
        cuts++;      
        vecptr=rotp(g,g->prop_varval_clauses);
        cutter=vecptr[abspropvar];
        if ((g->cut_clvec)[0]>cuts+1) {
          (g->cut_clvec)[cuts]=(gint)(rotp(g,cutter));
          (g->cut_clvec)[cuts+1]=(gint)NULL;
        }  
        continue;      
      }
    }  
    // check if already present or tautology 
    store=(g->tmp_litinf_vec);  
    noadd=0;  
    for(j=1;j<storepos;j++) {
      //printf("\n propvar %ld storepos %d j %d store[j] %ld equal %d\n",propvar,storepos,j,store[j],(store[j]==propvar));
      if (store[j]==propvar) {
        // already present
        noadd=1;
        break;
      } else if (store[j]==(0-propvar)) {
        // tautology
        (g->use_prop_constant)=0;
        (g->build_buffer)=tmp_build_buffer;
        return 1;
      }
    }
    if (noadd) continue;
    // store
    (g->tmp_litinf_vec)=wr_vec_store(g,g->tmp_litinf_vec,storepos,propvar);
    if ((g->tmp_litinf_vec)==NULL) {
      (g->use_prop_constant)=0;
      (g->build_buffer)=tmp_build_buffer;
      return -1;
    }     
    storepos++;  
  } 
  (g->use_prop_constant)=0;
  // create a new record
  length=storepos-1;
  if (!length) {
    //printf("\nfound prop contradiction\n");
    //wr_print_term_otter(g,(g->prop_constant),(g->print_clause_detaillevel));
    //printf("\n");
    g->proof_found=1;
    g->proof_history=wr_build_propinst_history(g,cl,(g->prop_constant),g->cut_clvec);
    wr_register_answer(g,NULL,g->proof_history);
    (g->build_buffer)=tmp_build_buffer;
    return 2;    
  }  
  rec=wr_create_raw_record(g,CLAUSE_EXTRAHEADERLEN+(length*PROP_LIT_WIDTH),
                           RECORD_META_PROP_CLAUSE,g->queue_termbuf);
  if (rec==NULL) {
    (g->build_buffer)=tmp_build_buffer;
    return -1; // error   
  }                         
  store=(g->tmp_litinf_vec);                           
  for(i=1;i<storepos;i++) {
    wg_set_prop_clause_atom(db,rec,i-1,store[i]);    
  }  
  history=wr_build_propinst_history(g,cl,(g->prop_constant),g->cut_clvec);  
  wr_set_history(g,rec,history);
  
  if ((g->print_flag) && (g->print_level_flag)>=40) {
    printf("\ncreated prop clause:\n");
    wr_print_clause(g,rec);
    //printf("\nhistory:\n");
    //wg_print_record(g->db,rotp(g,history));
    printf("\n");
  }    

  // store propclause
  vecptr=rotp(g,g->prop_clauses);
  tmp=wr_cvec_push(g,vecptr,rpto(g,rec));
  if (tmp==NULL) return -1;
  if (vecptr!=tmp) (g->prop_clauses)=rpto(g,tmp);
  if (storepos==2) {
    // unit clause, store value and corresponding clause
    vecptr=rotp(g,g->prop_varvals);
    value=store[1];
    if (value<0) { propvar=0-value; vecptr[propvar]=0-1; }
    else  { propvar=value; vecptr[propvar]=1; }     
  
    vecptr=rotp(g,g->prop_varval_clauses);
    tmp=wr_cvec_store(g,vecptr,propvar,rpto(g,rec));
    if (tmp==NULL) {
      (g->build_buffer)=tmp_build_buffer;  
      return -1;
    }  
    if (vecptr!=tmp) (g->prop_varval_clauses)=rpto(g,tmp);
  }
  ++(g->stat_prop_inst_derived_cl);
  // normal clause
  (g->build_buffer)=tmp_build_buffer;
  return 0;
}

gint wr_make_get_propvar(glb* g, gint atom) {
  gint hash; 
  gint propvar;
  gint gconst;
  gptr atomptr;
  gptr tmp, vecptr;
  
  atomptr=rotp(g,atom);
  gconst=(g->prop_constant);
  hash=wr_lit_hash(g,atom);  
  /* 
  printf("\n wr_make_get_propvar atom and gconst and hash:\n");
  wr_print_term(g,atom);
  printf(" ");
  wr_print_term(g,gconst);
  printf(" %ld \n",hash);
  */
  propvar=wr_find_prop_termhash(g,rotp(g,g->prop_hash_atoms),atomptr,hash,gconst);  
  if (propvar) {
    //printf("\n !!! found existing propvar %ld\n",propvar);
    return propvar;
  } else {
    propvar=(g->prop_counter);
    (g->prop_counter)++;
    tmp=wr_push_prop_termhash(g,rotp(g,g->prop_hash_atoms),hash,atomptr,gconst,propvar); 
    if (tmp==NULL) return 0;    
    //wr_print_prop_termhash(g,rotp(g,g->prop_hash_atoms));
    // set value initially to 0
    vecptr=rotp(g,g->prop_varvals);
    tmp=wr_cvec_store(g,vecptr,propvar,0);
    if (tmp==NULL) return 0;
    if (vecptr!=tmp) (g->prop_varvals)=rpto(g,tmp); 
    // set term/const pair
    vecptr=rotp(g,g->prop_groundings);
    tmp=wr_cvec_store(g,vecptr,(propvar*2)+1,gconst);
    if (tmp==NULL) return 0;
    wr_cvec_store(g,vecptr,(propvar*2),atom); // no need to check if succeeds
    if (vecptr!=tmp) (g->prop_groundings)=rpto(g,tmp);   
    // set atom/term pair
    return propvar;
  }
}


/* =========== clauses for prop ============== */


void wr_free_prop_clauses(glb* g, gptr arr) {
  //int i;

  if (arr==NULL) return;
  /* 
  for(i=1;i<arr[0] && i<arr[1];i++) {
    //if (arr[i]) wr_free(g,rotp(g,(arr[i])));
  }
  */
  wr_free(g,arr);
}


/* ===== hash for prop ========= */


/* =====================================================

  prop_hash_atoms: for storing prop atoms and their pvar numbers
  
  hashes are 0...maxhash
  hasharr is vec 0...maxhash+1 (len is maxhash+2) with hasrharr[0]==len
  hashkey k is stored as hasharr[k+1]
   
  say, hashes are 0...2 then maxhash=2 and vec has 4 els and vec[0]=4

  and N els, each el an offset to cvec where: 
    arr is a  cvec (i.e. el 0 length and el 1 next free)  
    els are pairs of term,clause: ie in a cvec we have:
     0: cvec len
     1: cvec next free

     2: terma
     3: terma_groundconst
     3: pvara

     4: termb
     5: termb_groundconst
     6: pvarb
     ...
    
    if no more space to add a new el, array is realloced

element equality:
  if gconst-s are equal,terms must be equal modulo var names
  if gconst-s are different, terms must be must be compared with appropriate gconst-s for each

====================================================== */


/*

store a prop atom in a termhash

*/

gint* wr_push_prop_termhash(glb* g, gint* hasharr, int hash, gptr term, gint gconst, gint pvar) {
  
  return wr_push_offset_prop_termhash(g,hasharr,hash,term,gconst,pvar);
  
}


/*

store a term in termhash using only offsets

*/

gint* wr_push_offset_prop_termhash(glb* g, gint* hasharr, int hash, gptr term, gint gconst, gint pvar) {
  cvec nvec;
  gint bucket,nbucket;
  int arrsize;
  //gint oterm;

  (g->stat_lit_hash_added)++;
  // negative hashes not ok: make positive
  if (hash<0) hash=0-hash;
  // too big hashes are recalculated
  if (hash+1 >= hasharr[0]) {
    hash=hash % (hasharr[0]-1);
  }
  bucket=hasharr[hash+1];  
  if (!bucket) {
    // el is empty: create a new cvec
    arrsize=TERMHASH_INITIAL_BUCKETSIZE;
    nvec=wr_cvec_new(g,arrsize);
    if (nvec==NULL) {
      (g->alloc_err)=1;    
      wr_alloc_err2int(g,"Cannot allocate memory for a termhash bucket with length",arrsize);
      return NULL;
    }
    bucket=rpto(g,nvec);
    hasharr[hash+1]=bucket;
  }
  // store to bucket: both term, gconst and pvar
  /* 
  oterm=rpto(g,term);
  printf("\n adding oterm %ld: \n",oterm);
  wr_print_term(g,oterm);
  printf("\n");
  */
  nvec=wr_cvec_push(g,rotp(g,bucket),rpto(g,term));
  if (nvec==NULL) {
    (g->alloc_err)=1;    
    wr_alloc_err2int(g,"Cannot realloc a termhash bucket with old length",(rotp(g,bucket))[0]);
    return NULL;
  }
  nvec=wr_cvec_push(g,nvec,gconst);
  if (nvec==NULL) {
    (g->alloc_err)=1;    
    wr_alloc_err2int(g,"Cannot realloc a termhash bucket with old length",(rotp(g,bucket))[0]);
    return NULL;
  }
  nvec=wr_cvec_push(g,nvec,pvar);
  if (nvec==NULL) {
    (g->alloc_err)=1;    
    wr_alloc_err2int(g,"Cannot realloc a termhash bucket with old length",(rotp(g,bucket))[0]);
    return NULL;
  }
  nbucket=rpto(g,nvec);
  if (bucket!=nbucket) hasharr[hash+1]=nbucket;
  return hasharr;
}


gint wr_find_prop_termhash(glb* g, gint* hasharr, gptr term, int hash, gint gconst) {

  return wr_find_offset_prop_termhash(g,hasharr,term,hash,gconst);
}


gint wr_find_offset_prop_termhash(glb* g, gint* hasharr, gptr term, int hash, gint gconst) {
  int j;  
  cvec bucket_asp;
  gint bucket;
  gint oterm; // offset of term

  /* 
  wr_printf("\nfind from termhash with length %d and hash %d the term: \n",hasharr[0],hash);
  wr_print_term(g,rpto(g,term));
  wr_printf("\n");
  */
 
  // negative hashes not ok: make positive
  if (hash<0) hash=0-hash;
  // too big hashes are recalculated
  if (hash+1 >= hasharr[0]) {
    hash=hash % (hasharr[0]-1);
  }

  bucket=hasharr[hash+1];  
  if (!bucket) {
    //printf("\n no bucket found for hash\n");
    (g->stat_prop_hash_match_miss)++;
    return 0;
  }  
  oterm=rpto(g,term);
  //printf("bucket for hash %d size %d next free %d\n",hash,bucket[0],bucket[1]);
  bucket_asp=rotp(g,bucket);
  for(j=2;j<bucket_asp[0] && j<bucket_asp[1]; j=j+3) {
    
    /*
    wr_printf("j; %ld, nr corr to j: %d ",j,(j-2)/3);
    //wr_print_term(g,rpto(g,bucket[j]));
    printf("\n bucket_asp[j] %ld\n",bucket_asp[j]);
    wr_print_term(g,bucket_asp[j]);
    wr_printf(" grounder ");
    wr_print_term(g,bucket_asp[j+1]);
    wr_printf(" propvar %ld\n",bucket_asp[j+2]);
    */

    if (wr_equal_grounded_term(g,oterm,bucket_asp[j],gconst,bucket_asp[j+1],1)) {
      //printf("equal term found in  wr_find_termhash !\n");
      //(g->stat_lit_hash_match_found)++;
      return bucket_asp[j+2];
    }
  }
  (g->stat_prop_hash_match_miss)++;
  return 0;
}


/*

free a termhash

*/

void wr_free_prop_termhash(glb* g, gint* hasharr) {

  wr_free_offset_prop_termhash(g,hasharr);
}

void wr_free_offset_prop_termhash(glb* g, gint* hasharr) {
  int i;
  for(i=1;i<hasharr[0];i++) {
    if (hasharr[i]) wr_free(g,rotp(g,(hasharr[i])));
  }
  wr_free(g,hasharr);
}

/*

print a termhash

*/

void wr_print_prop_termhash(glb* g, gint* hasharr) {

  wr_print_offset_prop_termhash(g,hasharr);
}


void wr_print_offset_prop_termhash(glb* g, gint* hasharr) {
  int i,j;
  cvec bucket;
 
  //printf("\nhashvec len %ld ptr %lx and els:\n",hasharr[0],(unsigned long int)hasharr);  
  wr_printf("\nhashvec len %ld els:\n",hasharr[0]);  
  for(i=1;i<hasharr[0];i++) {    
    if (hasharr[i]) {
      bucket=rotp(g,(hasharr[i]));
      //printf("\nhashslot i %d node %ld size %ld next free %ld\n",
      //        i,hasharr[i],bucket[0],bucket[1]);
      wr_printf("\nhashslot i %d size %ld next free %ld\n",i,bucket[0],bucket[1]);        
      if (1) {
        for(j=2;j<bucket[0] && j<bucket[1]; j=j+3) {
          wr_printf("term ");
          wr_print_term(g,bucket[j]);
          //printf(" path %d in cl ",0);
          //CP1
          //printf("\nj %d bucket[j+1] %ld \n",j,bucket[j+1]);
          //CP2          
          wr_printf(" grounder ");
          wr_print_term(g,bucket[j+1]);          
          wr_printf(" prop var %ld \n",bucket[j+2]);       
        }
      }  
    }
  }
}


/* ================ clause hash ============= */



/*

store a prop atom in a clausehash

*/

gint* wr_push_prop_clausehash(glb* g, gint* hasharr, int hash, gptr term) {
  gint* tmp;
  /* 
  printf("\nbefore really doing wr_push_prop_clausehash\n");
  wr_print_prop_clausehash(g,hasharr);  
  printf("\nabout to call wr_push_prop_clausehash with clause\n");
  wr_print_clause(g,term);
  printf("\n");
  */
  tmp=wr_push_offset_prop_clausehash(g,hasharr,hash,term);
  
  //printf("\nafter really doing wr_push_prop_clausehash\n");  
  //wr_print_prop_clausehash(g,hasharr);  
  
  return tmp;  
}


/*

store a term in clausehash using only offsets

*/

gint* wr_push_offset_prop_clausehash(glb* g, gint* hasharr, int hash, gptr term) {
  cvec nvec;
  gint bucket,nbucket;
  int arrsize;
  //gint oterm;

  //(g->stat_lit_hash_added)++;
  // negative hashes not ok: make positive
  if (hash<0) hash=0-hash;
  // too big hashes are recalculated
  if (hash+1 >= hasharr[0]) {
    hash=hash % (hasharr[0]-1);
  }
  bucket=hasharr[hash+1];  
  if (!bucket) {
    // el is empty: create a new cvec
    arrsize=TERMHASH_INITIAL_BUCKETSIZE;
    nvec=wr_cvec_new(g,arrsize);
    if (nvec==NULL) {
      (g->alloc_err)=1;    
      wr_alloc_err2int(g,"Cannot allocate memory for a clausehash bucket with length",arrsize);
      return NULL;
    }
    bucket=rpto(g,nvec);
    hasharr[hash+1]=bucket;
  }
  // store to bucket: both term, gconst and pvar
  /* 
  oterm=rpto(g,term);
  printf("\n adding oterm %ld: \n",oterm);
  wr_print_term(g,oterm);
  printf("\n");
  */
  //CP0  
  //wr_print_prop_clausehash(g,hasharr);  
  nvec=wr_cvec_push(g,rotp(g,bucket),rpto(g,term));  
  if (nvec==NULL) {
    (g->alloc_err)=1;    
    wr_alloc_err2int(g,"Cannot realloc a clausehash bucket with old length",(rotp(g,bucket))[0]);
    return NULL;
  }
  nbucket=rpto(g,nvec);
  if (bucket!=nbucket) { hasharr[hash+1]=nbucket; } //CP4; }
  //CP2  
  //wr_print_prop_clausehash(g,hasharr); 
  return hasharr;
}

int wr_find_prop_clausehash(glb* g, gint* hasharr, gptr rptr, int rpos, int hash) {

  return wr_find_offset_prop_clausehash(g,hasharr,rptr,rpos,hash);
}


int wr_find_offset_prop_clausehash(glb* g, gint* hasharr, gptr rptr, int rpos, int hash) {
  int j;  
  cvec bucket_asp;
  gint bucket;
  
#ifdef DEBUG
  wr_printf("\nfind from clausehash with length %d and hash %d the half-built clause: \n",hasharr[0],hash);
  wr_print_halfbuilt_clause(g,rptr,rpos);
  printf("\n");
#endif
 
  // negative hashes not ok: make positive
  if (hash<0) hash=0-hash;
  // too big hashes are recalculated
  if (hash+1 >= hasharr[0]) {
    hash=hash % (hasharr[0]-1);
  }

  bucket=hasharr[hash+1];  
  if (!bucket) {
    //printf("\n no bucket found for hash\n");
    //(g->stat_prop_hash_match_miss)++;
    return 0;
  }    
 
  bucket_asp=rotp(g,bucket);
  //printf("bucket_asp for hash %d size %ld next free %ld\n",hash,bucket_asp[0],bucket_asp[1]);
  for(j=2;j<bucket_asp[0] && j<bucket_asp[1]; j++) {
    

    //if (0) { // (wr_equal_instgen_clause(g,term,rotp(g,bucket_asp[j]))) {
#ifdef DEBUG
    printf("\nchecking equality in clausehash bucket:\n");
    wr_print_clause(g,rotp(g,bucket_asp[j]));
    printf("\n");    
#endif      
    if (wr_equal_instgen_halfblt_queue_clause(g,rptr,rpos,rotp(g,bucket_asp[j]))) {
#ifdef DEBUG
      printf("\nequal term found in  wr_find_clausehash:\n");
      wr_print_clause(g,rotp(g,bucket_asp[j]));
      printf("\n");
#endif      
      //(g->stat_lit_hash_match_found)++;
      return 1;
    }
  }
  //(g->stat_prop_hash_match_miss)++;
  return 0;
}


/*

free a clausehash

*/

void wr_free_prop_clausehash(glb* g, gint* hasharr) {

  wr_free_offset_prop_clausehash(g,hasharr);
}

void wr_free_offset_prop_clausehash(glb* g, gint* hasharr) {
  int i;
  for(i=1;i<hasharr[0];i++) {
    if (hasharr[i]) wr_free(g,rotp(g,(hasharr[i])));
  }
  wr_free(g,hasharr);
}

/*

print a clausehash

*/

void wr_print_prop_clausehash(glb* g, gint* hasharr) {

  wr_print_offset_prop_clausehash(g,hasharr);
}


void wr_print_offset_prop_clausehash(glb* g, gint* hasharr) {
  int i,j;
  cvec bucket;
 
  //printf("\nhashvec len %ld ptr %lx and els:\n",hasharr[0],(unsigned long int)hasharr);  
  wr_printf("\nhashvec len %ld els:\n",hasharr[0]);  
  for(i=1;i<hasharr[0];i++) {    
    if (hasharr[i]) {
      bucket=rotp(g,(hasharr[i]));
      //printf("\nhashslot i %d node %ld size %ld next free %ld\n",
      //        i,hasharr[i],bucket[0],bucket[1]);
      wr_printf("\nhashslot i %d size %ld next free %ld\n",i,bucket[0],bucket[1]);        
      if (1) {
        for(j=2;j<bucket[0] && j<bucket[1]; j++) {
          //wr_printf("\nclause offs %ld ptr %ld ",(gint)bucket[j],rotp(g,bucket[j]));
          printf("\n");
          wr_print_clause(g,rotp(g,bucket[j]));              
        }
      }
      printf("\n------");  
    }
  }
}


/* === clause equality of a half-built clause with a queue clause in clause hash ====== */


int wr_equal_instgen_halfblt_queue_clause(glb* g, gptr rptr, int rpos, gptr queuecl) {
  void* db=g->db;
  int i,j,foundcount,foundflag,tmp;
  gint xatom,xatommeta;
  gint yatom,yatommeta; 
  int ylen;
  
#ifdef DEBUG
  /* 
  printf("\ncalling wr_equal_instgen_halfblt_queue_clause on rptr/rpos\n");
  wr_print_halfbuilt_clause(g,rptr,rpos);
  printf("\n and queuecl\n");
  wr_print_clause(g,queuecl);
  printf("\n");
  */
#endif  
  UNUSED(db);
  if (wg_rec_is_fact_clause(db,queuecl)) return 0;
  else ylen=wg_count_clause_atoms(db,queuecl);
  if (ylen<=1) return 0;
  if (rpos<ylen) return 0;
  foundcount=0;
  for(i=0;i<rpos;i++) { 
    tmp=i*LIT_WIDTH;
    xatommeta=rptr[tmp+LIT_META_POS]; 
    if (!xatommeta) continue;    
    xatom=rptr[tmp+LIT_ATOM_POS];    
    foundflag=0; 
    for(j=0;j<ylen;j++) {      
      yatom=wg_get_rule_clause_atom(db,queuecl,j);
      yatommeta=wg_get_rule_clause_atom_meta(db,queuecl,j);
#ifdef DEBUG
      /* 
      printf("\n i %d j %d\n",i,j);
      wr_print_term(g,xatom);
      printf("\n");
      wr_print_term(g,yatom);
      printf("\n");
      */
#endif
      if (wg_atom_meta_is_neg(db,xatommeta)!=(wg_atom_meta_is_neg(db,yatommeta))) {        
        //printf("i %d polarity failed\n",i);
        continue;
      }
      if (!wr_equal_mod_vars_term(g,xatom,yatom,1)) {
  #ifdef DEBUG  
        /* 
        printf("i %d atom equality failed on xatom and yatom\n",i);
        wr_print_term(g,xatom);
        printf("\n");
        wr_print_term(g,yatom);
        printf("\n");
        */
  #endif      
        continue;
      }      
      foundflag=1;
      break;
    }
    if (!foundflag) return 0;  
    else foundcount++;
  }
#ifdef DEBUG  
  //printf("\nrpos %d foundcount %d\n",rpos,foundcount);
#endif
  if (foundcount==ylen) return 1;
  return 0;  
}




int wr_equal_instgen_clauses(glb* g, gptr xcl, gptr ycl) {
  void* db=g->db;
  int i,j,foundcount,foundflag;
  gint xatom,xatommeta;
  gint yatom,yatommeta;  
  int xlen,ylen; 

#ifdef DEBUG
  /* 
  printf("\ncalling wr_equal_instgen_halfblt_queue_clause on rptr/rpos\n");
  wr_print_halfbuilt_clause(g,rptr,rpos);
  printf("\n and queuecl\n");
  wr_print_clause(g,queuecl);
  printf("\n");
  */
#endif  
  UNUSED(db);
  if (wg_rec_is_fact_clause(db,xcl)) return 0;
  if (wg_rec_is_fact_clause(db,ycl)) return 0;
  xlen=wg_count_clause_atoms(db,xcl);
  ylen=wg_count_clause_atoms(db,ycl);
  if (xlen!=ylen) return 0;
  foundcount=0;
  for(i=0;i<xlen;i++) { 
    xatom=wg_get_rule_clause_atom(db,xcl,i);
    xatommeta=wg_get_rule_clause_atom_meta(db,xcl,i);   
    foundflag=0; 
    for(j=0;j<ylen;j++) {      
      yatom=wg_get_rule_clause_atom(db,ycl,j);
      yatommeta=wg_get_rule_clause_atom_meta(db,ycl,j);
#ifdef DEBUG
      /* 
      printf("\n i %d j %d\n",i,j);
      wr_print_term(g,xatom);
      printf("\n");
      wr_print_term(g,yatom);
      printf("\n");
      */
#endif
      if (wg_atom_meta_is_neg(db,xatommeta)!=(wg_atom_meta_is_neg(db,yatommeta))) {        
        //printf("i %d polarity failed\n",i);
        continue;
      }
      if (!wr_equal_mod_vars_term(g,xatom,yatom,1)) {
  #ifdef DEBUG  
        /* 
        printf("i %d atom equality failed on xatom and yatom\n",i);
        wr_print_term(g,xatom);
        printf("\n");
        wr_print_term(g,yatom);
        printf("\n");
        */
  #endif      
        continue;
      }      
      foundflag=1;
      break;
    }
    if (!foundflag) return 0;  
    else foundcount++;
  }
#ifdef DEBUG  
  //printf("\nrpos %d foundcount %d\n",rpos,foundcount);
#endif
  if (foundcount==ylen) return 1;
  return 0;  
}

/* ============== term equality with a grounding term =================== */



/** Plain term equality check using g->unify_samelen and g->unify_maxuseterms
*
*  Metainfo is not filtered out. Must be exactly the same.  
*
*  NB! For a faster version use macro WR_EQUAL_TERM doing the same thing
*  and assuming the presence of the gint eqencx variable
*/

gint wr_equal_grounded_term(glb* g, gint x, gint y, gint xg, gint yg, int uniquestrflag) {  
  gptr db;
  gint encx,ency;
  gptr xptr,yptr;
  int xlen,ylen,uselen,i,ilimit; 

#ifdef DEBUG
  printf("wr_equal_grounded_term called with x %d and y %d\n",x,y);
  wr_print_term(g,x); printf("\n");
  wr_print_term(g,y); printf("\n");
  wr_print_term(g,xg); printf("\n");
  wr_print_term(g,yg); printf("\n");
#endif

  // first check if immediately same: return 1 if yes 
  if (x==y && xg==yg)  return 1; 
  // replace vars with grounders
  if (isvar(x)) x=xg;
  if (isvar(y)) y=yg;
  if (x==y && ((xg==yg) || !isdatarec(x))) return 1;  
  // handle immediate check cases: for these bit suffixes x is equal to y iff x==y   
  encx=(x&NORMALPTRMASK);
  if ((encx==LONGSTRBITS && uniquestrflag) || encx==SMALLINTBITS || encx==NORMALPTRMASK) return 0; 
  // immediate value: must be unequal since x==y checked before
  if ((!isptr(x) || !isptr(y))) return 0;
  // here both x and y are ptr types
  // quick type check: last two bits
  if (((x)&NONPTRBITS)!=((y)&NONPTRBITS)) return 0;  
  // if one is datarec, the other must be as well
  if (!isdatarec(x)) {
    if (isdatarec(y)) return 0;
    // neither x nor y are datarecs
    // need to check pointed values    
    if (wr_equal_ptr_primitives(g,x,y,uniquestrflag)) return 1;
    else return 0;          
  } else {
    if (!isdatarec(y)) return 0;
    // both x and y are datarecs 
    db=g->db;
    xptr=decode_record(db,x);
    yptr=decode_record(db,y);
    xlen=get_record_len(xptr);
    ylen=get_record_len(yptr);
    if (g->unify_samelen) {
      if (xlen!=ylen) return 0;
#ifdef USE_TERM_META                  
      encx=*(xptr+(RECORD_HEADER_GINTS+TERM_META_POS));
      ency=*(yptr+(RECORD_HEADER_GINTS+TERM_META_POS));  
      if (issmallint(encx) && issmallint(ency)) {        
        if (encx!=ency) {   
#ifdef DEBUG                         
          printf("\n!!! meta-inequal:\n");
          wr_print_term(g,x);
          printf("\n");
          wr_print_term(g,y);
          printf("\n");
#endif          
          return 0;
        }  
      }       
#endif      
      uselen=xlen;      
    } else {
      if (xlen<=ylen) uselen=xlen;
      else uselen=ylen;
    }     
    if (g->unify_maxuseterms) {
      if (((g->unify_maxuseterms)+(g->unify_firstuseterm))<uselen) 
        uselen=(g->unify_firstuseterm)+(g->unify_maxuseterms);
    }
    ilimit=RECORD_HEADER_GINTS+uselen;
    for(i=RECORD_HEADER_GINTS+(g->unify_firstuseterm); i<ilimit; i++) {
      encx=*(xptr+i);
      ency=*(yptr+i);
      if (!wr_equal_grounded_term(g,encx,ency,xg,yg,uniquestrflag)) return 0;
      //if (!WR_EQUAL_TERM(g,encx,ency,uniquestrflag)) return 0;
    }           
    return 1;        
  }        
}  



/*

 defined in unify.c

int wr_equal_ptr_primitives(glb* g, gint a, gint b, int uniquestrflag) {
  gint t1,t2;
  gint l1,l2,ol;
  char* s1;
  char* s2;
  
  //printf("equal_ptr_primitives called with %d and %d\n",a,b);
  switch(a&NORMALPTRMASK) {  // last 3 bits
    case FULLINTBITSV0:
    case FULLINTBITSV1:
      if (isfullint(b) && 
          (dbfetch(g->db,decode_fullint_offset(a))==dbfetch(g->db,decode_fullint_offset(b))) )
        return 1;
      else 
        return 0;
    case FULLDOUBLEBITS:
      if (isfulldouble(b) &&
          wg_decode_double(g->db,a)==wg_decode_double(g->db,b) )
        return 1;
      else 
        return 0;
    case SHORTSTRBITS:
      //printf("shortstrbits \n");      
      if (isshortstr(b) &&
          !memcmp((void*)(offsettoptr(g->db,decode_shortstr_offset(a))),
                  (void*)(offsettoptr(g->db,decode_shortstr_offset(b))), 
                  SHORTSTR_SIZE))
        return 1; 
      else
        return 0;
    case LONGSTRBITS:
      if (uniquestrflag) {
        if (a==b) 
          return 1;
        else
          return 0;          
      } else {       
        t1=wg_get_encoded_type(g->db,a);
        t2=wg_get_encoded_type(g->db,b);
        if (t1!=t2) return 0;
        l1=wg_decode_unistr_lang_len(g->db,a,t1);
        l2=wg_decode_unistr_lang_len(g->db,b,t2);
        if (11!=l2) return 0;
        ol=l1;
        l1=wg_decode_unistr_len(g->db,a,t1);
        l2=wg_decode_unistr_len(g->db,b,t2);        
        if (11!=l2) return 0;
        s1=wg_decode_unistr_lang(g->db,a,t1);
        s2=wg_decode_unistr_lang(g->db,b,t2);
        if (s1!=s2 && (s1==NULL || s2==NULL || memcmp(s1,s2,ol))) return 0;
        s1=wg_decode_unistr(g->db,a,t1);
        s2=wg_decode_unistr(g->db,b,t2);
        if (s1!=s2 && (s1==NULL || s2==NULL || memcmp(s1,s2,l1))) return 0;
        return 1;
      }        
  }
  return 0;
}

*/

/* 

// https://faq.cprogramming.com/cgi-bin/smartfaq.cgi?answer=1044654269&id=1043284392


#include <stdlib.h>
int status = system("./foo 1 2 3");



Here's how you specify the arguments in the caller:

const char    *my_argv[64] = {"/foo/bar/baz" , "-foo" , "-bar" , NULL};

Then call the exec_prog function like this:

int rc = exec_prog(my_argv);

Here's the exec_prog function:

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>

static int exec_prog(const char **argv)
{
    pid_t   my_pid;
    int     status, timeout; // unused ifdef WAIT_FOR_COMPLETION 

    if (0 == (my_pid = fork())) {
            if (-1 == execve(argv[0], (char **)argv , NULL)) {
                    perror("child process execve failed [%m]");
                    return -1;
            }
    }

#ifdef WAIT_FOR_COMPLETION
    timeout = 1000;

    while (0 == waitpid(my_pid , &status , WNOHANG)) {
            if ( --timeout < 0 ) {
                    perror("timeout");
                    return -1;
            }
            sleep(1);
    }

    printf("%s WEXITSTATUS %d WIFEXITED %d [status %d]\n",
            argv[0], WEXITSTATUS(status), WIFEXITED(status), status);

    if (1 != WIFEXITED(status) || 0 != WEXITSTATUS(status)) {
            perror("%s failed, halt system");
            return -1;
    }

#endif
    return 0;
}


// https://stackoverflow.com/questions/70842/execute-program-from-within-a-c-program

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

void error(char *s);
char *data = "Some input data\n";

main()
{
  int in[2], out[2], n, pid;
  char buf[255];

  // In a pipe, xx[0] is for reading, xx[1] is for writing 
  if (pipe(in) < 0) error("pipe in");
  if (pipe(out) < 0) error("pipe out");

  if ((pid=fork()) == 0) {
    // This is the child process 

    // Close stdin, stdout, stderr 
    close(0);
    close(1);
    close(2);
    // make our pipes, our new stdin,stdout and stderr 
    dup2(in[0],0);
    dup2(out[1],1);
    dup2(out[1],2);

    // Close the other ends of the pipes that the parent will use, because if
    // we leave these open in the child, the child/parent will not get an EOF
    // when the parent/child closes their end of the pipe.
    //
    close(in[1]);
    close(out[0]);

    // Over-write the child process with the hexdump binary 
    execl("/usr/bin/hexdump", "hexdump", "-C", (char *)NULL);
    error("Could not exec hexdump");
  }

  printf("Spawned 'hexdump -C' as a child process at pid %d\n", pid);

  // This is the parent process 
  // Close the pipe ends that the child uses to read from / write to so
  // the when we close the others, an EOF will be transmitted properly.
  //
  close(in[0]);
  close(out[1]);

  printf("<- %s", data);
  // Write some data to the childs input 
  write(in[1], data, strlen(data));

  // Because of the small amount of data, the child may block unless we
  // close it's input stream. This sends an EOF to the child on it's
  // stdin.
  //
  close(in[1]);

  // Read back any output 
  n = read(out[0], buf, 250);
  buf[n] = 0;
  printf("-> %s",buf);
  exit(0);
}

void error(char *s)
{
  perror(s);
  exit(1);
}

*/

/* ============ solving prop with external prover ============= */



/** making a file and calling ext prover 
 * 
 * return 
 *
 *  -1 : error
 *   0: all ok, nothing found
 *   1: model built
 *   2: contradiction found
 * 
*/

int wr_prop_solve_current(glb* g) {
  int tmp;
  int extres;
  char buf[1000];
  int bytes;
  FILE *outfptr;  
  char* solvername=(g->prop_solver_name);
  char infname[200];
  char outfname[200]; 
  char* spos;
  char* provedstr;
  int provedflag;

  UNUSED(extres);
  if (!(g->instgen_strat) && !(g->propgen_strat)) return 0;

  strncpy(infname,(g->prop_file_name),190);
  strncpy(outfname,(g->prop_solver_outfile_name),190);  
  
  /*
  mktemp(infname);
  mktemp(outfname);
  */
  /*
  infname="/tmp/inprop.txt";
  outfname="/tmp/outprop.txt";
  */

  //printf("\n %s %s \n",infname,outfname);
  //exit(0);

  if (!infname[0] || !outfname[0]) return -1;
  tmp=wr_make_prop_file(g,infname);
  if (tmp<1) return -1;
  bytes=snprintf(buf,900,"%s %s --relaxed --no-binary --unsat --time=1 -n > %s",
    solvername,infname,outfname);     
  if (bytes<0) return -1;  

  printf("\n%s\n",buf);
  extres=system(buf);
  if (extres<0) {
    remove(infname);
    remove(outfname);
    return -1;
  }  
  //printf("\nextres: %d\n",extres);  

  outfptr=fopen(outfname,"r");
  if (outfptr==NULL) return -1;
  provedstr="s UNSATISFIABLE";
  provedflag=0;
  while(fgets(buf, 900, outfptr) != NULL) {
    printf("%s",buf);
    spos=strstr(buf,provedstr);
    if (spos!=NULL) {
      provedflag=1;
      break;
    }
  }
  fclose(outfptr);  

  if (provedflag) {
    g->proof_found=1;
    g->proof_history=wr_build_extprop_history(g);
    wr_register_answer(g,NULL,g->proof_history);
    return 2;
  } else {
    remove(infname);
    remove(outfname);
    return 0;
  }
}


int wr_make_prop_file(glb* g, char* fname) {
  FILE *fptr;
  int clnr,maxvar,i,tmp,bytes;
  cvec varvals, clauses;
  gptr rec;

  if ((g->prop_varvals)==(gint)NULL) return -1;
  if ((g->prop_clauses)==(gint)NULL) return -1;  
  if (fname==NULL) return -1;
  fptr=fopen(fname,"w");
  if (fptr==NULL) return -1;
  
  varvals=rotp(g,g->prop_varvals);
  clauses=rotp(g,g->prop_clauses);
  maxvar=varvals[1]; // max var plus 1; vars at pos 2,3,4,...
  clnr=clauses[1];  // max clause index plus 1
  bytes=0;
  tmp=fprintf(fptr,"c generated by gkc\n"); 
  if (tmp<0) { fclose(fptr); return -1; }
  bytes+=tmp;
  tmp=fprintf(fptr,"p cnf %d %d\n",(int)maxvar,(int)clnr-2); 
  if (tmp<0) { fclose(fptr); return -1; }
  bytes+=tmp;
  for(i=2;i<clnr;i++) {
    rec=rotp(g,clauses[i]);
    tmp=wr_fprint_prop_clause(g,fptr,rec); 
    if (tmp<0) { fclose(fptr); return -1; }
    bytes+=tmp;
  }
  fclose(fptr); 
  return bytes;
}


int wr_fprint_prop_clause(glb* g, FILE* fptr, gint* rec) {
  void* db=g->db;
  gint var;
  //gint meta;
  int i, len, tmp, bytes;

  UNUSEDVAR(db);
  if (rec==NULL) return 0;
  len = wg_count_prop_clause_atoms(db, rec);
  bytes=0;
  for(i=0; i<len; i++) {
    if (i>0 && i<len) {
      tmp=fprintf(fptr," ");
      if (tmp<0) return tmp;
      bytes+=tmp;      
    }  
    //meta=wg_get_rule_clause_atom_meta(db,rec,i);
    var=wg_get_prop_clause_atom(db,rec,i);
    tmp=fprintf(fptr,"%ld",var); 
    if (tmp<0) return tmp;
    bytes+=tmp;
  }
  tmp=fprintf(fptr," 0\n");
  if (tmp<0) return tmp;
  bytes+=tmp;

  //wr_fprint_prop_clause_atom_term(g,fptr,rec);

  return bytes;
}


int wr_fprint_prop_clause_atom_term(glb* g, FILE* fptr, gint* rec) {
  void* db=g->db;
  gint var, absvar;
  //gint meta;
  int i, len, blen, bpos, tmp, isneg;
  gint atom, term;
  char* buf;

  UNUSEDVAR(db);
  blen=1000;
  if (rec==NULL) return 0;
  len = wg_count_prop_clause_atoms(db, rec);
  buf=wr_malloc(g,blen);
  if (buf==NULL) {  return -1; }   
  bpos=0;
  bpos+=snprintf(buf+bpos,blen-bpos,"c "); 
  for(i=0; i<len; i++) {
    if (!wr_str_guarantee_space(g,&buf,&blen,bpos+500)) { wr_free(g,buf); return -1; };
    if (i>0 && i<len) {
      bpos+=snprintf(buf+bpos,blen-bpos," ");        
    }  
    //meta=wg_get_rule_clause_atom_meta(db,rec,i);
    var=wg_get_prop_clause_atom(db,rec,i);    
    bpos+=snprintf(buf+bpos,blen-bpos,"%ld:: ",var);   
   
    if (var<0) {isneg=1; absvar=0-var; }
    else { isneg=0; absvar=var; }
    atom=rotp(g,(g->prop_groundings))[absvar*2];
    term=rotp(g,(g->prop_groundings))[(absvar*2)+1];
    
    if (!wr_str_guarantee_space(g,&buf,&blen,bpos+100)) { wr_free(g,buf); return -1; }
    bpos=wr_strprint_atom_otter(g,atom,(g->print_clause_detaillevel),&buf,&blen,bpos,isneg);
    if (bpos<0) { wr_free(g,buf); return -1; } 
    if (!wr_str_guarantee_space(g,&buf,&blen,bpos+100)) { wr_free(g,buf); return -1; }  
    bpos+=snprintf(buf+bpos,blen-bpos," : "); 
    bpos=wr_strprint_term_otter(g,term,(g->print_clause_detaillevel),&buf,&blen,bpos);
    if (bpos<0) { wr_free(g,buf); return -1; } 
  }
  tmp=fprintf(fptr,"%s\n",buf);
  if (tmp<0) { wr_free(g,buf); return -1; }; 
  wr_free(g,buf);
  return 1;
}


#ifdef __cplusplus
}
#endif