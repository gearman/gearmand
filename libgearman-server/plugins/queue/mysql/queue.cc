/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011-2012 Data Differential, http://datadifferential.com/
 *  Copyright (C) 2011 Oleksiy Krivoshey
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
#include <libgearman-server/byte.h>

#include <libgearman-server/plugins/queue/mysql/queue.h>
#include <libgearman-server/plugins/queue/base.h>

#include <mysql.h>
#if !defined(MARIADB_BASE_VERSION) && !defined(MARIADB_VERSION_ID) && \
  MYSQL_VERSION_ID >= 80001 && MYSQL_VERSION_ID != 80002
typedef bool my_bool;
#endif

#include <errmsg.h>
#include <cerrno>

/**
 * Default values.
 */
#define GEARMAND_QUEUE_MYSQL_DEFAULT_TABLE "gearman_queue"

namespace gearmand { namespace plugins { namespace queue { class MySQL; } } }

static gearmand_error_t _initialize(gearman_server_st& server, gearmand::plugins::queue::MySQL *queue);

namespace gearmand 
{
namespace plugins
{
namespace queue 
{

class MySQL : public gearmand::plugins::Queue {
public:
  MySQL();
  ~MySQL();

  gearmand_error_t initialize();
  gearmand_error_t prepareAddStatement();
  gearmand_error_t prepareDoneStatement();

  MYSQL *con;
  MYSQL_STMT *add_stmt;
  MYSQL_STMT *done_stmt;
  std::string mysql_host;
  std::string mysql_user;
  std::string mysql_password;
  std::string mysql_db;
  std::string mysql_table;

  in_port_t port() const
  {
    return _port;
  }

private:
  in_port_t _port;
};

MySQL::MySQL() :
  Queue("MySQL"),
  con(NULL),
  add_stmt(NULL),
  done_stmt(NULL)
  {
    command_line_options().add_options()
      ("mysql-host", boost::program_options::value(&mysql_host)->default_value("localhost"), "MySQL host.")
      ("mysql-port", boost::program_options::value(&_port)->default_value(3306), "Port of server. (by default 3306)")
      ("mysql-user", boost::program_options::value(&mysql_user)->default_value(""), "MySQL user.")
      ("mysql-password", boost::program_options::value(&mysql_password)->default_value(""), "MySQL user password.")
      ("mysql-db", boost::program_options::value(&mysql_db)->default_value(""), "MySQL database.")
      ("mysql-table", boost::program_options::value(&mysql_table)->default_value(GEARMAND_QUEUE_MYSQL_DEFAULT_TABLE), "MySQL table name.");
  }

MySQL::~MySQL()
{
  if (add_stmt)
  {
    mysql_stmt_close(add_stmt);
  }
  if (con)
  {
    mysql_close(con);
  }
}

gearmand_error_t MySQL::initialize()
{
  return _initialize(Gearmand()->server, this);
}

gearmand_error_t MySQL::prepareAddStatement()
{
  char query_buffer[1024];

  if ((this->add_stmt= mysql_stmt_init(this->con)) == NULL)
  {
    gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "mysql_stmt_init failed: %s", mysql_error(this->con));
    return GEARMAND_QUEUE_ERROR;
  }

  int query_buffer_length= snprintf(query_buffer, sizeof(query_buffer),
                                    "INSERT INTO %s "
                                    "(unique_key, function_name, priority, data, when_to_run) "
                                    "VALUES(?, ?, ?, ?, ?)", this->mysql_table.c_str());

  if (mysql_stmt_prepare(this->add_stmt, query_buffer, query_buffer_length))
  {
    gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "mysql_stmt_prepare failed: %s", mysql_error(this->con));
    return GEARMAND_QUEUE_ERROR;
  }

  return GEARMAND_SUCCESS;
}

gearmand_error_t MySQL::prepareDoneStatement()
{
  char query_buffer[1024];

  if ((this->done_stmt= mysql_stmt_init(this->con)) == NULL)
  {
    gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "mysql_stmt_init failed: %s", mysql_error(this->con));
    return GEARMAND_QUEUE_ERROR;
  }

  int query_buffer_length= snprintf(query_buffer, sizeof(query_buffer),
                                    "DELETE FROM %s "
                                    "WHERE unique_key=? "
                                    "AND function_name=?", this->mysql_table.c_str());

  if (mysql_stmt_prepare(this->done_stmt, query_buffer, query_buffer_length))
  {
    gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "mysql_stmt_prepare failed: %s", mysql_error(this->con));
    return GEARMAND_QUEUE_ERROR;
  }

  return GEARMAND_SUCCESS;
}

void initialize_mysql()
{
  static MySQL local_instance;
}

} // namespace queue
} // namespace plugin
} // namespace gearmand

/* Queue callback functions. */
static gearmand_error_t _mysql_queue_add(gearman_server_st *server, void *context,
        const char *unique, size_t unique_size,
        const char *function_name,
        size_t function_name_size,
        const void *data, size_t data_size,
        gearman_job_priority_t priority,
        int64_t when);

static gearmand_error_t _mysql_queue_flush(gearman_server_st *server, void *context);

