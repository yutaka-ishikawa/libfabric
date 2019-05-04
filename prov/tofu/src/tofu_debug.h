/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef _TOFU_DEBUG_H
#define _TOFU_DEBUG_H

#include <stdlib.h>
extern int mypid;
extern int rdbgf, rdbgl;

#define RDEBUG 1
#ifdef RDEBUG
static inline void get_doption() {
    char	*cp = getenv("TOFU_DEBUG");
    if (cp) {
        rdbgf = atoi(cp);
    }
    cp = getenv("TOFU_DLEVEL");
    if (cp) {
        rdbgl = atoi(cp);
    }
}

/* setenv("TOFU_DEBUG", value, 1); */
#define RDBG_STDOUT     0x1     /* output to stdout */
#define RDBG_STDERR     0x2     /* output to stderr */
/* setenv("TOFU_DLEVEL", value, 1); */
#define RDBG_LEVEL1     0x1     /* general use */
#define RDBG_LEVEL2     0x2     /* selective */
#define RDBG_LEVEL3     0x2     /* general use */
#define RDBG_LEVEL4     0x4     /* state machine */
#define RDBG_LEVEL8     0x8     /* for finding DLST bug */

#define R_DBG(format, ...)                                              \
   do {									\
       printf("\t%d:" format " in %s:%d\n",                             \
              mypid, __VA_ARGS__, __func__, __LINE__);                  \
       fflush(stdout);                                                  \
       fprintf(stderr, "%d:" format " in %s:%d\n",                      \
               mypid, __VA_ARGS__, __func__, __LINE__);                 \
       fflush(stderr);                                                  \
   } while (0)

#define R_DBG0(level, format, ...)                                      \
   do {									\
	   get_doption();						\
	   if (rdbgf & 0x01 && level & rdbgl) {                         \
		   printf("\t%d:" format " in %s:%d\n",			\
			  mypid, __VA_ARGS__, __func__, __LINE__);	\
		   fflush(stdout);					\
	   } else if (rdbgf & 0x02 && level & rdbgl) {                  \
		   fprintf(stderr, "%d:" format " in %s:%d\n",		\
			   mypid, __VA_ARGS__, __func__, __LINE__);	\
		   fflush(stderr);					\
	   }								\
   } while (0)

#define R_DBG1(level, format, ...)                                      \
    do {								\
	    char buf1[128];						\
	    get_doption();						\
            if (rdbgf & 0x01 && level & rdbgl) {                        \
		    printf("\t%d:" format " in %s:%d\n",		\
			   mypid, __VA_ARGS__, __func__, __LINE__);	\
		    fflush(stdout);					\
            } else if (rdbgf & 0x02 && level & rdbgl) {                 \
		    fprintf(stderr, "%d:" format " in %s:%d\n",		\
			    mypid, __VA_ARGS__, __func__, __LINE__);	\
		    fflush(stderr);					\
	    }								\
    } while (0)

#define R_DBG2(level, format, ...)                                      \
   do {									\
	   char buf1[128], buf2[128];					\
	   get_doption();						\
           if (rdbgf & 0x01 && level & rdbgl) {                         \
		   printf("\t%d:" format " in %s:%d\n",			\
			  mypid, __VA_ARGS__, __func__, __LINE__);	\
		   fflush(stdout);					\
           } else if (rdbgf & 0x02 && level & rdbgl) {                  \
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
