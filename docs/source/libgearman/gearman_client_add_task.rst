==============
Creating tasks
==============

-------- 
SYNOPSIS 
--------

#include <libgearman/gearman.h>

.. c:function:: gearman_task_st *gearman_client_add_task(gearman_client_st *client, gearman_task_st *task, void *context, const char *function_name, const char *unique, const void *workload, size_t workload_size, gearman_return_t *ret_ptr)

.. c:function:: gearman_task_st *gearman_client_add_task_high(gearman_client_st *client, gearman_task_st *task, void *context, const char *function_name, const char *unique, const void *workload, size_t workload_size, gearman_return_t *ret_ptr)

.. c:function:: gearman_task_st *gearman_client_add_task_low(gearman_client_st *client, gearman_task_st *task, void *context, const char *function_name, const char *unique, const void *workload, size_t workload_size, gearman_return_t *ret_ptr)

Link with -lgearman

-----------
DESCRIPTION
-----------

:c:func:`gearman_client_add_task` creates a task and adds it to the given :c:type:`gearman_client_st`. Execution of the task does not begin until :c:func:`gearman_client_run_tasks` is called.

If the unique value is not set, then a unique will be assigned.

:c:func:`gearman_client_add_task_high` and :c:func:`gearman_client_add_task_low` are identical to :c:func:`gearman_client_do`, only they set the priority to either high or low. 

------------
RETURN VALUE
------------

The :c:type:`gearman_task_st` is created and a pointer to it is returned. On error NULL is returned and ret_ptr is set with a :c:type:`gearman_return_t`.

----
HOME
----

To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_


.. seealso::
  :manpage:`gearmand(8)` :manpage:`libgearman(3)`
