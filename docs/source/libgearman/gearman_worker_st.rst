==========================
Worker (gearman_worker_st)
==========================


--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

.. c:type:: gearman_worker_st

.. c:type:: gearman_worker_set_task_context_free_fn

.. c:function:: int gearman_worker_timeout(gearman_worker_st *worker)

.. c:function:: void gearman_worker_set_timeout(gearman_worker_st *worker, int timeout)

.. c:function:: void *gearman_worker_context(const gearman_worker_st *worker)

.. c:function:: void gearman_worker_set_context(gearman_worker_st *worker, void *context)

.. c:function:: void gearman_worker_set_workload_malloc_fn(gearman_worker_st *worker, gearman_malloc_fn *function, void *context)

.. c:function:: void gearman_worker_set_workload_free_fn(gearman_worker_st *worker, gearman_free_fn *function, void *context)

.. c:function:: gearman_return_t gearman_worker_wait(gearman_worker_st *worker)

.. c:function:: gearman_return_t gearman_worker_register(gearman_worker_st *worker, const char *function_name, uint32_t timeout)

.. c:function:: gearman_return_t gearman_worker_unregister(gearman_worker_st *worker, const char *function_name)

.. c:function:: gearman_return_t gearman_worker_unregister_all(gearman_worker_st *worker)

.. c:function:: void gearman_job_free_all(gearman_worker_st *worker)

.. c:function:: bool gearman_worker_function_exist(gearman_worker_st *worker, const char *function_name, size_t function_length)

.. c:function:: gearman_return_t gearman_worker_work(gearman_worker_st *worker)

.. c:function:: gearman_job_st *gearman_worker_grab_job(gearman_worker_st *worker, gearman_job_st *job, gearman_return_t *ret_ptr)

Link with -lgearman

-----------
DESCRIPTION
-----------

:c:type:`gearman_worker_st` is used for worker communication with the server. 

:c:func:`gearman_worker_context` and :c:func:`gearman_worker_set_context` can be used to store an arbitrary object for the user.

:c:func:`gearman_worker_set_task_context_free_fn` sets a trigger that will be called when a :c:type:`gearman_task_st` is released.

:c:func:`gearman_worker_timeout` and :c:func:`gearman_worker_set_timeout` get and set the current timeout value, in milliseconds, for the worker.

:c:func:`gearman_worker_function_exist` is used to determine if a given worker has a specific function.

:c:func:`gearman_worker_work` have the worker execute against jobs until an error occurs.

:c:func:`gearman_worker_grab_job` Takes a job from one of the job servers. It is the responsibility of the caller to free the job once they are done. This interface is used in testing, and is very rarely the correct interface to program against.

Normally :manpage:`malloc(3)` and :manpage:`free(3)` are used for allocation and releasing workloads. :c:func:`gearman_worker_set_workload_malloc_fn` and :c:func:`gearman_worker_set_workload_free_fn` can be used to replace these with custom functions.

If you need to remove a function from the server you can call either :c:func:`gearman_worker_unregister_all` to remove all functions that the worker has told the :program:`gearmand` server about, or you can use :c:func:`gearman_worker_unregister` to remove just a single function. 

------
RETURN
------

Various

----
HOME
----

To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_

--------
SEE ALSO
--------

:manpage:`gearmand(8)` :manpage:`libgearman(3)`
