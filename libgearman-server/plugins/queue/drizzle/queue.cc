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
 * @brief libdrizzle Queue Storage Definitions
 */

#include <gear_config.h>
#include <libgearman-server/common.h>
#include <libgearman-server/byte.h>
#include <libgearman-1.0/limits.h>

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>

#include <libgearman-server/plugins/queue/drizzle/queue.h>
#include <libgearman-server/plugins/queue/base.h>

#if defined(HAVE_LIBDRIZZLE) && HAVE_LIBDRIZZLE
# include <libdrizzle-5.1/drizzle_client.h>

using namespace gearmand_internal;
using namespace gearmand;

namespace gearmand { namespace plugins { namespace queue { class Drizzle; }}}

static gearmand_error_t gearman_server_queue_libdrizzle_init(plugins::queue::Drizzle *queue, gearman_server_st *server);

/**
 * @addtogroup plugins::queue::Drizzleatic Static libdrizzle Queue Storage Definitions
 * @ingroup gearman_queue_libdrizzle
 * @{
 */

/**
 * Default values.
 */
#define GEARMAND_QUEUE_LIBDRIZZLE_DEFAULT_DATABASE "gearman"
#define GEARMAND_QUEUE_LIBDRIZZLE_DEFAULT_TABLE "queue"
#define GEARMAND_QUEUE_QUERY_BUFFER 256

static bool libdrizzle_failed(drizzle_return_t arg) 
{
  return arg != DRIZZLE_RETURN_OK;
}

typedef std::vector<char> vchar_t;

ssize_t escape_string(vchar_t &destination, const char *from, size_t from_size)
{
  if (from_size == 0)
  {
    return 0;
  }

  destination.reserve(from_size * 2);
  char newchar;
  const char *end;

  for (end= from + from_size; from < end; from++)
  {
    newchar= 0;
    /* All multi-byte UTF8 characters have the high bit set for all bytes. */
    if (!(*from & 0x80))
    {
      switch (*from)
      {
      case 0:
        newchar= '0';
        break;
      case '\n':
        newchar= 'n';
        break;
      case '\r':
        newchar= 'r';
        break;
      case '\032':
        newchar= 'Z';
        break;
      case '\\':
        newchar= '\\';
        break;
      case '\'':
        newchar= '\'';
        break;
      case '"':
        newchar= '"';
        break;
      default:
        break;
      }
    }

    if (newchar != '\0')
    {
      destination.push_back('\\');
      destination.push_back(newchar);
    }
    else
    {
      destination.push_back(*from);
    }
  }

  return destination.size();
}

namespace gearmand {
namespace plugins {
namespace queue {

class Drizzle : public gearmand::plugins::Queue {
public:
  Drizzle();
  ~Drizzle();

  gearmand_error_t initialize();

  drizzle_result_st *result()
  {
    return _result;
  }

  void resize_query(size_t arg)
  {
    _query.resize(arg);
  }

  char *query_ptr()
  {
    return &_query[0];
  }

  void set_epoch_support(bool arg)
  {
    _epoch_support= arg;
  }

  bool epoch_support()
  {
    return _epoch_support;
  }

