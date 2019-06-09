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

 /** @file hash.c
  Hash and hash-based storage/search functions. 
 */

/* ====== Includes =============== */


#ifdef __cplusplus
extern "C" {
#endif

#include "rincludes.h"  
 

/* ====== Private defs =========== */



//#define DEBUG
#undef DEBUG  

//#define XDEBUG
//#define DEBUGHASH



/* ====== Private headers ======== */

static int int_hash(int x);
static int double_hash(double x);
static int str_hash(char* x); 
static int str_dual_hash(char* x, char* y);
  
  
/* ====== Functions ============== */


/* =====================================================

  top level hash storage funs for atom/clause 

====================================================== */


int wr_term_hashstore(glb* g, void* hashdata, gint term, gptr cl) {
  void* db=g->db;
  int pos;
  int bits;
  unsigned int hash=0;
  gptr tptr;
  int tlen;
  int uselen;
  int spos;
  int epos;
  int preflen;
  int prefpos;
  //int nonvarflag;
  gint el;
  int thash;
  int tmp;
  int i;
  //gint hashposbits=3; // bits 11
  //gptr nonvarhashbitset;
  //int nonvarhashbitsetsize;
  int nonvarhashbitsetsize=2;
  gint nonvarhashbitset[2];
  
  int maxhashpos=MAXHASHPOS;
  gint hasharr[MAXHASHPOS];
  
  
  nonvarhashbitset[0]=1;
  nonvarhashbitset[1]=3;
  
  // find the basic props of atom for hashing
  tptr=decode_record(db,term);  
  tlen=get_record_len(tptr);
  uselen=tlen;
  if (g->unify_maxuseterms) {
    if (((g->unify_maxuseterms)+(g->unify_firstuseterm))<uselen) 
      uselen=(g->unify_firstuseterm)+(g->unify_maxuseterms);
  }   
  spos=RECORD_HEADER_GINTS+(g->unify_firstuseterm);
  epos=RECORD_HEADER_GINTS+uselen;
  // loop over atom
  preflen=0;
  //nonvarflag=1;
  for(pos=spos, preflen=0; pos<epos; ++pos, ++preflen) {    
    el=*(tptr+pos);
    if (isvar(el)) break;    
  } 
  // now we have basic data for atom top-level
  if (pos>=epos) {
    // fully non-var top level
    // precalc hash vals for positions
    for(pos=spos, prefpos=0; pos<epos && prefpos<maxhashpos; ++pos, ++prefpos) {    
      el=*(tptr+pos);
      thash=wr_term_basehash(g,el);
      hasharr[prefpos]=thash;      
    }     
    /*
    // compute complexhash and store to hashvecs
    for(bits=hashposbits, pos=spos; bits>0 && pos<epos; bits=bits>>1, ++pos) {
      if (bits & 1) {
        el=*(tptr+pos);
        thash=wr_term_basehash(g,el);
        //hash=hash+thash;      
        hash = thash + (hash << 6) + (hash << 16) - hash;
      }      
    }
    */    
    // loop over  hashbitset, compute complex hash and store            
    for(i=0;i<nonvarhashbitsetsize;i++) { 
      hash=0;      
      for(bits=nonvarhashbitset[i], pos=spos, preflen=0; 
         bits>0 && pos<epos; 
         bits=bits>>1, ++pos, ++preflen) {
        if (bits & 1) {
          thash=hasharr[preflen];
          //hash=hash+thash;      
          hash = thash + (hash << 6) + (hash << 16) - hash;
        }      
      }  
      //if (hash<0) hash=0-hash; // uint hash!
      hash=(1+(hash%(NROF_CLTERM_HASHVEC_ELS-2)));      
      // here store!
      tmp=1; // dummy
      //tmp=wr_clterm_add_hashlist(g,hashvec,hash,atom,cl); 
      if (tmp) {
        //wr_sys_exiterr2int(g,"adding toplevel-nonvar term to hashlist in wr_term_hashstore, code ",tmp);
        return 1;        
      }        
    }  
    return 0;
  }  
  // now we have vars in top level: use predicate hashtables
  if (1) {    
    el=get_field(tptr,(g->unify_funpos));  
    thash=wr_term_basehash(g,el);
    // here store!
    //tmp=1; // dummy    
#ifdef XDEBUG 
    printf("adding to hash (vec)hashdata in wr_term_hashstore\n");
#endif    
    tmp=wr_clterm_add_hashlist(g,(vec)hashdata,thash,term,cl); 
    if (tmp) {
      //wr_sys_exiterr2int(g,"adding toplevel-nonvar term to hashlist in wr_term_hashstore, code ",tmp);
      return 1;        
    }        
  }    
  // everything ok
  return 0;
}  


gint wr_term_complexhash(glb* g, gint* hasharr, gint hashposbits, gint term) {
  int pos;
  int bits;
  unsigned int hash=0;
  gptr tptr;
  int tlen;
  int uselen;
  int spos;
  int epos;
  gint el;
  int thash;

#ifdef DEBUG
  printf("wr_term_complexhash called with term %d bits %d \n",term,hashposbits);
#endif    
  tptr=decode_record(g->db,term);  
  tlen=get_record_len(tptr);
  uselen=tlen;
  if (g->unify_maxuseterms) {
    if (((g->unify_maxuseterms)+(g->unify_firstuseterm))<uselen) 
      uselen=(g->unify_firstuseterm)+(g->unify_maxuseterms);
  }   
  spos=RECORD_HEADER_GINTS+(g->unify_firstuseterm);
  epos=RECORD_HEADER_GINTS+uselen;
  // first check if hashable (ie non-vars at hash positions)
  for(bits=hashposbits, pos=spos; bits>0 && pos<epos; bits=bits>>1, ++pos) {
    if (bits & 1) {
      el=*(tptr+pos);
      if (isvar(el)) return 0;
    }      
  } 
  // we know term is hashable: compute hash
  for(bits=hashposbits, pos=spos; bits>0 && pos<epos; bits=bits>>1, ++pos) {
    if (bits & 1) {
      el=*(tptr+pos);
      thash=wr_term_basehash(g,el);
      //hash=hash+thash;      
      hash = thash + (hash << 6) + (hash << 16) - hash;
    }      
  }  
  //if (hash<0) hash=0-hash;  // uint hash!
#ifdef DEBUG
  printf("wr_term_complexhash computed hash %d using NROF_CLTERM_HASHVEC_ELS-2 %d gives final res %d \n",
         hash,NROF_CLTERM_HASHVEC_ELS-2,1+(hash%(NROF_CLTERM_HASHVEC_ELS-2)));
#endif    
  return (gint)(1+(hash%(NROF_CLTERM_HASHVEC_ELS-2)));
}  



gint wr_atom_funhash(glb* g, gint atom) {
  void* db=g->db;
  gint fun;
  gint chash;
  
  //printf("wr_atom_funhash called\n");
  fun=get_field(decode_record(db,atom),(g->unify_funpos)); 
  //printf("fun %d\n",fun);  
  chash=wr_term_basehash(g,fun);
  //printf("chash %d\n",chash);
  return chash;
}    

gint wr_term_funhash(glb* g, gint term) {
  void* db=g->db;
  gint fun;
  gint chash;
  
  printf("\nwr_term_funhash called\n");
  if (!isdatarec(term)) {
    fun=term;
  } else {
    fun=get_field(decode_record(db,term),(g->unify_funpos)); 
  }  
  printf("\nfun %d\n",(int)fun);  
  chash=wr_term_basehash(g,fun);
  printf("\nchash %d\n",(int)chash);
  return chash;
}


int wr_atom_calc_prefhashes(glb* g, gint x, gint* prefhashes) {
  int preflen=0;

  *prefhashes=(gint)0;
  (*(prefhashes+1))=(gint)0;
  (*(prefhashes+2))=(gint)0;
#ifdef DEBUG
  printf("\nwr_atom_calc_prefhashes called with ");
  wg_print_record(g->db,rotp(g,x));
  printf("\n");
#endif
  wr_atom_calc_prefhashes_aux(g,x,&preflen,prefhashes);

  //*prefhashes=*prefhashes+(gint)0;
  //(*(prefhashes+1))=(*(prefhashes+1))+(gint)1;
  //(*(prefhashes+2))=(*(prefhashes+2))+(gint)2;
#ifdef DEBUG
  printf("\n preflen %d prefhashes %ld %ld %ld\n",
    preflen,
    *prefhashes,
    (*(prefhashes+1)),
    (*(prefhashes+2)) );
#endif
  return preflen;
}  


int wr_atom_calc_prefhashes_aux(glb* g, gint x, int* preflen, gint* prefhashes) {
  void* db;
  gptr xptr;
  int i, start, end;
  gint hash;
    
  //printf("\nwr_atom_calc_prefhashes_aux called with x %ld type %ld\n",x,wg_get_encoded_type(g->db,x));

  if (!isdatarec(x)) {
    // now we have a simple value      
    if (isvar(x)) {
      return 0;    
    } else {      
      hash=wr_term_basehash(g,x);
      // !! needs bitshifting improvement !!
      (*preflen)++;
      if (*preflen==1) (*prefhashes)=hash; 
      else if (*preflen==2) (*(prefhashes+1))=WR_HASH_ADD(hash,(*(prefhashes)));
      else if (*preflen>=3) {
        (*(prefhashes+2))=WR_HASH_ADD(hash,(*(prefhashes+1)));
        return 0;
      }  
      //else return 0;
    } 
    return 1;  
  }   
  // now we have a datarec
  db=g->db;
  xptr=decode_record(db,x);
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,xptr); 
  for(i=start;i<end;i++) {
    if (!wr_atom_calc_prefhashes_aux(g,xptr[i],preflen,prefhashes)) return 0;      
  }   
  return 1;
} 



