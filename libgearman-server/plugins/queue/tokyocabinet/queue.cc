/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011-2012 Data Differential, http://datadifferential.com/
 *  Copyright (C) 2008 Brian Moon
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

#include <gear_config.h>
#include <libgearman-server/common.h>

#include <libgearman-server/plugins/queue/tokyocabinet/queue.h>
#include <libgearman-server/plugins/queue/base.h>

#include <tcutil.h>
#include <tcadb.h>

namespace gearmand { namespace plugins { namespace queue { class TokyoCabinet;  }}}

/**
 * It is unclear from tokyocabinet's public headers what, if any, limit there is. 4k seems sane.
 */

#define GEARMAND_QUEUE_TOKYOCABINET_MAX_KEY_LEN 4096
gearmand_error_t _initialize(gearman_server_st *server,
                             gearmand::plugins::queue::TokyoCabinet *queue);

namespace gearmand {
namespace plugins {
namespace queue {

class TokyoCabinet : public Queue {
public:
  TokyoCabinet();
  ~TokyoCabinet();

  gearmand_error_t initialize();

  void destroy()
  {
    if (db)
    {
      tcadbdel(db);
      db= NULL;
    }
  }

  TCADB *db;
  std::string filename;
  bool optimize;
};

TokyoCabinet::TokyoCabinet() :
  Queue("libtokyocabinet"),
  db(NULL),
  optimize(false)
{
  command_line_options().add_options()
    ("libtokyocabinet-file", boost::program_options::value(&filename), "File name of the database. [see: man tcadb, tcadbopen() for name guidelines]")
    ("libtokyocabinet-optimize", boost::program_options::bool_switch(&optimize)->default_value(true), "Optimize database on open. [default=true]");
}

TokyoCabinet::~TokyoCabinet()
{
  destroy();
}

gearmand_error_t TokyoCabinet::initialize()
{
  return _initialize(&Gearmand()->server, this);
}

void initialize_tokyocabinet()
{
  static TokyoCabinet local_instance;
}

} // namespace queue
} // namespace plugins
} // namespace gearmand


/**
 * @addtogroup gearman_queue_libtokyocabinet libtokyocabinet Queue Storage Functions
 * @ingroup gearman_queue
 * @{
 */

/*
 * Private declarations
 */

/* Queue callback functions. */
static gearmand_error_t _libtokyocabinet_add(gearman_server_st *server, void *context,
                                             const char *unique,
                                             size_t unique_size,
                                             const char *function_name,
                                             size_t function_name_size,
                                             const void *data, size_t data_size,
                                             gearman_job_priority_t priority,
                                             int64_t when);

static gearmand_error_t _libtokyocabinet_flush(gearman_server_st *server, void *context);

static gearmand_error_t _libtokyocabinet_done(gearman_server_st *server, void *context,
                                              const char *unique,
                                              size_t unique_size, 
                                              const char *function_name, 
                                              size_t function_name_size);

static gearmand_error_t _libtokyocabinet_replay(gearman_server_st *server, void *context,
                                                gearman_queue_add_fn *add_fn,
                                                void *add_context);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

/**
 * Missing function from tcadb.c ??
 */
static const char * _libtokyocabinet_tcaerrmsg(TCADB *db)
{
  if (db)
  {
    switch (tcadbomode(db))
    {
    case ADBOHDB:
      return tcerrmsg(tchdbecode((TCHDB *)tcadbreveal(db)));

    case ADBOBDB:
      return tcerrmsg(tcbdbecode((TCBDB *)tcadbreveal(db)));

    default:
      return tcerrmsg(TCEMISC);
    }
  }

  return "no TCADB file";
}

gearmand_error_t _initialize(gearman_server_st *server,
                             gearmand::plugins::queue::TokyoCabinet *queue)
{
  gearmand_info("Initializing libtokyocabinet module");

  if ((queue->db= tcadbnew()) == NULL)
  {
    gearmand_error("tcadbnew");
    return GEARMAND_QUEUE_ERROR;
  }
     
  if (queue->filename.empty())
  {
    gearmand_error("No --file given");
    return GEARMAND_QUEUE_ERROR;
  }

  if (tcadbopen(queue->db, queue->filename.c_str()) == 0)
  {
    gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, 
                       "tcadbopen(%s): %s", queue->filename.c_str(), _libtokyocabinet_tcaerrmsg(queue->db));

    queue->destroy();

    return GEARMAND_QUEUE_ERROR;
  }

  if (queue->optimize)
  {
    gearmand_info("libtokyocabinet optimizing database file");
    if (tcadboptimize(queue->db, NULL) == 0)
    {
      queue->destroy();
      return gearmand_gerror("tcadboptimize", GEARMAND_QUEUE_ERROR);
    }
  }

  gearman_server_set_queue(*server, queue, _libtokyocabinet_add, _libtokyocabinet_flush, _libtokyocabinet_done, _libtokyocabinet_replay);   
   
  return GEARMAND_SUCCESS;
}

