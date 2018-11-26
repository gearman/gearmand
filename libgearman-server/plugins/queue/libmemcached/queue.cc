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
 * @brief libmemcached Queue Storage Definitions
 */

#include <libgearman-server/plugins/queue/libmemcached/queue.h>

#if defined(GEARMAND_PLUGINS_QUEUE_LIBMEMCACHED_H)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

using namespace gearmand;

/**
 * @addtogroup gearmand::plugins::queue::Libmemcachedatic Static libmemcached Queue Storage Functions
 * @ingroup gearman_queue_libmemcached
 * @{
 */

/**
 * Default values.
 */
namespace gearmand { namespace plugins { namespace queue { class Libmemcached;  }}}

namespace gearmand {
namespace queue {

class LibmemcachedQueue : public gearmand::queue::Context 
{
public:
  LibmemcachedQueue(memcached_server_st* servers, const std::string& prefix):
    memc_(nullptr),
    prefix_(prefix)
  { 
    memc_= memcached_create(nullptr);

    memcached_server_push(memc_, servers);
  }

  bool init()
  {
    return bool(bool(memc_) and memcached_server_count(memc_));
  }

  ~LibmemcachedQueue()
  {
    memcached_free(memc_);
    memc_= nullptr;
  }

  gearmand_error_t add(gearman_server_st *server,
                       const char *unique, size_t unique_size,
                       const char *function_name, size_t function_name_size,
                       const void *data, size_t data_size,
                       gearman_job_priority_t priority,
                       int64_t when);

  gearmand_error_t flush(gearman_server_st *server);

  gearmand_error_t done(gearman_server_st *server,
                        const char *unique, size_t unique_size,
                        const char *function_name, size_t function_name_size);

  gearmand_error_t replay(gearman_server_st *server);

private:
  memcached_st* memc_;
  const std::string prefix_;
};

} // namespace queue
} // namespace gearmand

namespace gearmand {
namespace plugins {
namespace queue {

class Libmemcached : public gearmand::plugins::Queue {
public:
  Libmemcached ();
  ~Libmemcached ();

  gearmand_error_t initialize();

  std::string server_list;
  std::string prefix;
private:

};

Libmemcached::Libmemcached() :
  Queue("libmemcached")
{
  command_line_options().add_options()
    ("libmemcached-servers", boost::program_options::value(&server_list), "List of Memcached servers to use.")
    ("libmemcached-prefix", boost::program_options::value(&prefix)->default_value("gear_"), "Prefix to use in memcached keys");
}

Libmemcached::~Libmemcached()
{
}

gearmand_error_t Libmemcached::initialize()
{
  gearmand_info("Initializing libmemcached module");

  memcached_server_st *servers= memcached_servers_parse(server_list.c_str());
  if (!servers)
  {
    return gearmand_gerror("memcached_servers_parse", GEARMAND_QUEUE_ERROR);
  }

  gearmand::queue::LibmemcachedQueue* exec_queue = new gearmand::queue::LibmemcachedQueue { servers, prefix };
  if (exec_queue and exec_queue->init())
  {
    gearman_server_set_queue(Gearmand()->server, exec_queue);

    memcached_server_list_free(servers);

    return GEARMAND_SUCCESS;
  }

  return gearmand_gerror("Libmemcached::initialize()", GEARMAND_QUEUE_ERROR);
}

void initialize_libmemcached()
{
  static Libmemcached local_instance {};
}

} // namespace queue
} // namespace plugins
} // namespace gearmand

/* Queue callback functions. */

namespace gearmand {
namespace queue {

gearmand_error_t LibmemcachedQueue::add(gearman_server_st *server,
                                        const char *unique,
                                        size_t unique_size,
                                        const char *function_name,
                                        size_t function_name_size,
                                        const void *data, size_t data_size,
                                        gearman_job_priority_t priority,
                                        int64_t when)
{
  if (when) // No support for EPOCH jobs
  {
    return gearmand_gerror("libmemcached queue does not support epoch jobs", GEARMAND_QUEUE_ERROR);
  }

  (void)server;

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "libmemcached add: %.*s", (uint32_t)unique_size, (char *)unique);

  char key[MEMCACHED_MAX_KEY];
  size_t key_length= (size_t)snprintf(key, MEMCACHED_MAX_KEY, "%s%.*s-%.*s",
                                      prefix_.c_str(),
                                      (int)function_name_size,
                                      (const char *)function_name, (int)unique_size,
                                      (const char *)unique);

  memcached_return_t rc= memcached_set(memc_, (const char *)key, key_length,
                                     (const char *)data, data_size, 0, (uint32_t)priority);

  if (!memcached_success(rc))
  {
    return gearmand_gerror(memcached_strerror(memc_, rc), GEARMAND_QUEUE_ERROR);
  }

  return GEARMAND_SUCCESS;
}

gearmand_error_t LibmemcachedQueue::flush(gearman_server_st *)
{
  gearmand_debug("libmemcached flush");

  return GEARMAND_SUCCESS;
}

gearmand_error_t LibmemcachedQueue::done(gearman_server_st*,
                                         const char *unique, size_t unique_size,
                                         const char *function_name, size_t function_name_size)
{
  char key[MEMCACHED_MAX_KEY];

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "libmemcached done: %.*s", (uint32_t)unique_size, (char *)unique);

