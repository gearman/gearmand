/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011-2012 Data Differential, http://datadifferential.com/
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
 * @brief Redis Queue Storage Definitions
 */

#include <gear_config.h>
#include <libgearman-server/common.h>

#include <libgearman-server/plugins/queue/redis/queue.h>
#include <libgearman-server/plugins/queue/base.h>

#if defined(HAVE_HIREDIS) && HAVE_HIREDIS

#include <hiredis/hiredis.h>

/* Queue callback functions. */
static gearmand_error_t _hiredis_add(gearman_server_st *server, void *context,
                                             const char *unique,
                                             size_t unique_size,
                                             const char *function_name,
                                             size_t function_name_size,
                                             const void *data, size_t data_size,
                                             gearman_job_priority_t priority,
                                             int64_t when);

static gearmand_error_t _hiredis_flush(gearman_server_st *server, void *context);

static gearmand_error_t _hiredis_done(gearman_server_st *server, void *context,
                                              const char *unique,
                                              size_t unique_size, 
                                              const char *function_name, 
                                              size_t function_name_size);

static gearmand_error_t _hiredis_replay(gearman_server_st *server, void *context,
                                                gearman_queue_add_fn *add_fn,
                                                void *add_context);


namespace gearmand { namespace plugins { namespace queue { class Hiredis;  }}}

namespace gearmand {
namespace plugins {
namespace queue {

class Hiredis : public Queue {
public:
  Hiredis();
  ~Hiredis();

  gearmand_error_t initialize();

  redisContext* redis()
  {
    return _redis;
  }

  std::string server;
  std::string service;
  std::string password;

private:
  redisContext *_redis;
};

Hiredis::Hiredis() :
  Queue("redis"),
  server("127.0.0.1"),
  service("6379"),
  _redis(NULL)
{
  command_line_options().add_options()
    ("redis-server", boost::program_options::value(&server), "Redis server")
    ("redis-port", boost::program_options::value(&service), "Redis server port/service")
	("redis-password", boost::program_options::value(&password), "Redis server password/service");
}

Hiredis::~Hiredis()
{
}

gearmand_error_t Hiredis::initialize()
{
  int service_port= atoi(service.c_str());
  if ((_redis= redisConnect(server.c_str(), service_port)) == NULL)
  {
    return gearmand_log_gerror(
      GEARMAN_DEFAULT_LOG_PARAM,
      GEARMAND_QUEUE_ERROR,
      "Could not connect to redis server: %s", _redis->errstr);
  }

  if (password.size())
  {
	  redisReply *reply = (redisReply*)redisCommand(_redis, "AUTH %s", password.c_str());
	  if (reply == NULL)
	  {
		  return gearmand_log_gerror(
		  GEARMAN_DEFAULT_LOG_PARAM,
		  GEARMAND_QUEUE_ERROR,
		  "Faild to auth with redis server error");
	  } else {
	      if(reply->type == REDIS_REPLY_ERROR) {
          char *reply_str = reply->str;
		  uint32_t reply_len = (uint32_t)reply->len;
          freeReplyObject(reply);
		  
		  return gearmand_log_gerror(
			GEARMAN_DEFAULT_LOG_PARAM,
			GEARMAND_QUEUE_ERROR,
		    "Could not auth with redis server,hires auth reply: %.*s",reply_len, reply_str);
        }
    }
    freeReplyObject(reply);
	gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Auth success");
  }

  gearmand_info("Initializing hiredis module");

  gearman_server_set_queue(Gearmand()->server, this, _hiredis_add, _hiredis_flush, _hiredis_done, _hiredis_replay);   

  return GEARMAND_SUCCESS;
}

void initialize_redis()
{
  static Hiredis local_instance;
}

} // namespace queue
} // namespace plugins
} // namespace gearmand

typedef std::vector<char> vchar_t;
#define GEARMAND_QUEUE_GEARMAND_DEFAULT_PREFIX "_gear_"
#define GEARMAND_QUEUE_GEARMAND_DEFAULT_PREFIX_SIZE sizeof(GEARMAND_QUEUE_GEARMAND_DEFAULT_PREFIX)
#define GEARMAND_KEY_LITERAL "%s-%.*s-%*s"

static size_t build_key(vchar_t &key,
                        const char *unique,
                        size_t unique_size, 
                        const char *function_name,
                        size_t function_name_size)
{
  key.resize(function_name_size +unique_size +GEARMAND_QUEUE_GEARMAND_DEFAULT_PREFIX_SIZE +4);
  int key_size= snprintf(&key[0], key.size(), GEARMAND_KEY_LITERAL,
                         GEARMAND_QUEUE_GEARMAND_DEFAULT_PREFIX,
                         (int)function_name_size, function_name,
                         (int)unique_size, unique);
  if (size_t(key_size) >= key.size() or key_size <= 0)
  {
    assert(0);
    return -1;
  }

  return key.size();
}


/**
 * @addtogroup gearman_queue_hiredis hiredis Queue Storage Functions
 * @ingroup gearman_queue
 * @{
 */

/*
 * Private declarations
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

/*
 * Private definitions
 */