/*
 * Private definitions
 */

static gearmand_error_t _libtokyocabinet_add(gearman_server_st*, void *context,
                                             const char *unique,
                                             size_t unique_size,
                                             const char *function_name,
                                             size_t function_name_size,
                                             const void *data, size_t data_size,
                                             gearman_job_priority_t priority,
                                             int64_t when)
{
  gearmand::plugins::queue::TokyoCabinet *queue= (gearmand::plugins::queue::TokyoCabinet *)context;

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "libtokyocabinet add: %.*s at %" PRId64,
                     (uint32_t)unique_size, (char *)unique, when);

  char key_str[GEARMAND_QUEUE_TOKYOCABINET_MAX_KEY_LEN];
  size_t key_length= (size_t)snprintf(key_str, GEARMAND_QUEUE_TOKYOCABINET_MAX_KEY_LEN, "%.*s-%.*s",
                               (int)function_name_size,
                               (const char *)function_name, (int)unique_size,
                               (const char *)unique);

  gearmand_error_t ret= GEARMAND_QUEUE_ERROR;
  TCXSTR* key;
  if ((key= tcxstrnew()))
  {
    tcxstrcat(key, key_str, (int)key_length);

    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "libtokyocabinet key: %.*s", (int)key_length, key_str);

    TCXSTR* job_data;
    if ((job_data= tcxstrnew()))
    {
      tcxstrcat(job_data, (const char *)function_name, (int)function_name_size);
      tcxstrcat(job_data, "\0", 1);
      tcxstrcat(job_data, (const char *)unique, (int)unique_size);
      tcxstrcat(job_data, "\0", 1);

      switch (priority)
      {
      case GEARMAN_JOB_PRIORITY_HIGH:
      case GEARMAN_JOB_PRIORITY_MAX:     
        tcxstrcat2(job_data, "0");
        break;

      case GEARMAN_JOB_PRIORITY_LOW:
        tcxstrcat2(job_data, "2");
        break;

      case GEARMAN_JOB_PRIORITY_NORMAL:
      default:
        tcxstrcat2(job_data, "1");
      }

      // get int64_t as string
      char timestr[32];
      snprintf(timestr, sizeof(timestr), "%" PRId64, when);

      // append to job_data
      tcxstrcat(job_data, (const char *)timestr, (int)strlen(timestr));
      tcxstrcat(job_data, "\0", 1);

      // add the rest...
      tcxstrcat(job_data, (const char *)data, (int)data_size);

      if (tcadbput(queue->db, tcxstrptr(key), tcxstrsize(key),
                   tcxstrptr(job_data), tcxstrsize(job_data)))
      {
        ret= GEARMAND_SUCCESS;
      }

      tcxstrdel(job_data);
    }

    tcxstrdel(key);
  }

  return ret;
}

static gearmand_error_t _libtokyocabinet_flush(gearman_server_st *, void *context)
{
  gearmand::plugins::queue::TokyoCabinet *queue= (gearmand::plugins::queue::TokyoCabinet *)context;
   
  gearmand_debug("libtokyocabinet flush");

  if (tcadbsync(queue->db) == 0)
  {
    return GEARMAND_QUEUE_ERROR;
  }
   
  return GEARMAND_SUCCESS;
}

static gearmand_error_t _libtokyocabinet_done(gearman_server_st *, void *context,
                                              const char *unique,
                                              size_t unique_size, 
                                              const char *function_name,
                                              size_t function_name_size)
{
  gearmand::plugins::queue::TokyoCabinet *queue= (gearmand::plugins::queue::TokyoCabinet *)context;

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "libtokyocabinet add: %.*s", (uint32_t)unique_size, (char *)unique);
  
  char key_str[GEARMAND_QUEUE_TOKYOCABINET_MAX_KEY_LEN];
  size_t key_length= (size_t)snprintf(key_str, GEARMAND_QUEUE_TOKYOCABINET_MAX_KEY_LEN, "%.*s-%.*s",
                                      (int)function_name_size,
                                      (const char *)function_name, (int)unique_size,
                                      (const char *)unique);

  TCXSTR* key= tcxstrnew();
  tcxstrcat(key, key_str, (int)key_length);
  bool rc= tcadbout(queue->db, tcxstrptr(key), tcxstrsize(key));
  tcxstrdel(key);

  if (rc)
  {
    return GEARMAND_SUCCESS;
  }

  return GEARMAND_QUEUE_ERROR;
}

