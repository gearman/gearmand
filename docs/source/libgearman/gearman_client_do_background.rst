================================
Issuing a single background task
================================

--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

.. c:type:: gearman_priority_t

.. c:function:: gearman_return_t gearman_client_do_background(gearman_client_st *client, const char *function_name, const char *unique, const void *workload, size_t workload_size, char *job_handle)

.. versionchanged:: 0.21
   :c:type:`GEARMAN_PAUSE` will no longer be returned. A do operation will now run until it has been submitted.

.. c:function:: gearman_return_t gearman_client_do_high_background(gearman_client_st *client, const char *function_name, const char *unique, const void *workload, size_t workload_size, gearman_job_handle_t job_handle)

.. c:function:: gearman_return_t gearman_client_do_low_background(gearman_client_st *client, const char *function_name, const char *unique, const void *workload, size_t workload_size, gearman_job_handle_t job_handle)

-----------
DESCRIPTION
-----------


:c:func:`gearman_client_do_background` executes a single request to the
gearmand server and returns the status via :c:type:`gearman_return_t`. 

:c:func:`gearman_client_add_task_high_background` and :c:func:`gearman_client_add_task_low_background` are identical to
:c:func:`gearman_client_do_background`, only they set the :c:type:`gearman_priority_t` to either high or low. 


If job_handle is not NULL, it will be populated with the name of the job_handle
for the task created. The job handle needs to be the size of
:c:macro:`GEARMAN_JOB_HANDLE_SIZE`. Please see :c:type:`gearman_job_handle_t` for more information.

------------
RETURN VALUE
------------

:c:type:`gearman_return_t`

-------
Example
-------

.. literalinclude:: examples/gearman_client_do_background_example.c
  :language: c


----
HOME
----


To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_


.. seealso::
  :manpage:`gearmand(8)` :manpage:`libgearman(3)` :manpage:`gearman_strerror(3)`

