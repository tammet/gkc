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


%{

#include <stdio.h>
#include "dbotterparse.h"

//#define PP ((void*)(((* parse_parm)parm)->foo))
#define PP (parm)

%}


/*
** 'pure_parser' tells bison to use no global variables and create a
** reentrant parser.
*/ 

%pure-parser
%parse-param {parse_parm *parm}
%parse-param {void *scanner}
%lex-param {yyscan_t *scanner}

%define parse.error verbose
/*
%define parse.lac full
*/

%token VARIABLE
%token FILEEND
 
%token TINT
%token TFLOAT
%token DATE
%token TIME
%token STRING
%token ID 
%token URI
%token TCONST
%token VAR

%token NEQ
%token EQ
%token IMP
%token AND
%token EQV

%token NEQV
%token RIMP
%token NOR
%token NAND


%left '&' NAND '|' NOR IMP RIMP EQV NEQV
%left '?' '!' 
%left '-' '~'


%% /* Grammar rules and actions follow */

input:    /* empty */
          | sentencelist       { 
			       (parm->result)=$1;             
			     }
;

sentence:  assertion         { $$ = $1; }          
;

sentencelist: sentence              { $$ = MKWGPAIR(PP,$1,MKWGNIL); }
            | sentencelist sentence { $$ = MKWGPAIR(PP,$2,$1); }
;


assertion: primsentence '.'      { $$ = $1; }
;
	

primsentence: log 	          { $$ = $1; }        
            | loglist            { $$ = $1; }
;



term:      prim                   { $$ = $1; }     
         | prim '(' ')'           { $$ = MKWGPAIR(PP,$1,MKWGNIL); }  
         | prim '(' termlist ')'  { $$ = MKWGPAIR(PP,$1,$3); }           
         | '(' term ')'           { $$ = $2; }
         | EQ '(' termlist ')'    { $$ = MKWGPAIR(PP,MKWGCONST(PP,"="),$3); }
         | NEQ '(' termlist ')'   { $$ = MKWGPAIR(
                                             PP,
                                             MKWGCONST(PP,"not"),
                                             MKWGPAIR(
                                               PP,
                                               MKWGPAIR(PP,MKWGCONST(PP,"="),$3),
                                               MKWGNIL)); }
         | term NEQ term          { $$ = MKWGPAIR(
                                             PP,
                                             MKWGCONST(PP,"not"),
                                             MKWGPAIR(
                                               PP,
                                               MKWGPAIR(PP,MKWGCONST(PP,"="),MKWGPAIR(PP,$1,MKWGPAIR(PP,$3,MKWGNIL))),
                                               MKWGNIL)); }                                           
         | term EQ term           { $$ = MKWGPAIR(PP,MKWGCONST(PP,"="),MKWGPAIR(PP,$1,MKWGPAIR(PP,$3,MKWGNIL))); }
         | term '+' term          { $$ = MKWGPAIR(PP,MKWGCONST(PP,"+"),MKWGPAIR(PP,$1,MKWGPAIR(PP,$3,MKWGNIL))); }
         | term '-' term          { $$ = MKWGPAIR(PP,MKWGCONST(PP,"-"),MKWGPAIR(PP,$1,MKWGPAIR(PP,$3,MKWGNIL))); }
         | term '*' term          { $$ = MKWGPAIR(PP,MKWGCONST(PP,"*"),MKWGPAIR(PP,$1,MKWGPAIR(PP,$3,MKWGNIL))); }
         | '-' term               { $$ = MKWGPAIR(PP,MKWGCONST(PP,"not"),MKWGPAIR(PP,$2,MKWGNIL)); }	 
         | '~' term               { $$ = MKWGPAIR(PP,MKWGCONST(PP,"not"),MKWGPAIR(PP,$2,MKWGNIL)); }
         | '(' loglist')'         { $$ = $2; }
         | '[' termlist ']'       { $$ = MKWGPAIR(PP,MKWGCONST(PP,"lst"),MKWGPAIR(PP,$2,MKWGNIL)); }
         | '[' ']'                { $$ = MKWGPAIR(PP,MKWGCONST(PP,"lst"),MKWGNIL); }
         | connlog                { $$ = $1; }
;


termlist:    term                { $$ = MKWGPAIR(PP,$1,MKWGNIL); } 
	| term ',' termlist   { $$ = MKWGPAIR(PP,$1,$3); }
;

log:       term                   { $$ = $1; }
        | connlog                 { $$ = $1; }
;