static gearmand_error_t _callback_for_record(gearman_server_st *server,
                                             TCXSTR *key, TCXSTR *data,
                                             gearman_queue_add_fn *add_fn,
                                             void *add_context)
{
  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "replaying: %s", (char *) tcxstrptr(key));

  char* data_cstr= (char *)tcxstrptr(data);
  size_t data_cstr_size= (size_t)tcxstrsize(data);

  const char* function= data_cstr;
  size_t function_len= strlen(function);

  char* unique= data_cstr +function_len +1;
  size_t unique_len= strlen(unique); // strlen is only safe because tcxstrptr guarantees nul term

  // +2 for nulls
  data_cstr += unique_len +function_len +2;
  data_cstr_size -= unique_len +function_len +2;

  assert(unique);
  assert(unique_len);
  assert(function);
  assert(function_len);

  // single char for priority
  gearman_job_priority_t priority;
  if (*data_cstr == '2')
  {
    priority= GEARMAN_JOB_PRIORITY_LOW;
  }
  else if (*data_cstr == '0')
  {
    priority= GEARMAN_JOB_PRIORITY_HIGH;
  }
  else
  {
    priority= GEARMAN_JOB_PRIORITY_NORMAL;
  }

  ++data_cstr;
  --data_cstr_size;

  // out ptr for strtoul
  char *new_data_cstr= NULL;
  
  // parse time from record
  int64_t when= (int64_t)strtoul(data_cstr, &new_data_cstr, 10);
  
  // decrease opaque data size by the length of the numbers read by strtoul
  data_cstr_size -= (new_data_cstr - data_cstr) + 1;
  
  // move data pointer to end of timestamp + 1 (null)
  data_cstr= new_data_cstr +1; 
  
  // data is freed later so we must make a copy
  void *data_ptr= (void *)malloc(data_cstr_size);
  if (data_ptr == NULL)
  {
    return GEARMAND_MEMORY_ALLOCATION_FAILURE;
  }
  memcpy(data_ptr, data_cstr, data_cstr_size); 

  return (*add_fn)(server, add_context, unique, unique_len,
                   function, function_len,
                   data_ptr, data_cstr_size,
                   priority, when);
}


static gearmand_error_t _libtokyocabinet_replay(gearman_server_st *server, void *context,
                                                gearman_queue_add_fn *add_fn,
                                                void *add_context)
{
  gearmand::plugins::queue::TokyoCabinet *queue= (gearmand::plugins::queue::TokyoCabinet *)context;
   
  gearmand_info("libtokyocabinet replay start");
  
  if (tcadbiterinit(queue->db) == 0)
  {
    return GEARMAND_QUEUE_ERROR;
  }

  TCXSTR* key= tcxstrnew();
  TCXSTR* data= tcxstrnew();
  gearmand_error_t gret= GEARMAND_SUCCESS;
  void *iter= NULL;
  uint64_t x= 0;
  int iter_size= 0;
  while ((iter= tcadbiternext(queue->db, &iter_size)))
  {     
    tcxstrclear(key);
    tcxstrclear(data);
    tcxstrcat(key, iter, iter_size);
    free(iter);
    iter= tcadbget(queue->db, tcxstrptr(key), tcxstrsize(key), &iter_size);
    if (iter == NULL)
    {
      gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "libtokyocabinet replay key disappeared: %s", (char *)tcxstrptr(key));
      continue;
    }
    tcxstrcat(data, iter, iter_size);
    free(iter);

    if (_callback_for_record(server, key, data, add_fn, add_context) != GEARMAND_SUCCESS)
    {
      gret= GEARMAND_QUEUE_ERROR;
      break;
    }

    ++x;
  }
  tcxstrdel(key);
  tcxstrdel(data);

  gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "libtokyocabinet replayed %ld records", x);

  return gret;
}
#pragma GCC diagnostic pop