/* =====================================================

  proper hash funs for atoms and terms

====================================================== */

gint wr_lit_hash(glb* g, gint x) {
  (g->stat_lit_hash_computed)++;
  return wr_term_hash(g,x);
}  

gint wr_term_hash(glb* g, gint x) {
  void* db;
  gptr xptr;
  int i, start, end;  
  gint hash=0;
  gint tmp;

  //printf("\nwr_term_hash called with x %d type %d\n",x,wg_get_encoded_type(g->db,x));
  if (!isdatarec(x)) {
    // now we have a simple value     
    hash=wr_term_basehash(g,x);   
    //printf("\nhash for simple value %d \n:",hash); 
    return hash;  
  }   
  // now we have a datarec
  db=g->db;
  xptr=decode_record(db,x);
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,xptr);
  for(i=start;i<end;i++) {
    tmp=wr_term_hash(g,xptr[i]);  
    //printf("\nhash for subterm %d is %d \n:",i,hash); 
    hash = tmp + (hash << 6) + (hash << 16) - hash;
    if (hash<0) hash=0-hash;
  }   
  hash=(gint)(0+(hash%(NROF_CLTERM_HASHVEC_ELS-2)));
  if (hash<0) hash=0-hash;
  return hash;
}  

gint wr_term_basehash(glb* g, gint enc) {
  void* db=g->db;
  int hash;
  int intdata;
  char *strdata, *exdata;
  double doubledata;
  //gint res;
  
#ifdef DEBUG
  printf("wr_termhash called with enc %d visually ", enc);
  wr_print_simpleterm_otter(g,enc,(g->print_clause_detaillevel));  
  printf("\n");
  printf("wg_get_encoded_type %d\n",wg_get_encoded_type(db,enc));
#endif   
  switch(wg_get_encoded_type(db, enc)) {    
    case WG_NULLTYPE:
      hash=0;
      /*
      printf("\n in wr_term_basehash nulltype\n");
      wr_print_simpleterm_otter(g,enc,(g->print_clause_detaillevel)); 
      printf("\n");
      */
      break;       
    case WG_INTTYPE:
      intdata = wg_decode_int(db, enc);
      hash=int_hash(intdata);   
      break;
    case WG_DOUBLETYPE:
      doubledata = wg_decode_double(db, enc);
      hash=double_hash(doubledata);      
      break;
    case WG_STRTYPE:      
      strdata = wg_decode_unistr(db,enc,WG_STRTYPE);
      hash=str_hash(strdata);
      break;
    case WG_URITYPE:
      //printf("\nto call wg_decode_uri_rhash \n");
      hash=(int)wg_decode_uri_rhash(db, enc);
      //printf("\nhash for strdata %s found %d \n", wg_decode_unistr(db, enc,WG_URITYPE), hash);
      if (!hash) {
        strdata = wg_decode_unistr(db, enc,WG_URITYPE);
        exdata =  wg_decode_unistr_lang(db, enc,WG_URITYPE);
        hash=str_dual_hash(strdata,exdata);      
        //printf("\nstrdata %s exdata %s hash %d\n",strdata,exdata,hash);
        wg_set_uri_rhash(db,enc,(gint)hash);
      }  
      break;
    case WG_XMLLITERALTYPE:
      strdata = wg_decode_unistr(db,enc,WG_XMLLITERALTYPE);
      exdata = wg_decode_unistr_lang(db,enc,WG_XMLLITERALTYPE);
      hash=str_dual_hash(strdata,exdata);
      break;
    case WG_CHARTYPE:      
      hash=int_hash(enc);
      break;
    case WG_DATETYPE:     
      hash=int_hash(enc);
      break;
    case WG_TIMETYPE:            
      hash=int_hash(enc);
      break;
    case WG_VARTYPE:
      hash=int_hash(decode_var(enc));
      //hash=int_hash(0);
      break;
    case WG_ANONCONSTTYPE:     
      hash=int_hash(enc);
      break; 
    case WG_RECORDTYPE:
      //  ptrdata = (gint) wg_decode_record(db, enc);
      //  wg_print_subrecord_otter(db,(gint*)ptrdata);
      hash=1;
      break;
    default:
      hash=2;
      break;
  }
  if (hash<0) hash=0-hash;
#ifdef DEBUG
  printf("wr_termhash computed hash %d using NROF_CLTERM_HASHVEC_ELS-2 %d gives final res %d \n",
         hash,NROF_CLTERM_HASHVEC_ELS-2,0+(hash%(NROF_CLTERM_HASHVEC_ELS-2)));
#endif 
  /*  
  res=(gint)(1+(hash%(NROF_CLTERM_HASHVEC_ELS-3)));
  if (res==0) {
    //wr_sys_exiterr2int(g,"adding term to hashlist in  wr_cl_store_eq_arg, code ",tmp);
    //return 1;   
    printf("\n in wr_term_basehash res %d hash %d NROF_CLTERM_HASHVEC_ELS-2 %d \n",
     res,hash,NROF_CLTERM_HASHVEC_ELS-2);
    wr_print_simpleterm_otter(g,enc,(g->print_clause_detaillevel)); 
    printf("\n");
  }
  return res;
  */
  return (gint)(1+(hash%(NROF_CLTERM_HASHVEC_ELS-3)));
}


