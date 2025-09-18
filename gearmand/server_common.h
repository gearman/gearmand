/* Gearman server and library
 * Copyright (C) 2008 Brian Aker, Eric Day
 * All rights reserved.
 *
 * Use and distribution licensed under the BSD license.  See
 * the COPYING file in the parent directory for full text.
 */

#ifndef __GEARMAN_SERVER_COMMON_H__
#define __GEARMAN_SERVER_COMMON_H__

#define GEARMAN_INTERNAL 1

#include <libgearman/gearman.h>

#ifdef HAVE_ASSERT_H
#include <assert.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#include <time.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_EVENT_H
#include <event.h>
#endif

#include "connection.h"

#define SERVER_PORT "7003"
#define MAX_MSG 100

#define SUCCESS 0
#define ERROR   1

#endif /* __GEARMAN_SERVER_COMMON_H__ */
