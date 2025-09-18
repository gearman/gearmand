/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2012 Data Differential, http://datadifferential.com/
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

#include "libgearman-server/plugins/base.h"
#include "libgearman-server/plugins/queue/sqlite/instance.hpp"

#include <cerrno>

namespace gearmand {
namespace queue {

Instance::Instance(const std::string& schema_, const std::string& table_):
  _epoch_support(true),
  _check_replay(false),
  _in_trans(0),
  _db(NULL),
  delete_sth(NULL),
  insert_sth(NULL),
  replay_sth(NULL),
  _schema(schema_),
  _table(table_)
  { 
    _delete_query+= "DELETE FROM ";
    _delete_query+= _table;
    _delete_query+= " WHERE unique_key=? and function_name=?";

    if (_epoch_support)
    {
      _insert_query+= "INSERT OR REPLACE INTO ";
      _insert_query+= _table;
      _insert_query+= " (priority, unique_key, function_name, data, when_to_run) VALUES (?,?,?,?,?)";
    }
    else
    {
      _insert_query+= "INSERT OR REPLACE INTO ";
      _insert_query+= _table;
      _insert_query+= " (priority, unique_key, function_name, data) VALUES (?,?,?,?,?)";
    }
  }

Instance::~Instance()
{
  _sqlite3_finalize(delete_sth);
  delete_sth= NULL;

  _sqlite3_finalize(insert_sth);
  insert_sth= NULL;

  _sqlite3_finalize(replay_sth);
  replay_sth= NULL;

  assert(_db);
  if (_db)
  {
    if (sqlite3_close(_db) != SQLITE_OK)
    {
      gearmand_error(sqlite3_errmsg(_db));
    }
    _db= NULL;
    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "sqlite shutdown database");
  }

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "sqlite shutdown");
}

void Instance::_sqlite3_finalize(sqlite3_stmt* sth)
{
  if (sth)
  {
    if (sqlite3_finalize(sth) != SQLITE_OK )
    {
      gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "finalize error: %s", sqlite3_errmsg(_db));
    }
  }
}

bool Instance::_sqlite_prepare(const std::string& query, sqlite3_stmt ** sth)
{
  reset_error();
  if (query.size() > UINT32_MAX)
  {
    _error_string= "query size too big";
    return false;
  }

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "sqlite query: %s", query.c_str());
  if (sqlite3_prepare_v2(_db, query.c_str(), -1, sth, NULL) != SQLITE_OK)
  {
    _error_string= sqlite3_errmsg(_db);
    return false;
  }

  return true;
}

bool Instance::_sqlite_lock()
{
  /* already in transaction? */
  if (_in_trans == 0)
  {
    if (_sqlite_dispatch("BEGIN TRANSACTION") == false)
    {
      return false;
    }
  }
  _in_trans++;

  return true;
}

static int sql_count(void * rows_, int argc, char **, char **)
{
  int *rows= (int*)rows_;
  assert(argc == 1);
  (void)argc;
  assert(rows);
  *rows= *rows +1;

  return 0;
}

bool Instance::_sqlite_dispatch(const char* arg)
{
  int count;
  return _sqlite_count(arg, count);
}

bool Instance::_sqlite_count(const char* arg, int& count)
{
  reset_error();
  count= 0;

  char* error= NULL;
  int errcode= sqlite3_exec(_db, arg, sql_count, &count, &error);
  if (error != NULL or errcode != SQLITE_OK)
  {
    assert(errcode != SQLITE_OK);
    _error_string= error;
    sqlite3_free(error);

    return false;
  }

  return true;
}

bool Instance::_sqlite_count(const std::string& arg, int& count)
{
  return _sqlite_count(arg.c_str(), count);
}

bool Instance::_sqlite_dispatch(const std::string& arg)
{
  int count;
  return _sqlite_count(arg.c_str(), count);
}

bool Instance::_sqlite_commit()
{
  /* not in transaction? */
  if (_in_trans)
  {
    if (_sqlite_dispatch("COMMIT") == false)
    {
      return false;
    }
  }

  _in_trans= 0;

  return true;
}