static int int_hash(int x) {
  int a;
  
  a=(x*2654435769)%(NROF_CLTERM_HASHVEC_ELS-2);
  if (a>=0) return a;
  else return 0-a;
  
  /* 
  if (x>=0 && x<NROF_CLTERM_HASHVEC_ELS-4) return x+3;
  a=(unsigned int)x;
  a -= (a<<6);
  a ^= (a>>17);
  a -= (a<<9);
  a ^= (a<<4);
  a -= (a<<3);
  a ^= (a<<10);
  a ^= (a>>15);
  return (int)a;  
  */
}  

/*
#include <inttypes.h>
uint32_t knuth_mul_hash(void* k) {
  ptrdiff_t v = (ptrdiff_t)k * UINT32_C(2654435761);
  v >>= ((sizeof(ptrdiff_t) - sizeof(uint32_t)) * 8); // Right-shift v by the size difference between a pointer and a 32-bit integer (0 for x86, 32 for x64)
  return (uint32_t)(v & UINT32_MAX);
}
*/


static int double_hash(double x) {
  if (x==(double)0) return 20;
  return int_hash((int)(x*1000));  
} 

// see also http://www.stdlib.net/~colmmacc/strlen.c.html for speedup ideas
/*

unsigned char checksum = 0;
while (!feof(fp) && !ferror(fp)) {
   checksum ^= fgetc(fp);
}

...

unsigned char 
 Checksum(unsigned char *data, unsigned char length) {
      
     unsigned char result = 0;
     unsigned int l = length - 1;
     asm volatile ( "repeat %2\nsub.b %0,[%1++],%0"   
           : "+r" (result), "+r" (data)
           : "r" (l)
           : "_RCOUNT");
     return result;
 }

 ...

 https://create.stephan-brumme.com/crc32/#slicing-by-8-overview
...

uint16_t
fletcher16(const uint8_t *data, size_t len)
{
        uint32_t c0, c1;
        unsigned int i;

        for (c0 = c1 = 0; len >= 5802; len -= 5802) {
                for (i = 0; i < 5802; ++i) {
                        c0 = c0 + *data++;
                        c1 = c1 + c0;
                }
                c0 = c0 % 255;
                c1 = c1 % 255;
        }
        for (i = 0; i < len; ++i) {
                c0 = c0 + *data++;
                c1 = c1 + c0;
        }
        c0 = c0 % 255;
        c1 = c1 % 255;
        return (c1 << 8 | c0);
}
*/


static int str_hash(char* x) {
  unsigned long hash = 0;
  int c;  
    
  if (x!=NULL) {
    while(1) {
      c = (int)(*x);
      if (!c) break;
      hash = c + (hash << 6) + (hash << 16) - hash;
      x++;
    }
  }      
  return (int)hash;  
} 


static int str_dual_hash(char* x, char* y) {
  unsigned long hash = 0;
  int c;  
  
  //printf("x %s y %s\n",x,y);  
  if (x!=NULL) {
    while(1) {
      c = (int)(*x);
      if (!c) break;
      hash = c + (hash << 6) + (hash << 16) - hash;
      x++;
    }
  }    
  if (y!=NULL) {
    while(1) {
      c = (int)(*y);
      if (!c) break;
      hash = c + (hash << 6) + (hash << 16) - hash;
      y++;
    }
  }   
  return (int)hash;  
  
}


/* =====================================================

  storage to hashdata 

====================================================== */



int wr_clterm_add_hashlist(glb* g, vec hashvec, gint hash, gint term, gptr cl) {
  return wr_clterm_add_hashlist_withpath(g,hashvec,hash,term,cl,0);
}


int wr_clterm_add_hashlist_withpath(glb* g, vec hashvec, gint hash, gint term, gptr cl, int path) {
  void* db=g->db;
  gint vlen;
  gint cell;
  gptr node;
  gptr prevnode;
  gint nextnode;

#ifdef DEBUG
  printf("\nwr_clterm_add_hashlist_withpath called\n");
  printf("\nhash %ld path %d term %ld %lx:\n",hash,path,term,rotp(g,term));
  wr_print_term(g, term);
  printf("\n cl %lx:\n",cl);
  wr_print_record(g,cl);
  printf("\n");
#endif  
  vlen=VEC_LEN(hashvec);  
  if ((hash>=vlen) || (hash<1)) { 
    // err case
    printf("\nCP1 hash %d vlen %d\n",(int)hash,(int)vlen);
    if  (hash>=vlen)  {
       printf("\nhash>=vlen hash %d vlen %d\n",(int)hash,(int)vlen);
    }
    if  (hash<1)  {
       printf("\n(hash<1) hash %d vlen %d\n",(int)hash,(int)vlen);
    }
    printf("\n record \n");
    printf("\n term as gint %d\n",(int)term);
    wr_print_record(g,cl);
    printf("\n");
    wr_print_clause(g,cl);
    printf("\n");
    //wr_print_term(g,otp(db,term));
    printf("\n");
    return 1; 
  } 
  cell=hashvec[hash];
  if (cell==0) {
    // no hash chain yet: add first len-containing node
    prevnode=wr_clterm_alloc_hashnode(g);
    if (prevnode==NULL) {
      wr_sys_exiterr(g,"could not allocate node for hashlist in wr_clterm_add_hashlist_withpath");
      return 1;
    }  
    hashvec[hash]=pto(db,prevnode);
    prevnode[CLTERM_HASHNODE_LEN_POS]=1;            
    nextnode=0;
  } else {
    // hash chain exists: first node contains counter to increase
    // then take next ptr for node to handle
    prevnode=otp(db,cell);
    prevnode[CLTERM_HASHNODE_LEN_POS]++;
    nextnode=prevnode[CLTERM_HASHNODE_NEXT_POS];
  } 
  // make new node and add to chain
  node=wr_clterm_alloc_hashnode(g);
  if (node==NULL) {
    wr_sys_exiterr(g,"could not allocate node for hashlist in wr_clterm_add_hashlist_withpath");
    return 1;
  } 
  //printf("\nnode for hash %d allocated as ptr %d and pto(db,node) %d\n",hash,node,pto(db,node));
  //context as called tmp=wr_clterm_add_hashlist_withpath(g,hashvec,hash,term,cl,path);
  node[CLTERM_HASHNODE_TERM_POS]=term;    
  node[CLTERM_HASHNODE_CL_POS]=pto(db,cl);
  node[CLTERM_HASHNODE_PATH_POS]=wg_encode_int(db,path);
  //node[CLTERM_HASHNODE_NEXT_POS]=nextnode;  
  node[CLTERM_HASHNODE_NEXT_POS]=nextnode;
  prevnode[CLTERM_HASHNODE_NEXT_POS]=pto(db,node);
  //printf("all node[CLTERM_HASHNODE_TERM_POS] %d\n",node[CLTERM_HASHNODE_TERM_POS]);  
  //printf("all node[CLTERM_HASHNODE_CL_POS] %d\n",node[CLTERM_HASHNODE_CL_POS]);
  //printf("all node[CLTERM_HASHNODE_NEXT_POS] %d\n",node[CLTERM_HASHNODE_NEXT_POS]);
  
  return 0;
}  



