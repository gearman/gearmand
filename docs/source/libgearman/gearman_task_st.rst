======================
Task (gearman_task_st)
======================


-------- 
SYNOPSIS 
--------


#include <libgearman/gearman.h>

.. c:type:: gearman_task_st

.. c:function:: void gearman_task_free(gearman_task_st *task)

.. c:function:: void *gearman_task_context(const gearman_task_st *task)

.. c:function:: void gearman_task_set_context(gearman_task_st *task, void *context)

.. c:function:: const char *gearman_task_function_name(const gearman_task_st *task)

.. c:function:: const char *gearman_task_unique(const gearman_task_st *task)

.. c:function:: const char *gearman_task_job_handle(const gearman_task_st *task)

.. c:function:: bool gearman_task_is_known(const gearman_task_st *task)

.. c:function:: bool gearman_task_is_running(const gearman_task_st *task)

.. c:function:: uint32_t gearman_task_numerator(const gearman_task_st *task)

.. c:function:: uint32_t gearman_task_denominator(const gearman_task_st *task)

.. c:function:: void gearman_task_give_workload(gearman_task_st *task, const void *workload, size_t workload_size)

.. c:function:: size_t gearman_task_send_workload(gearman_task_st *task, const void *workload, size_t workload_size, gearman_return_t *ret_ptr)

.. c:function:: const void *gearman_task_data(const gearman_task_st *task)

.. c:function:: size_t gearman_task_data_size(const gearman_task_st *task)

.. c:function:: void *gearman_task_take_data(gearman_task_st *task, size_t *data_size)

.. c:function:: size_t gearman_task_recv_data(gearman_task_st *task, void *data, size_t data_size, gearman_return_t *ret_ptr) 

.. c:function:: const char *gearman_task_error(const gearman_task_st *task)

.. versionadded:: 0.21

.. c:function:: gearman_return_t gearman_task_return(const gearman_task_st *task)

.. versionadded:: 0.21

Link with -lgearman

-----------
DESCRIPTION
-----------

A :c:type:`gearman_task_st` represents a :term:`task`.  Work that is sent by a :term:`client` to a gearman server is seen as a task (a :term:`worker` receives a task in the form of a :term:`job`.

Tasks, i.e. :c:type:`gearman_task_st` are created by calling either :c:func:`gearman_execute`, :c:func:`gearman_client_add_task`, or :c:func:`gearman_client_add_task_background`.

:c:func:`gearman_client_add_task_status` can also create :c:type:`gearman_task_st`, these tasks will be used to
monitor a previously created :c:type:`gearman_task_st`.

:c:func:`gearman_task_free` is used to free a task. This only needs to be
done if a task was created with a preallocated structure or if you want to clean up the memory of a specific task.

:c:func:`gearman_task_set_context` sets the given context of the :c:type:`gearman_task_st`. The context can be used to pass information to a :c:type:`gearman_task_st`.

:c:func:`gearman_task_context` returns the context that was used in the creation of the :c:type:`gearman_task_st` (or that was set with :c:func:`gearman_task_set_context`.

:c:func:`gearman_task_data` returns the current data that has been returned to the task. :c:func:`gearman_task_data_size` will give you the size of the value. :c:func:`gearman_task_take_data` is the same as :c:func:`gearman_task_data` but the value that is returned must be freed by the client (:manpage:`free(3)`). :c:func:`gearman_task_recv_data` can be used with pre-allocated buffers.

:c:func:`gearman_task_is_known`, :c:func:`gearman_task_is_running`, :c:func:`gearman_task_numerator`, and :c:func:`gearman_task_denominator`, return values related to the last status update that was made to the :c:type:`gearman_task_st`. They do not cause the :c:type:`gearman_task_st` to update itself.

:c:func:`gearman_task_error` return the last error message that the
:c:type:`gearman_task_st` encountered. :c:func:`gearman_task_return`
return the last :c:type:`gearman_return_t` stored. A value of
:c:type:`GEARMAN_UNKNOWN_STATE` means that the task has not been submitted to
server yet, or that no function was available if the job has been submitted.

------------
RETURN VALUE
------------

Various. Values that are returned by :c:func:`gearman_task_take_data` must have :manpage:`free(3)` called on them.

----
HOME
----

To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_

--------
SEE ALSO
--------

:manpage:`gearmand(8)` :manpage:`libgearman(3)`
