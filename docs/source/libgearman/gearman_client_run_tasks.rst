============= 
Running tasks 
============= 

-------- 
SYNOPSIS 
--------

#include <libgearman/gearman.h>

.. c:function:: gearman_return_t gearman_client_run_tasks(gearman_client_st *client)

Link with -lgearman

-----------
DESCRIPTION
-----------

:c:func:`gearman_client_run_tasks` executes one or more tasks that have
been added via :c:func:`gearman_client_add_task`,
:c:func:`gearman_client_add_task_status` or
:c:func:`gearman_client_add_task_background`.

:c:func:`gearman_client_run_tasks` can also be used with
:c:func:`gearman_execute` if either non-blocking or background tasks were
created with it.

------------
RETURN VALUE
------------

:c:type:`gearman_return_t`

If :c:type:`GEARMAN_PAUSE` is returned one of the tasks has "paused" in
order to give the caller an opportunity to take some actions (like read
data, handle an exception, etc...). See :c:type:`gearman_task_st` for more
information about the state of a task.

----
HOME
----

To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_


.. seealso::
  :manpage:`gearmand(8)` :manpage:`libgearman(3)`
