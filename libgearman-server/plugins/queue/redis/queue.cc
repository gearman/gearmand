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
#include <libgearman-server/plugins/queue/redis/queue.h>

#if defined(GEARMAND_PLUGINS_QUEUE_REDIS_H)

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

/**
 * gearmand::plugins::queue::Hiredis::redis()
 *
 * returns _redis
 */
redisContext* gearmand::plugins::queue::Hiredis::redis()
{
  return this->_redis;
}

/*
 * gearmand::plugins::queue::Hiredis::hmset(vchar_t key, const void *data, size_t data_size, uint32_t priority)
 *
 * returns true if hiredis HMSET succeeded
 */
bool gearmand::plugins::queue::Hiredis::hmset(vchar_t key, const void *data, size_t data_size, uint32_t priority) {
  redisContext* context = this->redis();
  const size_t argc = 6;
  std::string _priority = std::to_string((uint32_t)priority);

  const size_t argvlen[argc] = {
    (size_t)5,
    (size_t)key.size(),
    (size_t)4,
    (size_t)data_size,
    (size_t)8,
    _priority.size()
  };

  std::vector<const char*> argv {"HMSET"};
  argv.push_back( &key[0] );
  argv.push_back( "data" );
  argv.push_back( static_cast<const char*>(data) );
  argv.push_back( "priority" );
  argv.push_back( _priority.c_str() );

  redisReply *reply = (redisReply *)redisCommandArgv(context, static_cast<int>(argv.size()), &(argv[0]), &(argvlen[0]) );
  if (reply == nullptr)
      return false;

  bool res = (reply->type == REDIS_REPLY_STATUS);

  freeReplyObject(reply);

  return res;
}

/*
 * bool gearmand::plugins::queue::Hiredis::fetch(char *key, gearmand::plugins::queue::redis_record_t &req)
 *
 * fetch redis result for the key by HGETALL command and put it into the redis_record_t
 *
 * returns true on success
 */
bool gearmand::plugins::queue::Hiredis::fetch(char *key, gearmand::plugins::queue::redis_record_t &req)
{
  redisContext * context = this->redis();
  redisReply * reply = (redisReply*)redisCommand(context, "HGETALL %s", key);
  if (reply == nullptr)
    return false;

  //FIXME remove workaround
  if(reply->type == REDIS_REPLY_ERROR) {
    // workaround to ensure gearmand upgrade.
    // gearmand <=1.1.15 stores data in string, not in hash.
    gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "redis replies for HGETALL: %s", reply->str);

    reply = (redisReply*)redisCommand(context, "TYPE %s", key);
    if (reply == nullptr)
      return false;

    if(strcmp(reply->str, "string") != 0) {
      gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "unexpected type of the value stored in key: %s", reply->str);
      return false;
    }

    reply = (redisReply*)redisCommand(context, "GET %s", key);
    if (reply == nullptr)
      return false;

    std::string s{reply->str};
    req.data = s;
    req.priority = GEARMAN_JOB_PRIORITY_NORMAL;
  } else {
    // 2 x (key + value)
    assert(reply->elements == 4);
    auto fk = reply->element[0]->str;
    if(strcmp(fk, "data") == 0) {
      std::string s{reply->element[1]->str};
      req.data = s;
      req.priority = (uint32_t)std::stoi(reply->element[3]->str);
    } else if (strcmp(fk, "priority") == 0) {
      std::string s{reply->element[3]->str};
      req.data = s;
      req.priority = (uint32_t)std::stoi(reply->element[1]->str);
    } else {
      gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "unexpected key %s", fk);
      return false;
    }
  }

  freeReplyObject(reply);

  return true;
}

/**
 * gearmand::plugins::queue::Hiredis::Hiredis()
 *
 * setup server, service and password properties
 *
 */
gearmand::plugins::queue::Hiredis::Hiredis() :
  Queue("redis"),
  _redis(nullptr),
  server("127.0.0.1"),
  service("6379"),
  prefix("_gear_")
{
  command_line_options().add_options()
    ("redis-server", boost::program_options::value(&server), "Redis server")
    ("redis-port", boost::program_options::value(&service), "Redis server port/service")
    ("redis-password", boost::program_options::value(&password), "Redis server password/service")
    ("redis-prefix", boost::program_options::value(&prefix), "Prefix to use in redis keys");
}

/**
 * gearmand::plugins::queue::Hiredis::~Hiredis()
 *
 * free _redis context
 */
gearmand::plugins::queue::Hiredis::~Hiredis()
{
  if(this->_redis)
    redisFree(this->_redis);
}

gearmand_error_t gearmand::plugins::queue::Hiredis::initialize()
{
  int service_port= atoi(service.c_str());
  if ((_redis= redisConnect(server.c_str(), service_port)) == nullptr)
  {
    return gearmand_log_gerror(
      GEARMAN_DEFAULT_LOG_PARAM,
      GEARMAND_QUEUE_ERROR,
      "Could not connect to redis server: %s", _redis->errstr);
  }

  if (password.size())
  {
    redisReply *reply = (redisReply*)redisCommand(_redis, "AUTH %s", password.c_str());
    if(reply == nullptr)
    {
        return gearmand_log_gerror(
          GEARMAN_DEFAULT_LOG_PARAM,
          GEARMAND_QUEUE_ERROR,
          "Failed to exec AUTH command, redis server reply: %s", _redis->errstr);
    }

    if(reply->type == REDIS_REPLY_ERROR)
    {
        gearmand_log_gerror(
          GEARMAN_DEFAULT_LOG_PARAM,
          GEARMAND_QUEUE_ERROR,
          "Could not pass redis server auth, redis server reply: %s", reply->str);
        freeReplyObject(reply);

        return GEARMAND_QUEUE_ERROR;
    }

    freeReplyObject(reply);
    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Auth success");
  }

  gearmand_info("Initializing hiredis module");

  gearman_server_set_queue(Gearmand()->server, this, _hiredis_add, _hiredis_flush, _hiredis_done, _hiredis_replay);

  return GEARMAND_SUCCESS;
}

