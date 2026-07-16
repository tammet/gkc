/* Bounded numeric instantiation for non-ground arithmetic clauses. */

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "rincludes.h"

typedef struct {
  int comp;
  int literal;
  int negative;
  int variable_count;
  int variables[WR_ARITH_MAX_TUPLE_VALUES];
} wr_arith_frontier;

typedef struct {
  wr_arith_frontier frontiers[WR_ARITH_MAX_FRONTIERS];
  int frontier_count;
  int variables[WR_ARITH_MAX_CLAUSE_VARS];
  int variable_count;
  unsigned char directly_bindable[WR_ARITH_MAX_CLAUSE_VARS];
  wr_arith_value values[WR_ARITH_MAX_LOCAL_VALUES];
  int value_count;
  int truncated;
} wr_arith_scan;

static int wr_arith_add_value(glb*,wr_arith_value*,int*,int,gint,
                              unsigned char,unsigned short);
static void wr_arith_scan_term(glb*,gint,int,int,wr_arith_scan*);

void wr_arithinst_configure(glb* g) {
  if (g->arith_instantiation<0 || g->arith_instantiation>2) {
    wr_warn(g,"arith_instantiation must be 0, 1, or 2; clamping it");
    if (g->arith_instantiation<0) g->arith_instantiation=0;
    else g->arith_instantiation=2;
  }
  if (!(g->arith_inst_explicit_mask&1u))
    g->arith_inst_max_vars=(g->arith_instantiation==2) ? 2 : 1;
  if (!(g->arith_inst_explicit_mask&2u))
    g->arith_inst_candidate_limit=(g->arith_instantiation==2) ? 6 : 4;
  if (!(g->arith_inst_explicit_mask&4u))
    g->arith_inst_probe_limit=(g->arith_instantiation==2) ? 32 : 16;
  if (!(g->arith_inst_explicit_mask&8u))
    g->arith_inst_keep_limit=(g->arith_instantiation==2) ? 4 : 2;
  if (!(g->arith_inst_explicit_mask&16u))
    g->arith_inst_depth_limit=(g->arith_instantiation==2) ? 2 : 1;
  if (g->arith_inst_max_vars<1 || g->arith_inst_max_vars>2) {
    wr_warn(g,"arith_inst_max_vars must be 1 or 2; clamping it");
    if (g->arith_inst_max_vars<1) g->arith_inst_max_vars=1;
    else g->arith_inst_max_vars=2;
  }
  if (g->arith_inst_candidate_limit<1) {
    wr_warn(g,"arith_inst_candidate_limit must be positive; using 1");
    g->arith_inst_candidate_limit=1;
  }
  if (g->arith_inst_candidate_limit>WR_ARITH_MAX_LOCAL_VALUES)
    g->arith_inst_candidate_limit=WR_ARITH_MAX_LOCAL_VALUES;
  if (g->arith_inst_probe_limit<1) {
    wr_warn(g,"arith_inst_probe_limit must be positive; using 1");
    g->arith_inst_probe_limit=1;
  }
  if (g->arith_inst_probe_limit>1024) g->arith_inst_probe_limit=1024;
  if (g->arith_inst_keep_limit<1) {
    wr_warn(g,"arith_inst_keep_limit must be positive; using 1");
    g->arith_inst_keep_limit=1;
  }
  if (g->arith_inst_keep_limit>64) g->arith_inst_keep_limit=64;
  if (g->arith_inst_depth_limit<1) {
    wr_warn(g,"arith_inst_depth_limit must be positive; using 1");
    g->arith_inst_depth_limit=1;
  }
  if (g->arith_inst_depth_limit>16) g->arith_inst_depth_limit=16;
  if (g->arith_inst_global_limit<1) {
    wr_warn(g,"arith_inst_global_limit must be positive; using 1");
    g->arith_inst_global_limit=1;
  }
  if (g->arith_inst_global_limit>1000000) g->arith_inst_global_limit=1000000;
}

void wr_arithinst_reset_run(glb* g) {
  g->arith_inst_probe_active=0;
  g->tmp_arithinst_calc_count=0;
  g->tmp_arithinst_deleted_lits=0;
  g->tmp_arithinst_tautology=0;
  g->arith_inst_global_candidate_count=0;
  g->arith_inst_cache_saturated=0;
  if (g->arith_inst_cache && g->arith_inst_cache_size>0)
    memset(g->arith_inst_cache,0,
           (size_t)g->arith_inst_cache_size*sizeof(wr_arith_cache_entry));
  g->stat_arithinst_clauses_scanned=0;
  g->stat_arithinst_frontiers=0;
  g->stat_arithinst_candidates=0;
  g->stat_arithinst_probes=0;
  g->stat_arithinst_calculated=0;
  g->stat_arithinst_rejected_no_progress=0;
  g->stat_arithinst_rejected_tautology=0;
  g->stat_arithinst_rejected_cache=0;
  g->stat_arithinst_rejected_budget=0;
  g->stat_arithinst_truncated=0;
  g->stat_arithinst_kept=0;
  g->stat_arithinst_proofs=0;
}

static int wr_arith_checked_add(gint a,gint b,gint* out) {
  if ((b>0 && a>(gint)INTPTR_MAX-b) || (b<0 && a<(gint)INTPTR_MIN-b)) return 0;
  *out=a+b;
  return 1;
}

static int wr_arith_checked_sub(gint a,gint b,gint* out) {
  if ((b<0 && a>(gint)INTPTR_MAX+b) || (b>0 && a<(gint)INTPTR_MIN+b)) return 0;
  *out=a-b;
  return 1;
}

