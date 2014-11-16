#serial 1
# Author: Jorden Whitefield <jw00241@surrey.ac.uk>
#
# SYNOPSIS
#
#   ACX_PROB([ACTION-IF-FOUND[, ACTION-IF-NOT-FOUND]])
#
AC_DEFUN([ACX_PROB], [
AC_ARG_WITH([prob],
  [AS_HELP_STRING([--with-prob],
    [Compile ProBWrapper])],
  [],
  [with_prob=check])

case "$with_prob" in
  check)
        AC_CHECK_PROGS(JAVAC, [javac])
        #CHECK_JDK # Does not find OpenJDK
        #if test x$found_jdk != xyes       
        if test x"$JAVAC" = x 
        then
            AC_MSG_WARN([No JDK found.])
        else
            acx_prob=yes
        fi
     ;;
  yes) acx_prob=yes ;;
  *)   : ;;
esac
case "$acx_prob" in
  yes) :
    $1 ;;
  *) :
    $2 ;;
esac
])