static gearmand_error_t _mysql_queue_done(gearman_server_st *server, void *context,
        const char *unique,
        size_t unique_size,
        const char *function_name,
        size_t function_name_size);

static gearmand_error_t _mysql_queue_replay(gearman_server_st *server, void *context,
        gearman_queue_add_fn *add_fn,
        void *add_context);


gearmand_error_t _initialize(gearman_server_st& server, gearmand::plugins::queue::MySQL *queue)
{

  MYSQL_RES * result;
  my_bool  my_true= true;

  gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM,"Initializing MySQL module");

  gearman_server_set_queue(server, queue, _mysql_queue_add, _mysql_queue_flush, _mysql_queue_done, _mysql_queue_replay);

  queue->con= mysql_init(queue->con);

  mysql_options(queue->con, MYSQL_READ_DEFAULT_GROUP, "gearmand");
  mysql_options(queue->con, MYSQL_OPT_RECONNECT, &my_true);

  if (!mysql_real_connect(queue->con,
                          queue->mysql_host.c_str(),
                          queue->mysql_user.c_str(),
                          queue->mysql_password.c_str(),
                          queue->mysql_db.c_str(), 
                          queue->port(), NULL, 0))
  {
    gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "Failed to connect to database: %s", mysql_error(queue->con));

    return GEARMAND_QUEUE_ERROR;
  }

  if (!(result= mysql_list_tables(queue->con, queue->mysql_table.c_str())))
  {
    gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "mysql_list_tables failed: %s", mysql_error(queue->con));
    return GEARMAND_QUEUE_ERROR;
  }

  if (mysql_num_rows(result) == 0)
  {
    char query_buffer[1024];
    int query_buffer_length= snprintf(query_buffer, sizeof(query_buffer),
                                      "CREATE TABLE %s"
                                      "("
                                      "unique_key VARCHAR(%d),"
                                      "function_name VARCHAR(255),"
                                      "priority INT,"
                                      "data LONGBLOB,"
                                      "when_to_run INT,"
                                      "unique key (unique_key, function_name)"
                                      ")",
                                      queue->mysql_table.c_str(), GEARMAN_UNIQUE_SIZE);

    gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM,"MySQL module: creating table %s", queue->mysql_table.c_str());

    if (mysql_real_query(queue->con, query_buffer, query_buffer_length))
    {
      gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "MySQL module: create table failed: %s", mysql_error(queue->con));
      return GEARMAND_QUEUE_ERROR;
    }
  }

  mysql_free_result(result);

  if (queue->prepareAddStatement() == GEARMAND_QUEUE_ERROR)
  {
    return GEARMAND_QUEUE_ERROR;
  }

  if (queue->prepareDoneStatement() == GEARMAND_QUEUE_ERROR)
  {
    return GEARMAND_QUEUE_ERROR;
  }

  return GEARMAND_SUCCESS;
}

/*
 * Static definitions
 */


static gearmand_error_t _mysql_queue_add(gearman_server_st *, void *context,
        const char *unique, size_t unique_size,
        const char *function_name,
        size_t function_name_size,
        const void *data, size_t data_size,
        gearman_job_priority_t priority,
        int64_t when)
{
  MYSQL_BIND    bind[5];

  gearmand::plugins::queue::MySQL *queue= (gearmand::plugins::queue::MySQL *)context;

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,"MySQL queue add: %.*s %.*s", (uint32_t) unique_size, (char *) unique,
                     (uint32_t) function_name_size, (char *) function_name);

  mysql_ping(queue->con);

  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *)unique;
  bind[0].buffer_length= unique_size;
  bind[0].is_null= 0;
  bind[0].length= (unsigned long*)&unique_size;

  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= (char *)function_name;
  bind[1].buffer_length= function_name_size;
  bind[1].is_null= 0;
  bind[1].length= (unsigned long*)&function_name_size;

  bind[2].buffer_type= MYSQL_TYPE_LONG;
  bind[2].buffer= (char *)&priority;
  bind[2].is_null= 0;
  bind[2].length= 0;

  bind[3].buffer_type= MYSQL_TYPE_LONG_BLOB;
  bind[3].buffer= (char *)data;
  bind[3].buffer_length= data_size;
  bind[3].is_null= 0;
  bind[3].length= (unsigned long*)&data_size;

  bind[4].buffer_type= MYSQL_TYPE_LONG;
  bind[4].buffer= (char *)&when;
  bind[4].is_null= 0;
  bind[4].length= 0;

  while(1)
  {
    if (mysql_stmt_bind_param(queue->add_stmt, bind))
    {
      if ( mysql_stmt_errno(queue->add_stmt) == CR_NO_PREPARE_STMT )
      {
        if (queue->prepareAddStatement() == GEARMAND_QUEUE_ERROR)
        {
          return GEARMAND_QUEUE_ERROR;
        }
        continue;
      }
      else
      {
        gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "mysql_stmt_bind_param failed: %s", mysql_error(queue->con));
        return GEARMAND_QUEUE_ERROR;
      }
    }

    if (mysql_stmt_execute(queue->add_stmt))
    {
      if ( mysql_stmt_errno(queue->add_stmt) == CR_SERVER_LOST )
      {
        mysql_stmt_close(queue->add_stmt);
        if (queue->prepareAddStatement() != GEARMAND_QUEUE_ERROR)
        {
          continue;
        }
      }
      gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "mysql_stmt_execute failed: %s", mysql_error(queue->con));
      return GEARMAND_QUEUE_ERROR;
    }

    break;
  }

  return GEARMAND_SUCCESS;
}