static int wr_arith_value_equal(const wr_arith_value* a,const wr_arith_value* b) {
  if (a->kind!=b->kind) return 0;
  if (a->kind==WR_ARITH_VALUE_INT) return a->integer_value==b->integer_value;
  return a->real_value==b->real_value ||
         (a->real_value==0.0 && b->real_value==0.0);
}

static int wr_arith_add_value(glb* g,wr_arith_value* values,int* count,
                              int kind,gint ivalue,unsigned char origin,
                              unsigned short rank) {
  wr_arith_value value;
  int i;
  value.kind=(unsigned char)kind;
  value.origin=origin;
  value.rank=rank;
  value.integer_value=ivalue;
  value.real_value=0.0;
  for(i=0;i<*count;i++) {
    if (wr_arith_value_equal(values+i,&value)) {
      if (rank<values[i].rank) {
        values[i].rank=rank;
        values[i].origin=origin;
      }
      return 0;
    }
  }
  if (*count>=WR_ARITH_MAX_LOCAL_VALUES) {
    ++(g->stat_arithinst_truncated);
    return 0;
  }
  values[*count]=value;
  ++(*count);
  return 1;
}

static int wr_arith_add_real(glb* g,wr_arith_value* values,int* count,
                             double dvalue,unsigned char origin,
                             unsigned short rank) {
  wr_arith_value value;
  int i;
  if (!isfinite(dvalue)) return 0;
  if (dvalue==0.0) dvalue=0.0;
  value.kind=WR_ARITH_VALUE_REAL;
  value.origin=origin;
  value.rank=rank;
  value.integer_value=0;
  value.real_value=dvalue;
  for(i=0;i<*count;i++) {
    if (wr_arith_value_equal(values+i,&value)) {
      if (rank<values[i].rank) {
        values[i].rank=rank;
        values[i].origin=origin;
      }
      return 0;
    }
  }
  if (*count>=WR_ARITH_MAX_LOCAL_VALUES) {
    ++(g->stat_arithinst_truncated);
    return 0;
  }
  values[*count]=value;
  ++(*count);
  return 1;
}

static int wr_arith_decode_number(glb* g,gint term,wr_arith_value* value) {
  int type;
  if (isvar(term)) return 0;
  if (isdatarec(term)) {
    gptr ptr=decode_record(g->db,term);
    gint arg,negated;
    wr_arith_value inner;
    if (wr_computable_termptr(g,ptr)!=COMP_FUN_UMINUS) return 0;
    arg=ptr[RECORD_HEADER_GINTS+g->unify_funarg1pos];
    if (!wr_arith_decode_number(g,arg,&inner)) return 0;
    if (inner.kind==WR_ARITH_VALUE_INT) {
      if (!wr_arith_checked_sub(0,inner.integer_value,&negated)) return 0;
      value->kind=WR_ARITH_VALUE_INT;
      value->integer_value=negated;
      value->real_value=0.0;
    } else {
      if (!isfinite(-inner.real_value)) return 0;
      value->kind=WR_ARITH_VALUE_REAL;
      value->integer_value=0;
      value->real_value=-inner.real_value;
      if (value->real_value==0.0) value->real_value=0.0;
    }
    return 1;
  }
  type=wg_get_encoded_type(g->db,term);
  if (type==WG_INTTYPE) {
    value->kind=WR_ARITH_VALUE_INT;
    value->integer_value=wg_decode_int(g->db,term);
    value->real_value=0.0;
    return 1;
  }
  if (type==WG_DOUBLETYPE) {
    value->kind=WR_ARITH_VALUE_REAL;
    value->integer_value=0;
    value->real_value=wg_decode_double(g->db,term);
    if (!isfinite(value->real_value)) return 0;
    if (value->real_value==0.0) value->real_value=0.0;
    return 1;
  }
  return 0;
}

static int wr_arith_scan_var_index(wr_arith_scan* scan,int var) {
  int i;
  for(i=0;i<scan->variable_count;i++) if (scan->variables[i]==var) return i;
  if (scan->variable_count>=WR_ARITH_MAX_CLAUSE_VARS) return -1;
  scan->variables[scan->variable_count]=var;
  scan->directly_bindable[scan->variable_count]=0;
  return scan->variable_count++;
}

static void wr_arith_collect_vars(glb* g,gint term,int* vars,int* count,int max,
                                  wr_arith_scan* scan) {
  gptr ptr;
  int i,j,start,end,var;
  if (isvar(term)) {
    var=decode_var(term);
    wr_arith_scan_var_index(scan,var);
    for(j=0;j<*count;j++) if (vars[j]==var) return;
    if (*count<max) vars[(*count)++]=var;
    return;
  }
  if (!isdatarec(term)) return;
  ptr=decode_record(g->db,term);
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,ptr);
  for(i=start;i<end;i++) wr_arith_collect_vars(g,ptr[i],vars,count,max,scan);
}

static void wr_arith_add_number(glb* g,wr_arith_scan* scan,
                                const wr_arith_value* value,
                                unsigned char origin,unsigned short rank) {
  gint prev,next;
  if (value->kind==WR_ARITH_VALUE_INT) {
    wr_arith_add_value(g,scan->values,&scan->value_count,WR_ARITH_VALUE_INT,
                       value->integer_value,origin,rank);
    if (origin==WR_ARITH_ORIGIN_CLAUSE || origin==WR_ARITH_ORIGIN_OCCURRENCE) {
      if (wr_arith_checked_sub(value->integer_value,1,&prev))
        wr_arith_add_value(g,scan->values,&scan->value_count,WR_ARITH_VALUE_INT,
                           prev,WR_ARITH_ORIGIN_CLOSURE,35);
      if (wr_arith_checked_add(value->integer_value,1,&next))
        wr_arith_add_value(g,scan->values,&scan->value_count,WR_ARITH_VALUE_INT,
                           next,WR_ARITH_ORIGIN_CLOSURE,35);
    }
  } else {
    wr_arith_add_real(g,scan->values,&scan->value_count,value->real_value,origin,rank);
  }
}

