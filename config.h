/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Use additional validation checks */
/* #undef CHECK */

/* Journal file directory */
#define DBLOG_DIR "/tmp"

/* Encoded data is 64-bit */
#define HAVE_64BIT_GINT 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `m' library (-lm). */
#define HAVE_LIBM 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define if you have POSIX threads libraries and header files. */
#define HAVE_PTHREAD 1

/* Compile with raptor rdf library */
/* #undef HAVE_RAPTOR */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Select locking protocol: reader-preference spinlock */
#define LOCK_PROTO 3

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Name of package */
#define PACKAGE "gkc"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "GKC"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "GKC 0.4"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "gkc"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.4"

/* Define to necessary symbol if this constant uses a non-standard name on
   your system. */
/* #undef PTHREAD_CREATE_JOINABLE */

/* The size of `ptrdiff_t', as computed by sizeof. */
#define SIZEOF_PTRDIFF_T 8

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* String hash size (% of db size) */
#define STRHASH_SIZE 2

/* Use chained T-tree index nodes */
#define TTREE_CHAINED_NODES 1

/* Use single-compare T-tree mode */
#define TTREE_SINGLE_COMPARE 1

/* Use record banklinks */
#define USE_BACKLINKING 1

/* Enable child database support */
/* #undef USE_CHILD_DB */

/* Use dblog module for transaction logging */
/* #undef USE_DBLOG */

/* Use match templates for indexes */
#define USE_INDEX_TEMPLATE 1

/* Enable reasoner */
#define USE_REASONER 1

/* Version number of package */
#define VERSION "0.4"

/* Package major version */
#define VERSION_MAJOR 0

/* Package minor version */
#define VERSION_MINOR 4

/* Package revision number */
#define VERSION_REV 0

/* Define to 1 if `lex' declares `yytext' as a `char *' by default, not a
   `char[]'. */
#define YYTEXT_POINTER 1