  size_t key_length= (size_t)snprintf(key, MEMCACHED_MAX_KEY, "%s%.*s-%.*s",
                                      prefix_.c_str(),
                                      (int)function_name_size,
                                      (const char *)function_name, (int)unique_size,
                                      (const char *)unique);

  /* For the moment we will assume it happened */
  memcached_return_t rc= memcached_delete(memc_, (const char *)key, key_length, 0);
  if (!memcached_success(rc))
  {
    if(rc != MEMCACHED_NOTFOUND)
      return gearmand_gerror(memcached_strerror(memc_, rc), GEARMAND_QUEUE_ERROR);
  }

  return GEARMAND_SUCCESS;
}

class Replay
{
public:
  Replay(gearman_server_st* server_arg, memcached_st* _memc, const std::string& _prefix) :
    server_(server_arg),
    memc_(nullptr),
    prefix_(_prefix)
  {
    memc_= memcached_clone(nullptr, _memc);
  }

  ~Replay()
  {
    memcached_free(memc_);
    memc_= nullptr;
  }

  bool init()
  {
    return bool(memc_);
  }

  memcached_st* memc()
  {
    assert(memc_);
    return memc_;
  }

  gearman_server_st* server()
  {
    return server_;
  }

  const char* prefix()
  {
    return prefix_.c_str();
  }

private:
  gearman_server_st* server_;
  memcached_st* memc_;
  const std::string prefix_;
};

static memcached_return_t callback_loader(const memcached_st*,
                                        memcached_result_st* result,
                                        void *context)
{
  Replay* replay= (Replay*)context;

  const char *key= memcached_result_key_value(result);
  const char *prefix= replay->prefix();

  if (strncmp(key, prefix, strlen(prefix)) != 0)
  {
    gearmand_debug("memcached key did not match specified prefix");
    return MEMCACHED_SUCCESS;
  }

  const char* function= key +strlen(prefix);

  const char* unique= index(function, '-');
  if (!unique)
  {
    gearmand_debug("memcached key was malformed was not found");
    return MEMCACHED_SUCCESS;
  }

  size_t function_len= size_t(unique -function);
  unique++;
  size_t unique_size= strlen(unique);

  assert(function);
  assert(function_len);

  /* need to make a copy here ... gearman_server_job_free will free it later */
  char* data= (char*)malloc(memcached_result_length(result));
  if (!data)
  {
    gearmand_perror(errno, "malloc");
    return MEMCACHED_MEMORY_ALLOCATION_FAILURE;
  } 
  memcpy(data, memcached_result_value(result), memcached_result_length(result));

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "libmemcached replay_add: %.*s", (uint32_t)unique_size, (char *)unique);

  /* Currently not looking at failure cases */
  LibmemcachedQueue::replay_add(replay->server(),
                                nullptr,
                                unique, unique_size,
                                function, function_len,
                                data, memcached_result_length(result),
                                static_cast<gearman_job_priority_t>(memcached_result_flags(result)), int64_t(0));


  return MEMCACHED_SUCCESS;
}

/* Grab the object and load it into the loader */
static memcached_return_t callback_for_key(const memcached_st*,
                                         const char *key, size_t key_length,
                                         void *context)
{
  Replay* replay= (Replay*)context;
  memcached_execute_fn callbacks{(memcached_execute_fn)&callback_loader};
  char *passable{(char *)key};
  memcached_return_t rc = memcached_mget(replay->memc(), &passable, &key_length, 1);
  if (memcached_success(rc))
  {
    gearmand_debug(memcached_strerror(replay->memc(), rc));
  }

  /* Just void errors for the moment, since other treads might have picked up the object. */
  (void)memcached_fetch_execute(replay->memc(), &callbacks, replay, 1);

  return MEMCACHED_SUCCESS;
}

/*
  If we have any failures for loading values back into replay we just ignore them.
*/
gearmand_error_t LibmemcachedQueue::replay(gearman_server_st *server)
{
  memcached_dump_fn callbacks{(memcached_dump_fn)&callback_for_key};

  gearmand_debug("libmemcached replay start");

  memcached_st* local_clone= memcached_clone(nullptr, memc_);

  if (local_clone)
  {
    Replay replay_exec(server, memc_, prefix_);

    if (replay_exec.init())
    {
      (void)memcached_dump(local_clone, &callbacks, (void *)&replay_exec, 1);
    }
    else
    {
      gearmand_debug("libmemcached failed to init() Replay");
    }
    memcached_free(local_clone);
    local_clone= nullptr;
  }
  gearmand_debug("libmemcached replay stop");

  return GEARMAND_SUCCESS;
}

} // queue
} // gearmand

#pragma GCC diagnostic pop
#endif // defined(GEARMAND_PLUGINS_QUEUE_LIBMEMCACHED_H)
