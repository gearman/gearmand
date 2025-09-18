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
 * @brief Client Declarations
 */

#pragma once

#include <libgearman-1.0/interface/client.h>

/** @addtogroup gearman_client Client Declarations
 *
 * This is the interface gearman clients should use. You can run tasks one at a
 * time or concurrently.
 *
 * @ref main_page_client "See Main Page for full details."
 * @{
 */

enum gearman_client_t {
  GEARMAN_CLIENT_STATE_IDLE,
  GEARMAN_CLIENT_STATE_NEW,
  GEARMAN_CLIENT_STATE_SUBMIT,
  GEARMAN_CLIENT_STATE_PACKET
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize a client structure. Always check the return value even if passing
 * in a pre-allocated structure. Some other initialization may have failed. It
 * is not required to memset() a structure before providing it.
 *
 * @param[in] client Caller allocated structure, or NULL to allocate one.
 * @return On success, a pointer to the (possibly allocated) structure. On
 *  failure this will be NULL.
 */
GEARMAN_API
gearman_client_st *gearman_client_create(gearman_client_st *client);

/**
 * Clone a client structure.
 *
 * @param[in] client Caller allocated structure, or NULL to allocate one.
 * @param[in] from Structure to use as a source to clone from.
 * @return Same return as gearman_client_create().
 */
GEARMAN_API
gearman_client_st *gearman_client_clone(gearman_client_st *client,
                                        const gearman_client_st *from);

/**
 * Free resources used by a client structure.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 */
GEARMAN_API
void gearman_client_free(gearman_client_st *client);

/**
 * See gearman_error() for details.
 */
GEARMAN_API
const char *gearman_client_error(const gearman_client_st *client);

GEARMAN_API
gearman_return_t gearman_client_error_code(const gearman_client_st *client);

/**
 * See gearman_errno() for details.
 */
GEARMAN_API
int gearman_client_errno(const gearman_client_st *client);

/**
 * Get options for a client structure.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @return Options set for the client structure.
 */
GEARMAN_API
gearman_client_options_t gearman_client_options(const gearman_client_st *client);

/**
 * Set options for a client structure.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @param[in] options Available options for client structures.
 */
GEARMAN_API
void gearman_client_set_options(gearman_client_st *client,
                                gearman_client_options_t options);

/**
 * Add options for a client structure.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @param[in] options Available options for client structures.
 */
GEARMAN_API
void gearman_client_add_options(gearman_client_st *client,
                                gearman_client_options_t options);

GEARMAN_API
  bool gearman_client_has_option(gearman_client_st *client,
                                 gearman_client_options_t option);

/**
 * Remove options for a client structure.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @param[in] options Available options for client structures.
 */
GEARMAN_API
void gearman_client_remove_options(gearman_client_st *client,
                                   gearman_client_options_t options);

/**
 * See gearman_universal_timeout() for details.
 */
GEARMAN_API
int gearman_client_timeout(gearman_client_st *client);

/**
 * See gearman_universal_set_timeout() for details.
 */
GEARMAN_API
void gearman_client_set_timeout(gearman_client_st *client, int timeout);

/**
 * See gearman_universal_set_ssl() for details.
 */
GEARMAN_API
void gearman_client_set_ssl(gearman_client_st *client, bool ssl,
    const char *ca_file, const char *certificate, const char *key_file);

/**
 * Get the application context for a client.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @return Application context that was previously set, or NULL.
 */
GEARMAN_API
void *gearman_client_context(const gearman_client_st *client);

/**
 * Set the application context for a client.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @param[in] context Application context to set.
 */
GEARMAN_API
void gearman_client_set_context(gearman_client_st *client, void *context);

/**
 * See gearman_set_log_fn() for details.
 */
GEARMAN_API
void gearman_client_set_log_fn(gearman_client_st *client,
                               gearman_log_fn *function, void *context,
                               gearman_verbose_t verbose);

/**
 * See gearman_set_workload_malloc_fn() for details.
 */
GEARMAN_API
void gearman_client_set_workload_malloc_fn(gearman_client_st *client,
                                           gearman_malloc_fn *function,
                                           void *context);

/**
 * See gearman_set_workload_malloc_fn() for details.
 */
GEARMAN_API
void gearman_client_set_workload_free_fn(gearman_client_st *client,
                                         gearman_free_fn *function,
                                         void *context);


GEARMAN_API
  gearman_return_t gearman_client_set_memory_allocators(gearman_client_st *,
                                                        gearman_malloc_fn *malloc_fn,
                                                        gearman_free_fn *free_fn,
                                                        gearman_realloc_fn *realloc_fn,
                                                        gearman_calloc_fn *calloc_fn,
                                                        void *context);

/**
 * Add a job server to a client. This goes into a list of servers that can be
 * used to run tasks. No socket I/O happens here, it is just added to a list.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @param[in] host Hostname or IP address (IPv4 or IPv6) of the server to add.
 * @param[in] port Port of the server to add.
 * @return Standard gearman return value.
 */
GEARMAN_API
gearman_return_t gearman_client_add_server(gearman_client_st *client,
                                           const char *host, in_port_t port);

/**
 * Add a list of job servers to a client. The format for the server list is:
 * SERVER[:PORT][,SERVER[:PORT]]...
 * Some examples are:
 * 10.0.0.1,10.0.0.2,10.0.0.3
 * localhost234,jobserver2.domain.com:7003,10.0.0.3
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @param[in] servers Server list described above.
 * @return Standard gearman return value.
 */
GEARMAN_API
gearman_return_t gearman_client_add_servers(gearman_client_st *client,
                                            const char *servers);

/**
 * Remove all servers currently associated with the client.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 */
GEARMAN_API
void gearman_client_remove_servers(gearman_client_st *client);

/**
 * When in non-blocking I/O mode, wait for activity from one of the servers.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @return Standard gearman return value.
 */
GEARMAN_API
gearman_return_t gearman_client_wait(gearman_client_st *client);

/** @} */

/**
 * @addtogroup gearman_client_single Single Task Interface
 * @ingroup gearman_client
 * Use the following set of functions to run one task at a time.
 *
 * @ref main_page_client_single "See Main Page for full details."
 * @{
 */

/**
 * Run a single task and return an allocated result.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @param[in] function_name The name of the function to run.
 * @param[in] unique Optional unique job identifier, or NULL for a new UUID.
 * @param[in] workload The workload to pass to the function when it is run.
 * @param[in] workload_size Size of the workload.
 * @param[out] result_size The size of the data being returned.
 * @param[out] ret_ptr Standard gearman return value. In the case of
 *  GEARMAN_WORK_DATA, GEARMAN_WORK_WARNING, or GEARMAN_WORK_STATUS, the caller
 *  should take any actions to handle the event and then call this function
 *  again. This may happen multiple times until a GEARMAN_WORK_ERROR,
 *  GEARMAN_WORK_FAIL, or GEARMAN_SUCCESS (work complete) is returned. For
 *  GEARMAN_WORK_DATA or GEARMAN_WORK_WARNING, the result_size will be set to
 *  the intermediate data chunk being returned and an allocated data buffer
 *  will be returned. For GEARMAN_WORK_STATUS, the caller can use
 *  gearman_client_do_status() to get the current tasks status.
 * @return The result allocated by the library, this needs to be freed when the
 *  caller is done using it.
 */
GEARMAN_API
void *gearman_client_do(gearman_client_st *client,
                        const char *function_name,
                        const char *unique,
                        const void *workload, size_t workload_size,
                        size_t *result_size,
                        gearman_return_t *ret_ptr);

/**
 * Run a high priority task and return an allocated result. See
 * gearman_client_do() for parameter and return information.
 */
GEARMAN_API
void *gearman_client_do_high(gearman_client_st *client,
                             const char *function_name, const char *unique,
                             const void *workload, size_t workload_size,
                             size_t *result_size, gearman_return_t *ret_ptr);

/**
 * Run a low priority task and return an allocated result. See
 * gearman_client_do() for parameter and return information.
 */
GEARMAN_API
void *gearman_client_do_low(gearman_client_st *client,
                            const char *function_name, const char *unique,
                            const void *workload, size_t workload_size,
                            size_t *result_size, gearman_return_t *ret_ptr);

/**
 * Get the job handle for the running task. This should be used between
 * repeated gearman_client_do() (and related) calls to get information.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @return Pointer to static buffer in the client structure that holds the job
 *  handle.
 */
GEARMAN_API
const char *gearman_client_do_job_handle(gearman_client_st *client);

// Deprecatd 
GEARMAN_API
void gearman_client_do_status(gearman_client_st *client, uint32_t *numerator,
                              uint32_t *denominator);

/**
 * Run a task in the background.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @param[in] function_name The name of the function to run.
 * @param[in] unique Optional unique job identifier, or NULL for a new UUID.
 * @param[in] workload The workload to pass to the function when it is run.
 * @param[in] workload_size Size of the workload.
 * @param[out] job_handle A buffer to store the job handle in. Must be at least
    GEARMAN_JOB_HANDLE_SIZE bytes long.
 * @return Standard gearman return value.
 */
GEARMAN_API
gearman_return_t gearman_client_do_background(gearman_client_st *client,
                                              const char *function_name,
                                              const char *unique,
                                              const void *workload,
                                              size_t workload_size,
                                              gearman_job_handle_t job_handle);

/**
 * Run a high priority task in the background. See
 * gearman_client_do_background() for parameter and return information.
 */
GEARMAN_API
gearman_return_t gearman_client_do_high_background(gearman_client_st *client,
                                                   const char *function_name,
                                                   const char *unique,
                                                   const void *workload,
                                                   size_t workload_size,
                                                   gearman_job_handle_t job_handle);

/**
 * Run a low priority task in the background. See
 * gearman_client_do_background() for parameter and return information.
 */
GEARMAN_API
gearman_return_t gearman_client_do_low_background(gearman_client_st *client,
                                                  const char *function_name,
                                                  const char *unique,
                                                  const void *workload,
                                                  size_t workload_size,
                                                  gearman_job_handle_t job_handle);

/**
 * Get the status for a backgound job.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @param[in] job_handle The job handle to get status for.
 * @param[out] is_known Optional parameter to store the known status in.
 * @param[out] is_running Optional parameter to store the running status in.
 * @param[out] numerator Optional parameter to store the numerator in.
 * @param[out] denominator Optional parameter to store the denominator in.
 * @return Standard gearman return value.
 */
GEARMAN_API
gearman_return_t gearman_client_job_status(gearman_client_st *client,
                                           const gearman_job_handle_t job_handle,
                                           bool *is_known, bool *is_running,
                                           uint32_t *numerator,
                                           uint32_t *denominator);

GEARMAN_API
gearman_status_t gearman_client_unique_status(gearman_client_st *client,
                                              const char *unique, size_t unique_length);

// This is not in the API yet, subject to change
GEARMAN_API
gearman_task_st *gearman_client_add_task_status_by_unique(gearman_client_st *client,
                                                          gearman_task_st *task_ptr,
                                                          const char *unique_handle,
                                                          gearman_return_t *ret_ptr);

/**
 * Send data to all job servers to see if they echo it back. This is a test
 * function to see if the job servers are responding properly.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @param[in] workload The workload to ask the server to echo back.
 * @param[in] workload_size Size of the workload.
 * @return Standard gearman return value.
 */
GEARMAN_API
gearman_return_t gearman_client_echo(gearman_client_st *client,
                                     const void *workload,
                                     size_t workload_size);

/** @} */

/**
 * @addtogroup gearman_client_concurrent Concurrent Task Interface
 * @ingroup gearman_client
 * Use the following set of functions to multiple run tasks concurrently.
 *
 * @ref main_page_client_concurrent "See Main Page for full details."
 * @{
 */

/**
 * Free all tasks for a gearman structure.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 */
GEARMAN_API
void gearman_client_task_free_all(gearman_client_st *client);

/**
 * Set function to call when tasks are being cleaned up so applications can
 * clean up the task context.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @param[in] function Function to call to clean up task context.
 */
GEARMAN_API
void gearman_client_set_task_context_free_fn(gearman_client_st *client,
                                             gearman_task_context_free_fn *function);


/**
 * Add a task to be run in parallel.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @param[in] task Caller allocated structure, or NULL to allocate one.
 * @param[in] context Application context to associate with the task.
 * @param[in] function_name The name of the function to run.
 * @param[in] unique Optional unique job identifier, or NULL for a new UUID.
 * @param[in] workload The workload to pass to the function when it is run.
 * @param[in] workload_size Size of the workload.
 * @param[out] ret_ptr Standard gearman return value.
 * @return On success, a pointer to the (possibly allocated) structure. On
 *  failure this will be NULL.
 */
GEARMAN_API
gearman_task_st *gearman_client_add_task(gearman_client_st *client,
                                         gearman_task_st *task,
                                         void *context,
                                         const char *function_name,
                                         const char *unique,
                                         const void *workload,
                                         size_t workload_size,
                                         gearman_return_t *ret_ptr);

/**
 * Add a high priority task to be run in parallel. See
 * gearman_client_add_task() for details.
 */
GEARMAN_API
gearman_task_st *gearman_client_add_task_high(gearman_client_st *client,
                                              gearman_task_st *task,
                                              void *context,
                                              const char *function_name,
                                              const char *unique,
                                              const void *workload,
                                              size_t workload_size,
                                              gearman_return_t *ret_ptr);

/**
 * Add a low priority task to be run in parallel. See
 * gearman_client_add_task() for details.
 */
GEARMAN_API
gearman_task_st *gearman_client_add_task_low(gearman_client_st *client,
                                             gearman_task_st *task,
                                             void *context,
                                             const char *function_name,
                                             const char *unique,
                                             const void *workload,
                                             size_t workload_size,
                                             gearman_return_t *ret_ptr);

/**
 * Add a background task to be run in parallel. See
 * gearman_client_add_task() for details.
 */
GEARMAN_API
gearman_task_st *gearman_client_add_task_background(gearman_client_st *client,
                                                    gearman_task_st *task,
                                                    void *context,
                                                    const char *function_name,
                                                    const char *unique,
                                                    const void *workload,
                                                    size_t workload_size,
                                                    gearman_return_t *ret_ptr);

/**
 * Add a high priority background task to be run in parallel. See
 * gearman_client_add_task() for details.
 */
GEARMAN_API
gearman_task_st *gearman_client_add_task_high_background(gearman_client_st *client,
                                                         gearman_task_st *task,
                                                         void *context,
                                                         const char *function_name,
                                                         const char *unique,
                                                         const void *workload,
                                                         size_t workload_size,
                                                         gearman_return_t *ret_ptr);

/**
 * Add a low priority background task to be run in parallel. See
 * gearman_client_add_task() for details.
 */
GEARMAN_API
gearman_task_st *gearman_client_add_task_low_background(gearman_client_st *client,
                                                        gearman_task_st *task,
                                                        void *context,
                                                        const char *function_name,
                                                        const char *unique,
                                                        const void *workload,
                                                        size_t workload_size,
                                                        gearman_return_t *ret_ptr);

/**
 * Add task to get the status for a backgound task in parallel.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @param[in] task Caller allocated structure, or NULL to allocate one.
 * @param[in] context Application context to associate with the task.
 * @param[in] job_handle The job handle to get status for.
 * @param[out] ret_ptr Standard gearman return value.
 * @return On success, a pointer to the (possibly allocated) structure. On
 *  failure this will be NULL.
 */
GEARMAN_API
gearman_task_st *gearman_client_add_task_status(gearman_client_st *client,
                                                gearman_task_st *task,
                                                void *context,
                                                const char *job_handle,
                                                gearman_return_t *ret_ptr);

/**
 * Callback function when workload data needs to be sent for a task.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @param[in] function Function to call.
 */
GEARMAN_API
void gearman_client_set_workload_fn(gearman_client_st *client,
                                    gearman_workload_fn *function);

/**
 * Callback function when a job has been created for a task.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @param[in] function Function to call.
 */
GEARMAN_API
void gearman_client_set_created_fn(gearman_client_st *client,
                                   gearman_created_fn *function);

/**
 * Callback function when there is a data packet for a task.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @param[in] function Function to call.
 */
GEARMAN_API
void gearman_client_set_data_fn(gearman_client_st *client,
                                gearman_data_fn *function);

/**
 * Callback function when there is a warning packet for a task.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @param[in] function Function to call.
 */
GEARMAN_API
void gearman_client_set_warning_fn(gearman_client_st *client,
                                   gearman_warning_fn *function);

/**
 * Callback function when there is a status packet for a task.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @param[in] function Function to call.
 */
GEARMAN_API
void gearman_client_set_status_fn(gearman_client_st *client,
                                  gearman_universal_status_fn *function);

/**
 * Callback function when a task is complete.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @param[in] function Function to call.
 */
GEARMAN_API
void gearman_client_set_complete_fn(gearman_client_st *client,
                                    gearman_complete_fn *function);

/**
 * Callback function when there is an exception packet for a task.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @param[in] function Function to call.
 */
GEARMAN_API
void gearman_client_set_exception_fn(gearman_client_st *client,
                                     gearman_exception_fn *function);

/**
 * Callback function when a task has failed.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @param[in] function Function to call.
 */
GEARMAN_API
void gearman_client_set_fail_fn(gearman_client_st *client,
                                gearman_fail_fn *function);

/**
 * Clear all task callback functions.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 */
GEARMAN_API
void gearman_client_clear_fn(gearman_client_st *client);

/**
 * Run tasks that have been added in parallel.
 *
 * @param[in] client Structure previously initialized with
 *  gearman_client_create() or gearman_client_clone().
 * @return Standard gearman return value.
 */
GEARMAN_API
gearman_return_t gearman_client_run_tasks(gearman_client_st *client);

GEARMAN_API
bool gearman_client_compare(const gearman_client_st *first, const gearman_client_st *second);

GEARMAN_API
bool gearman_client_set_server_option(gearman_client_st *self, const char *option_arg, size_t option_arg_size);

GEARMAN_API
void gearman_client_set_namespace(gearman_client_st *self, const char *namespace_key, size_t namespace_key_size);

GEARMAN_API
gearman_return_t gearman_client_set_identifier(gearman_client_st *client,
                                               const char *id, size_t id_size);

const char *gearman_client_namespace(gearman_client_st *self);

GEARMAN_API
bool gearman_client_has_tasks(const gearman_client_st *client);

GEARMAN_API
bool gearman_client_has_active_tasks(gearman_client_st *client);

/** @} */

#ifdef __cplusplus
}
#endif