static void wr_arith_add_boundary(glb* g,wr_arith_scan* scan,
                                  const wr_arith_value* value) {
  gint neighbor;
  if (value->kind==WR_ARITH_VALUE_INT) {
    wr_arith_add_value(g,scan->values,&scan->value_count,WR_ARITH_VALUE_INT,
                       value->integer_value,WR_ARITH_ORIGIN_BOUNDARY,20);
    if (wr_arith_checked_sub(value->integer_value,1,&neighbor))
      wr_arith_add_value(g,scan->values,&scan->value_count,WR_ARITH_VALUE_INT,
                         neighbor,WR_ARITH_ORIGIN_NEIGHBOR,30);
    if (wr_arith_checked_add(value->integer_value,1,&neighbor))
      wr_arith_add_value(g,scan->values,&scan->value_count,WR_ARITH_VALUE_INT,
                         neighbor,WR_ARITH_ORIGIN_NEIGHBOR,30);
  } else {
    wr_arith_add_real(g,scan->values,&scan->value_count,value->real_value,
                      WR_ARITH_ORIGIN_BOUNDARY,20);
    wr_arith_add_real(g,scan->values,&scan->value_count,value->real_value-1.0,
                      WR_ARITH_ORIGIN_NEIGHBOR,30);
    wr_arith_add_real(g,scan->values,&scan->value_count,value->real_value+1.0,
                      WR_ARITH_ORIGIN_NEIGHBOR,30);
  }
}

static void wr_arith_add_frontier(glb* g,wr_arith_scan* scan,int comp,
                                  int literal,int negative,gint term) {
  wr_arith_frontier* frontier;
  int vars[WR_ARITH_MAX_CLAUSE_VARS],count=0,i,j,same;
  wr_arith_collect_vars(g,term,vars,&count,WR_ARITH_MAX_CLAUSE_VARS,scan);
  if (count<1) return;
  if (count>WR_ARITH_MAX_TUPLE_VALUES) return;
  for(i=0;i<scan->frontier_count;i++) {
    frontier=scan->frontiers+i;
    if (frontier->variable_count!=count) continue;
    same=1;
    for(j=0;j<count;j++) if (frontier->variables[j]!=vars[j]) same=0;
    if (same) return;
  }
  if (scan->frontier_count>=WR_ARITH_MAX_FRONTIERS) {
    scan->truncated=1;
    ++(g->stat_arithinst_truncated);
    return;
  }
  frontier=scan->frontiers+scan->frontier_count++;
  frontier->comp=comp;
  frontier->literal=literal;
  frontier->negative=negative;
  frontier->variable_count=count;
  for(i=0;i<count;i++) frontier->variables[i]=vars[i];
}

static void wr_arith_scan_comparison(glb* g,gptr ptr,wr_arith_scan* scan) {
  gint a,b;
  wr_arith_value value;
  a=ptr[RECORD_HEADER_GINTS+g->unify_funarg1pos];
  b=ptr[RECORD_HEADER_GINTS+g->unify_funarg2pos];
  if (wr_arith_decode_number(g,a,&value)) wr_arith_add_boundary(g,scan,&value);
  if (wr_arith_decode_number(g,b,&value)) wr_arith_add_boundary(g,scan,&value);
}

static void wr_arith_scan_term(glb* g,gint term,int literal,int negative,
                               wr_arith_scan* scan) {
  gptr ptr;
  int i,start,end,comp;
  wr_arith_value value;
  if (wr_arith_decode_number(g,term,&value)) {
    wr_arith_add_number(g,scan,&value,WR_ARITH_ORIGIN_CLAUSE,40);
    return;
  }
  if (isvar(term)) {
    wr_arith_scan_var_index(scan,decode_var(term));
    return;
  }
  if (!isdatarec(term)) return;
  ptr=decode_record(g->db,term);
  comp=wr_computable_termptr(g,ptr);
  if (wr_computation_is_arithmetic(comp)) {
    wr_arith_add_frontier(g,scan,comp,literal,negative,term);
    if (comp==COMP_FUN_LESS || comp==COMP_FUN_LESSEQ ||
        comp==COMP_FUN_GREATER || comp==COMP_FUN_GREATEREQ)
      wr_arith_scan_comparison(g,ptr,scan);
  }
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,ptr);
  for(i=start;i<end;i++) wr_arith_scan_term(g,ptr[i],literal,negative,scan);
}

