/*  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 * 
 *  Gearmand client and server library.
 *
 *  Copyright (C) 2011-2012 Data Differential, http://datadifferential.com/
 *  Copyright (C) 2009 Cory Bennett
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
 * @brief libsqlite3 Queue Storage Definitions
 */

#include <gear_config.h>
#include <libgearman-server/common.h>

#include <libgearman-server/plugins/queue/sqlite/queue.h>
#include <libgearman-server/plugins/queue/base.h>

#include "libgearman-server/plugins/queue/sqlite/instance.hpp"

/** Default values.
 */
#define GEARMAND_QUEUE_SQLITE_DEFAULT_TABLE "gearman_queue"

namespace gearmand {
namespace plugins {
namespace queue {

class Sqlite : public gearmand::plugins::Queue
{
public:
  Sqlite();
  ~Sqlite();

  gearmand_error_t initialize();

  std::string schema;
  std::string table;

private:
  bool _store_on_shutdown;
};

Sqlite::Sqlite() :
  Queue("libsqlite3")
{
  command_line_options().add_options()
    ("libsqlite3-db", boost::program_options::value(&schema), "Database file to use.")
    ("store-queue-on-shutdown", boost::program_options::bool_switch(&_store_on_shutdown)->default_value(false), "Store queue on shutdown only.")
    ("libsqlite3-table", boost::program_options::value(&table)->default_value(GEARMAND_QUEUE_SQLITE_DEFAULT_TABLE), "Table to use.")
    ;
}

Sqlite::~Sqlite()
{
}

gearmand_error_t Sqlite::initialize()
{
  gearmand::queue::Instance* exec_queue;

  try
  {
    exec_queue= new gearmand::queue::Instance { schema, table };
  }
  catch (std::bad_alloc &)
  {
    return GEARMAND_MEMORY_ALLOCATION_FAILURE;
  }

  exec_queue->store_on_shutdown(_store_on_shutdown);

  gearmand_error_t rc{};
  if ((rc= exec_queue->init()) != GEARMAND_SUCCESS)
  {
    delete exec_queue;
    return rc;
  }
  gearman_server_set_queue(Gearmand()->server, exec_queue);

  return rc;
}

void initialize_sqlite()
{
  static Sqlite local_instance{};
}

} // namespace queue
} // namespace plugins
} // namespace gearmand
