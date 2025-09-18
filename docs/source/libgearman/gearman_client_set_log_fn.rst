===============
Logging Workers
===============

--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

.. c:function:: void gearman_client_set_log_fn(gearman_worker_st *worker, gearman_log_fn *function, void *context, gearman_verbose_t verbose)

----------- 
DESCRIPTION 
-----------

:c:func:`gearman_client_set_log_fn` is similar to :c:func:`gearman_worker_set_log_fn` but it used with clients, aka, :c:type:`gearman_client_st`.
:c:func:`gearman_client_set_log_fn` allows you to register a callback that will be passed all error messages that are givin to the client.  

See :c:type:`gearman_log_fn` for more information on the callback.

See :c:type:`gearman_verbose_t` for more information on logging levels.

------------ 
RETURN VALUE 
------------

None

----
HOME
----

To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_

--------
SEE ALSO
--------

:manpage:`gearmand(8)` :manpage:`libgearman(3)` :manpage:`gearman_log_fn(3)`