static void wr_arith_inverse_expression(glb* g,gint expression,
                                        const wr_arith_value* target,
                                        wr_arith_scan* scan) {
  gptr ptr;
  gint a,b,result;
  int comp,vars[3],count=0;
  wr_arith_value constant;
  double tdouble,cdouble,rdouble;
  if (!isdatarec(expression)) return;
  ptr=decode_record(g->db,expression);
  comp=wr_computable_termptr(g,ptr);
  if (comp!=COMP_FUN_PLUS && comp!=COMP_FUN_MINUS && comp!=COMP_FUN_MULT) return;
  wr_arith_collect_vars(g,expression,vars,&count,3,scan);
  if (count!=1) return;
  a=ptr[RECORD_HEADER_GINTS+g->unify_funarg1pos];
  b=ptr[RECORD_HEADER_GINTS+g->unify_funarg2pos];
  if (isvar(a) && wr_arith_decode_number(g,b,&constant) &&
      target->kind==WR_ARITH_VALUE_INT && constant.kind==WR_ARITH_VALUE_INT) {
    if (comp==COMP_FUN_PLUS && wr_arith_checked_sub(target->integer_value,constant.integer_value,&result))
      wr_arith_add_value(g,scan->values,&scan->value_count,WR_ARITH_VALUE_INT,result,WR_ARITH_ORIGIN_INVERSE,10);
    else if (comp==COMP_FUN_MINUS && wr_arith_checked_add(target->integer_value,constant.integer_value,&result))
      wr_arith_add_value(g,scan->values,&scan->value_count,WR_ARITH_VALUE_INT,result,WR_ARITH_ORIGIN_INVERSE,10);
    else if (comp==COMP_FUN_MULT && constant.integer_value!=0 &&
             !(target->integer_value==(gint)INTPTR_MIN && constant.integer_value==-1) &&
             target->integer_value%constant.integer_value==0)
      wr_arith_add_value(g,scan->values,&scan->value_count,WR_ARITH_VALUE_INT,
                         target->integer_value/constant.integer_value,
                         WR_ARITH_ORIGIN_INVERSE,10);
  } else if (isvar(b) && wr_arith_decode_number(g,a,&constant) &&
             target->kind==WR_ARITH_VALUE_INT && constant.kind==WR_ARITH_VALUE_INT) {
    if (comp==COMP_FUN_PLUS && wr_arith_checked_sub(target->integer_value,constant.integer_value,&result))
      wr_arith_add_value(g,scan->values,&scan->value_count,WR_ARITH_VALUE_INT,result,WR_ARITH_ORIGIN_INVERSE,10);
    else if (comp==COMP_FUN_MINUS && wr_arith_checked_sub(constant.integer_value,target->integer_value,&result))
      wr_arith_add_value(g,scan->values,&scan->value_count,WR_ARITH_VALUE_INT,result,WR_ARITH_ORIGIN_INVERSE,10);
    else if (comp==COMP_FUN_MULT && constant.integer_value!=0 &&
             !(target->integer_value==(gint)INTPTR_MIN && constant.integer_value==-1) &&
             target->integer_value%constant.integer_value==0)
      wr_arith_add_value(g,scan->values,&scan->value_count,WR_ARITH_VALUE_INT,
                         target->integer_value/constant.integer_value,
                         WR_ARITH_ORIGIN_INVERSE,10);
  } else if (isvar(a) && wr_arith_decode_number(g,b,&constant)) {
    tdouble=target->kind==WR_ARITH_VALUE_INT ? (double)target->integer_value : target->real_value;
    cdouble=constant.kind==WR_ARITH_VALUE_INT ? (double)constant.integer_value : constant.real_value;
    if (comp==COMP_FUN_PLUS) rdouble=tdouble-cdouble;
    else if (comp==COMP_FUN_MINUS) rdouble=tdouble+cdouble;
    else {
      if (cdouble==0.0) return;
      rdouble=tdouble/cdouble;
    }
    wr_arith_add_real(g,scan->values,&scan->value_count,rdouble,
                      WR_ARITH_ORIGIN_INVERSE,10);
  } else if (isvar(b) && wr_arith_decode_number(g,a,&constant)) {
    tdouble=target->kind==WR_ARITH_VALUE_INT ? (double)target->integer_value : target->real_value;
    cdouble=constant.kind==WR_ARITH_VALUE_INT ? (double)constant.integer_value : constant.real_value;
    if (comp==COMP_FUN_PLUS) rdouble=tdouble-cdouble;
    else if (comp==COMP_FUN_MINUS) rdouble=cdouble-tdouble;
    else {
      if (cdouble==0.0) return;
      rdouble=tdouble/cdouble;
    }
    wr_arith_add_real(g,scan->values,&scan->value_count,rdouble,
                      WR_ARITH_ORIGIN_INVERSE,10);
  }
}

static void wr_arith_scan_equality_hints(glb* g,gint atom,wr_arith_scan* scan) {
  gptr ptr;
  gint a,b;
  wr_arith_value target;
  if (!isdatarec(atom)) return;
  ptr=decode_record(g->db,atom);
  if (wr_computable_termptr(g,ptr)!=COMP_FUN_EQUAL) return;
  a=ptr[RECORD_HEADER_GINTS+g->unify_funarg1pos];
  b=ptr[RECORD_HEADER_GINTS+g->unify_funarg2pos];
  if (wr_arith_decode_number(g,b,&target)) wr_arith_inverse_expression(g,a,&target,scan);
  if (wr_arith_decode_number(g,a,&target)) wr_arith_inverse_expression(g,b,&target,scan);
}

static void wr_arith_mark_direct_bindable(glb* g,gint atom,wr_arith_scan* scan) {
  gptr ptr;
  int i,index,start,end;
  if (!isdatarec(atom) || wr_answer_lit(g,atom)) return;
  ptr=decode_record(g->db,atom);
  if (wr_computable_termptr(g,ptr)!=0) return;
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,ptr);
  for(i=start;i<end;i++) {
    if (isvar(ptr[i])) {
      index=wr_arith_scan_var_index(scan,decode_var(ptr[i]));
      if (index>=0) scan->directly_bindable[index]=1;
    }
  }
}

static void wr_arith_scan_clause(glb* g,gptr cl,wr_arith_scan* scan) {
  int i,len,negative;
  gint atom,meta;
  memset(scan,0,sizeof(*scan));
  if (wg_rec_is_rule_clause(g->db,cl)) {
    len=wg_count_clause_atoms(g->db,cl);
    for(i=0;i<len;i++) {
      meta=wg_get_rule_clause_atom_meta(g->db,cl,i);
      atom=wg_get_rule_clause_atom(g->db,cl,i);
      negative=wg_atom_meta_is_neg(g->db,meta)!=0;
      wr_arith_scan_term(g,atom,i,negative,scan);
      wr_arith_scan_equality_hints(g,atom,scan);
      wr_arith_mark_direct_bindable(g,atom,scan);
    }
  } else {
    atom=encode_record(g->db,cl);
    wr_arith_scan_term(g,atom,0,0,scan);
    wr_arith_scan_equality_hints(g,atom,scan);
    wr_arith_mark_direct_bindable(g,atom,scan);
  }
}

