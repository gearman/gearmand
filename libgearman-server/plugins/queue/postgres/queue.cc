/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011 Data Differential, http://datadifferential.com/
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
 * @brief libpq Queue Storage Definitions
 */

#include <gear_config.h>
#include <libgearman-server/common.h>
#include <libgearman-server/byte.h>

#include <libgearman-server/plugins/queue/postgres/queue.h>
#include <libgearman-server/plugins/queue/base.h>

#pragma GCC diagnostic push
#if defined(HAVE_LIBPQ) and HAVE_LIBPQ
# pragma GCC diagnostic ignored "-Wundef"
# include <libpq-fe.h>
#endif

#include <cerrno>

/**
 * @addtogroup plugins::queue::Postgresatic Static libpq Queue Storage Definitions
 * @ingroup gearman_queue_libpq
 * @{
 */

/**
 * Default values.
 */
#define GEARMAND_QUEUE_LIBPQ_DEFAULT_TABLE "queue"
#define GEARMAND_QUEUE_QUERY_BUFFER 256

namespace gearmand { namespace plugins { namespace  queue { class Postgres; }}}

static gearmand_error_t _initialize(gearman_server_st& server, gearmand::plugins::queue::Postgres *queue);

namespace gearmand {
namespace plugins {
namespace queue {

class Postgres : public gearmand::plugins::Queue {
public:
  Postgres();
  ~Postgres();

  gearmand_error_t initialize();

  const std::string &insert()
  {
    return _insert_query;
  }

  const std::string &select()
  {
    return _select_query;
  }

  const std::string &create()
  {
    return _create_query;
  }

  PGconn *con;
  std::string postgres_connect_string;
  std::string table;
  std::vector<char> query_buffer;

public:
  std::string _insert_query;
  std::string _select_query;
  std::string _create_query;
};

Postgres::Postgres() :
  Queue("Postgres"),
  con(NULL),
  postgres_connect_string(""),
  table(""),
  query_buffer()
{
  command_line_options().add_options()
    ("libpq-conninfo", boost::program_options::value(&postgres_connect_string)->default_value(""), "PostgreSQL connection information string.")
    ("libpq-table", boost::program_options::value(&table)->default_value(GEARMAND_QUEUE_LIBPQ_DEFAULT_TABLE), "Table to use.");
}

Postgres::~Postgres ()
{
  if (con)
    PQfinish(con);
}

gearmand_error_t Postgres::initialize()
{
  _create_query+= "CREATE TABLE IF NOT EXISTS " +table +" (unique_key VARCHAR" +"(" + TOSTRING(GEARMAN_UNIQUE_SIZE) +"), ";
  _create_query+= "function_name VARCHAR(255), priority INTEGER, data BYTEA, when_to_run INTEGER, UNIQUE (unique_key, function_name))";

  gearmand_error_t ret= _initialize(Gearmand()->server, this);

  _insert_query+= "INSERT INTO " +table +" (priority, unique_key, function_name, data, when_to_run) VALUES($1,$2,$3,$4::BYTEA,$5)";

  _select_query+= "SELECT unique_key,function_name,priority,data,when_to_run FROM " +table;

  return ret;
}

void initialize_postgres()
{
  static Postgres local_instance;
}

} // namespace queue
} // namespace plugins
} // namespace gearmand

/**
 * PostgreSQL notification callback.
 */
static void _libpq_notice_processor(void *arg, const char *message);

/* Queue callback functions. */
static gearmand_error_t _libpq_add(gearman_server_st *server, void *context,
                                   const char *unique, size_t unique_size,
                                   const char *function_name,
                                   size_t function_name_size,
                                   const void *data, size_t data_size,
                                   gearman_job_priority_t priority,
                                   int64_t when);

static gearmand_error_t _libpq_flush(gearman_server_st *server, void *context);

static gearmand_error_t _libpq_done(gearman_server_st *server, void *context,
                                    const char *unique,
                                    size_t unique_size,
                                    const char *function_name,
                                    size_t function_name_size);

static gearmand_error_t _libpq_replay(gearman_server_st *server, void *context,
                                      gearman_queue_add_fn *add_fn,
                                      void *add_context);

/** @} */

/*
 * Public definitions
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"

gearmand_error_t _initialize(gearman_server_st& server,
                             gearmand::plugins::queue::Postgres *queue)
{
  gearmand_info("Initializing libpq module");

  gearman_server_set_queue(server, queue, _libpq_add, _libpq_flush, _libpq_done, _libpq_replay);

  queue->con= PQconnectdb(queue->postgres_connect_string.c_str());

  if (queue->con == NULL || PQstatus(queue->con) != CONNECTION_OK)
  {
    gearman_server_set_queue(server, NULL, NULL, NULL, NULL, NULL);
    return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR, "PQconnectdb: %s", PQerrorMessage(queue->con));
  }

  (void)PQsetNoticeProcessor(queue->con, _libpq_notice_processor, &server);

  std::string query("SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME ='" +queue->table + "'");

  PGresult* result= PQexec(queue->con, query.c_str());
  if (result == NULL || PQresultStatus(result) != PGRES_TUPLES_OK)
  {
    std::string error_string= "PQexec:";
    error_string+= PQerrorMessage(queue->con);
    gearmand_gerror(error_string.c_str(), GEARMAND_QUEUE_ERROR);
    PQclear(result);
    return GEARMAND_QUEUE_ERROR;
  }

  if (PQntuples(result) == 0)
  {
    PQclear(result);

    gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "libpq module creating table '%s'", queue->table.c_str());

    result= PQexec(queue->con, queue->create().c_str());
    if (result == NULL || PQresultStatus(result) != PGRES_COMMAND_OK)
    {
      gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM,
                         "PQexec:%s", PQerrorMessage(queue->con));
      PQclear(result);
      gearman_server_set_queue(server, NULL, NULL, NULL, NULL, NULL);
      return GEARMAND_QUEUE_ERROR;
    }
  }

  PQclear(result);

  return GEARMAND_SUCCESS;
}

/*
 * Static definitions
 */