static gearmand_error_t _hiredis_add(gearman_server_st *, void *context,
                                     const char *unique,
                                     size_t unique_size,
                                     const char *function_name,
                                     size_t function_name_size,
                                     const void *data, size_t data_size,
                                     gearman_job_priority_t,
                                     int64_t when)
{
  gearmand::plugins::queue::Hiredis *queue= (gearmand::plugins::queue::Hiredis *)context;

  if (when) // No support for EPOCH jobs
  {
    return GEARMAND_QUEUE_ERROR;
  }

  gearmand_log_debug(
    GEARMAN_DEFAULT_LOG_PARAM,
    "hires add: %.*s", (uint32_t)unique_size, (char *)unique);

  std::vector<char> key;
  build_key(key, unique, unique_size, function_name, function_name_size);
  gearmand_log_debug(
    GEARMAN_DEFAULT_LOG_PARAM,
    "hires key: %u", (uint32_t)key.size());

  redisReply *reply= (redisReply*)redisCommand(queue->redis(), "SET %s %b", &key[0], data, data_size);
  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "got reply");
  if (reply == NULL)
  {
    return gearmand_log_gerror(
      GEARMAN_DEFAULT_LOG_PARAM,
      GEARMAND_QUEUE_ERROR,
      "failed to insert '%.*s' into redis", key.size(), &key[0]);
  }
  freeReplyObject(reply);

  return GEARMAND_SUCCESS;
}

static gearmand_error_t _hiredis_flush(gearman_server_st *, void *)
{
  return GEARMAND_SUCCESS;
}

static gearmand_error_t _hiredis_done(gearman_server_st *, void *context,
                                      const char *unique,
                                      size_t unique_size, 
                                      const char *function_name,
                                      size_t function_name_size)
{
  gearmand::plugins::queue::Hiredis *queue= (gearmand::plugins::queue::Hiredis *)context;

  gearmand_log_debug(
    GEARMAN_DEFAULT_LOG_PARAM,
    "hires done: %.*s", (uint32_t)unique_size, (char *)unique);

  std::vector<char> key;
  build_key(key, unique, unique_size, function_name, function_name_size);

  redisReply *reply= (redisReply*)redisCommand(queue->redis(), "DEL %s", &key[0]);
  if (reply == NULL)
  {
    return gearmand_log_gerror(
      GEARMAN_DEFAULT_LOG_PARAM,
      GEARMAND_QUEUE_ERROR,
      "Failed to call DEL for key %s: %s", &key[0], queue->redis()->errstr);
  }
  freeReplyObject(reply);

  return GEARMAND_SUCCESS;
}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
static gearmand_error_t _hiredis_replay(gearman_server_st *server, void *context,
                                                gearman_queue_add_fn *add_fn,
                                                void *add_context)
{
  gearmand::plugins::queue::Hiredis *queue= (gearmand::plugins::queue::Hiredis *)context;

  gearmand_info("hiredis replay start");

  redisReply *reply= (redisReply*)redisCommand(queue->redis(), "KEYS %s*", GEARMAND_QUEUE_GEARMAND_DEFAULT_PREFIX);
  if (reply == NULL)
  {
    return gearmand_log_gerror(
      GEARMAN_DEFAULT_LOG_PARAM,
      GEARMAND_QUEUE_ERROR,
      "Failed to call KEYS during QUEUE replay: %s", queue->redis()->errstr);
  }

  for (size_t x= 0; x < reply->elements; x++)
  {
    char prefix[GEARMAND_QUEUE_GEARMAND_DEFAULT_PREFIX_SIZE];
    char function_name[GEARMAN_FUNCTION_MAX_SIZE];
    char unique[GEARMAN_MAX_UNIQUE_SIZE];

    char fmt_str[100] = "";
    int fmt_str_length= snprintf(fmt_str, sizeof(fmt_str), "%%%d[^-]-%%%d[^-]-%%%ds",
                                 int(GEARMAND_QUEUE_GEARMAND_DEFAULT_PREFIX_SIZE),
                                 int(GEARMAN_FUNCTION_MAX_SIZE),
                                 int(GEARMAN_MAX_UNIQUE_SIZE));
    if (fmt_str_length <= 0 or size_t(fmt_str_length) >= sizeof(fmt_str))
    {
      assert(fmt_str_length != 1);
      return gearmand_gerror(
        "snprintf() failed to produce a valud fmt_str for redis key",
        GEARMAND_QUEUE_ERROR);
    }
    int ret= sscanf(reply->element[x]->str,
                    fmt_str,
                    prefix,
                    function_name,
                    unique);
    if (ret == 0)
    {
      continue;
    }

    redisReply *get_reply= (redisReply*)redisCommand(queue->redis(), "GET %s", reply->element[x]->str);
    if (get_reply == NULL)
    {
      gearmand_log_debug(
        GEARMAN_DEFAULT_LOG_PARAM,
        "GET %s failed: %s", reply->element[x]->str, queue->redis()->errstr);
      continue;
    }

    /* need to make a copy here ... gearman_server_job_free will free it later */
    char * data = (char*)malloc(get_reply->len);
    if (data == NULL)
    {
      return gearmand_perror(errno, "malloc");
    }
    memcpy(data, get_reply->str, get_reply->len);

    (void)(add_fn)(server, add_context,
                   unique, strlen(unique),
                   function_name, strlen(function_name),
                   data, get_reply->len,
                   GEARMAN_JOB_PRIORITY_NORMAL, 0);
    freeReplyObject(get_reply);
  }
  freeReplyObject(reply);

  return GEARMAND_SUCCESS;
}
#pragma GCC diagnostic pop
#pragma GCC diagnostic pop

#endif // defined(HAVE_HIREDIS) && HAVE_HIREDIS
