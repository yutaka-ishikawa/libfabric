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

#define UTOFU_CALL(func, ...) do {					\
    char msg[256];							\
    int rc;								\
    DEBUG(DLEVEL_UTOFU) {						\
	snprintf(msg, 256, "%s: calling %s(%s)\n", __func__, #func, #__VA_ARGS__); \
	utf_printf("%s", msg);						\
    }									\
    rc = func(__VA_ARGS__);						\
    UTOFU_ERRCHECK_EXIT(rc);						\
} while (0);

#define ERRCHECK_RETURN(val1, op, val2, rc) do {			\
    if (val1 op val2) return rc;					\
} while (0);

#define INITCHECK() do {						\
    if (utf_initialized == 0) return -1;				\
} while (0);
