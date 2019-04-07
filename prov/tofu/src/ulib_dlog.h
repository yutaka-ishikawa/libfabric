/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/* vim: set ts=8 sts=4 sw=4 noexpandtab : */

#ifndef _ULIB_DLOG_H
#define _ULIB_DLOG_H

#include "ulib_conf.h"

/* ==================================================================== */

#ifdef	CONF_ULIB_LOG_VERBOSE

#include <stdio.h>		    /* for fprintf() */
#include <inttypes.h>

#define ENTER_LOG(FP)		    fprintf( (FP), "->\t%s:%d\n", \
					__func__, __LINE__)
#define EXIT_LOG(FP)		    fprintf( (FP), "<-\t%s:%d\n", \
					__func__, __LINE__)
#else
#define ENTER_LOG(FP)
#define EXIT_LOG(FP)
#endif

#ifdef  CONF_ULIB_LOG_FAIL

#include <stdio.h>		    /* for fprintf() */
#include <inttypes.h>

#define ENTER_LOG_FAIL(EL_)	    int EL_ = 0
#define PRINT_LOG_FAIL(FP,RC,EL_)   if ((EL_) != 0) { \
					fprintf( (FP), "ERROR %d\t%s:%d\n", \
					    (RC), __func__, (EL_)); \
					fflush( (FP) ); \
				    }
#define RECORD_LOG_FAIL(EL_)	    (EL_) = __LINE__
#define CLEAR_LOG_FAIL(EL_)	    (EL_) = 0
#else
#define ENTER_LOG_FAIL(EL_)
#define PRINT_LOG_FAIL(FP,RC,EL_)
#define RECORD_LOG_FAIL(EL_)
#define CLEAR_LOG_FAIL(EL_)
#endif

#define ENTER_RC_C(RC)		    ENTER_LOG_FAIL(el_); \
				    ENTER_LOG(stderr); \
				    if (0) { if ((RC) != 0) { goto fn_fail; } }
#define RETURN_RC_C(RC,CL_)	    fn_exit: \
				    PRINT_LOG_FAIL(stderr, RC, el_); \
				    EXIT_LOG(stderr); \
				    return (RC) ; \
				    fn_fail: CL_ ; goto fn_exit

#define RETURN_OK_C(RC)		    goto fn_exit
#define RETURN_BAD_C(RC)	    RECORD_LOG_FAIL(el_); goto fn_fail
#define RETURN_IGN_C(RC)	    goto fn_fail

#define IGNORE_RC_C(RC)		    (RC) = 0; CLEAR_LOG_FAIL(el_)

#endif	/* _ULIB_DLOG_H */