int wr_clterm_add_hashlist_offset_withpath(glb* g, vec hashvec, gint hash, gint term, gptr cl, int path) {
  void* db=g->db;
  gint vlen;
  gint cell;
  gptr node;
  gptr prevnode;
  gint nextnode;
  vlen=VEC_LEN(hashvec);  
  if ((hash>=vlen) || (hash<1)) { 
    // err case
    printf("\nCP2 hash %d vlen %d\n",(int)hash,(int)vlen);
    if  (hash>=vlen)  {
       printf("\nhash>=vlen hash %d vlen %d\n",(int)hash,(int)vlen);
    }
    if  (hash<1)  {
       printf("\n(hash<1) hash %d vlen %d\n",(int)hash,(int)vlen);
    }
    printf("\n record \n");
    printf("\n term as gint %d\n",(int)term);
    wr_print_record(g,cl);
    printf("\n");
    wr_print_clause(g,cl);
    printf("\n");
    //wr_print_term(g,otp(db,term));
    printf("\n");
    return 1; 
  } 
  cell=hashvec[hash];
  if (cell==0) {
    // no hash chain yet: add first len-containing node
    prevnode=wr_clterm_alloc_hashnode(g);
    if (prevnode==NULL) {
      wr_sys_exiterr(g,"could not allocate node for hashlist in wr_clterm_add_hashlist_offset_withpath");
      return 1;
    }  
    hashvec[hash]=pto(db,prevnode);
    prevnode[CLTERM_HASHNODE_LEN_POS]=1;            
    nextnode=0;
  } else {
    // hash chain exists: first node contains counter to increase
    // then take next ptr for node to handle
    // printf("\n hash chain exists !!!!\n");    
    prevnode=otp(db,cell);
    prevnode[CLTERM_HASHNODE_LEN_POS]++;
    nextnode=prevnode[CLTERM_HASHNODE_NEXT_POS];
  } 
  // make new node and add to chain
  node=wr_clterm_alloc_hashnode(g);
  if (node==NULL) {
    wr_sys_exiterr(g,"could not allocate node for hashlist in wr_clterm_add_hashlist_offset_withpath");
    return 1;
  } 
  //printf("\nnode for hash %d allocated as ptr %d and pto(db,node) %d\n",hash,node,pto(db,node));  
  node[CLTERM_HASHNODE_TERM_POS]=term;    
  node[CLTERM_HASHNODE_CL_POS]=pto(db,cl);
  node[CLTERM_HASHNODE_PATH_POS]=wg_encode_int(db,path);
  //node[CLTERM_HASHNODE_NEXT_POS]=nextnode;  
  node[CLTERM_HASHNODE_NEXT_POS]=nextnode;
  prevnode[CLTERM_HASHNODE_NEXT_POS]=pto(db,node);  
  return 0;
}  

/*
int wr_clterm_add_hashlist_new (glb* g, vec hashvec, gint hash, gint term, gptr cl) {
  void* db=g->db;
  gint vlen;
  gint cell;
  gptr node;
  gptr prevnode;
  gint nextnode;
  
  vlen=VEC_LEN(hashvec);
  if (hash>=vlen || hash<1) return 1; // err case
  cell=hashvec[hash];
  if (cell==0) {
    // no hash chain yet: add first len-containing node
    prevnode=wr_clterm_alloc_hashnode(g);
    if (prevnode==NULL) {
      wr_sys_exiterr(g,"could not allocate node for hashlist in cl_store_res_terms");
      return 1;
    }  
    hashvec[hash]=pto(db,prevnode);
    prevnode[CLTERM_HASHNODE_LEN_POS]=1;            
    nextnode=0;
  } else {
    // hash chain exists: first node contains counter to increase
    // then take next ptr for node to handle
    prevnode=otp(db,cell);
    prevnode[CLTERM_HASHNODE_LEN_POS]++;
    nextnode=prevnode[CLTERM_HASHNODE_NEXT_POS];
  } 
  // make new node and add to chain
  node=wr_clterm_alloc_hashnode(g);  
  if (node==NULL) {
    wr_sys_exiterr(g,"could not allocate node for hashlist in cl_store_res_terms");
    return 1;
  } 
  node[CLTERM_HASHNODE_TERM_POS]=term;    
  node[CLTERM_HASHNODE_CL_POS]=pto(db,cl);
  node[CLTERM_HASHNODE_NEXT_POS]=nextnode;  
  prevnode[CLTERM_HASHNODE_NEXT_POS]=pto(db,node);
  return 0;
}  
*/

int wr_clterm_hashlist_len(glb* g, vec hashvec, gint hash) {
  gint vlen;
  gint cell;
  
  vlen=VEC_LEN(hashvec);
  if (hash>=vlen || hash<1) return -1; // err case
  cell=hashvec[hash];
  if (cell==0) return 0;
  return (int)((rotp(g,cell))[CLTERM_HASHNODE_LEN_POS]);
}  

gint wr_clterm_hashlist_start(glb* g, vec hashvec, gint hash) {
  gint vlen;
  gint cell;
#ifdef DEBUG  
  printf("wr_clterm_hashlist_start len %d hash %d\n",VEC_LEN(hashvec),hash);
#endif  
  vlen=VEC_LEN(hashvec);
  if (hash>=vlen || hash<1) return 0; // err case
  cell=hashvec[hash];
  if (cell==0) return 0; // empty case
  return (rotp(g,cell))[CLTERM_HASHNODE_NEXT_POS];
}  

gint wr_clterm_hashlist_next(glb* g, vec hashvec, gint lastel) {
  return (rotp(g,lastel))[CLTERM_HASHNODE_NEXT_POS];
}  

