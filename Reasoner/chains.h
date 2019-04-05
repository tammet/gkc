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

 /** @file chain.h
 * Chain and other non-direct resolution,
 */


#ifndef DEFINED_CHAIN_H
#define DEFINED_CHAIN_H

/* ==== Includes ==== */

#include "glb.h"


/* chain encoding:

 -ostrich(X) | bird(X)   (def)
 -bird(X) | canfly(X)    (observe)
 -canfly(X) | -living(X)  | haswings(X)    (observe)

  ostrich(osty)
  ... can unify with ...
  -canfly(X)  | -living(X)  | haswings(X)

  if given cl
  ostrich(osty) we look for active predicates up

    -ostrich
    -bird
    -canfly

  if given cl
  -canfly(X)  | -living(X)  | haswings(X) we look for active predicates down

    canfly
    bird
    ostrich

  we need to find:
    * predicate up /down 
    * the confidence of chain 
    * the rules in the chain (we can compute confidence from this)
  
  we put onchain flag to each predicate on a taxonomy chain to disable useless search

  option0:
    double pointer chains
    * you search uo through pointer chains
    * you search down through pointer chains

  option1:
    * when you add to active you add all up-direction versions to the active
    * when you add to unit you add all up-direction versions to unit
  
  option2:
    bitmatrix 100 000 x 100 000 ?
    20 000 000 000 cells, each 1 bit then ca 2 000 000 000 bytes

  option3:
    you add an up-vector of all up predicates to a predicate
    you add a down-vector of all down predicates to a predicate
  
  

*/

/* ==== Global defines ==== */

void check_process_chain_clause(glb* g, gptr cl);

/* ==== Protos ==== */


#endif