/**
 * define static gearmand::plugins::queue::Hiredis
 */
void gearmand::plugins::queue::initialize_redis()
{
  static gearmand::plugins::queue::Hiredis local_instance;
}

#define GEARMAND_KEY_LITERAL "%s-%.*s-%*s"

static size_t build_key(vchar_t &key,
                        const char *prefix,
                        size_t prefix_size,
                        const char *unique,
                        size_t unique_size,
                        const char *function_name,
                        size_t function_name_size)
{
  size_t buf_size = function_name_size + unique_size + prefix_size + 4;
  char buf[buf_size];
  // buf size is overestimated
  // so buf contains some \0 at the end
  int key_size= snprintf(buf, buf_size, GEARMAND_KEY_LITERAL,
                         prefix,
                         (int)function_name_size, function_name,
                         (int)unique_size, unique);
  if (size_t(key_size) >= buf_size or key_size <= 0)
  {
    assert(0);
    return -1;
  }

  // std::string removes all \0 at the end of buf
  std::string s{buf};

  key.resize(0);
  std::copy(s.begin(), s.end(), std::back_inserter(key));

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
                                     gearman_job_priority_t priority,
                                     int64_t when)
{
  if (when) // No support for EPOCH jobs
  {
    return gearmand_gerror("hiredis queue does not support epoch jobs", GEARMAND_QUEUE_ERROR);
  }

  gearmand_log_debug(
    GEARMAN_DEFAULT_LOG_PARAM,
    "hires add func: %.*s, unique: %.*s",
    (uint32_t)function_name_size, function_name,
    (uint32_t)unique_size, (char *)unique);

  gearmand::plugins::queue::Hiredis *queue= (gearmand::plugins::queue::Hiredis *)context;

  vchar_t key;
  build_key(key, queue->prefix.c_str(), queue->prefix.size(), unique, unique_size, function_name, function_name_size);

  gearmand_log_debug(
    GEARMAN_DEFAULT_LOG_PARAM,
    "hires key: %u", (uint32_t)key.size());
  if (queue->hmset(key, data, data_size, (uint32_t)priority))
    return GEARMAND_SUCCESS;

  return gearmand_log_gerror(
    GEARMAN_DEFAULT_LOG_PARAM,
    GEARMAND_QUEUE_ERROR,
    "failed to insert '%.*s' into redis", (uint32_t)key.size(), &key[0]);
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

  vchar_t key;
  build_key(key, queue->prefix.c_str(), queue->prefix.size(), unique, unique_size, function_name, function_name_size);

  redisReply *reply= (redisReply*)redisCommand(queue->redis(), "DEL %b", &key[0], key.size());
  if (reply == nullptr)
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

  redisReply *reply= (redisReply*)redisCommand(queue->redis(), "KEYS %s*", queue->prefix.c_str());
  if (reply == nullptr)
  {
    return gearmand_log_gerror(
      GEARMAN_DEFAULT_LOG_PARAM,
      GEARMAND_QUEUE_ERROR,
      "Failed to call KEYS during QUEUE replay: %s", queue->redis()->errstr);
  }

  for (size_t x= 0; x < reply->elements; x++)
  {
    char* prefix= (char*) malloc(queue->prefix.size() * sizeof(char));
    char function_name[GEARMAN_FUNCTION_MAX_SIZE];
    char unique[GEARMAN_MAX_UNIQUE_SIZE];

    char fmt_str[100] = "";
    int fmt_str_length= snprintf(fmt_str, sizeof(fmt_str), "%%%d[^-]-%%%d[^-]-%%%d[^*]",
                                 int(queue->prefix.size()),
                                 int(GEARMAN_FUNCTION_MAX_SIZE),
                                 int(GEARMAN_MAX_UNIQUE_SIZE));
    if (fmt_str_length <= 0 or size_t(fmt_str_length) >= sizeof(fmt_str))
    {
      free(prefix);
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

    free(prefix);
    if (ret != 3)
    {
      continue;
    }

    gearmand::plugins::queue::redis_record_t record;
    if(!queue->fetch(reply->element[x]->str, record))
    {
      return gearmand_log_gerror(
        GEARMAN_DEFAULT_LOG_PARAM,
        GEARMAND_QUEUE_ERROR,
        "Failed to fetch data for the key: %s", reply->element[x]->str);
    }

    /* need to make a copy here ... gearman_server_job_free will free it later */
    char *data = strdup(record.data.c_str());
    size_t data_size = record.data.size();
    gearman_job_priority_t priority = static_cast<gearman_job_priority_t>(record.priority);

    (void)(add_fn)(server, add_context,
                   unique, strlen(unique),
                   function_name, strlen(function_name),
                   data, data_size,
                   priority, 0);
  }

  freeReplyObject(reply);

  return GEARMAND_SUCCESS;
}
#pragma GCC diagnostic pop
#pragma GCC diagnostic pop
#endif // defined(GEARMAND_PLUGINS_QUEUE_REDIS_H)