gptr wr_clterm_alloc_hashnode(glb* g) {
/*  
  printf("\n wr_clterm_alloc_hashnode %d \n",sizeof(gint)*CLTERM_HASHNODE_GINT_NR);
  printf("\n ++(g->stat_wr_mallocs) %d (g->stat_wr_malloc_bytes) %d\n",
    (g->stat_wr_mallocs),(g->stat_wr_malloc_bytes));
*/    
#ifdef MALLOC_HASHNODES    
  return wr_malloc(g,sizeof(gint)*CLTERM_HASHNODE_GINT_NR);
#else
  return wr_alloc_from_cvec(g,g->hash_nodes,CLTERM_HASHNODE_GINT_NR);
#endif  
}  

void wr_clterm_free_hashnode(glb* g,gptr node) {
#ifdef MALLOC_HASHNODES
  wr_free(g,node);
#endif  
} 


void wr_clterm_hashlist_free(glb* g, vec hashvec) {
  void* db=g->db;
  gint vlen;
  gint node;
  gint tmp;
  int i; 

#ifdef MALLOC_HASHNODES
  // need to free nodes only if created by malloc
  vlen=VEC_LEN(hashvec);     
  for(i=VEC_START;i<vlen+1;i++) {
    //printf("\ni %d i hashvec[i]",i,hashvec[i]);
    if (hashvec[i]!=0) {
      node=hashvec[i]; 
      while(node!=0) {             
        tmp=node;
        node=(otp(db,node))[CLTERM_HASHNODE_NEXT_POS];
        wr_clterm_free_hashnode(g,otp(db,tmp));        
      }        
    }  
  } 
#endif  
  wr_vec_free(g,hashvec);
} 

void wr_clterm_hashlist_print(glb* g, vec hashvec) {
  gint vlen;
  gint node;
  gint tmp,path;
  int i;
   
  vlen=VEC_LEN(hashvec); 
  //printf("\nhashvec len %d ptr %lx and els:\n",(int)vlen,(unsigned long int)hashvec); 
  printf("\nhashvec len %d els:\n",(int)vlen);
  for(i=VEC_START;i<vlen+1;i++) {
    if (hashvec[i]!=0) {
      //printf("\ni %d hashvec[i] %d \n",i,hashvec[i]);      
      node=(rotp(g,hashvec[i]))[CLTERM_HASHNODE_NEXT_POS];
      //printf("\nhashslot i %d node %ld len %d:\n",
      //   i, node, (int)((rotp(g,hashvec[i]))[CLTERM_HASHNODE_LEN_POS]));
      printf("\nhashslot %d len %d:\n",
         i, (int)((rotp(g,hashvec[i]))[CLTERM_HASHNODE_LEN_POS]));   
      while(node!=0) {        
        tmp=(rotp(g,node))[CLTERM_HASHNODE_PATH_POS];
        if (!tmp) {
          path=0;
        } else {
          path=wg_decode_int(g->db,tmp);
        } 
        wr_print_term(g,(rotp(g,node))[CLTERM_HASHNODE_TERM_POS]);
        printf(" at path %d ",(int)path);       
        printf("in cl ");
        wr_print_clause(g,rotp(g,(rotp(g,node))[CLTERM_HASHNODE_CL_POS])); 
        printf("\n");       
        //printf(" as rec ");      
        //wg_print_record(g->db,rotp(g,tmp));
        //wg_print_record(g->db,rotp(g,(rotp(g,node))[CLTERM_HASHNODE_CL_POS]));
        node=(rotp(g,node))[CLTERM_HASHNODE_NEXT_POS];
        /*
        //printf("term ");
        tmp=(rotp(g,node))[CLTERM_HASHNODE_TERM_POS];
        //printf("\nterm (rotp(g,node))[CLTERM_HASHNODE_TERM_POS] %ld\n",tmp);
  
        wg_print_record(g->db,rotp(g,tmp));
        printf(" as term ");
        wr_print_term(g,(rotp(g,node))[CLTERM_HASHNODE_TERM_POS]);

        tmp=(rotp(g,node))[CLTERM_HASHNODE_PATH_POS];
        if (!tmp) {
          path=0;
        } else {
          path=wg_decode_int(g->db,tmp);
        }        
        printf(" at path %d ",(int)path);       
        printf("in cl ");
        wr_print_clause(g,rotp(g,(rotp(g,node))[CLTERM_HASHNODE_CL_POS]));
        printf("\n");        
        node=(rotp(g,node))[CLTERM_HASHNODE_NEXT_POS];
        //printf(" node %d \n",node);
        */
      }        
    }  
  } 
  //printf("hashvec printed\n");  
}  


void wr_clterm_hashlist_print_para(glb* g, vec hashvec) {
  gint vlen;
  gint node;
  gint tmp,path;
  int i;
  //gint tmpel;
  
  vlen=VEC_LEN(hashvec); 
  printf("\nhashvec len %d ptr %lx and els:\n",(int)vlen,(unsigned long int)hashvec);  
  for(i=VEC_START;i<vlen+1;i++) {
    if (hashvec[i]!=0) {
      //printf("\ni %d hashvec[i] %d \n",i,hashvec[i]);   
      node=(rotp(g,hashvec[i]))[CLTERM_HASHNODE_NEXT_POS];
      printf("\nhashslot i %d node %ld len %d:\n",
         i, node, (int)((rotp(g,hashvec[i]))[CLTERM_HASHNODE_LEN_POS]));
      while(node!=0) {
        printf("term ");      
      
        wr_print_term(g,(rotp(g,node))[CLTERM_HASHNODE_TERM_POS]);
        tmp=(rotp(g,node))[CLTERM_HASHNODE_PATH_POS];
        if (!tmp) {
          path=0;
        } else {
          path=wg_decode_int(g->db,tmp);
        }        
        printf(" at path %d ",(int)path);       
        printf("in cl ");
        wr_print_clause(g,rotp(g,(rotp(g,node))[CLTERM_HASHNODE_CL_POS]));
        printf("\n");        
        node=(rotp(g,node))[CLTERM_HASHNODE_NEXT_POS];
        //printf(" node %d \n",node);
      }        
    }  
  } 
  //printf("hashvec printed\n");  
}  


/* =====================================================

  termhash: for storing ground unit clauses and similar
  
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
     3: cla
     4: termb
     5: clb
     ...
    
    if no more space to add a new el, array is realloced

====================================================== */

/*   
  
make a termhash  

  gsize is the length of the hash array

*/

/*
gint* wr_make_termhash(glb* g, int maxhash) {
  gint* hasharr;
  int i,gsize;
  
  gsize=maxhash+1;
  hasharr=wr_vec_new(g,gsize);
  if (hasharr==NULL) {
    (g->alloc_err)=1;    
    wr_alloc_err2int(g,"Cannot allocate memory for a termhash with size %d",gsize);    
    return NULL;
  }
  wr_vec_zero(hasharr);
  return hasharr;  
}
*/

/*

store a term in a termhash

*/