static void _libpq_notice_processor(void *arg, const char *message)
{
  (void)arg;
  gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "PostgreSQL %s", message);
}

static gearmand_error_t _libpq_add(gearman_server_st*, void *context,
                                   const char *unique, size_t unique_size,
                                   const char *function_name,
                                   size_t function_name_size,
                                   const void *data, size_t data_size,
                                   gearman_job_priority_t priority,
                                   int64_t when)
{
  gearmand::plugins::queue::Postgres *queue= (gearmand::plugins::queue::Postgres *)context;

  char priority_buffer[GEARMAN_MAXIMUM_INTEGER_DISPLAY_LENGTH +1];
  int priority_buffer_length= snprintf(priority_buffer, sizeof(priority_buffer), "%u", static_cast<uint32_t>(priority));
  char when_buffer[GEARMAN_MAXIMUM_INTEGER_DISPLAY_LENGTH +1];
  int when_buffer_length= snprintf(when_buffer, sizeof(when_buffer), "%" PRId64, when);

  const char *param_values[]= {
    (char *)priority_buffer,
    (char *)unique,
    (char *)function_name,
    (char *)data,
    (char *)when_buffer };

  int param_lengths[]= {
    (int)priority_buffer_length,
    (int)unique_size,
    (int)function_name_size,
    (int)data_size,
    (int)when_buffer_length };

  int param_formats[] = { 0, 0, 0, 1, 0 };

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "libpq add: %.*s", (uint32_t)unique_size, (char *)unique);

  PGresult *result= PQexecParams(queue->con, queue->insert().c_str(),
                                 gearmand_array_size(param_lengths),
                                 NULL, param_values, param_lengths, param_formats, 0);
  if (result == NULL || PQresultStatus(result) != PGRES_COMMAND_OK)
  {
    gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "PQexec:%s", PQerrorMessage(queue->con));
    PQclear(result);
    return GEARMAND_QUEUE_ERROR;
  }

  PQclear(result);

  return GEARMAND_SUCCESS;
}

static gearmand_error_t _libpq_flush(gearman_server_st *, void *)
{
  gearmand_debug("libpq flush");

  return GEARMAND_SUCCESS;
}

static gearmand_error_t _libpq_done(gearman_server_st*, void *context,
                                    const char *unique,
                                    size_t unique_size,
                                    const char *function_name,
                                    size_t function_name_size)
{
  (void)function_name_size;
  gearmand::plugins::queue::Postgres *queue= (gearmand::plugins::queue::Postgres *)context;
  PGresult *result;

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "libpq done: %.*s", (uint32_t)unique_size, (char *)unique);

  std::string query;
  query.reserve(function_name_size +unique_size + 80);
  query+= "DELETE FROM ";
  query+= queue->table;
  query+= " WHERE unique_key='";
  query+= (const char *)unique;
  query+= "' AND function_name='";
  query+= (const char *)function_name;
  query+= "'";

  result= PQexec(queue->con, query.c_str());
  if (result == NULL || PQresultStatus(result) != PGRES_COMMAND_OK)
  {
    gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "PQexec:%s", PQerrorMessage(queue->con));
    PQclear(result);
    return GEARMAND_QUEUE_ERROR;
  }

  PQclear(result);

  return GEARMAND_SUCCESS;
}

static gearmand_error_t _libpq_replay(gearman_server_st *server, void *context,
                                      gearman_queue_add_fn *add_fn,
                                      void *add_context)
{
  gearmand::plugins::queue::Postgres *queue= (gearmand::plugins::queue::Postgres *)context;

  gearmand_info("libpq replay start");

  std::string query("SELECT unique_key,function_name,priority,data,when_to_run FROM " + queue->table);

  PGresult *result= PQexecParams(queue->con, query.c_str(), 0, NULL, NULL, NULL, NULL, 1);
  if (result == NULL || PQresultStatus(result) != PGRES_TUPLES_OK)
  {
    gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "PQexecParams:%s", PQerrorMessage(queue->con));
    PQclear(result);
    return GEARMAND_QUEUE_ERROR;
  }

  for (int row= 0; row < PQntuples(result); row++)
  {
    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                       "libpq replay: %.*s",
                       PQgetlength(result, row, 0),
                       PQgetvalue(result, row, 0));

    size_t data_length;
    char *data;
    if (PQgetlength(result, row, 3) == 0)
    {
      data= NULL;
      data_length= 0;
    }
    else
    {
      data_length= size_t(PQgetlength(result, row, 3));
      data= (char *)malloc(data_length);
      if (data == NULL)
      {
        PQclear(result);
        return gearmand_perror(errno, "malloc");
      }

      memcpy(data, PQgetvalue(result, row, 3), data_length);
    }

    gearmand_error_t ret;
    ret= (*add_fn)(server, add_context, PQgetvalue(result, row, 0),
                   (size_t)PQgetlength(result, row, 0),
                   PQgetvalue(result, row, 1),
                   (size_t)PQgetlength(result, row, 1),
                   data, data_length,
                   (gearman_job_priority_t)atoi(PQgetvalue(result, row, 2)),
                   atoll(PQgetvalue(result, row, 4)));
    if (gearmand_failed(ret))
    {
      PQclear(result);
      return ret;
    }
  }

  PQclear(result);

  return GEARMAND_SUCCESS;
}
#pragma GCC diagnostic pop
#pragma GCC diagnostic pop
