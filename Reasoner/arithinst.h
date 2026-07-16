/* Bounded numeric instantiation for non-ground arithmetic clauses. */

#ifndef DEFINED_ARITHINST_H
#define DEFINED_ARITHINST_H

#include <stdint.h>
#include "types.h"

struct glb;

#define WR_ARITH_MAX_FRONTIERS 32
#define WR_ARITH_MAX_CLAUSE_VARS 32
#define WR_ARITH_MAX_LOCAL_VALUES 32
#define WR_ARITH_MAX_GLOBAL_VALUES 32
#define WR_ARITH_MAX_TUPLE_VALUES 2
#define WR_ARITH_CACHE_SIZE 4096

#define WR_ARITH_VALUE_INT 1
#define WR_ARITH_VALUE_REAL 2

#define WR_ARITH_ORIGIN_INVERSE 1
#define WR_ARITH_ORIGIN_BOUNDARY 2
#define WR_ARITH_ORIGIN_NEIGHBOR 3
#define WR_ARITH_ORIGIN_OCCURRENCE 4
#define WR_ARITH_ORIGIN_CLAUSE 5
#define WR_ARITH_ORIGIN_EVALUATED 6
#define WR_ARITH_ORIGIN_GLOBAL 7
#define WR_ARITH_ORIGIN_FIXED 8
#define WR_ARITH_ORIGIN_CLOSURE 9

typedef struct wr_arith_value {
  unsigned char kind;
  unsigned char origin;
  unsigned short rank;
  gint integer_value;
  double real_value;
} wr_arith_value;

typedef struct wr_arith_cache_entry {
  unsigned char used;
  unsigned char var_count;
  unsigned char kinds[WR_ARITH_MAX_TUPLE_VALUES];
  unsigned char reserved;
  gint parent_id;
  int variables[WR_ARITH_MAX_TUPLE_VALUES];
  uint64_t value_bits[WR_ARITH_MAX_TUPLE_VALUES];
} wr_arith_cache_entry;

void wr_arithinst_configure(struct glb* g);
void wr_arithinst_reset_run(struct glb* g);
int wr_generate_arith_instances(struct glb* g, gptr cl);

#endif