gint* wr_push_termhash(glb* g, gint* hasharr, int hash, gptr term, gptr cl) {
  //int elstart; // start of a bucket el in the hasharr

  return wr_push_offset_termhash(g,hasharr,hash,term,cl);
  /*
  cvec bucket,nbucket;
  int arrsize;

  (g->stat_lit_hash_added)++;
  // negative hashes not ok: make positive
  if (hash<0) hash=0-hash;
  // too big hashes are recalculated
  if (hash+1 >= hasharr[0]) {
    hash=hash % (hasharr[0]-1);
  }
  bucket=(cvec)(hasharr[hash+1]);  
  if (!bucket) {
    // el is empty: create a new cvec
    arrsize=TERMHASH_INITIAL_BUCKETSIZE;
    bucket=wr_cvec_new(g,arrsize);
    if (bucket==NULL) {
      (g->alloc_err)=1;    
      wr_alloc_err2int(g,"Cannot allocate memory for a termhash bucket with length",arrsize);
      return NULL;
    }
    hasharr[hash+1]=(gint)bucket;
  }
  // store to bucket: both term and cl
  nbucket=wr_cvec_push(g,bucket,(gint)term);
  if (nbucket==NULL) {
    (g->alloc_err)=1;    
    wr_alloc_err2int(g,"Cannot realloc a termhash bucket with old length",bucket[0]);
    return NULL;
  }
  nbucket=wr_cvec_push(g,nbucket,(gint)cl);
  if (nbucket==NULL) {
    (g->alloc_err)=1;    
    wr_alloc_err2int(g,"Cannot realloc a termhash bucket with old length",bucket[0]);
    return NULL;
  }
  if (bucket!=nbucket) hasharr[hash+1]=(gint)nbucket;
  return hasharr;
  */
}


/*

store a term in a termhash using only offsets

*/

gint* wr_push_offset_termhash(glb* g, gint* hasharr, int hash, gptr term, gptr cl) {
  //int elstart; // start of a bucket el in the hasharr
  
  //cvec bucket,nbucket;
  cvec nvec;
  gint bucket,nbucket;
  int arrsize;



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
  // store to bucket: both term and cl
  nvec=wr_cvec_push(g,rotp(g,bucket),rpto(g,term));
  if (nvec==NULL) {
    (g->alloc_err)=1;    
    wr_alloc_err2int(g,"Cannot realloc a termhash bucket with old length",(rotp(g,bucket))[0]);
    return NULL;
  }
  nvec=wr_cvec_push(g,nvec,rpto(g,cl));
  if (nvec==NULL) {
    (g->alloc_err)=1;    
    wr_alloc_err2int(g,"Cannot realloc a termhash bucket with old length",(rotp(g,bucket))[0]);
    return NULL;
  }
  nbucket=rpto(g,nvec);
  if (bucket!=nbucket) hasharr[hash+1]=nbucket;
  return hasharr;
}


gint* wr_find_termhash(glb* g, gint* hasharr, gptr term, int hash) {

  return wr_find_offset_termhash(g,hasharr,term,hash);
  /*
  int j;
  cvec bucket;
  gint oterm; // offset of term

  
  //printf("\nfind from termhash with length %d and hash %d the term: \n",hasharr[0],hash);
  //wr_print_clause(g,term);
  //printf("\n");
  
 
  // negative hashes not ok: make positive
  if (hash<0) hash=0-hash;
  // too big hashes are recalculated
  if (hash+1 >= hasharr[0]) {
    hash=hash % (hasharr[0]-1);
  }

  bucket=(cvec)(hasharr[hash+1]);  
  if (!bucket) {
    //printf("\n no bucket found for hash\n");
    (g->stat_lit_hash_match_miss)++;
    return NULL;
  }  
  oterm=rpto(g,term);
  //printf("bucket for hash %d size %d next free %d\n",hash,bucket[0],bucket[1]);
  for(j=2;j<bucket[0] && j<bucket[1]; j=j+2) {
    
    //printf("%d ",(j-2)/2);
    //wr_print_term(g,rpto(g,bucket[j]));
    //printf(" in cl ");
    //wr_print_clause(g,bucket[j+1]);
    //printf("\n");
    
    (g->tmp4)++;
    if (wr_equal_term(g,oterm,rpto(g,bucket[j]),1)) {
      //printf("equal term found in  wr_find_termhash !\n");
      (g->stat_lit_hash_match_found)++;
      return (gint*)(bucket[j+1]);
    }
  }
  (g->stat_lit_hash_match_miss)++;
  return NULL;
  */
}


gint* wr_find_offset_termhash(glb* g, gint* hasharr, gptr term, int hash) {
  int j;
  
  cvec bucket_asp;
  gint bucket;

  gint oterm; // offset of term

  /*
  printf("\nfind from termhash with length %d and hash %d the term: \n",hasharr[0],hash);
  wr_print_clause(g,term);
  printf("\n");
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
    (g->stat_lit_hash_match_miss)++;
    return NULL;
  }  
  oterm=rpto(g,term);
  //printf("bucket for hash %d size %d next free %d\n",hash,bucket[0],bucket[1]);
  bucket_asp=rotp(g,bucket);
  for(j=2;j<bucket_asp[0] && j<bucket_asp[1]; j=j+2) {
    /*
    printf("%d ",(j-2)/2);
    wr_print_term(g,rpto(g,bucket[j]));
    printf(" in cl ");
    wr_print_clause(g,bucket[j+1]);
    printf("\n");
    */
    (g->tmp4)++;
    if (wr_equal_term(g,oterm,bucket_asp[j],1)) {
      //printf("equal term found in  wr_find_termhash !\n");
      (g->stat_lit_hash_match_found)++;
      return rotp(g,bucket_asp[j+1]);
    }
  }
  (g->stat_lit_hash_match_miss)++;
  return NULL;
}

/*
  version of wr_find_termhash with an optional clause arg skipcl
  which should be skipped during search 

*/
/*
gint* wr_find_termhash_skip(glb* g, gint* hasharr, gptr term, int hash, gptr skipcl) {
  int j;
  cvec bucket;
  gint oterm; // offset of term


 
  // negative hashes not ok: make positive
  if (hash<0) hash=0-hash;
  // too big hashes are recalculated
  if (hash+1 >= hasharr[0]) {
    hash=hash % (hasharr[0]-1);
  }

  bucket=(cvec)(hasharr[hash+1]);  
  if (!bucket) {
    //printf("\n no bucket found for hash\n");
    (g->stat_lit_hash_match_miss)++;
    return NULL;
  }  
  oterm=rpto(g,term);
  //printf("bucket for hash %d size %d next free %d\n",hash,bucket[0],bucket[1]);
  for(j=2;j<bucket[0] && j<bucket[1]; j=j+2) {

    if ((gint*)(bucket[j+1])==skipcl) continue;
    if (wr_equal_term(g,oterm,rpto(g,bucket[j]),1)) {
      //printf("equal term found in  wr_find_termhash !\n");
      (g->stat_lit_hash_match_found)++;
      return (gint*)(bucket[j+1]);
    }
  }
  (g->stat_lit_hash_match_miss)++;
  return NULL;
}

*/