  drizzle_st *drizzle;
  drizzle_st *insert_con;
  drizzle_result_st* _result;
  std::vector<char> _query;
  std::string host;
  std::string username;
  std::string password;
  std::string uds;
  std::string user;
  std::string schema;
  std::string table;
  bool mysql_protocol;
  in_port_t port;

private:
  bool _epoch_support;
};


Drizzle::Drizzle () :
  Queue("libdrizzle"),
  drizzle(NULL),
  insert_con(NULL),
  _result(NULL),
  _query(),
  username(""),
  mysql_protocol(false),
  port(DRIZZLE_DEFAULT_TCP_PORT),
  _epoch_support(true)
{
  command_line_options().add_options()
  ("libdrizzle-host", boost::program_options::value(&host)->default_value(DRIZZLE_DEFAULT_TCP_HOST), "Host of server.")
  ("libdrizzle-port", boost::program_options::value(&port)->default_value(DRIZZLE_DEFAULT_TCP_PORT), "Port of server. (by default Drizzle)")
  ("libdrizzle-uds", boost::program_options::value(&uds), "Unix domain socket for server.")
  ("libdrizzle-user", boost::program_options::value(&user)->default_value("root"), "User name for authentication.")
  ("libdrizzle-password", boost::program_options::value(&password), "Password for authentication.")
  ("libdrizzle-db", boost::program_options::value(&schema)->default_value(GEARMAND_QUEUE_LIBDRIZZLE_DEFAULT_DATABASE), "Database to use.")
  ("libdrizzle-table", boost::program_options::value(&table)->default_value(GEARMAND_QUEUE_LIBDRIZZLE_DEFAULT_TABLE), "Table to use.")
  ("libdrizzle-mysql", boost::program_options::bool_switch(&mysql_protocol)->default_value(false), "Use MySQL protocol.")
  ;

  drizzle_set_timeout(drizzle, -1);
  assert(drizzle_timeout(drizzle) == -1);
}

Drizzle::~Drizzle()
{
  drizzle_quit(insert_con);
  drizzle_quit(drizzle);
}

gearmand_error_t Drizzle::initialize()
{
  return gearman_server_queue_libdrizzle_init(this, &Gearmand()->server);
}

void initialize_drizzle()
{
  static Drizzle local_instance;
}

} // namespace queue
} // namespace plugins
} // namespace gearmand

/*
 * thin wrapper around drizzle_query to handle the server going away
 * this happens usually because of a low wait_timeout value
 * we attempt to connect back only once
 */
static drizzle_result_st *_libdrizzle_query_with_retry(drizzle_st* con,
                                                       const char *query, size_t query_size,
                                                       drizzle_return_t& ret_ptr)
{
  drizzle_result_st *query_result= NULL;
  ret_ptr= DRIZZLE_RETURN_LOST_CONNECTION;
  for (int retry= 0; (ret_ptr == DRIZZLE_RETURN_LOST_CONNECTION) && (retry < 2); ++retry)
  {
    query_result= drizzle_query(con, query, query_size, &ret_ptr);
  }

  if (libdrizzle_failed(ret_ptr))
  {
    if (ret_ptr == DRIZZLE_RETURN_COULD_NOT_CONNECT)
    {
      gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM,
                        "Failed to connect to database instance. host: %s:%d user: %s schema: %s (%s)",
                         drizzle_host(con),
                         int(drizzle_port(con)),
                         drizzle_user(con),
                         drizzle_db(con),
                         drizzle_error(con));
    }
    else
    {
      gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM,
                        "libdrizled error '%s' executing '%.*s'",
                         drizzle_error(con),
                         query_size, query);
    }
  }

  return query_result;
}

/**
 * Query handling function.
 */
static drizzle_return_t _libdrizzle_query(plugins::queue::Drizzle *queue,
                                          const char *query, size_t query_size)
{
  drizzle_return_t ret;

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "libdrizzle query: %.*s", (uint32_t)query_size, query);

  drizzle_result_st *result= _libdrizzle_query_with_retry(queue->drizzle, query, query_size, ret);
  if (libdrizzle_failed(ret))
  {
    return ret;
  }
  (void)result;

  return DRIZZLE_RETURN_OK;
}

static drizzle_return_t _libdrizzle_insert(plugins::queue::Drizzle *queue,
                                           const vchar_t& query)
{
  drizzle_return_t ret;

  drizzle_result_st *result= _libdrizzle_query_with_retry(queue->insert_con, &query[0], query.size() -1, ret);
  if (libdrizzle_failed(ret))
  {
    return ret;
  }

  ret= drizzle_result_buffer(result);

  drizzle_result_free(result);

  return ret;
}

/* Queue callback functions. */
static gearmand_error_t _libdrizzle_add(gearman_server_st *server,
                                        void *context,
                                        const char *unique, size_t unique_size,
                                        const char *function_name, size_t function_name_size,
                                        const void *data, size_t data_size,
                                        gearman_job_priority_t priority,
                                        int64_t when);

static gearmand_error_t _libdrizzle_flush(gearman_server_st *gearman,
                                          void *context);

static gearmand_error_t _libdrizzle_done(gearman_server_st *gearman,
                                          void *context, const char *unique, size_t unique_size,
                                         const char *function_name, size_t function_name_size);

