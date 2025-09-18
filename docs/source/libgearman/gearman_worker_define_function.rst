======================================================
Creating workers with gearman_worker_define_function()
======================================================

--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

.. c:type:: gearman_function_t

.. c:function:: gearman_return_t gearman_worker_define_function(gearman_worker_st *worker, const char *function_name, const size_t function_name_length, const gearman_function_t function, const uint32_t timeout, void *context)

.. c:type:: gearman_function_fn

.. c:type:: gearman_aggregator_fn

Link with -lgearman

-----------
DESCRIPTION
-----------

:c:func:`gearman_worker_define_function` defines functions for a worker.

The interface is callback by design. When the server has a job for the worker, :c:type:`gearman_function_fn` is evoked with a :c:type:`gearman_job_st` representing the job, and the context that was defined originally when the function was defined.

Results are sent back to the client by invoking :c:func:`gearman_job_send_data`.

If the client specified an reducer function, then the output of the :c:type:`gearman_function_fn` will be sent to that function. You can split the work out to the reducer function by sending data multiple times with :c:func:`gearman_job_send_data`.

If any errors are detected then the entire job is cancelled.  The :c:type:`gearman_aggregator_fn` will
be called when all mapped jobs have completed. The result of this function
will be what is returned to the client. 

The callback function needs to return one of the following errors:

:c:type:`GEARMAN_SUCCESS`

The function was successful.

:c:type:`GEARMAN_FAIL` 

An error has occurred, the job we not processed, and the worker cannot continue.

:c:type:`GEARMAN_ERROR`

A transient error has occurred, like a network failure, and the job can be restarted.

If a value other then the above are returned it is converted to a :c:type:`GEARMAN_FAIL` and :c:func:`gearman_worker_work` returns :c:type:`GEARMAN_INVALID_ARGUMENT`.

:c:func:`gearman_job_send_complete` and :c:func:`gearman_job_send_fail` cannot be used with any functions created with :c:func:`gearman_worker_define_function`.

------------
RETURN VALUE
------------

:c:type:`gearman_return_t`

----
HOME
----


To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_


--------
SEE ALSO
--------

:manpage:`gearmand(8)` :manpage:`libgearman(3)` :manpage:`gearman_strerror(3)` :manpage:`gearman_client_error` :manpage:`gearman_client_execute_reduce`


