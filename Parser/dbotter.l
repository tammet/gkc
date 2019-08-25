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

 /** @file dbotter.l
 *  Lexer rules for otter parser
 *
 */


%{
#include <math.h>
#include <stdio.h>
#include "dbotterparse.h"
#include "dbotter.tab.h"

/*  reentrant stuff starts */

#define PARM    yyget_extra(yyscanner)

/*
#define YY_INPUT(buffer, res, max_size)             \
  if (PARM->foo!=NULL) { \
    do {                                                \
        if (PARM->pos >= PARM->length)                  \
            res = YY_NULL;                              \
        else                                            \
        {                                               \
            res = PARM->length - PARM->pos;             \
            res > (int)max_size ? res = max_size : 0;   \
            memcpy(buffer, PARM->buf + PARM->pos, res); \
            PARM->pos += res;                           \
        }                                               \
    } while (0);\
  } else { \
    int c = getchar(); \
    res = ((c == EOF) ? YY_NULL : (buffer[0] = c, 1)); \
  } 
*/


#define YY_INPUT(buffer, res, max_size)             \
  if (PARM->foo!=NULL) { \
    do {                                                \
        if (PARM->pos >= PARM->length)                  \
            res = YY_NULL;                              \
        else                                            \
        {                                               \
            res = PARM->length - PARM->pos;             \
            res > (int)max_size ? res = max_size : 0;   \
            memcpy(buffer, PARM->buf + PARM->pos, res); \
            PARM->pos += res;                           \
        }                                               \
    } while (0);\
  } else { \
    int n = fread(buffer,1,max_size,stdin); \
    if (n<=0) res=YY_NULL;\
    else res=n; \
  } 


/*

void
lex_parsestr(const char *s)
{
        YY_BUFFER_STATE yyhandle;

        yyhandle = YY_CURRENT_BUFFER;
        yy_scan_string(s);
        yylex();
        yy_delete_buffer(YY_CURRENT_BUFFER);
        yy_switch_to_buffer(yyhandle);
}

*/

char linebuf[1024];
char elmparsestrbuf[1024]; 
char *s; 

%}

%option reentrant 
%option bison-bridge
%option noyywrap
%option yylineno 
%option nounput
%option noinput


%x STRSTATE 
%x QUOTESTATE
%x COMMENT



DIGIT    [0-9]
ID       [a-zA-Z][a-zA-Z0-9_:+\-*/<>]*

%%

"!-"|"/"|"<"|">"|">="  {
              parseprintf( "an op: %s", yytext);
              *yylval=yytext;	      
              return URI; 
	    }

"!="  {
              parseprintf( "an neq: %s", yytext);
              *yylval=yytext;	      
              return NEQ; 
	    }
      

"="   {
              parseprintf( "an eq: %s", yytext);
              *yylval=yytext;	      
              return EQ; 
	    }      

"=>"   {
              parseprintf( "an rimp: %s", yytext);
              *yylval=yytext;	      
              return IMP; 
	    } 

"<="   {
              parseprintf( "an limp: %s", yytext);
              *yylval=yytext;	      
              return RIMP; 
	    }       

"<=>"   {
              parseprintf( "an eqv: %s", yytext);
              *yylval=yytext;	      
              return EQV; 
	    }      

"<~>"   {
              parseprintf( "an eqv: %s", yytext);
              *yylval=yytext;	      
              return NEQV; 
	    } 

"~&"   {
              parseprintf( "an eqv: %s", yytext);
              *yylval=yytext;	      
              return NAND; 
	    }

"~|"   {
              parseprintf( "an eqv: %s", yytext);
              *yylval=yytext;	      
              return NOR; 
	    }


{DIGIT}+    { parseprintf( "An integer: %s (%d)\n",yytext,atoi(yytext));	           
              *yylval=yytext;              
              return TINT; 
	    }
{DIGIT}+"."{DIGIT}+    {
              parseprintf( "A float: %s", yytext);
              *yylval=yytext;	      
              return TFLOAT; 
	    }
	    
{DIGIT}{DIGIT}{DIGIT}{DIGIT}"-"{DIGIT}{DIGIT}"-"{DIGIT}{DIGIT} {
              parseprintf( "A date: %s\n", yytext);
              *yylval=yytext;	      
              return DATE;      
      }
      