static int wr_arith_value_cmp(const wr_arith_value* a,const wr_arith_value* b) {
  long double aa,bb;
  if (a->rank!=b->rank) return a->rank<b->rank ? -1 : 1;
  aa=a->kind==WR_ARITH_VALUE_INT ? fabsl((long double)a->integer_value) : fabsl(a->real_value);
  bb=b->kind==WR_ARITH_VALUE_INT ? fabsl((long double)b->integer_value) : fabsl(b->real_value);
  if (aa!=bb) return aa<bb ? -1 : 1;
  if (a->kind!=b->kind) return a->kind<b->kind ? -1 : 1;
  if (a->kind==WR_ARITH_VALUE_INT) {
    if (a->integer_value==b->integer_value) return 0;
    return a->integer_value<b->integer_value ? -1 : 1;
  }
  if (a->real_value==b->real_value) return 0;
  return a->real_value<b->real_value ? -1 : 1;
}

static void wr_arith_sort_values(wr_arith_value* values,int count) {
  int i,j;
  wr_arith_value value;
  for(i=1;i<count;i++) {
    value=values[i];
    for(j=i;j>0 && wr_arith_value_cmp(&value,&values[j-1])<0;j--)
      values[j]=values[j-1];
    values[j]=value;
  }
}

static void wr_arith_global_add(glb* g,const wr_arith_value* source,
                                unsigned short rank) {
  wr_arith_value value=*source;
  int i,worst=0;
  value.origin=WR_ARITH_ORIGIN_GLOBAL;
  value.rank=rank;
  for(i=0;i<g->arith_inst_global_candidate_count;i++) {
    if (wr_arith_value_equal(g->arith_inst_global_candidates+i,&value)) {
      if (rank<g->arith_inst_global_candidates[i].rank)
        g->arith_inst_global_candidates[i].rank=rank;
      return;
    }
  }
  if (g->arith_inst_global_candidate_count<WR_ARITH_MAX_GLOBAL_VALUES) {
    g->arith_inst_global_candidates[g->arith_inst_global_candidate_count++]=value;
    return;
  }
  for(i=1;i<WR_ARITH_MAX_GLOBAL_VALUES;i++)
    if (wr_arith_value_cmp(g->arith_inst_global_candidates+worst,
                           g->arith_inst_global_candidates+i)<0) worst=i;
  if (wr_arith_value_cmp(&value,g->arith_inst_global_candidates+worst)<0)
    g->arith_inst_global_candidates[worst]=value;
}

static void wr_arith_complete_candidates(glb* g,wr_arith_scan* scan) {
  int i,j,initial_count=scan->value_count;
  double midpoint;
  for(i=0;i<initial_count;i++) {
    if (scan->values[i].kind!=WR_ARITH_VALUE_REAL) continue;
    for(j=i+1;j<initial_count;j++) {
      if (scan->values[j].kind!=WR_ARITH_VALUE_REAL) continue;
      midpoint=(scan->values[i].real_value/2.0)+(scan->values[j].real_value/2.0);
      wr_arith_add_real(g,scan->values,&scan->value_count,midpoint,
                        WR_ARITH_ORIGIN_NEIGHBOR,30);
      break;
    }
    break;
  }
  for(i=0;i<scan->value_count;i++) wr_arith_global_add(g,scan->values+i,70);
  for(i=0;i<g->arith_inst_global_candidate_count;i++) {
    if (g->arith_inst_global_candidates[i].kind==WR_ARITH_VALUE_INT)
      wr_arith_add_value(g,scan->values,&scan->value_count,WR_ARITH_VALUE_INT,
        g->arith_inst_global_candidates[i].integer_value,WR_ARITH_ORIGIN_GLOBAL,70);
    else
      wr_arith_add_real(g,scan->values,&scan->value_count,
        g->arith_inst_global_candidates[i].real_value,WR_ARITH_ORIGIN_GLOBAL,70);
  }
  wr_arith_add_value(g,scan->values,&scan->value_count,WR_ARITH_VALUE_INT,0,
                     WR_ARITH_ORIGIN_FIXED,80);
  wr_arith_add_value(g,scan->values,&scan->value_count,WR_ARITH_VALUE_INT,1,
                     WR_ARITH_ORIGIN_FIXED,80);
  wr_arith_add_value(g,scan->values,&scan->value_count,WR_ARITH_VALUE_INT,-1,
                     WR_ARITH_ORIGIN_FIXED,80);
  wr_arith_sort_values(scan->values,scan->value_count);
  g->stat_arithinst_candidates+=scan->value_count;
}

static int wr_arith_frontier_bindable(wr_arith_scan* scan,
                                      const wr_arith_frontier* frontier) {
  int i,j;
  for(i=0;i<frontier->variable_count;i++) {
    for(j=0;j<scan->variable_count;j++) {
      if (scan->variables[j]==frontier->variables[i] && scan->directly_bindable[j])
        return 1;
    }
  }
  return 0;
}

static uint64_t wr_arith_value_bits(const wr_arith_value* value) {
  uint64_t bits=0;
  if (value->kind==WR_ARITH_VALUE_INT) return (uint64_t)(uintptr_t)value->integer_value;
  memcpy(&bits,&value->real_value,sizeof(bits));
  return bits;
}

static uint64_t wr_arith_hash_mix(uint64_t h,uint64_t x) {
  h^=x+UINT64_C(0x9e3779b97f4a7c15)+(h<<6)+(h>>2);
  return h;
}