static gearmand_error_t _libdrizzle_replay(gearman_server_st *gearman,
                                           void *context,
                                           gearman_queue_add_fn *add_fn,
                                           void *add_context);

/** @} */

/*
 * Public definitions
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

gearmand_error_t gearman_server_queue_libdrizzle_init(plugins::queue::Drizzle *queue,  gearman_server_st *server)
{
  gearmand_info("Initializing libdrizzle module");

#if 0
  if (queue->mysql_protocol)
  {
    drizzle_set_options(queue->drizzle, DRIZZLE_CON_MYSQL);
    drizzle_set_options(queue->insert_con, DRIZZLE_CON_MYSQL);
  }
#endif

  if (queue->uds.empty())
  {
    queue->drizzle= drizzle_create(queue->host.c_str(), queue->port,
                                   queue->user.c_str(), queue->password.c_str(),
                                   "INFORMATION_SCHEMA",
                                   0);
    queue->insert_con= drizzle_create(queue->host.c_str(), queue->port,
                                      queue->user.c_str(), queue->password.c_str(),
                                      "INFORMATION_SCHEMA",
                                      0);

  }
  else
  {
    queue->drizzle= drizzle_create(queue->uds.c_str(), 0,
                                   queue->user.c_str(), queue->password.c_str(),
                                   "INFORMATION_SCHEMA",
                                   0);
    queue->insert_con= drizzle_create(queue->uds.c_str(), 0,
                                      queue->user.c_str(), queue->password.c_str(),
                                      "INFORMATION_SCHEMA",
                                      0);
  }

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "Using '%s' as the username", queue->user.c_str());

  std::string query;
  {
    query.clear();

    query+= "CREATE SCHEMA IF NOT EXISTS  " +queue->schema;
    if (libdrizzle_failed(_libdrizzle_query(queue, query.c_str(), query.size())))
    {
      return gearmand_gerror(drizzle_error(queue->drizzle), GEARMAND_QUEUE_ERROR);
    }

    if (libdrizzle_failed(drizzle_column_skip_all(queue->result())))
    {
      drizzle_result_free(queue->result());
      return gearmand_gerror(drizzle_error(queue->drizzle), GEARMAND_QUEUE_ERROR);
    }
    drizzle_result_free(queue->result());
  }

  // Look for schema
  {
    query.clear();
    query+= "SELECT SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME = \"";
    query+= queue->schema;
    query+= "\"";
    if (libdrizzle_failed(_libdrizzle_query(queue, query.c_str(), query.size())))
    {
      return gearmand_gerror("Error occurred while searching for gearman queue schema", GEARMAND_QUEUE_ERROR);
    }

    if (libdrizzle_failed(drizzle_result_buffer(queue->result())))
    {
      return gearmand_gerror(drizzle_error(queue->drizzle), GEARMAND_QUEUE_ERROR);
    }

    if (drizzle_result_row_count(queue->result()) == 0)
    {
      return gearmand_gerror("Error occurred while search for gearman queue schema", GEARMAND_QUEUE_ERROR);
    }
    drizzle_result_free(queue->result());
  }

  drizzle_return_t ret;
  ret= drizzle_select_db(queue->drizzle, queue->schema.c_str());
  if (libdrizzle_failed(ret))
  {
    return gearmand_gerror(drizzle_error(queue->drizzle), GEARMAND_QUEUE_ERROR);
  }

  ret= drizzle_select_db(queue->insert_con, queue->schema.c_str());
  if (libdrizzle_failed(ret))
  {
    return gearmand_gerror(drizzle_error(queue->insert_con), GEARMAND_QUEUE_ERROR);
  }

  // We need to check and see if the tables exists, and if not create it
  query.clear();
  query+= "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME = \"" +queue->table +"\"";
  if (libdrizzle_failed(_libdrizzle_query(queue, query.c_str(), query.size())))
  {
    return gearmand_gerror(drizzle_error(queue->drizzle), GEARMAND_QUEUE_ERROR);
  }

  if (libdrizzle_failed(drizzle_result_buffer(queue->result())))
  {
    return gearmand_gerror(drizzle_error(queue->drizzle), GEARMAND_QUEUE_ERROR);
  }
  bool create_table= drizzle_result_row_count(queue->result());
  drizzle_result_free(queue->result());

  if (create_table == false)
  {
    gearmand_log_info("libdrizzle module creating table '%s.%s'",
                      drizzle_db(queue->drizzle), queue->table.c_str());

    query.clear();

    query+= "CREATE TABLE " +queue->schema + "." +queue->table + "( unique_key VARCHAR(" + TOSTRING(GEARMAN_UNIQUE_SIZE) + "),";
    query+= "function_name VARCHAR(255), priority INT, data LONGBLOB, when_to_run BIGINT, unique key (unique_key, function_name))";

    if (libdrizzle_failed(_libdrizzle_query(queue, query.c_str(), query.size())))
    {
      return gearmand_gerror(drizzle_error(queue->drizzle), GEARMAND_QUEUE_ERROR);
    }

    if (libdrizzle_failed(drizzle_column_skip_all(queue->result())))
    {
      drizzle_result_free(queue->result());
      return gearmand_gerror(drizzle_error(queue->drizzle), GEARMAND_QUEUE_ERROR);
    }
    drizzle_result_free(queue->result());
  }
  else
  {
    gearmand_log_info("libdrizzle module using table '%s.%s'",
                      drizzle_db(queue->drizzle), queue->table.c_str());

    query.clear();
    query+= "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = \"" +queue->schema +"\" ";
    query+= "AND TABLE_NAME = \"" +queue->table +"\" AND COLUMN_NAME = \"when_to_run\"";
    if (libdrizzle_failed(_libdrizzle_query(queue, query.c_str(), query.size())))
    {
      return gearmand_gerror(drizzle_error(queue->drizzle), GEARMAND_QUEUE_ERROR);
    }

    if (libdrizzle_failed(drizzle_result_buffer(queue->result())))
    {
      return gearmand_gerror(drizzle_error(queue->drizzle), GEARMAND_QUEUE_ERROR);
    }

    if (drizzle_result_row_count(queue->result()) == 0)
    {
      gearmand_info("Current schema does not have when_to_run column");
      queue->set_epoch_support(false);
    }
    drizzle_result_free(queue->result());
  }

  gearman_server_set_queue(*server, queue, _libdrizzle_add, _libdrizzle_flush, _libdrizzle_done, _libdrizzle_replay);

  return GEARMAND_SUCCESS;
}

/*
 * Static definitions
 */