{DIGIT}{DIGIT}":"{DIGIT}{DIGIT}":"{DIGIT}{DIGIT} {
              parseprintf( "A time: %s\n", yytext);			  
	            *yylval=yytext; 
              return TIME; 
	    }  	    

\"              { BEGIN STRSTATE; s = elmparsestrbuf; } 
<STRSTATE>\\n   { *s++ = '\n'; } 
<STRSTATE>\\t   { *s++ = '\t'; } 
<STRSTATE>\\\"  { *s++ = '\"'; } 
<STRSTATE>\"    { *s = 0; BEGIN 0; 
                  parseprintf("found '%s'\n", elmparsestrbuf); 
		              *yylval=elmparsestrbuf;
		              return STRING;
                } 
<STRSTATE>\n    { *s++ = '\n'; 
                 /* parseprintf("elm parser error: invalid string (newline in string)"); 
                    exit(1); 
		             */   
	        } 
<STRSTATE>.     { *s++ = *yytext; }


\'              { BEGIN QUOTESTATE; s = elmparsestrbuf; *s++ = '#';} 
<QUOTESTATE>\\n   { *s++ = '\n'; } 
<QUOTESTATE>\\t   { *s++ = '\t'; } 
<QUOTESTATE>\\\'  { *s++ = '\''; } 
<QUOTESTATE>\'    { *s = 0; BEGIN 0; 
                  parseprintf("found '%s'\n", elmparsestrbuf); 
		              *yylval=elmparsestrbuf;
		              return URI;
                } 
<QUOTESTATE>\n    { *s++ = '\n'; 
                 /* parseprintf("elm parser error: invalid string (newline in quote)"); 
                    exit(1); 
		             */   
	        } 
<QUOTESTATE>.     { *s++ = *yytext; }


"/*"         BEGIN(COMMENT);  

<COMMENT>[^*\n]*        /* eat anything that's not a '*' */
<COMMENT>"*"+[^*/\n]*   /* eat up '*'s not followed by '/'s */
<COMMENT>\n             ;
<COMMENT>"*"+"/"        BEGIN(INITIAL);

{ID}  {
	      parseprintf( "An identifier: %s\n", yytext );
	      *yylval=yytext;
	      return URI;
	    }  
      
[$]{ID}  {
	      parseprintf( "An identifier: %s\n", yytext );
	      *yylval=yytext;
	      return URI;
	    }      

[?]{ID} {
	        parseprintf( "A variable: %s\n", yytext );
	        *yylval=yytext;
	        return VAR;
	      } 

\n.* { strncpy(linebuf, yytext+1, sizeof(linebuf));
       yyless(1);      
     }

"%".*  { } /* eat up line comment until end of line */

[-] return '-';
[~] return '~';
[|] return '|';
[)] return ')';
[(] return '(';
[}] return '}';
[{] return '{';
[,] return ',';
[.] return '.';

[\]] return ']';
[\[] return '[';
[!] return '!';
[?] return '?';
[:] return ':';
[&] return '&';

[*] return '*';
[+] return '+';

<<EOF>>     { parseprintf("file end\n"); 
              //return FILEEND; 
	            yyterminate();
	          }

[ \t]+     /* eat up whitespace */

.         { parseprintf( "Unrecognized character: %s\n", yytext ); }


%%


void wg_yyottererror (parse_parm* parm, void* scanner, const char* msg) {
  int i;
  char *s;
  //char* errmsg;
  //printf("\n yyerror called with xx msg %s\n",msg);
  /*
  if (linebuf!=NULL) {
    for(i=0;linebuf[i]!=0;i++) {
      if (linebuf[i]=='"') linebuf[i]='\'';
    }
  }
  */
  for(i=0;linebuf[i]!=0;i++) {
    if (linebuf[i]=='"') linebuf[i]='\'';
  }
  s=yyget_text(scanner);
  if (s!=NULL) {
    for(i=0;s[i]!=0;i++) {
      if (s[i]=='"') s[i]='\'';
    }
  }
  if (parm->errmsg) return;
  parm->errmsg=malloc(1000);
  snprintf(parm->errmsg,900,"{\"error\": \"%s: file %s place '%s' in line %d:\n%s\"}", 
          msg,parm->filename,s,yyget_lineno(scanner),linebuf);
  (parm->errmsg)[900]=0;
  // printf("{\"error\": \"%s: file %s place '%s' in line %d:\n%s\"}\n", 
  //        msg,parm->filename,s,yyget_lineno(scanner),linebuf);  
}

   
