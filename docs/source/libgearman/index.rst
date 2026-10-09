==========
Libgearman
==========

`libgearman` is a small, thread-safe client library for the
gearman protocol. The code has all been written with an eye to allow
for both web and embedded usage. It handles the work behind routing
particular keys to specific servers that you specify. By default jobs go to
the first idle server in the order you added them; a client can opt into
routing by the job's unique instead, see
:c:func:`gearman_client_set_server_selection_by_unique`. It implements both
the :term:`client` and :term:`worker` interfaces.

***************
Getting Started
***************

.. toctree::
   :titlesonly:

   workers

********
Examples
********

.. toctree::
   :titlesonly:

   examples

****************
Client Functions
****************

.. toctree::
   :titlesonly:

   gearman_client
   gearman_client_add_server
   gearman_client_options
   gearman_client_set_server_selection_by_unique
   gearman_client_do
   gearman_client_do_background
   gearman_execute
   gearman_client_error
   gearman_client_set_log_fn
   gearman_client_job_status

**************
Creating Tasks
**************

.. toctree::
   :titlesonly:

   gearman_client_add_task
   gearman_client_run_tasks
   gearman_client_add_task_background
   gearman_client_add_task_status

****************
Worker Functions
****************

.. toctree::
   :titlesonly:

   gearman_worker_create
   gearman_worker_add_server
   gearman_worker_options
   gearman_worker_add_function
   gearman_worker_define_function
   gearman_worker_error
   gearman_worker_set_log_fn
   gearman_worker_set_identifier

****
Misc
****

.. toctree::
   :titlesonly:

   gearman_misc_functions
   namespace
   gearman_client_wait
   gearman_client_echo

**********
Structures
**********

.. toctree::
   :titlesonly:

   gearman_client_st 
   gearman_job_st
   gearman_task_st
   gearman_worker_st 
   gearman_job_handle_t
   gearman_argument_t 
   gearman_string_t 
   gearman_result_t 
   gearman_task_attr_t


*****
Types
*****

.. toctree::
   :titlesonly:

   gearman_allocator_t
   gearman_return_t 
   gearman_actions_t
   gearman_log_fn
   gearman_verbose_t

*****
Extra
*****

.. toctree::
   :titlesonly:

   error_descriptions
   types

