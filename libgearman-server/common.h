/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011-2012 Data Differential, http://datadifferential.com/
 *  Copyright (C) 2008 Brian Aker, Eric Day
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *      * Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *  copyright notice, this list of conditions and the following disclaimer
 *  in the documentation and/or other materials provided with the
 *  distribution.
 *
 *      * The names of its contributors may not be used to endorse or
 *  promote products derived from this software without specific prior
 *  written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/**
 * @file
 * @brief System Include Files
 */

#pragma once

#include <libgearman-server/gearmand.h>
#include "libgearman-server/config.hpp"

#include "libgearman/assert.hpp"

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef HAVE_PTHREAD
# include <pthread.h>
#endif
#ifdef HAVE_STDARG_H
# include <stdarg.h>
#endif
#ifdef HAVE_STDDEF_H
# include <stddef.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_SYS_UTSNAME_H
# include <sys/utsname.h>
#endif
#ifdef HAVE_NETINET_TCP_H
# include <netinet/tcp.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(__GNUC__) || (__GNUC__ == 2 && __GNUC_MINOR__ < 96)
#define likely(__x) if((__x))
#define unlikely(__x) if((__x))
#else
#define likely(__x) if(__builtin_expect((__x), 1))
#define unlikely(__x) if(__builtin_expect((__x), 0))
#endif

/**
 * Add an object to a list.
 * @ingroup gearman_constants
 */
#define GEARMAND_LIST_ADD(__list, __obj, __prefix) { \
  if (__list ## _list != NULL) \
    __list ## _list->__prefix ## prev= __obj; \
  __obj->__prefix ## next= __list ## _list; \
  __obj->__prefix ## prev= NULL; \
  __list ## _list= __obj; \
  __list ## _count++; \
}

#define GEARMAND_LIST__ADD(__list, __obj) { \
  if (__list ## _list != NULL) \
    __list ## _list->prev= __obj; \
  __obj->next= __list ## _list; \
  __obj->prev= NULL; \
  __list ## _list= __obj; \
  __list ## _count++; \
}

/**
 * Delete an object from a list.
 * @ingroup gearman_constants
 */
#define GEARMAND_LIST_DEL(__list, __obj, __prefix) { \
  if (__list ## _list == __obj) \
    __list ## _list= __obj->__prefix ## next; \
  if (__obj->__prefix ## prev != NULL) \
    __obj->__prefix ## prev->__prefix ## next= __obj->__prefix ## next; \
  if (__obj->__prefix ## next != NULL) \
    __obj->__prefix ## next->__prefix ## prev= __obj->__prefix ## prev; \
  __list ## _count--; \
}

#define GEARMAND_LIST__DEL(__list, __obj) { \
  if (__list ## _list == __obj) \
    __list ## _list= __obj->next; \
  if (__obj->prev != NULL) \
    __obj->prev->next= __obj->next; \
  if (__obj->next != NULL) \
    __obj->next->prev= __obj->prev; \
  __list ## _count--; \
}

/**
 * Add an object to a fifo list.
 * @ingroup gearman_constants
 */
#define GEARMAND_FIFO_ADD(__list, __obj, __prefix) { \
  if (__list ## _end == NULL) \
    __list ## _list= __obj; \
  else \
    __list ## _end->__prefix ## next= __obj; \
  __list ## _end= __obj; \
  __list ## _count++; \
}

#define GEARMAND_FIFO__ADD(__list, __obj) { \
  if (__list ## _end == NULL) \
    __list ## _list= __obj; \
  else \
    __list ## _end->next= __obj; \
  __list ## _end= __obj; \
  __list ## _count++; \
}

/**
 * Delete an object from a fifo list.
 * @ingroup gearman_constants
 */
#define GEARMAND_FIFO_DEL(__list, __obj, __prefix) { \
  __list ## _list= __obj->__prefix ## next; \
  if (__list ## _list == NULL) \
    __list ## _end= NULL; \
  __list ## _count--; \
}

#define GEARMAND_FIFO__DEL(__list, __obj) { \
  __list ## _list= __obj->next; \
  if (__list ## _list == NULL) \
    __list ## _end= NULL; \
  __list ## _count--; \
}

/**
 * Add an object to a hash.
 * @ingroup gearman_constants
 */
#define GEARMAND_HASH_ADD(__hash, __key, __obj, __prefix) { \
  if (__hash ## _hash[__key] != NULL) \
    __hash ## _hash[__key]->__prefix ## prev= __obj; \
  __obj->__prefix ## next= __hash ## _hash[__key]; \
  __obj->__prefix ## prev= NULL; \
  __hash ## _hash[__key]= __obj; \
  __hash ## _count++; \
}

#define GEARMAND_HASH__ADD(__hash, __key, __obj) { \
  if (__hash ## _hash[__key] != NULL) \
    __hash ## _hash[__key]->prev= __obj; \
  __obj->next= __hash ## _hash[__key]; \
  __obj->prev= NULL; \
  __hash ## _hash[__key]= __obj; \
  __hash ## _count++; \
}

/**
 * Delete an object from a hash.
 * @ingroup gearman_constants
 */
#define GEARMAND_HASH_DEL(__hash, __key, __obj, __prefix) { \
  if (__hash ## _hash[__key] == __obj) \
    __hash ## _hash[__key]= __obj->__prefix ## next; \
  if (__obj->__prefix ## prev != NULL) \
    __obj->__prefix ## prev->__prefix ## next= __obj->__prefix ## next; \
  if (__obj->__prefix ## next != NULL) \
    __obj->__prefix ## next->__prefix ## prev= __obj->__prefix ## prev; \
  __hash ## _count--; \
}

#define GEARMAND_HASH__DEL(__hash, __key, __obj) { \
  if (__hash ## _hash[__key] == __obj) \
    __hash ## _hash[__key]= __obj->next; \
  if (__obj->prev != NULL) \
    __obj->prev->next= __obj->next; \
  if (__obj->next != NULL) \
    __obj->next->prev= __obj->prev; \
  __hash ## _count--; \
}

#define gearmand_array_size(__object) (sizeof((__object)) / sizeof(*(__object)))

#ifdef __cplusplus
}
#endif

#ifdef NI_MAXHOST
#define GEARMAND_NI_MAXHOST NI_MAXHOST
#else
#define GEARMAND_NI_MAXHOST 1025
#endif

#ifdef NI_MAXSERV
#define GEARMAND_NI_MAXSERV NI_MAXSERV
#else
#define GEARMAND_NI_MAXSERV 32
#endif