static gearmand_error_t _mysql_queue_flush(gearman_server_st*, void*)
{

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,"MySQL queue flush");

  return GEARMAND_SUCCESS;
}

static gearmand_error_t _mysql_queue_done(gearman_server_st*, void *context,
                                          const char *unique,
                                          size_t unique_size,
                                          const char *function_name,
                                          size_t function_name_size)
{

  MYSQL_BIND    bind[2];

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,"MySQL queue done: %.*s %.*s", (uint32_t) unique_size, (char *) unique,
                     (uint32_t) function_name_size, (char *) function_name);

  gearmand::plugins::queue::MySQL *queue= (gearmand::plugins::queue::MySQL *)context;

  mysql_ping(queue->con);

  bind[0].buffer_type= MYSQL_TYPE_STRING;
  bind[0].buffer= (char *)unique;
  bind[0].buffer_length= unique_size;
  bind[0].is_null= 0;
  bind[0].length= (unsigned long*)&unique_size;

  bind[1].buffer_type= MYSQL_TYPE_STRING;
  bind[1].buffer= (char *)function_name;
  bind[1].buffer_length= function_name_size;
  bind[1].is_null= 0;
  bind[1].length= (unsigned long*)&function_name_size;

  while(1)
  {
    if (mysql_stmt_bind_param(queue->done_stmt, bind))
    {
      if ( mysql_stmt_errno(queue->done_stmt) == CR_NO_PREPARE_STMT )
      {
        if (queue->prepareDoneStatement() == GEARMAND_QUEUE_ERROR)
        {
          return GEARMAND_QUEUE_ERROR;
        }
        continue;
      }
      else
      {
        gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "mysql_stmt_bind_param failed: %s", mysql_error(queue->con));
        return GEARMAND_QUEUE_ERROR;
      }
    }

    if (mysql_stmt_execute(queue->done_stmt))
    {
      if ( mysql_stmt_errno(queue->done_stmt) == CR_SERVER_LOST )
      {
        mysql_stmt_close(queue->done_stmt);
        if (queue->prepareDoneStatement() != GEARMAND_QUEUE_ERROR)
        {
          continue;
        }
      }
      gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "mysql_stmt_execute failed: %s", mysql_error(queue->con));

      return GEARMAND_QUEUE_ERROR;
    }

    break;
  }

  return GEARMAND_SUCCESS;
}

static gearmand_error_t _mysql_queue_replay(gearman_server_st* server, void *context,
                                            gearman_queue_add_fn *add_fn,
                                            void *add_context) 
{

  MYSQL_RES * result;
  MYSQL_ROW row;
  char query_buffer[1024];

  gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM,"MySQL queue replay");

  gearmand::plugins::queue::MySQL *queue= (gearmand::plugins::queue::MySQL *)context;

  int query_buffer_length= snprintf(query_buffer, sizeof(query_buffer),
                                    "SELECT unique_key, function_name, data, priority, when_to_run FROM %s",
                                    queue->mysql_table.c_str());

  mysql_ping(queue->con);

  if (mysql_real_query(queue->con, query_buffer, query_buffer_length))
  {
    gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "mysql_real_query failed: %s", mysql_error(queue->con));
    return GEARMAND_QUEUE_ERROR;
  }

  if (!(result= mysql_store_result(queue->con)))
  {
    gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "mysql_store_result failed: %s", mysql_error(queue->con));
    return GEARMAND_QUEUE_ERROR;
  }

  if (mysql_num_fields(result) < 5)
  {
    gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "MySQL queue: insufficient row fields in queue table");
    return GEARMAND_QUEUE_ERROR;
  }

  gearmand_error_t ret= GEARMAND_SUCCESS;

  while ((row= mysql_fetch_row(result))) 
  {
    unsigned long *lengths;
    gearman_job_priority_t priority= (gearman_job_priority_t)0;
    int when= 0;

    lengths= mysql_fetch_lengths(result);

    /* need to make a copy here ... gearman_server_job_free will free it later */
    size_t data_size= lengths[2];
    char * data= (char *)malloc(data_size);
    if (data == NULL)
    {
      return gearmand_perror(errno, "malloc failed");
    }
    memcpy(data, row[2], data_size);

    if (lengths[3])
    {
      priority= (gearman_job_priority_t) atoi(row[3]);
    }

    if (lengths[4])
    {
      when= atoi(row[4]);
    }

    ret= (*add_fn)(server, add_context,
                   row[0], (size_t) lengths[0],
                   row[1], (size_t) lengths[1],
                   data, data_size,
                   priority,
                   when);

    if (ret != GEARMAND_SUCCESS)
    {
      break;
    }
  }

  mysql_free_result(result);

  return ret;
}
