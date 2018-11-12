/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011-2013 Data Differential, http://datadifferential.com/
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
 * @brief Gearman State Definitions
 */

#include "gear_config.h"

#include "libgearman-server/common.h"
#include "libgearman-server/timer.h"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <pthread.h>

#ifdef _WIN32
# include <malloc.h>
#else
# include <alloca.h>
#endif

#pragma GCC diagnostic push
#ifndef __INTEL_COMPILER
# pragma GCC diagnostic ignored "-Wold-style-cast"
# pragma GCC diagnostic ignored "-Wformat-nonliteral"
# pragma GCC diagnostic ignored "-Wformat-security"
#endif


static pthread_key_t logging_key;
static pthread_once_t intitialize_log_once= PTHREAD_ONCE_INIT;

static void delete_log(void * ptr)
{
  if (ptr)
  {
    free(ptr);
  }
}

static void create_log(void)
{
  int pthread_error;
  if ((pthread_error= pthread_key_create(&logging_key, delete_log)))
  {
    gearmand_log_fatal_perror(GEARMAND_AT, "pthread_key_create", pthread_error, "pthread_key_create");
    abort();
  }
}

gearmand_error_t gearmand_initialize_thread_logging(const char *identity)
{
  if (identity)
  {
    int pthread_error;
    if ((pthread_error= pthread_once(&intitialize_log_once, create_log)))
    {
      return gearmand_log_fatal_perror(GEARMAND_AT, "pthread_once", pthread_error, "identity: %s", identity);
    }

    if (pthread_getspecific(logging_key) == NULL)
    {
      const char *key_to_use= strdup(identity);
      if ((pthread_error= pthread_setspecific(logging_key, key_to_use)))
      {
        return gearmand_log_fatal_perror(GEARMAND_AT, "pthread_setspecific", pthread_error, "identity: %s", identity);
      }
    }

    return GEARMAND_SUCCESS;
  }

  gearmand_fatal("identity was NULL");

  return GEARMAND_INVALID_ARGUMENT;
}

#if __GNUC__ >= 7
  #pragma GCC diagnostic warning "-Wimplicit-fallthrough"
#endif
static gearmand_error_t __errno_to_gearmand_error_t(int local_errno)
{
  gearmand_error_t error_to_report= GEARMAND_ERRNO;

  switch (local_errno)
  {
  case ENOMEM:
    error_to_report= GEARMAND_MEMORY_ALLOCATION_FAILURE;
    break;
  case ECONNRESET:
  case EHOSTDOWN:
    error_to_report= GEARMAND_LOST_CONNECTION;
    break;
  default:
    break;
  }

  return error_to_report;
}

/**
 * Log a message.
 *
 * @param[in] gearman Structure previously initialized with gearman_create() or
 *  gearman_clone().
 * @param[in] verbose Logging level of the message.
 * @param[in] format Format and variable argument list of message.
 * @param[in] args Variable argument list that has been initialized.
 */

static void gearmand_log(const char *position, const char *func /* func */, 
                         gearmand_verbose_t verbose,
                         const gearmand_error_t error_arg,
                         const char *format, va_list args)
{
  struct timeval current_epoch;

  if (Gearmand() and Gearmand()->verbose < GEARMAND_VERBOSE_DEBUG)
  {
    current_epoch= libgearman::server::Epoch::current();
    current_epoch.tv_usec= 0;
  }
  else
  {
    (void)gettimeofday(&current_epoch, NULL);
  }

  struct tm current_tm;
  if (current_epoch.tv_sec == 0)
  {
    (void)gettimeofday(&current_epoch, NULL);
  }

  if ((localtime_r(&current_epoch.tv_sec, &current_tm) == NULL))
  {
    memset(&current_epoch, 0, sizeof(current_epoch));
  }

  (void) pthread_once(&intitialize_log_once, create_log);

  const char *identity= (const char *)pthread_getspecific(logging_key);

  if (identity == NULL)
  {
    identity= "[  main ]";
  }

  char log_buffer[GEARMAN_MAX_ERROR_SIZE*2] = { 0 };
  if (Gearmand() && Gearmand()->log_fn)
  {
    char *log_buffer_ptr= log_buffer;
    size_t remaining_size= sizeof(log_buffer);

    {
      int length= snprintf(log_buffer, sizeof(log_buffer), "%04d-%02d-%02d %02d:%02d:%02d.%06d %s ",
                           int(1900 +current_tm.tm_year), current_tm.tm_mon +1, current_tm.tm_mday, current_tm.tm_hour,
                           current_tm.tm_min, current_tm.tm_sec, int(current_epoch.tv_usec),
                           identity);
      // We just return whatever we have if this occurs
      if (length <= 0 or (size_t)length >= sizeof(log_buffer))
      {
        remaining_size= 0;
      }
      else
      {
        remaining_size-= size_t(length);
        log_buffer_ptr+= length;
      }
    }

    if (remaining_size)
    {
      int length= vsnprintf(log_buffer_ptr, remaining_size, format, args);
      if (length <= 0 or size_t(length) >= remaining_size)
      { 
        remaining_size= 0;
      }
      else
      {
        remaining_size-= size_t(length);
        log_buffer_ptr+= length;
      }
    }

    if (remaining_size and error_arg != GEARMAND_SUCCESS)
    {
      int length= snprintf(log_buffer_ptr, remaining_size, " %s(%s)", func, gearmand_strerror(error_arg));
      if (length <= 0 or size_t(length) >= remaining_size)
      {
        remaining_size= 0;
      }
      else
      {
        remaining_size-= size_t(length);
        log_buffer_ptr+= length;
      }
    }

    if (remaining_size and position and verbose != GEARMAND_VERBOSE_INFO)
    {
      int length= snprintf(log_buffer_ptr, remaining_size, " -> %s", position);
      if (length <= 0 or size_t(length) >= remaining_size)
      {
        remaining_size= 0;
      }
    }

    // Make sure this is null terminated
    log_buffer[sizeof(log_buffer) -1]= 0;
  }

  if (Gearmand() and Gearmand()->log_fn)
  {
    Gearmand()->log_fn(log_buffer, verbose, (void *)Gearmand()->log_context);
  }
  else
  {
    fprintf(stderr, "%s -> %s",
            log_buffer,  gearmand_verbose_name(verbose));
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
  }
}