static int wr_arith_cache_seen(glb* g,gptr parent,int var_count,const int* vars,
                               const wr_arith_value* values) {
  wr_arith_cache_entry key,*entry;
  uint64_t hash;
  int i,step,slot;
  if (!g->arith_inst_cache || g->arith_inst_cache_size<=0 ||
      g->arith_inst_cache_saturated) return 0;
  memset(&key,0,sizeof(key));
  key.used=1;
  key.var_count=(unsigned char)var_count;
  /* Pointer identity is unambiguous even when local and attached-KB clause
     history identifiers overlap, or history storage is disabled. */
  key.parent_id=(gint)(uintptr_t)parent;
  hash=(uint64_t)(uintptr_t)key.parent_id;
  for(i=0;i<var_count;i++) {
    key.variables[i]=vars[i];
    key.kinds[i]=values[i].kind;
    key.value_bits[i]=wr_arith_value_bits(values+i);
    hash=wr_arith_hash_mix(hash,(uint64_t)(unsigned int)vars[i]);
    hash=wr_arith_hash_mix(hash,key.value_bits[i] ^ key.kinds[i]);
  }
  slot=(int)(hash%(uint64_t)g->arith_inst_cache_size);
  for(step=0;step<g->arith_inst_cache_size;step++) {
    entry=g->arith_inst_cache+slot;
    if (!entry->used) {
      *entry=key;
      return 0;
    }
    if (entry->parent_id==key.parent_id && entry->var_count==key.var_count &&
        !memcmp(entry->variables,key.variables,sizeof(key.variables)) &&
        !memcmp(entry->kinds,key.kinds,sizeof(key.kinds)) &&
        !memcmp(entry->value_bits,key.value_bits,sizeof(key.value_bits))) return 1;
    slot++;
    if (slot==g->arith_inst_cache_size) slot=0;
  }
  g->arith_inst_cache_saturated=1;
  ++(g->stat_arithinst_rejected_budget);
  return 0;
}

static int wr_arith_ancestry_depth(glb* g,gptr cl) {
  gint history,head,parent;
  gptr hptr;
  int depth=0,tag;
  while(cl) {
    history=wr_get_history(g,cl);
    if (!history || wg_get_encoded_type(g->db,history)!=WG_RECORDTYPE) break;
    hptr=otp(g->db,history);
    if (wg_get_record_len(g->db,hptr)<=HISTORY_DERIVATION_TAG_POS) break;
    head=wr_get_history_record_field(g,hptr,HISTORY_DERIVATION_TAG_POS);
    if (wg_get_encoded_type(g->db,head)!=WG_INTTYPE) break;
    tag=wr_get_history_tag(g,wg_decode_int(g->db,head));
    if (tag!=WR_HISTORY_TAG_ARITHINST) break;
    depth++;
    parent=wr_get_history_record_field(g,hptr,HISTORY_ARITH_PARENT_POS);
    if (!parent || wg_get_encoded_type(g->db,parent)!=WG_RECORDTYPE) break;
    cl=otp(g->db,parent);
  }
  return depth;
}

static gint wr_arith_encode_candidate(glb* g,const wr_arith_value* value) {
  if (!g->local_db) return WG_ILLEGAL;
  if (value->kind==WR_ARITH_VALUE_INT)
    return wg_encode_int(g->local_db,value->integer_value);
  if (value->kind==WR_ARITH_VALUE_REAL && isfinite(value->real_value))
    return wg_encode_double(g->local_db,value->real_value);
  return WG_ILLEGAL;
}

static int wr_arith_staged_answer(glb* g,gptr rptr,int rpos) {
  int i;
  if (!rpos) return 0;
  for(i=0;i<rpos;i++) if (!wr_answer_lit(g,rptr[i*LIT_WIDTH+LIT_ATOM_POS])) return 0;
  return 1;
}

static int wr_arith_has_any_var(glb* g,gint term) {
  gptr ptr;
  int i,start,end;
  if (isvar(term)) return 1;
  if (!isdatarec(term)) return 0;
  ptr=decode_record(g->db,term);
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,ptr);
  for(i=start;i<end;i++) if (wr_arith_has_any_var(g,ptr[i])) return 1;
  return 0;
}

static int wr_arith_count_term_frontiers(glb* g,gint term) {
  gptr ptr;
  int i,start,end,count=0,comp;
  if (!isdatarec(term)) return 0;
  ptr=decode_record(g->db,term);
  comp=wr_computable_termptr(g,ptr);
  if (wr_computation_is_arithmetic(comp) && wr_arith_has_any_var(g,term)) count++;
  start=wr_term_unify_startpos(g);
  end=wr_term_unify_endpos(g,ptr);
  for(i=start;i<end;i++) count+=wr_arith_count_term_frontiers(g,ptr[i]);
  return count;
}

static int wr_arith_count_clause_frontiers(glb* g,gptr cl) {
  int i,len,count=0;
  if (wg_rec_is_rule_clause(g->db,cl)) {
    len=wg_count_clause_atoms(g->db,cl);
    for(i=0;i<len;i++)
      count+=wr_arith_count_term_frontiers(g,wg_get_rule_clause_atom(g->db,cl,i));
  } else count=wr_arith_count_term_frontiers(g,encode_record(g->db,cl));
  return count;
}

static int wr_arith_count_staged_frontiers(glb* g,gptr rptr,int rpos) {
  int i,count=0;
  for(i=0;i<rpos;i++)
    count+=wr_arith_count_term_frontiers(g,rptr[i*LIT_WIDTH+LIT_ATOM_POS]);
  return count;
}

