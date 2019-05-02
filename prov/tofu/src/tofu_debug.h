/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef _TOFU_DEBUG_H
#define _TOFU_DEBUG_H

#include <stdlib.h>
extern int mypid;
extern int rdbgf;

#define RDEBUG 1
#ifdef RDEBUG
static inline void get_doption() {
	char	*cp = getenv("TOFU_DEBUG");
	if (cp) {
		rdbgf = atoi(cp);
	}
}

#define R_DBG0(format, ...)						\
   do {									\
	   get_doption();						\
	   if (rdbgf & 0x01) {						\
		   printf("\t%d:" format " in %s:%d\n",			\
			  mypid, __VA_ARGS__, __func__, __LINE__);	\
		   fflush(stdout);					\
	   } else if (rdbgf & 0x02) {					\
		   fprintf(stderr, "%d:" format " in %s:%d\n",		\
			   mypid, __VA_ARGS__, __func__, __LINE__);	\
		   fflush(stderr);					\
	   }								\
   } while (0)

#define R_DBG1(format, ...)						\
    do {								\
	    char buf1[128];						\
	    get_doption();						\
	    if (rdbgf & 0x01) {						\
		    printf("\t%d:" format " in %s:%d\n",		\
			   mypid, __VA_ARGS__, __func__, __LINE__);	\
		    fflush(stdout);					\
	    } else if (rdbgf & 0x2) {					\
		    fprintf(stderr, "%d:" format " in %s:%d\n",		\
			    mypid, __VA_ARGS__, __func__, __LINE__);	\
		    fflush(stderr);					\
	    }								\
    } while (0)

#define R_DBG2(format, ...)						\
   do {									\
	   char buf1[128], buf2[128];					\
	   get_doption();						\
	   if (rdbgf & 0x01) {						\
		   printf("\t%d:" format " in %s:%d\n",			\
			  mypid, __VA_ARGS__, __func__, __LINE__);	\
		   fflush(stdout);					\
	   } else if (rdbgf & 0x02) {					\
		   fprintf(stderr, "%d:" format " in %s:%d\n",		\
			  mypid, __VA_ARGS__, __func__, __LINE__);	\
		   fflush(stderr);					\
	   }								\
   } while (0)

#else
#define R_DBG0(format, ...)
#define R_DBG1(format, ...)
#define R_DBG2(format, ...)
static inline void get_doption() { return; }
#endif

#endif	/* _TOFU_DEBUG_H */