/*

free a termhash

*/

void wr_free_termhash(glb* g, gint* hasharr) {

  wr_free_offset_termhash(g,hasharr);
  /*
  int i;
  for(i=1;i<hasharr[0];i++) {
    if (hasharr[i]) wr_free(g,(void *)(hasharr[i]));
  }
  wr_free(g,hasharr);
  */
}

void wr_free_offset_termhash(glb* g, gint* hasharr) {
  int i;
  for(i=1;i<hasharr[0];i++) {
    if (hasharr[i]) wr_free(g,rotp(g,(hasharr[i])));
  }
  wr_free(g,hasharr);
}

/*

print a termhash

*/

void wr_print_termhash(glb* g, gint* hasharr) {

  wr_print_offset_termhash(g,hasharr);
  /*
  int i,j;
  cvec bucket;
  
  printf("\ntermhash with length %d: \n",(int)(hasharr[0]));  

  for(i=1;i<hasharr[0];i++) {

    

    if (hasharr[i]) {

      printf("\n i %d\n",i);
      printf("\n hasharr[i] %ld\n",hasharr[i]);

      bucket=(cvec)(hasharr[i]);

      printf("\nbucket %ld\n",bucket);
      printf("\nbucket[0] %ld bucket[1] %ld\n",bucket[0],bucket[1]);

      printf("bucket for hash %d size %d next free %d\n",i-1,(int)(bucket[0]),(int)(bucket[1]));
      if (1) {
        for(j=2;j<bucket[0] && j<bucket[1]; j=j+2) {
          printf("%d ",(j-2)/2);
          wr_print_term(g,rpto(g,bucket[j]));
          printf(" path %d in cl ",0);
          wr_print_clause(g,(gptr)(bucket[j+1]));
          printf("\n");
        }
      }  
    }
  }
  */
}


void wr_print_offset_termhash(glb* g, gint* hasharr) {
  int i,j;
  cvec bucket;
 
  //printf("\nhashvec len %ld ptr %lx and els:\n",hasharr[0],(unsigned long int)hasharr);  
  printf("\nhashvec len %ld els:\n",hasharr[0]);  
  for(i=1;i<hasharr[0];i++) {    
    if (hasharr[i]) {
      bucket=rotp(g,(hasharr[i]));
      //printf("\nhashslot i %d node %ld size %ld next free %ld\n",
      //        i,hasharr[i],bucket[0],bucket[1]);
      printf("\nhashslot i %d size %ld next free %ld\n",i,bucket[0],bucket[1]);        
      if (1) {
        for(j=2;j<bucket[0] && j<bucket[1]; j=j+2) {
          printf("term ");
          wr_print_term(g,bucket[j]);
          //printf(" path %d in cl ",0);
          //CP1
          //printf("\nj %d bucket[j+1] %ld \n",j,bucket[j+1]);
          //CP2          
          printf(" in clause ");
          wr_print_clause(g,rotp(g,bucket[j+1]));          
          printf(" as rec ");
          wg_print_record(g->db,rotp(g,bucket[j+1]));
          printf("\n");
        }
      }  
    }
  }
}


/* 

  atom occurrences hashes

  bucket is a cvec of sequentially stored nodes

  each node contains:

  atom
  atom nr
  cvec of clauses atom occurs positively
  cvec of clauses atom occurs negatively

*/

gint* wr_push_atomhash(glb* g, gint* hasharr, int hash, gint atom, gptr cl, int pos, int negflag) {
  //int elstart; // start of a bucket el in the hasharr
  cvec bucket,nbucket;
  int arrsize;
  gint atomnr;
  cvec clvec, nclvec;
  int bnodestart, found, clvecpos;

#ifdef DEBUG
  printf("\nwwr_push_atomhash called with atom ");
  wr_print_term(g,atom);
  printf(" and cl: ");
  wr_print_clause(g,cl);
  printf("\n");
#endif

  (g->stat_atom_hash_added)++;
  // negative hashes not ok: make positive
  if (hash<0) hash=0-hash;
  // too big hashes are recalculated
  if (hash+1 >= hasharr[0]) {
    hash=hash % (hasharr[0]-1);
  }
  found=0; // will be set to 1 iff atom already present in hash structure
  bucket=(cvec)(hasharr[hash+1]);  
  if (!bucket) {
    // el is empty: create a new cvec as initial bucket contents
    arrsize=ATOMHASH_NODE_SIZE; // ATOMHASH_INITIAL_BUCKET_SIZE
    bucket=wr_cvec_new(g,arrsize);
    if (bucket==NULL) {
      (g->alloc_err)=1;    
      wr_alloc_err2int(g,"Cannot allocate memory for atomhash bucket with length",arrsize);
      return NULL;
    }
    bucket[1]=bucket[1]+ATOMHASH_NODE_SIZE;
    bnodestart=2; // first node inside bucket
    // insert atom and atomnr
    bucket[bnodestart+ATOMHASH_ATOM_POS]=atom;    
    atomnr=0;
    bucket[bnodestart+ATOMHASH_ATOMNR_POS]=atomnr;
    // clear clvec pointers
    bucket[bnodestart+ATOMHASH_POSCLAUSES_POS]=(gint)NULL;
    bucket[bnodestart+ATOMHASH_NEGCLAUSES_POS]=(gint)NULL;
    // store new bucket    
    hasharr[hash+1]=(gint)bucket;
  } else {
    // bucket found: look for the correct atom inside the bucket    
    for(bnodestart=2;
        bnodestart<bucket[0] && bnodestart<bucket[1]; 
        bnodestart+=ATOMHASH_NODE_SIZE) { // 4 els in a node        
      if (wr_equal_term(g,atom,bucket[bnodestart+ATOMHASH_ATOM_POS],1)) {
        // start using bucket for storage       
        found=1;
        break;
      }
    }
    if (!found) {
      // atom not found: increase bucket with new node with four els
      nbucket=wr_add_atomhash_node(g,atom,bucket);
      if (nbucket==NULL) {
        (g->alloc_err)=1;    
        //wr_alloc_err2int(g,"Cannot realloc atomhash bucket with old length",bucket[0]);
        return NULL;
      }
      if (bucket!=nbucket) hasharr[hash+1]=(gint)nbucket; // replace with realloced bucket     
      bucket=nbucket; // start using bucket for storage
    }  
  }    
  // here bucket and node in bucket exists ok: if found==1, then old node, else new node
  // and bnodestart is the position of the node
  // and the atom and atomnr has been added
  // add clauses to node in the bucket
  if (negflag) clvecpos=bnodestart+ATOMHASH_NEGCLAUSES_POS;
  else clvecpos=bnodestart+ATOMHASH_POSCLAUSES_POS;
  clvec=(gptr)(bucket[clvecpos]);
  if (clvec==NULL) {
    // first clause to store
    clvec=wr_cvec_new(g,ATOMHASH_CLAUSE_CVEC_INITIAL_SIZE);
    if (clvec==NULL) {
      (g->alloc_err)=1;    
      wr_alloc_err(g,"Cannot allocate memory for atomhash clvec bucket");
      return NULL;
    }
    nclvec=wr_cvec_store(g,clvec,2,(gint)cl);
    if (nclvec==NULL) {
      (g->alloc_err)=1;    
      wr_alloc_err(g,"Cannot allocate memory for new clause in atomhash cvec bucket");
      return NULL;
    }
    bucket[clvecpos]=(gint)nclvec;
  } else {
    // some clauses already present
    nclvec=wr_cvec_push(g,clvec,(gint)cl); 
    if (nclvec==NULL) {
      (g->alloc_err)=1;    
      wr_alloc_err(g,"Cannot allocate memory for new clause in atomhash cvec bucket");
      return NULL;
    }
    if (clvec!=nclvec) bucket[clvecpos]=(gint)nclvec;
  }
  return hasharr;
}

