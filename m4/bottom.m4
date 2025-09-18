AC_DEFUN([CONFIG_EXTRA], [

AH_TOP([
#pragma once

/* _SYS_FEATURE_TESTS_H is Solaris, _FEATURES_H is GCC */
#if defined( _SYS_FEATURE_TESTS_H) || defined(_FEATURES_H)
#error "You should include gear_config.h as your first include file"
#endif

])

AH_BOTTOM([

/* This seems to be required for older compilers @note http://stackoverflow.com/questions/8132399/how-to-printf-uint64-t  */
#ifndef __STDC_FORMAT_MACROS
#  define __STDC_FORMAT_MACROS
#endif
 
#if defined(__cplusplus)
#  include CINTTYPES_H
#else
#  include <inttypes.h>
#endif

#if !defined(HAVE_ULONG) && !defined(__USE_MISC)
# define HAVE_ULONG 1
typedef unsigned long int ulong;
#endif

/* To hide the platform differences between MS Windows and Unix, I am
 * going to use the Microsoft way and #define the Microsoft-specific
 * functions to the unix way. Microsoft use a separate subsystem for sockets,
 * but Unix normally just use a filedescriptor on the same functions. It is
 * a lot easier to map back to the unix way with macros than going the other
 * way without side effect.
 */
#ifdef _WIN32
#define random() rand()
#define srandom(a) srand(a)
#define get_socket_errno() WSAGetLastError()
#else
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(a) close(a)
#define get_socket_errno() errno
#endif // _WIN32

#ifndef HAVE_MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif // HAVE_MSG_NOSIGNAL

#ifndef HAVE_MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif // HAVE_MSG_DONTWAIT

#include "libboost/config/workaround.hpp"

])

])dnl CONFIG_EXTRA