connlog:           
         | log '&' log    %prec '&'   { $$ = MKWGPAIR(
                                          PP,
                                          MKWGCONST(PP,"and"),
                                          MKWGPAIR(
                                            PP,
                                            $1,                                                     
                                            MKWGPAIR(
                                              PP,
                                              $3,
                                              MKWGNIL))); }

         | log NAND log   %prec NAND   { $$ = 
                                      MKWGPAIR(
                                        PP,
                                        MKWGCONST(PP,"not"),
                                        MKWGPAIR(
                                          PP,
                                          MKWGPAIR(
                                            PP,
                                            MKWGCONST(PP,"and"),
                                            MKWGPAIR(
                                              PP,
                                              $1,                                                     
                                              MKWGPAIR(
                                                PP,
                                                $3,
                                                MKWGNIL))),
                                          MKWGNIL)); }                                              

         | log '|' log    %prec '|'   { $$ = MKWGPAIR(
                                          PP,
                                          MKWGCONST(PP,"or"),
                                          MKWGPAIR(
                                            PP,
                                            $1,                                                     
                                            MKWGPAIR(
                                              PP,
                                              $3,
                                              MKWGNIL))); }                                              


         | log NOR log    %prec NOR   { $$ = 
                                      MKWGPAIR(
                                        PP,
                                        MKWGCONST(PP,"not"),
                                        MKWGPAIR(
                                          PP,
                                          MKWGPAIR(
                                            PP,
                                            MKWGCONST(PP,"or"),
                                            MKWGPAIR(
                                              PP,
                                              $1,                                                     
                                              MKWGPAIR(
                                                PP,
                                                $3,
                                                MKWGNIL))),
                                          MKWGNIL)); }                                                  

         | log IMP log    %prec IMP   { $$ = MKWGPAIR(
                                          PP,
                                          MKWGCONST(PP,"imp"),
                                          MKWGPAIR(
                                            PP,
                                            $1,                                                     
                                            MKWGPAIR(
                                              PP,
                                              $3,
                                              MKWGNIL))); } 

         | log RIMP log   %prec RIMP    { $$ = MKWGPAIR(
                                          PP,
                                          MKWGCONST(PP,"imp"),
                                          MKWGPAIR(
                                            PP,
                                            $3,                                                     
                                            MKWGPAIR(
                                              PP,
                                              $1,
                                              MKWGNIL))); }                                     

         | log EQV log    %prec EQV    { $$ = MKWGPAIR(
                                          PP,
                                          MKWGCONST(PP,"eqv"),
                                          MKWGPAIR(
                                            PP,
                                            $1,                                                     
                                            MKWGPAIR(
                                              PP,
                                              $3,
                                              MKWGNIL))); } 

         | log NEQV log   %prec NEQV    { $$ =
                                      MKWGPAIR(
                                        PP,
                                        MKWGCONST(PP,"not"),
                                        MKWGPAIR(
                                          PP,
                                          MKWGPAIR(
                                            PP,
                                            MKWGCONST(PP,"eqv"),
                                            MKWGPAIR(
                                              PP,
                                              $1,                                                     
                                              MKWGPAIR(
                                                PP,
                                                $3,
                                                MKWGNIL))),
                                          MKWGNIL) ); }                                              

          | '?' '[' termlist ']' ':' log  %prec '?' { $$ = MKWGPAIR(
                                                   PP,
                                                   MKWGCONST(PP,"exists"),
                                                   MKWGPAIR(
                                                     PP,
                                                     $3,                                                     
                                                     MKWGPAIR(
                                                       PP,
                                                       $6,
                                                       MKWGNIL))); }  

         | '!' '[' termlist ']' ':' log  %prec '!' { $$ = MKWGPAIR(
                                                   PP,
                                                   MKWGCONST(PP,"all"),
                                                   MKWGPAIR(
                                                     PP,
                                                     $3,                                                     
                                                     MKWGPAIR(
                                                       PP,
                                                       $6,
                                                       MKWGNIL))); }  
         | '-' log  %prec '-'             { $$ = MKWGPAIR(PP,MKWGCONST(PP,"not"),MKWGPAIR(PP,$2,MKWGNIL)); }	 
         | '~' log  %prec '~'             { $$ = MKWGPAIR(PP,MKWGCONST(PP,"not"),MKWGPAIR(PP,$2,MKWGNIL)); } 
                                                                                                                                                        
        
;         


loglist:   log                   { $$ = MKWGPAIR(PP,$1,MKWGNIL); }
         | log '|' loglist       { $$ = MKWGPAIR(PP,$1,$3); }

;
	  

prim:     TINT             { $$ = MKWGINT(PP,$1); }
        | TFLOAT           { $$ = MKWGFLOAT(PP,$1); }        
      	| DATE            { $$ = MKWGDATE(PP,$1); }
	      | TIME            { $$ = MKWGTIME(PP,$1); }
        | STRING          { $$ = MKWGSTRING(PP,$1); }
	      | VAR             { $$ = MKWGVAR(PP,$1); }
        | URI	            { $$ = MKWGURI(PP,$1); }
        | ID	            { $$ = MKWGCONST(PP,$1); }
        | TCONST	           { $$ = MKWGCONST(PP,$1); }
;


	
%%