static gearmand_error_t _libdrizzle_add(gearman_server_st *,
                                        void *context,
                                        const char *unique, size_t unique_size,
                                        const char *function_name, size_t function_name_size,
                                        const void *data, size_t data_size,
                                        gearman_job_priority_t priority,
                                        int64_t when)
{
  plugins::queue::Drizzle *queue= (plugins::queue::Drizzle *)context;

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "libdrizzle add: %.*s, %.*s(%u bytes)",
                     uint32_t(function_name_size), function_name, 
                     uint32_t(unique_size), (char *)unique, uint32_t(data_size));

  if (when and queue->epoch_support() == false)
  {
    return gearmand_gerror("Table lacks when_to_run field", GEARMAND_QUEUE_ERROR);
  }

  vchar_t escaped_unique_name;
  vchar_t escaped_function_name;

  (void)escape_string(escaped_unique_name, (const char *)unique, unique_size);
  (void)escape_string(escaped_function_name, (const char *)function_name, function_name_size);

  // @todo test for overallocation failure
  size_t query_size;

  char query_buffer[GEARMAND_QUEUE_QUERY_BUFFER +(GEARMAN_MAX_UNIQUE_SIZE*2) +(GEARMAN_FUNCTION_MAX_SIZE*2)];
  if (queue->epoch_support())
  {
    query_size= (size_t)snprintf(query_buffer, sizeof(query_buffer),
                                 "INSERT INTO %.*s.%.*s SET priority=%u,when_to_run=%lld,unique_key='%.*s',function_name='%.*s',data='",
                                 int(queue->schema.size()), queue->schema.c_str(),
                                 int(queue->table.size()), queue->table.c_str(),
                                 uint32_t(priority),
                                 (long long unsigned int)when,
                                 int(escaped_unique_name.size()), &escaped_unique_name[0],
                                 int(escaped_function_name.size()), &escaped_function_name[0]
                                );
  }
  else
  {
    query_size= (size_t)snprintf(query_buffer, sizeof(query_buffer),
                                 "INSERT INTO %.*s.%.*s SET priority=%u,unique_key='%.*s',function_name='%.*s',data='",
                                 int(queue->schema.size()), queue->schema.c_str(),
                                 int(queue->table.size()), queue->table.c_str(),
                                 uint32_t(priority),
                                 int(escaped_unique_name.size()), &escaped_unique_name[0],
                                 int(escaped_function_name.size()), &escaped_function_name[0]
                                );
  }

  vchar_t query;
  query.resize(query_size);
  memcpy(&query[0], query_buffer, query_size);

  query_size+= size_t(escape_string(query, (const char *)data, data_size));
  query.push_back('\'');
  query.resize(query.size() +1);

  gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "%.*s", query.size() -1, &query[0]);

  if (libdrizzle_failed(_libdrizzle_insert(queue, query)))
  {
    return gearmand_gerror(drizzle_error(queue->drizzle), GEARMAND_QUEUE_ERROR);
  }

  return GEARMAND_SUCCESS;
}