gearmand_error_t Instance::init()
{
  gearmand_info("Initializing libsqlite3 module");

  if (_schema.empty())
  {
    return gearmand_gerror("missing required --libsqlite3-db=<dbfile> argument", GEARMAND_QUEUE_ERROR);
  }

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM, "sqlite open: %s", _schema.c_str());

  assert(_db == NULL);
  if (sqlite3_open_v2(_schema.c_str(), &_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK)
  {
    return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR, "sqlite3_open failed with: %s", sqlite3_errmsg(_db));
  }

  if (_db == NULL)
  {
    return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR, "Unknown error while opening up sqlite file");
  }

  // The only reason why we do this is because during testing we might read the
  // database which can cause a lock conflict.
  sqlite3_busy_timeout(_db, 6000);

  int rows;
  std::string check_table_str("SELECT 1 FROM sqlite_master WHERE type='table' AND name='");
  check_table_str+= _table;
  check_table_str+= "'";

  if (_sqlite_count(check_table_str, rows) == false)
  {
    return gearmand_gerror(_error_string.c_str(), GEARMAND_QUEUE_ERROR);
  }

  if (rows)
  {
    std::string query("SELECT when_to_run FROM ");
    query+= _table;
    sqlite3_stmt* select_sth= NULL;
    if (_sqlite_prepare(query, &select_sth) == false)
    {
      gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                          "Error from '%s': %s",
                          query.c_str(),
                          _error_string.c_str());
      query.clear();
      query+= "ALTER TABLE ";
      query+= _table;
      query+= " ADD COLUMN when_to_run INTEGER";
      if (_sqlite_dispatch(query) == false)
      {
        gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM,
                           "Error from '%s': %s",
                           query.c_str(),
                           _error_string.c_str());
        gearmand_info("No epoch support in sqlite queue");
        _epoch_support= false;
      }
    }
    _sqlite3_finalize(select_sth);
  }
  else
  {
    std::string query("CREATE TABLE ");
    query+= _table;
    query+= " ( unique_key TEXT, function_name TEXT, priority INTEGER, data BLOB, when_to_run INTEGER, PRIMARY KEY (unique_key, function_name))";

    gearmand_log_info(GEARMAN_DEFAULT_LOG_PARAM, "sqlite module creating table '%s'", _table.c_str());

    if (_sqlite_dispatch(query) == false)
    {
      return gearmand_gerror(_error_string.c_str(), GEARMAND_QUEUE_ERROR);
    }
  }

  if (_sqlite_prepare(_delete_query, &delete_sth) == false)
  {
    return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR,
                               "DELETE PREPARE error: %s",
                               _error_string.c_str());
  }

  if (_sqlite_prepare(_insert_query, &insert_sth) == false)
  {
    return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR,
                               "INSERT PREPARE: %s",  _error_string.c_str());
  }

  {
    std::string query;
    if (_epoch_support)
    {
      query+= "SELECT unique_key,function_name,priority,data,when_to_run FROM ";
    }
    else
    {
      query+= "SELECT unique_key,function_name,priority,data FROM ";
    }
    query+= _table;

    if (_sqlite_prepare(query, &replay_sth) == false)
    {
      return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR,
                                 "REPLAY PREPARE: %s", _error_string.c_str());
    }
  }


  return GEARMAND_SUCCESS;
}

bool Instance::_sqlite_rollback()
{
  /* not in transaction? */
  if (_in_trans)
  {
    if (_sqlite_dispatch("ROLLBACK") == false)
    {
      return false;
    }
  }

  _in_trans= 0;

  return true;
}

