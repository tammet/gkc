/*
* $Id:  $
* $Version: $
*
* Copyright (c) Priit J�rv 2010
*
* This file is part of WhiteDB
*
* WhiteDB is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* WhiteDB is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with WhiteDB.  If not, see <http://www.gnu.org/licenses/>.
*
*/

 /** @file dbutil.h
 * Public headers for miscellaneous functions.
 */

#ifndef DEFINED_DBUTIL_H
#define DEFINED_DBUTIL_H

#ifdef HAVE_RAPTOR
#include <raptor.h>
#endif

#ifdef _WIN32
#include "../config-w32.h"
#else
#include "../config.h"
#endif


/* ====== data structures ======== */

#ifdef HAVE_RAPTOR
struct wg_triple_handler_params {
  void *db;
  int pref_fields;  /** number of fields preceeding the triple */
  int suff_fields;  /** number of fields to reserve at the end */
  gint (*callback) (void *, void *);    /** function called after
                                         *the triple is stored */
  raptor_parser *rdf_parser;            /** parser object */
  int count;                            /** return status: rows parsed */
  int error;                            /** return status: error level */
};
#endif

/* === DB ERROR CODES FOR REASONER dbmemsegh(db)->errflag === */

#define DB_MEMORY_ALLOC_ERROR 1
#define DB_MEMORY_ALLOC_ERROR2 2
#define DB_IO_ERROR 3
#define DB_IO_ERROR2 4
#define DB_LOG_ERROR 5
#define DB_MPOOL_ERROR 6
#define DB_MPOOL_ERROR2 7 
#define DB_DATA_ERROR 8
#define DB_DATA_ERROR2 9
#define DB_DATA_ERROR3 10
#define DB_DATA_ERROR4 11

/* ==== Protos ==== */

/* API functions (copied in dbapi.h) */
void wg_print_db(void *db);
void wg_print_record(void *db, gint* rec);
void wg_snprint_value(void *db, gint enc, char *buf, int buflen);
gint wg_parse_and_encode(void *db, char *buf);
gint wg_parse_and_encode_param(void *db, char *buf);
void wg_export_db_csv(void *db, char *filename);
gint wg_import_db_csv(void *db, char *filename);

#ifdef USE_ERROR_CALLBACK
int wg_set_error_callback(void *errcallback);
int wg_unset_error_callback(void *errcallback);

// for internal use, not part of public api
void error_callback(const gint errnumber, const char* errmessage);
#endif

#ifdef USE_ERROR_CALLBACK 
#define LOG_ERROR(errnumber, errformat, errmessage, ...) \
fprintf(stderr, errformat, errmessage, ##__VA_ARGS__); \
char *buffer = malloc(256); \
if (buffer != NULL) \
{ \
int size = snprintf(buffer, 256, errformat, errmessage, ##__VA_ARGS__); \
if (size >= 256) { free(buffer); buffer = malloc(size); snprintf(buffer, size, errformat, errmessage, ##__VA_ARGS__); } \
error_callback(errnumber, buffer); \
free(buffer); \
} 
#else
//#define LOG_ERROR(errnumber, errformat, errmessage, ...) fprintf(stderr, errformat, errmessage, ##__VA_ARGS__)
//#define LOG_ERROR(errnumber, errformat, errmessage, ...) printf(errformat, errmessage, ##__VA_ARGS__)
#define LOG_ERROR(errnumber, errformat, errmessage, ...) do {} while (0)
#endif

/* Separate raptor API (copied in rdfapi.h) */
#ifdef HAVE_RAPTOR
gint wg_import_raptor_file(void *db, gint pref_fields, gint suff_fields,
  gint (*callback) (void *, void *), char *filename);
gint wg_import_raptor_rdfxml_file(void *db, gint pref_fields, gint suff_fields,
  gint (*callback) (void *, void *), char *filename);
gint wg_rdfparse_default_callback(void *db, void *rec);
gint wg_export_raptor_file(void *db, gint pref_fields, char *filename,
  char *serializer);
gint wg_export_raptor_rdfxml_file(void *db, gint pref_fields, char *filename);
#endif

void wg_pretty_print_memsize(gint memsz, char *buf, size_t buflen);

#endif /* DEFINED_DBUTIL_H */