gearmand_error_t gearmand_log_fatal(const char *position, const char *func, const char *format, ...)
{
  if (Gearmand() and  Gearmand()->verbose < GEARMAND_VERBOSE_FATAL)
  {
    return GEARMAND_ERRNO;
  }

  {
    va_list args;
    va_start(args, format);
    gearmand_log(position, func, GEARMAND_VERBOSE_FATAL, GEARMAND_SUCCESS, format, args);
    va_end(args);
  }
  gearmand_wakeup(Gearmand(), GEARMAND_WAKEUP_SHUTDOWN_GRACEFUL);

  return GEARMAND_ERRNO;
}

gearmand_error_t gearmand_log_fatal_perror(const char *position, const char *function, const int local_errno, const char *format, ...)
{
  if (Gearmand() and  Gearmand()->verbose < GEARMAND_VERBOSE_FATAL)
  {
    return GEARMAND_ERRNO;
  }

  char* message_buffer= NULL;
  {
    va_list args;
    va_start(args, format);

    size_t ask= snprintf(0, 0, format);
    ask++; // for null
    message_buffer= (char*)alloca(sizeof(char) * ask);
    if (message_buffer)
    {
      vsnprintf(message_buffer, ask, format, args);
    }
    va_end(args);
  }

  {
    const char *errmsg_ptr;
    char errmsg[GEARMAN_MAX_ERROR_SIZE]; 
    errmsg[0]= 0; 

#ifdef STRERROR_R_CHAR_P
    errmsg_ptr= strerror_r(local_errno, errmsg, sizeof(errmsg));
#else
    strerror_r(local_errno, errmsg, sizeof(errmsg));
    errmsg_ptr= errmsg;
#endif

    if (message_buffer)
    {
      gearmand_log_fatal(position, function, "%s(%s)", message_buffer, errmsg_ptr);
    }
    else
    {
      gearmand_log_fatal(position, function, "%s", errmsg_ptr);
    }
  }

  return __errno_to_gearmand_error_t(local_errno);
}

gearmand_error_t gearmand_log_error(const char *position, const char *function, const char *format, ...)
{
  if (not Gearmand() or Gearmand()->verbose >= GEARMAND_VERBOSE_ERROR)
  {
    va_list args;
    va_start(args, format);
    gearmand_log(position, function, GEARMAND_VERBOSE_ERROR, GEARMAND_SUCCESS, format, args);
    va_end(args);
  }

  return GEARMAND_UNKNOWN_OPTION;
}

void gearmand_log_warning(const char *position, const char *function, const char *format, ...)
{
  if (not Gearmand() or Gearmand()->verbose >= GEARMAND_VERBOSE_WARN)
  {
    va_list args;
    va_start(args, format);
    gearmand_log(position, function, GEARMAND_VERBOSE_WARN, GEARMAND_SUCCESS, format, args);
    va_end(args);
  }
}

// LOG_NOTICE is only used for reporting job status.
void gearmand_log_notice(const char *position, const char *function, const char *format, ...)
{
  if (not Gearmand() or Gearmand()->verbose >= GEARMAND_VERBOSE_NOTICE)
  {
    va_list args;
    va_start(args, format);
    gearmand_log(position, function, GEARMAND_VERBOSE_NOTICE, GEARMAND_SUCCESS, format, args);
    va_end(args);
  }
}

void gearmand_log_info(const char *position, const char *function, const char *format, ...)
{
  if (not Gearmand() or Gearmand()->verbose >= GEARMAND_VERBOSE_INFO)
  {
    va_list args;
    va_start(args, format);
    gearmand_log(position, function, GEARMAND_VERBOSE_INFO, GEARMAND_SUCCESS, format, args);
    va_end(args);
  }
}

