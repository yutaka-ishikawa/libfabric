extern void utofu_get_last_error(const char*);

#define SYSERRCHECK_EXIT(rc, cond, val, msg) do {			\
    if (rc cond val) {							\
	printf("[%d] error: %s @ %d in %s\n",				\
	       mypid, msg, __LINE__, __FILE__);				\
	fprintf(stderr, "[%d] error: %s  @ %d in %s\n",			\
	        mypid, msg, __LINE__, __FILE__);			\
	perror("");							\
	fflush(NULL);							\
    }									\
} while(0);

#define LIBERRCHECK_EXIT(rc, cond, val, msg) do {			\
    if (rc cond val) {							\
	printf("[%d] error: %s @ %d in %s\n",				\
	       mypid, msg, __LINE__, __FILE__);				\
	fprintf(stderr, "[%d] error: %s  @ %d in %s\n",			\
	        mypid, msg, __LINE__, __FILE__);			\
	fflush(NULL);							\
    }									\
} while(0);

#define UTOFU_ERRCHECK_EXIT(rc) do {					\
    if (rc != UTOFU_SUCCESS) {						\
	char msg[1024];							\
	utofu_get_last_error(msg);					\
	printf("[%d] error: %s (code:%d) @ %d in %s\n",			\
	       mypid, msg, rc, __LINE__, __FILE__);			\
	fprintf(stderr, "[%d] error: %s (code:%d) @ %d in %s\n",	\
	        mypid, msg, rc, __LINE__, __FILE__);			\
	fflush(stdout); fflush(stderr);					\
	abort();							\
    }									\
} while(0);

#define UTOFU_ERRCHECK_EXIT_IF(abrt, rc) do {				\
    if (rc != UTOFU_SUCCESS) {						\
	char msg[1024];							\
	utofu_get_last_error(msg);					\
	printf("[%d] error: %s (code:%d) @ %d in %s\n",			\
	       mypid, msg, rc, __LINE__, __FILE__);			\
	fprintf(stderr, "[%d] error: %s (code:%d) @ %d in %s\n",	\
	        mypid, msg, rc, __LINE__, __FILE__);			\
	fflush(stdout); fflush(stderr);					\
	if (abrt) abort();						\
    }									\
} while(0);

#define UTOFU_CALL(abrt, func, ...) do {				\
    char msg[256];							\
    int rc;								\
    DEBUG(DLEVEL_UTOFU) {						\
	snprintf(msg, 256, "%s: calling %s(%s)\n", __func__, #func, #__VA_ARGS__); \
	utf_printf("%s", msg);						\
    }									\
    rc = func(__VA_ARGS__);						\
    UTOFU_ERRCHECK_EXIT_IF(abrt, rc);					\
} while (0);

#define ERRCHECK_RETURN(val1, op, val2, rc) do {			\
    if (val1 op val2) return rc;					\
} while (0);

#define INITCHECK() do {						\
    if (utf_initialized == 0) return -1;				\
} while (0);

#define CMD_ARMW4	1
#define CMD_ARMW8	2
#define CMD_ADD		3
#define CMD_PUT_PIGGY	4
#define CMD_PUT		5
#define CMD_GET		6

#define UTOFU_LATEST_CMDINFO(cmd, rstadd)	\
{						\
    dbg_tofu_cmd = cmd;				\
    dbg_tofu_rstadd = rstadd;			\
    dbg_tofu_file = __FILE__;			\
    dbg_tofu_line = __LINE__;			\
} while (0);