static int wr_arith_store_result(glb* g,gptr parent,gptr rptr,int rpos,
                                 int var_count,const int* vars,const gint* values) {
  int tmp,ruleflag,weight,size,depth,length;
  gint history,resmeta,hash,clhash=0,initial_next;
  gptr res;
  double avg;
  gint cl_metablock[CLMETABLOCK_ELS];
  gptr cuts=NULL;

  ++(g->stat_derived_cl);
  if (!rpos) {
    history=wr_build_arithinst_history(g,parent,var_count,vars,values,NULL);
    if (!history || history==WG_ILLEGAL) return 0;
    g->proof_history=history;
    g->proof_found=1;
    wr_register_answer(g,NULL,g->proof_history);
    ++(g->stat_arithinst_proofs);
    return 1;
  }
  tmp=wr_derived_cl_cut_and_subsume(g,rptr,rpos,&clhash);
  if (tmp<0) return 0;
  if (tmp>0) {
    cuts=g->cut_clvec;
    wr_process_resolve_result_remove_cuts(g,rptr,&rpos,tmp);
    ++(g->stat_derived_cut);
    if (!rpos) {
      history=wr_build_arithinst_history(g,parent,var_count,vars,values,cuts);
      if (!history || history==WG_ILLEGAL) return 0;
      g->proof_history=history;
      g->proof_found=1;
      wr_register_answer(g,NULL,g->proof_history);
      ++(g->stat_arithinst_proofs);
      return 1;
    }
  }
  ruleflag=wr_process_resolve_result_isrulecl(g,rptr,rpos);
  wr_process_resolve_result_setupquecopy(g);
  initial_next=CVEC_NEXT(g->build_buffer);
  history=wr_build_arithinst_history(g,parent,var_count,vars,values,cuts);
  if (!history || history==WG_ILLEGAL) {
    CVEC_NEXT(g->build_buffer)=initial_next;
    return 0;
  }
  res=wr_derived_build_cl_from_initial_cl(g,rptr,rpos,ruleflag,history);
  if (!res) {
    CVEC_NEXT(g->build_buffer)=initial_next;
    return 0;
  }
  wr_sort_cl(g,res);
  if (wr_cl_derived_is_answer(g,res)>0) {
    wr_register_answer(g,res,history);
    g->proof_found=1;
    g->proof_history=history;
    ++(g->stat_arithinst_kept);
    return 1;
  }
  if (g->print_derived_cl) {
    wr_printf("\n+ derived by arithinst: ");
    wr_print_clause(g,res);
  } else if (g->print_derived) wr_print_clause_via_buf(g,res);
  weight=wr_calc_clause_weight(g,res,&size,&depth,&length);
  ++(g->stat_kept_cl);
  avg=g->avg_kept_weight;
  avg+=(weight-avg)/((g->stat_kept_cl)+1);
  g->avg_kept_weight=avg;
  resmeta=wr_calc_clause_meta(g,res,cl_metablock);
  hash=wr_add_cl_to_unithash(g,res,resmeta);
  if (hash<0) wr_push_prop_clausehash(g,rotp(g,g->prop_hash_clauses),clhash,res);
  if (g->propagate) {
    if (hash>=0) {
      wr_propagate_unit(g,res,resmeta,hash,parent);
      if (g->proof_found || g->alloc_err) return 1;
    } else wr_store_atom_occurrences(g,res);
  }
  if (g->use_strong_unit_cutoff) wr_cl_store_res_units(g,res);
  wr_push_cl_clpick_queues(g,g->clpick_queues,res,weight);
  wr_cl_create_propinst(g,res);
  ++(g->stat_arithinst_kept);
  return 1;
}

static int wr_arith_probe(glb* g,gptr parent,int var_count,const int* vars,
                          const wr_arith_value* candidates) {
  int i,atom_count,rlen,rpos=0,cutpos=0,ok,progress,kept=0,setup=0;
  gint encoded[WR_ARITH_MAX_TUPLE_VALUES];
  gptr rptr;

  if (var_count<1 || var_count>WR_ARITH_MAX_TUPLE_VALUES) return 0;
  if (g->stat_arithinst_probes>=g->arith_inst_global_limit) {
    ++(g->stat_arithinst_rejected_budget);
    return 0;
  }
  if (wr_arith_cache_seen(g,parent,var_count,vars,candidates)) {
    ++(g->stat_arithinst_rejected_cache);
    return 0;
  }
  wr_clear_varstack(g,g->varstack);
  g->tmp_unify_vc=((gptr)g->varstack)+1;
  for(i=0;i<var_count;i++) {
    if (vars[i]<0 || vars[i]>=NROF_VARBANKS*NROF_VARSINBANK) goto cleanup;
    if (g->varbanks[vars[i]]!=UNASSIGNED) goto cleanup;
    encoded[i]=wr_arith_encode_candidate(g,candidates+i);
    if (!encoded[i] || encoded[i]==WG_ILLEGAL) {
      wr_alloc_err(g,"could not encode numeric-instantiation candidate");
      goto cleanup;
    }
    SETVAR(encode_var(vars[i]),encoded[i],g->varbanks,g->varstack,g->tmp_unify_vc);
  }
  atom_count=wg_rec_is_rule_clause(g->db,parent) ?
    wg_count_clause_atoms(g->db,parent) : 1;
  if (atom_count<1 || atom_count>INT_MAX/LIT_WIDTH) goto cleanup;
  rlen=atom_count*LIT_WIDTH;
  g->derived_termbuf[1]=2;
  rptr=wr_alloc_from_cvec(g,g->derived_termbuf,rlen);
  if (!rptr) {
    ++(g->stat_internlimit_discarded_cl);
    wr_alloc_err(g,"could not allocate numeric-instantiation staging vector");
    goto cleanup;
  }
  wr_process_resolve_result_setupsubst(g);
  setup=1;
  g->tmp_arithinst_calc_count=0;
  g->tmp_arithinst_deleted_lits=0;
  g->tmp_arithinst_tautology=0;
  g->arith_inst_probe_active=1;
  ++(g->stat_arithinst_probes);
  ok=wr_process_instgen_result_aux(g,parent,0,atom_count,rptr,&rpos,&cutpos,-1,0);
  g->arith_inst_probe_active=0;
  wr_process_resolve_result_cleanupsubst(g);
  setup=0;
  if (g->tmp_arithinst_calc_count>0) ++(g->stat_arithinst_calculated);
  if (!ok || g->tmp_arithinst_tautology) {
    ++(g->stat_arithinst_rejected_tautology);
    goto cleanup;
  }
  progress=(g->tmp_arithinst_calc_count>0) &&
    (!rpos || g->tmp_arithinst_deleted_lits>0 ||
     wr_arith_staged_answer(g,rptr,rpos) ||
     (rpos==1 && wr_ground_term(g,rptr[LIT_ATOM_POS])) ||
     (g->arith_instantiation>=2 &&
      wr_arith_count_staged_frontiers(g,rptr,rpos)<
      wr_arith_count_clause_frontiers(g,parent)));
  if (!progress) {
    ++(g->stat_arithinst_rejected_no_progress);
    goto cleanup;
  }
  wr_clear_varstack(g,g->varstack);
  kept=wr_arith_store_result(g,parent,rptr,rpos,var_count,vars,encoded);
  return kept;

cleanup:
  g->arith_inst_probe_active=0;
  if (setup) wr_process_resolve_result_cleanupsubst(g);
  wr_clear_varstack(g,g->varstack);
  return kept;
}