gearmand_error_t Instance::add(gearman_server_st*,
                                  const char *unique, size_t unique_size,
                                  const char *function_name,
                                  size_t function_name_size,
                                  const void *data, size_t data_size,
                                  gearman_job_priority_t priority,
                                  int64_t when)
{
  assert(_check_replay == false);
  if (when and _epoch_support == false)
  {
    return gearmand_gerror("Table lacks when_to_run field", GEARMAND_QUEUE_ERROR);
  }

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                     "sqlite add: priority: %d, unique_key: %.*s, function_name: %.*s when: %ld size: %u",
                     int(priority),
                     int(unique_size), (char*)unique,
                     int(function_name_size), (char*)function_name,
                     (long int)when,
                     uint32_t(data_size));

  if (_sqlite_lock() ==  false)
  {
    return gearmand_gerror(_error_string.c_str(), GEARMAND_QUEUE_ERROR);
  }

  if (sqlite3_reset(insert_sth) != SQLITE_OK)
  {
    return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR,
                               "failed to reset INSERT prep statement: %s", sqlite3_errmsg(_db));
  }

  if (sqlite3_bind_int(insert_sth,  1, priority) != SQLITE_OK)
  {
    return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR,
                               "failed to bind priority [%d]: %s", priority, sqlite3_errmsg(_db));
  }

  if (sqlite3_bind_text(insert_sth, 2, (const char *)unique, (int)unique_size, SQLITE_TRANSIENT) != SQLITE_OK)
  {
    return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR,
                               "failed to bind unique [%.*s]: %s", (uint32_t)unique_size, (char*)unique, sqlite3_errmsg(_db));
  }

  if (sqlite3_bind_text(insert_sth, 3, (const char *)function_name, (int)function_name_size, SQLITE_TRANSIENT) != SQLITE_OK)
  {
    return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR,
                               "failed to bind function [%.*s]: %s", (uint32_t)function_name_size, (char*)function_name, sqlite3_errmsg(_db));
  }

  if (sqlite3_bind_blob(insert_sth, 4, data, (int)data_size, SQLITE_TRANSIENT) != SQLITE_OK)
  {
    return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR, 
                               "failed to bind data blob: %s", sqlite3_errmsg(_db));
  }

  // epoch data
  if (sqlite3_bind_int64(insert_sth,  5, when) != SQLITE_OK)
  {
    return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR,
                               "failed to bind epoch int64_t(%ld): %s", (long int)when, sqlite3_errmsg(_db));
  }

  // INSERT happens here
  if (sqlite3_step(insert_sth) != SQLITE_DONE)
  {
    return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR,
                               "INSERT error: %s", sqlite3_errmsg(_db));
  }

  return GEARMAND_SUCCESS;
}

gearmand_error_t Instance::flush(gearman_server_st*)
{
  gearmand_debug("sqlite flush");

  if (_sqlite_commit() == false)
  {
    return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR,
                               "COMMIT called on FLUSH error: %s",
                               _error_string.c_str());
  }

  return GEARMAND_SUCCESS;
}

gearmand_error_t Instance::done(gearman_server_st*,
                                   const char *unique,
                                   size_t unique_size,
                                   const char *function_name,
                                   size_t function_name_size)
{
  if (is_store_on_shutdown())
  {
    return GEARMAND_SUCCESS;
  }

  gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                     "sqlite done: unique_key: %.*s, function_name: %.*s",
                     int(unique_size), (char*)unique,
                     int(function_name_size), (char*)function_name);

  if (_sqlite_lock() == false)
  {
    return gearmand_gerror(_error_string.c_str(), GEARMAND_QUEUE_ERROR);
  }

  if (sqlite3_reset(delete_sth) != SQLITE_OK)
  {
    return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR,
                               "failed to reset DELETE prep statement: %s", sqlite3_errmsg(_db));
  }

  if (sqlite3_bind_text(delete_sth, 1, (const char *)unique, int(unique_size), SQLITE_TRANSIENT) != SQLITE_OK)
  {
    return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR,
                               "failed to bind unique [%.*s]: %s", uint32_t(unique_size), (char*)unique, sqlite3_errmsg(_db));
  }

  if (sqlite3_bind_text(delete_sth, 2, (const char *)function_name, int(function_name_size), SQLITE_TRANSIENT) != SQLITE_OK)
  {
    return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR,
                               "failed to bind function [%.*s]: %s", uint32_t(function_name_size), (char*)function_name, sqlite3_errmsg(_db));
  }

  // DELETE happens here
  if (sqlite3_step(delete_sth) != SQLITE_DONE)
  {
    return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR,
                               "DELETE error: %s",
                               sqlite3_errmsg(_db));
  }

  if (_sqlite_commit() == false)
  {
    return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR, "DELETE error: %s", _error_string.c_str());
  }

  return GEARMAND_SUCCESS;
}