static gearmand_error_t _libdrizzle_flush(gearman_server_st *, void *)
{
  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "libdrizzle flush");

  return GEARMAND_SUCCESS;
}

static gearmand_error_t _libdrizzle_done(gearman_server_st *,
                                         void *context,
                                         const char *unique, size_t unique_size,
                                         const char *function_name, size_t function_name_size)
{
  plugins::queue::Drizzle *queue= (plugins::queue::Drizzle *)context;

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
		     "libdrizzle done: %.*s(%.*s)", 
                     uint32_t(function_name_size), (char *)function_name,
                     uint32_t(unique_size), (char *)unique);

  vchar_t escaped_unique_name;
  vchar_t escaped_function_name;

  (void)escape_string(escaped_unique_name, (const char *)unique, unique_size);
  (void)escape_string(escaped_function_name, (const char *)function_name, function_name_size);

  vchar_t query;
  query.resize(escaped_unique_name.size()
               +escaped_function_name.size() 
               +queue->schema.size()
               +queue->table.size()
               +GEARMAND_QUEUE_QUERY_BUFFER);

  int query_size= snprintf(&query[0], query.size(),
                           "DELETE FROM %.*s.%.*s WHERE unique_key='%.*s' and function_name= '%.*s'",
                           int(queue->schema.size()), queue->schema.c_str(),
                           int(queue->table.size()), queue->table.c_str(),
                           int(escaped_unique_name.size()), &escaped_unique_name[0],
                           int(escaped_function_name.size()), &escaped_function_name[0]
                          );

  if (query_size < 0 or size_t(query_size) > query.size())
  {
    return gearmand_gerror("snprintf(DELETE)", GEARMAND_MEMORY_ALLOCATION_FAILURE);
  }

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "%.*", query_size, &query[0]);
  
  if (libdrizzle_failed(_libdrizzle_query(queue, &query[0], query_size)))
  {
    return gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "Error occurred from %.*s", query_size, &query[0]);
  }

  drizzle_result_free(queue->result());

  return GEARMAND_SUCCESS;
}

#if 0
static gearmand_error_t _dump_queue(plugins::queue::Drizzle *queue)
{
  char query[DRIZZLE_MAX_TABLE_SIZE + GEARMAND_QUEUE_QUERY_BUFFER];

  size_t query_size;
  query_size= (size_t)snprintf(query, sizeof(query),
                               "SELECT unique_key,function_name FROM %s",
                               queue->table.c_str());

  if (_libdrizzle_query(NULL, queue, query, query_size) != DRIZZLE_RETURN_OK)
  {
    gearmand_log_error("drizzle_column_skip_all:%s", drizzle_error(queue->drizzle));
    return GEARMAND_QUEUE_ERROR;
  }

  if (drizzle_column_skip_all(queue->result()) != DRIZZLE_RETURN_OK)
  {
    drizzle_result_free(queue->result());
    gearmand_log_error("drizzle_column_skip_all:%s", drizzle_error(queue->drizzle));

    return GEARMAND_QUEUE_ERROR;
  }

  gearmand_debug("Shutting down with the following items left to be processed.");
  while (1)
  {
    drizzle_return_t ret;
    drizzle_row_t row= drizzle_row_buffer(queue->result(), &ret);

    if (ret != DRIZZLE_RETURN_OK)
    {
      drizzle_result_free(queue->result());
      gearmand_log_error("drizzle_row_buffer:%s", drizzle_error(queue->drizzle));

      return GEARMAND_QUEUE_ERROR;
    }

    if (row == NULL)
      break;

    size_t *field_sizes;
    field_sizes= drizzle_row_field_sizes(queue->result());

    gearmand_log_debug("\t unique: %.*s function: %.*s",
                       (uint32_t)field_sizes[0], row[0],
                       (uint32_t)field_sizes[1], row[1]
                       );

    drizzle_row_free(queue->result(), row);
  }

  drizzle_result_free(queue->result());

  return GEARMAND_SUCCESS;
}
#endif

