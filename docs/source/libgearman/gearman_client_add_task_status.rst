==================================================
Adding a task to monitor a previously created task
==================================================

-------- 
SYNOPSIS 
--------

#include <libgearman/gearman.h>

.. c:function:: gearman_task_st *gearman_client_add_task_status(gearman_client_st *client, gearman_task_st *task, void *context, const char *job_handle, gearman_return_t *ret_ptr)

Link with -lgearman

-----------
DESCRIPTION
-----------

:c:func:`gearman_client_add_task_status` creates a :c:type:`gearman_task_st` that can be used to monitor a previously created task.

.. warning::
        You may wish to avoid using :c:func:`gearman_client_add_task` with a
        stack based allocated :c:type:`gearman_task_st`. The most common issues related to ABI safety involve
        stack allocated structures. If you use a stack based :c:type:`gearman_task_st` you must free it with :c:func:`gearman_task_free`.

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
  :manpage:`gearmand(8)` :manpage:`libgearman(3)` :manpage:`gearman_task_st(3)`