void gearmand_log_debug(const char *position, const char *function, const char *format, ...)
{
  va_list args;

  if (not Gearmand() || Gearmand()->verbose >= GEARMAND_VERBOSE_DEBUG)
  {
    va_start(args, format);
    gearmand_log(position, function, GEARMAND_VERBOSE_DEBUG, GEARMAND_SUCCESS, format, args);
    va_end(args);
  }
}

gearmand_error_t gearmand_log_perror(const char *position, const char *function, const int local_errno, const char *format, ...)
{
  if (not Gearmand() or (Gearmand()->verbose >= GEARMAND_VERBOSE_ERROR))
  {
    char* message_buffer= NULL;
    {
      va_list args;
      va_start(args, format);

      size_t ask= snprintf(0, 0, format);
      ask++; // for null
      message_buffer= (char*)alloca(sizeof(char) * ask);
      vsnprintf(message_buffer, ask, format, args);

      va_end(args);
    }

    const char *errmsg_ptr;
    char errmsg[GEARMAN_MAX_ERROR_SIZE]; 
    errmsg[0]= 0; 

#ifdef STRERROR_R_CHAR_P
    errmsg_ptr= strerror_r(local_errno, errmsg, sizeof(errmsg));
#else
    strerror_r(local_errno, errmsg, sizeof(errmsg));
    errmsg_ptr= errmsg;
#endif
    if (message_buffer)
    {
      gearmand_log_error(position, function, "%s(%s)", message_buffer, errmsg_ptr);
    }
    else
    {
      gearmand_log_error(position, function, "%s", errmsg_ptr);
    }
  }

  return __errno_to_gearmand_error_t(local_errno);
}

void gearmand_log_perror_warn(const char *position, const char *function, const int local_errno, const char *format, ...)
{
  if (not Gearmand() or (Gearmand()->verbose >= GEARMAND_VERBOSE_WARN))
  {
    char* message_buffer= NULL;
    {
      va_list args;
      va_start(args, format);

      size_t ask= snprintf(0, 0, format);
      ask++; // for null
      message_buffer= (char*)alloca(sizeof(char) * ask);
      vsnprintf(message_buffer, ask, format, args);

      va_end(args);
    }

    const char *errmsg_ptr;
    char errmsg[GEARMAN_MAX_ERROR_SIZE]; 
    errmsg[0]= 0; 

#ifdef STRERROR_R_CHAR_P
    errmsg_ptr= strerror_r(local_errno, errmsg, sizeof(errmsg));
#else
    strerror_r(local_errno, errmsg, sizeof(errmsg));
    errmsg_ptr= errmsg;
#endif
    if (message_buffer)
    {
      gearmand_log_warning(position, function, "%s(%s)", message_buffer, errmsg_ptr);
    }
    else
    {
      gearmand_log_warning(position, function, "%s", errmsg_ptr);
    }
  }
}

gearmand_error_t gearmand_log_gerror(const char *position, const char *function, const gearmand_error_t rc, const char *format, ...)
{
  if (gearmand_failed(rc) and rc != GEARMAND_IO_WAIT)
  {
    va_list args;

    if (Gearmand() == NULL or Gearmand()->verbose >= GEARMAND_VERBOSE_ERROR)
    {
      va_start(args, format);
      gearmand_log(position, function, GEARMAND_VERBOSE_ERROR, rc, format, args);
      va_end(args);
    }
  }
  else if (rc == GEARMAND_IO_WAIT)
  { }

  return rc;
}

void gearmand_log_gerror_warn(const char *position, const char *function, const gearmand_error_t rc, const char *format, ...)
{
  if (gearmand_failed(rc) and rc != GEARMAND_IO_WAIT)
  {
    va_list args;

    if (Gearmand() == NULL or Gearmand()->verbose >= GEARMAND_VERBOSE_WARN)
    {
      va_start(args, format);
      gearmand_log(position, function, GEARMAND_VERBOSE_WARN, rc, format, args);
      va_end(args);
    }
  }
}

gearmand_error_t gearmand_log_gai_error(const char *position, const char *function, const int rc, const char *message)
{
  if (rc == EAI_SYSTEM)
  {
    return gearmand_log_perror(position, function, errno, message);
  }

  gearmand_log_error(position, function, "%s getaddrinfo(%s)", message, gai_strerror(rc));

  return GEARMAND_GETADDRINFO;
}

gearmand_error_t gearmand_log_memory_error(const char *position, const char *function, const char *allocator, const char *object_type, size_t count, size_t size)
{
  if (count > 1)
  {
    gearmand_log_error(position, function, "%s(%s, count: %lu size: %lu)", allocator, object_type, static_cast<unsigned long>(count), static_cast<unsigned long>(size));
  }
  else
  {
    gearmand_log_error(position, function, "%s(%s, size: %lu)", allocator, object_type, static_cast<unsigned long>(count), static_cast<unsigned long>(size));
  }

  return GEARMAND_MEMORY_ALLOCATION_FAILURE;
}
#pragma GCC diagnostic pop