gearmand_error_t Instance::replay(gearman_server_st *server)
{
  gearmand_error_t ret;
  _check_replay= true;

  if (gearmand_failed(ret= replay_loop(server)))
  {
    if (_sqlite_rollback() == false)
    {
      gearmand_log_error(GEARMAN_DEFAULT_LOG_PARAM, "failed to rollback sqlite from failed replay  error: %s", _error_string.c_str());
    }
  }
  _check_replay= false;

  return ret;
}

gearmand_error_t Instance::replay_loop(gearman_server_st *server)
{
  gearmand_info("sqlite replay start");

  gearmand_error_t gret= GEARMAND_UNKNOWN_STATE;
  size_t row_count= 0;
  while (sqlite3_step(replay_sth) == SQLITE_ROW)
  {
    const char *unique, *function_name;
    size_t unique_size, function_name_size;

    row_count++;

    if (sqlite3_column_type(replay_sth, 0) == SQLITE_TEXT)
    {
      unique= (char *)sqlite3_column_text(replay_sth, 0);
      unique_size= size_t(sqlite3_column_bytes(replay_sth, 0));
    }
    else
    {
      return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR, "column %d is not type TEXT: %d", 0, int(sqlite3_column_type(replay_sth, 0)));
    }

    if (sqlite3_column_type(replay_sth, 1) == SQLITE_TEXT)
    {
      function_name= (char *)sqlite3_column_text(replay_sth, 1);
      function_name_size= size_t(sqlite3_column_bytes(replay_sth, 1));
    }
    else
    {
      return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR,
                                 "column %d is not type TEXT", 1);
    }

    gearman_job_priority_t priority;
    if (sqlite3_column_type(replay_sth, 2) == SQLITE_INTEGER)
    {
      priority= (gearman_job_priority_t)sqlite3_column_int64(replay_sth, 2);
    }
    else
    {
      return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR,
                                 "column %d is not type INTEGER", 2);
    }

    if (sqlite3_column_type(replay_sth, 3) != SQLITE_BLOB)
    {
      return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR, "column %d is not type TEXT", 3);
    }

    size_t data_size= (size_t)sqlite3_column_bytes(replay_sth, 3);
    char* data= (char*)malloc(data_size);
    /* need to make a copy here ... gearman_server_job_free will free it later */
    if (data == NULL)
    {
      return gearmand_perror(errno, "malloc");
    }
    memcpy(data, sqlite3_column_blob(replay_sth, 3), data_size);
    
    int64_t when;
    if (_epoch_support)
    {
      if (sqlite3_column_type(replay_sth, 4) == SQLITE_INTEGER)
      {
        when= int64_t(sqlite3_column_int64(replay_sth, 4));
      }
      else
      {
        return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR, "column %d is not type INTEGER", 3);
      }
    }
    else
    {
      when= 0;
    }

    gearmand_log_debug(GEARMAN_DEFAULT_LOG_PARAM,
                       "sqlite replay: unique_key: %.*s, function_name: %.*s",
                       int(unique_size), (char*)unique,
                       int(function_name_size), (char*)function_name);

    gret= Instance::replay_add(server,
                               NULL,
                               unique, unique_size,
                               function_name, function_name_size,
                               data, data_size,
                               priority, when);

    if (gearmand_failed(gret))
    {
      break;
    }
  }

  if (sqlite3_reset(replay_sth) != SQLITE_OK)
  {
    return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR,
                               "failed to reset REPLAY prep statement: %s", sqlite3_errmsg(_db));
  }

  /* truncate database now if in store_on_shutdown mode */
  if (is_store_on_shutdown())
  {
    std::string query("DELETE FROM ");
    query+= _table;
    if (_sqlite_dispatch(query) == false) {
      return gearmand_log_gerror(GEARMAN_DEFAULT_LOG_PARAM, GEARMAND_QUEUE_ERROR,
                                 "failed to truncate table: %s", sqlite3_errmsg(_db));
    }
  }

  if (row_count == 0)
  {
    return GEARMAND_SUCCESS;
  }

  return gret;
}

} // namespace queue
} // namespace gearmand
