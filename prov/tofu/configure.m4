dnl Configury specific to the libfabrics tofu provider

dnl Called to configure this provider
dnl
dnl Arguments:
dnl
dnl $1: action if configured successfully
dnl $2: action if not configured successfully
dnl
dnl
AC_DEFUN([FI_TOFU_CONFIGURE],[
    # Determine if we can support the tofu provider
    tofu_happy=0
    AS_IF([test x"$enable_tofu" = x"yes"], [tofu_happy=1])
    AS_IF([test $tofu_happy -eq 1], [$1], [$2])
])

