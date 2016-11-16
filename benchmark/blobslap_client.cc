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
 * @brief Blob slap client utility
 */

#include "gear_config.h"
#include <benchmark/benchmark.h>

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <unistd.h>

#define BLOBSLAP_DEFAULT_NUM_TASKS 10
#define BLOBSLAP_DEFAULT_BLOB_MIN_SIZE 0
#define BLOBSLAP_DEFAULT_BLOB_MAX_SIZE 1024
#define BLOBSLAP_BUFFER_SIZE 8192

static gearman_return_t _created(gearman_task_st *task);
static gearman_return_t _data(gearman_task_st *task);
static gearman_return_t _status(gearman_task_st *task);
static gearman_return_t _complete(gearman_task_st *task);
static gearman_return_t _fail(gearman_task_st *task);

namespace {
  void client_logging_fn(const char *message, gearman_verbose_t verbose, void*)
  {
    fprintf(stderr, "%s (%s)\n", message, gearman_verbose_name(verbose));
  }
}

static void _usage(char *name);

int main(int argc, char *argv[])
{
  gearman_benchmark_st benchmark;
  int c;
  char *host= NULL;
  in_port_t port= 0;
  const char *function= GEARMAN_BENCHMARK_DEFAULT_FUNCTION;
  uint32_t num_tasks= BLOBSLAP_DEFAULT_NUM_TASKS;
  size_t min_size= BLOBSLAP_DEFAULT_BLOB_MIN_SIZE;
  size_t max_size= BLOBSLAP_DEFAULT_BLOB_MAX_SIZE;
  unsigned long int count= 1;
  gearman_client_st master_client;
  bool shutdown_worker= false;

  if (gearman_client_create(&master_client) == NULL)
  {
    std::cerr << "Failed to allocate memory for client" << std::endl;
    return EXIT_FAILURE;
  }

  gearman_client_add_options(&master_client, GEARMAN_CLIENT_UNBUFFERED_RESULT);

  while ((c= getopt(argc, argv, "bc:f:h:m:M:n:p:s:ve?")) != -1)
  {
    switch(c)
    {
    case 'b':
      benchmark.background= true;
      break;

    case 'c':
      count= strtoul(optarg, NULL, 10);
      break;

    case 'f':
      function= optarg;
      break;

    case 'h':
      {
        if (gearman_failed(gearman_client_add_server(&master_client, host, port)))
        {
          std::cerr << "Failed while adding server " << host << ":" << port << " :" << gearman_client_error(&master_client) << std::endl;
          exit(EXIT_FAILURE);
        }
      }
      break;

    case 'm':
      min_size= static_cast<size_t>(strtoul(optarg, NULL, 10));
      break;

    case 'M':
      max_size= static_cast<size_t>(strtoul(optarg, NULL, 10));
      break;

    case 'n':
      num_tasks= uint32_t(strtoul(optarg, NULL, 10));
      break;

    case 'p':
      port= in_port_t(atoi(optarg));
      break;

    case 's':
      srand(uint32_t(atoi(optarg)));
      break;

    case 'e':
      shutdown_worker= true;
      break;

    case 'v':
      benchmark.verbose++;
      break;

    case '?':
      gearman_client_free(&master_client);
      _usage(argv[0]);
      exit(EXIT_SUCCESS);

    default:
      gearman_client_free(&master_client);
      _usage(argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  if (benchmark.verbose > 2)
  {
    gearman_client_set_log_fn(&master_client, client_logging_fn, NULL, GEARMAN_VERBOSE_DEBUG);
  }

  if (host == NULL)
  {
    if (getenv("GEARMAN_SERVERS") == NULL)
    {
      if (gearman_failed(gearman_client_add_server(&master_client, NULL, port)))
      {
        std::cerr << "Failing to add localhost:" << port << " :" << gearman_client_error(&master_client) << std::endl;
        exit(EXIT_FAILURE);
      }
    }
  }

  if (min_size > max_size)
  {
    std::cerr << "Min data size must be smaller than max data size" << std::endl;
    exit(EXIT_FAILURE);
  }

  if (num_tasks == 0)
  {
    std::cerr << "Number of tasks must be larger than zero\n" << std::endl;
    exit(EXIT_FAILURE);
  }

  gearman_task_st *tasks= new gearman_task_st[num_tasks];
  if (not tasks)
  {
    std::cerr << "Failed to allocate " << num_tasks << " tasks" << std::endl;
    exit(EXIT_FAILURE);
  }
  
  char *blob= new char[max_size];
  if (not blob)
  {
    std::cerr << "Failed to allocate blob with length of " << max_size << std::endl;
    exit(EXIT_FAILURE);
  }
  memset(blob, 'x', max_size); 

  bool error= false;
  do
  {
    gearman_client_st client;
    if (gearman_client_clone(&client, &master_client) == NULL)
    {
      std::cerr << "Failed to allocate client clone" << std::endl;
      exit(EXIT_FAILURE);
    }

    for (uint32_t x= 0; x < num_tasks; x++)
    {
      size_t blob_size;

      if (min_size == max_size)
      {
        blob_size= max_size;
      }
      else
      {
        blob_size= size_t(rand());

        if (max_size > RAND_MAX)
          blob_size*= size_t(rand()) + 1;

        blob_size= (blob_size % (max_size - min_size)) + min_size;
      }

      const char *blob_ptr= blob_size ? blob : NULL;

      gearman_return_t ret;
      if (benchmark.background)
      {
        (void)gearman_client_add_task_background(&client, &(tasks[x]),
                                                 &benchmark, function, NULL,
                                                 blob_ptr, blob_size, &ret);
      }
      else
      {
        (void)gearman_client_add_task(&client, &(tasks[x]), &benchmark,
                                      function, NULL, blob_ptr, blob_size,
                                      &ret);
      }

      if (gearman_failed(ret))
      {
        if (ret == GEARMAN_LOST_CONNECTION)
        {
          if (benchmark.verbose > 1)
          {
            std::cerr << "Error occurred while trying to add task: " << gearman_client_error(&client);
          }
          continue;
        }

        if (benchmark.background)
        {
          std::cerr << "Task #" << x << " failed during gearman_client_add_task_background(" << gearman_strerror(ret) << " -> " << gearman_client_error(&client) << std::endl ;
        }
        else
        {
          std::cerr << "Task #" << x << " failed during gearman_client_add_task(" << gearman_strerror(ret) << " -> " << gearman_client_error(&client) << std::endl ;
        }

        error= true;
        goto exit_immediately;
      }
    }

    gearman_client_set_created_fn(&client, _created);
    gearman_client_set_data_fn(&client, _data);
    gearman_client_set_status_fn(&client, _status);
    gearman_client_set_complete_fn(&client, _complete);
    gearman_client_set_fail_fn(&client, _fail);

    gearman_client_set_timeout(&client, 1000);
    gearman_return_t ret;
    do {
      ret= gearman_client_run_tasks(&client);
    } while (gearman_continue(ret));

    if (ret == GEARMAN_TIMEOUT)
    {
      error= true;
    }
    else if (gearman_failed(ret) and ret != GEARMAN_LOST_CONNECTION)
    {
      std::cerr << "gearman_client_run_tasks(" << gearman_strerror(ret) << ") -> " << gearman_client_error(&client);
      for (uint32_t x= 0; x < num_tasks; x++)
      {
        if (gearman_task_error(&tasks[x]))
        {
          std::cerr << "\t Task #" << x << " failed with " << gearman_task_error(&tasks[x]) << std::endl; 
        }
      }

      error= true;
    }

    for (uint32_t x= 0; x < num_tasks; x++)
    {
      gearman_task_free(&(tasks[x]));
    }

    count--;

    gearman_client_free(&client);
  } while (count or error);

exit_immediately:
  if (shutdown_worker)
  {
    gearman_client_do(&master_client, "shutdown", 0, 0, 0, 0, 0);
  }

  delete [] blob;
  delete [] tasks;
  gearman_client_free(&master_client);

  if (benchmark.verbose)
  {
    std::cout << "Successfully completed all tasks" << std::endl;
  }

  return error ? EXIT_FAILURE : 0;
}

static gearman_return_t _created(gearman_task_st *task)
{
  gearman_benchmark_st *benchmark= static_cast<gearman_benchmark_st *>(gearman_task_context(task));

  if (benchmark->background && benchmark->verbose > 0)
  {
    benchmark_check_time(benchmark);
  }

  if (benchmark->verbose > 2)
  {
    std::cout << "Created: " << gearman_task_job_handle(task) << std::endl;
  }

  return GEARMAN_SUCCESS;
}

static gearman_return_t _status(gearman_task_st *task)
{
  gearman_benchmark_st *benchmark= static_cast<gearman_benchmark_st *>(gearman_task_context(task));

  if (benchmark->verbose > 2)
  {
    std::cout << "Status " << gearman_task_job_handle(task) << " " << gearman_task_numerator(task) << " " << gearman_task_denominator(task) << std::endl;
  }

  return GEARMAN_SUCCESS;
}

static gearman_return_t _data(gearman_task_st *task)
{
  char buffer[BLOBSLAP_BUFFER_SIZE];
  gearman_return_t ret;

  gearman_benchmark_st *benchmark= static_cast<gearman_benchmark_st *>(gearman_task_context(task));

  while (1)
  {
    size_t size= gearman_task_recv_data(task, buffer, BLOBSLAP_BUFFER_SIZE, &ret);
    if (gearman_failed(GEARMAN_SUCCESS))
    {
      return ret;
    }
    if (size == 0)
    {
      break;
    }
  }

  if (benchmark->verbose > 2)
  {
    std::cerr << "Data: " <<  gearman_task_job_handle(task)  << " " << gearman_task_data_size(task) << std::endl;
  }

  return GEARMAN_SUCCESS;
}

static gearman_return_t _complete(gearman_task_st *task)
{
  char buffer[BLOBSLAP_BUFFER_SIZE];

  gearman_benchmark_st *benchmark= static_cast<gearman_benchmark_st *>(gearman_task_context(task));

  while (1)
  {
    gearman_return_t ret;
    size_t size= gearman_task_recv_data(task, buffer, BLOBSLAP_BUFFER_SIZE, &ret);
    if (gearman_failed(ret))
    {
      return ret;
    }

    if (size == 0)
    {
      break;
    }
  }

  if (benchmark->verbose > 0)
  {
    benchmark_check_time(benchmark);
  }

  if (benchmark->verbose > 1)
  {
    std::cout << "Completed: " << gearman_task_job_handle(task) << " " << gearman_task_data_size(task) << std::endl;
  }

  return GEARMAN_SUCCESS;
}

static gearman_return_t _fail(gearman_task_st *task)
{
  gearman_benchmark_st *benchmark= static_cast<gearman_benchmark_st *>(gearman_task_context(task));

  if (benchmark->verbose > 0)
    benchmark_check_time(benchmark);

  if (benchmark->verbose > 1)
  {
    std::cerr << "Failed " << gearman_task_job_handle(task) << " " << gearman_task_error(task) << std::endl;
  }

  return GEARMAN_SUCCESS;
}

static void _usage(char *name)
{
  printf("\nusage: %s\n"
         "\t[-c count] [-f <function>] [-h <host>] [-m <min_size>]\n"
         "\t[-M <max_size>] [-n <num_tasks>] [-p <port>] [-s] [-v]\n\n", name);
  printf("\t-c <count>     - number of times to run all tasks\n");
  printf("\t-f <function>  - function name for tasks (default %s)\n", GEARMAN_BENCHMARK_DEFAULT_FUNCTION);
  printf("\t-h <host>      - job server host, can specify many\n");
  printf("\t-m <min_size>  - minimum blob size (default %d)\n", BLOBSLAP_DEFAULT_BLOB_MIN_SIZE);
  printf("\t-M <max_size>  - maximum blob size (default %d)\n", BLOBSLAP_DEFAULT_BLOB_MAX_SIZE);
  printf("\t-n <num_tasks> - number of tasks to run at once (default %d)\n", BLOBSLAP_DEFAULT_NUM_TASKS);
  printf("\t-p <port>      - job server port\n");
  printf("\t-s <seed>      - seed random number for blobsize with <seed>\n");
  printf("\t-e             - tell worker to shutdown when done\n");
  printf("\t-b             - background\n");
  printf("\t-v             - increase verbose level\n");
}