cvec wr_add_atomhash_node(glb* g, gint atom, cvec bucket) {
  cvec nbucket, clvec;
  int atomnr;

  nbucket=wr_cvec_push(g,bucket,atom);
  if (nbucket==NULL) {
    (g->alloc_err)=1;    
    wr_alloc_err2int(g,"Cannot realloc atomhash bucket with old length",bucket[0]);
    return NULL;
  }
  atomnr=0;
  nbucket=wr_cvec_push(g,nbucket,atomnr);
  if (nbucket==NULL) {
    (g->alloc_err)=1;    
    wr_alloc_err2int(g,"Cannot realloc atomhash bucket with old length",bucket[0]);
    return NULL;
  }
  clvec=NULL;  // cvec of clauses atom occurs positively
  nbucket=wr_cvec_push(g,nbucket,(gint)clvec);
  if (nbucket==NULL) {
    (g->alloc_err)=1;    
    wr_alloc_err2int(g,"Cannot realloc atomhash bucket with old length",bucket[0]);
    return NULL;
  }
  clvec=NULL;  // cvec of clauses atom occurs negatively
  nbucket=wr_cvec_push(g,nbucket,(gint)clvec);
  if (nbucket==NULL) {
    (g->alloc_err)=1;    
    wr_alloc_err2int(g,"Cannot realloc atomhash bucket with old length",bucket[0]);
    return NULL;
  }
  return nbucket;
}

gint* wr_find_atomhash_bucketnode(glb* g, gint* hasharr, gint atom, int hash) {
  cvec bucket; 
  int bnodestart;
#ifdef DEBUG  
  printf("\nwr_find_atomhash called with hash %d and the term: \n",hash);
  wr_print_term(g,atom);
  printf("\n");  
#endif 
  // negative hashes not ok: make positive
  if (hash<0) hash=0-hash;
  // too big hashes are recalculated
  if (hash+1 >= hasharr[0]) {
    hash=hash % (hasharr[0]-1);
  }

  bucket=(cvec)(hasharr[hash+1]);  
  if (!bucket) {
    //printf("\n no bucket found for hash\n");
    (g->stat_atom_hash_match_miss)++;
    return NULL;
  }  
  for(bnodestart=2;
      bnodestart<bucket[0] && bnodestart<bucket[1]; 
      bnodestart+=ATOMHASH_NODE_SIZE) { // 4 els in a node        
    if (wr_equal_term(g,atom,bucket[bnodestart+ATOMHASH_ATOM_POS],1)) {
      // right bucket found      
      return (gptr)(bucket+bnodestart);
    }
  }
  // did not find right bucket
  return NULL;
}


/*

free atomhash

*/


void wr_free_atomhash(glb* g, gint* hasharr) {
  //void* db=g->db;
  int i,j; 
  
#ifdef MALLOC_HASHNODES
  cvec bucket;

  for(i=1;i<hasharr[0];i++) {
    bucket=(cvec)(hasharr[i]); 
    if (bucket) {       
      for(j=2;j<bucket[0]+2 && j<bucket[1]; j=j+ATOMHASH_NODE_SIZE) { // 4 els in a node
        if (bucket[j+ATOMHASH_POSCLAUSES_POS]) {
          // pos occurrence cvec
          wr_free(g,(void *)bucket[j+ATOMHASH_POSCLAUSES_POS]);
        }  
        if (bucket[j+ATOMHASH_NEGCLAUSES_POS]) {
          // neg occurrence cvec
          wr_free(g,(void *)bucket[j+ATOMHASH_NEGCLAUSES_POS]);    
        }
      }    
      wr_free(g,(void *)bucket);
    }  
  }
#endif  
  wr_free(g,hasharr);
} 

/*

print atomhash

*/

void wr_print_atomhash(glb* g, gint* hasharr) {
  int i,j;
  cvec bucket;
  
  printf("\natomhash with length %d: \n",(int)(hasharr[0]));  

  for(i=1;i<hasharr[0];i++) {
    if (hasharr[i]) {
      bucket=(cvec)(hasharr[i]);
      printf("bucket for hash %d size %d next free %d\n",i-1,(int)(bucket[0]),(int)(bucket[1]));
      if (1) {
        for(j=2;j<bucket[0]+2 && j<bucket[1]; j=j+ATOMHASH_NODE_SIZE) {
          printf("j %d nr %d ",j,(j-2)/ATOMHASH_NODE_SIZE);
          wr_print_term(g,bucket[j+ATOMHASH_ATOM_POS]);
          printf("  atomnr %d ",(int)(bucket[j+ATOMHASH_ATOMNR_POS]));
          if (bucket[j+ATOMHASH_POSCLAUSES_POS]) {
            printf("  pos clauses count %d nextfree %d:\n",
              (int)(((gptr)(bucket[j+ATOMHASH_POSCLAUSES_POS]))[0]),
              (int)(((gptr)(bucket[j+ATOMHASH_POSCLAUSES_POS]))[1]));
            wr_print_atomhash_clvec(g,(gptr)(bucket[j+ATOMHASH_POSCLAUSES_POS])); 
          }
          if (bucket[j+ATOMHASH_NEGCLAUSES_POS]) {
            printf("  neg clauses count %d nextfree %d:\n",
              (int)(((gptr)(bucket[j+ATOMHASH_NEGCLAUSES_POS]))[0]),
              (int)(((gptr)(bucket[j+ATOMHASH_NEGCLAUSES_POS]))[1]));
            wr_print_atomhash_clvec(g,(gptr)(bucket[j+ATOMHASH_NEGCLAUSES_POS])); 
          }
        }
      }  
    }
  }
}

void wr_print_atomhash_clvec(glb* g, cvec clvec) {
  int i;

  //printf("\nwr_print_atomhash_clvec called, clvec[0] %d clvec[1] %d\n",clvec[0],clvec[1]); 
  for(i=2;i<clvec[0]+2 && i<clvec[1];i++) {
    printf("    i %d ",i);
    wr_print_clause(g,(gptr)(clvec[i]));
    printf("\n");
  }
  //printf("\nwr_print_atomhash_clvec ended\n"); 
}  


#ifdef __cplusplus
}
#endif