static int wr_arith_same_vars(const wr_arith_frontier* a,
                              const wr_arith_frontier* b) {
  int i;
  if (a->variable_count!=b->variable_count) return 0;
  for(i=0;i<a->variable_count;i++) if (a->variables[i]!=b->variables[i]) return 0;
  return 1;
}

int wr_generate_arith_instances(glb* g,gptr cl) {
  wr_arith_scan scan;
  wr_arith_frontier* frontier;
  wr_arith_value tuple[WR_ARITH_MAX_TUPLE_VALUES];
  int f,previous,i,j,sum,limit,clause_probes=0,kept=0;

  if (!g->arith_instantiation || !g->use_comp_funs_strat ||
      !g->use_comp_arithmetic || !g->have_nonground_arithmetic) return 0;
  if (!g->arith_inst_cache && !g->arith_inst_cache_saturated) {
    g->arith_inst_cache=(wr_arith_cache_entry*)sys_calloc(
      WR_ARITH_CACHE_SIZE,sizeof(wr_arith_cache_entry));
    if (g->arith_inst_cache) g->arith_inst_cache_size=WR_ARITH_CACHE_SIZE;
    else g->arith_inst_cache_saturated=1;
  }
  if (wr_arith_ancestry_depth(g,cl)>=g->arith_inst_depth_limit) {
    ++(g->stat_arithinst_rejected_budget);
    return 0;
  }
  ++(g->stat_arithinst_clauses_scanned);
  wr_arith_scan_clause(g,cl,&scan);
  wr_arith_complete_candidates(g,&scan);
  g->stat_arithinst_frontiers+=scan.frontier_count;
  if (!scan.frontier_count || !scan.value_count) return 0;
  limit=scan.value_count;
  if (limit>g->arith_inst_candidate_limit) limit=g->arith_inst_candidate_limit;
  for(f=0;f<scan.frontier_count;f++) {
    frontier=scan.frontiers+f;
    if (frontier->variable_count>g->arith_inst_max_vars) continue;
    if (wr_arith_frontier_bindable(&scan,frontier)) continue;
    previous=0;
    for(i=0;i<f;i++) if (wr_arith_same_vars(scan.frontiers+i,frontier)) previous=1;
    if (previous) continue;
    if (frontier->variable_count==1) {
      for(i=0;i<limit;i++) {
        if (clause_probes>=g->arith_inst_probe_limit ||
            g->stat_arithinst_probes>=g->arith_inst_global_limit ||
            kept>=g->arith_inst_keep_limit || g->proof_found) break;
        tuple[0]=scan.values[i];
        kept+=wr_arith_probe(g,cl,1,frontier->variables,tuple);
        clause_probes++;
      }
    } else if (g->arith_instantiation>=2 && frontier->variable_count==2) {
      for(sum=0;sum<=2*(limit-1);sum++) {
        for(i=0;i<limit;i++) {
          j=sum-i;
          if (j<0 || j>=limit) continue;
          if (clause_probes>=g->arith_inst_probe_limit ||
              g->stat_arithinst_probes>=g->arith_inst_global_limit ||
              kept>=g->arith_inst_keep_limit || g->proof_found) break;
          tuple[0]=scan.values[i];
          tuple[1]=scan.values[j];
          kept+=wr_arith_probe(g,cl,2,frontier->variables,tuple);
          clause_probes++;
        }
        if (clause_probes>=g->arith_inst_probe_limit ||
            g->stat_arithinst_probes>=g->arith_inst_global_limit ||
            kept>=g->arith_inst_keep_limit || g->proof_found) break;
      }
    }
    if (clause_probes>=g->arith_inst_probe_limit ||
        g->stat_arithinst_probes>=g->arith_inst_global_limit ||
        kept>=g->arith_inst_keep_limit || g->proof_found) break;
  }
  if ((clause_probes>=g->arith_inst_probe_limit ||
       g->stat_arithinst_probes>=g->arith_inst_global_limit) && !g->proof_found)
    ++(g->stat_arithinst_rejected_budget);
  return kept;
}