static gearmand_error_t _libdrizzle_replay(gearman_server_st *server,
                                           void *context,
                                           gearman_queue_add_fn *add_fn,
                                           void *add_context)
{
  plugins::queue::Drizzle *queue= (plugins::queue::Drizzle *)context;
  size_t *field_sizes;
  gearmand_error_t gret;

  gearmand_info("libdrizzle replay start");

  std::string query;
  if (queue->epoch_support())
  {
    query+= "SELECT unique_key,function_name,priority,data,when_to_run FROM ";
    query+= queue->schema;
    query+= ".";
    query+= queue->table;
  }
  else
  {
    query+= "SELECT unique_key,function_name,priority,data FROM ";
    query+= queue->schema;
    query+= ".";
    query+= queue->table;
  }

  if (libdrizzle_failed(_libdrizzle_query(queue, query.c_str(), query.size())))
  {
    return gearmand_gerror(drizzle_error(queue->drizzle), GEARMAND_QUEUE_ERROR);
  }

  if (libdrizzle_failed(drizzle_column_buffer(queue->result())))
  {
    drizzle_result_free(queue->result());
    return gearmand_gerror(drizzle_error(queue->drizzle), GEARMAND_QUEUE_ERROR);
  }

  while (1)
  {
    drizzle_return_t ret;
    drizzle_row_t row= drizzle_row_buffer(queue->result(), &ret);

    if (libdrizzle_failed(ret))
    {
      drizzle_result_free(queue->result());
      return gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "drizzle_row_buffer:%s", drizzle_error(queue->drizzle));
    }

    if (row == NULL)
    {
      break;
    }

    field_sizes= drizzle_row_field_sizes(queue->result());

    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "libdrizzle replay: %.*s", (uint32_t)field_sizes[0], row[0]);

    size_t data_size= field_sizes[3];
    /* need to make a copy here ... gearman_server_job_free will free it later */
    char *data= (char *)malloc(data_size);
    if (data == NULL)
    {
      gearmand_perror(errno, "Failed to allocate data while replaying the queue");
      drizzle_row_free(queue->result(), row);
      drizzle_result_free(queue->result());
      return GEARMAND_MEMORY_ALLOCATION_FAILURE;
    }
    memcpy(data, row[3], data_size);

    int64_t when;
    if (not queue->epoch_support())
    {
      when= 0;
    }
    else
    {
      when= atoi(row[4]);
    }

    gret= (*add_fn)(server, add_context, row[0], field_sizes[0],
                    row[1], field_sizes[1],
                    data, data_size, (gearman_job_priority_t)atoi(row[2]), when);

    if (gret != GEARMAND_SUCCESS)
    {
      drizzle_row_free(queue->result(), row);
      drizzle_result_free(queue->result());
      return gret;
    }

    drizzle_row_free(queue->result(), row);
  }

  drizzle_result_free(queue->result());

  return GEARMAND_SUCCESS;
}
#pragma GCC diagnostic pop

#else // if defined(HAVE_LIBDRIZZLE) && HAVE_LIBDRIZZLE

namespace gearmand {
namespace plugins {
namespace queue {

void initialize_drizzle()
{
}

} // namespace queue
} // namespace plugins
} // namespace gearmand

#endif // if defined(HAVE_LIBDRIZZLE) && HAVE_LIBDRIZZLE
