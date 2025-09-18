
=============================
Errors returned to the worker
=============================

--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

.. c:function:: const char *gearman_worker_error(const gearman_worker_st *worker)

.. c:function:: int gearman_worker_errno(gearman_worker_st *worker)

Link with -lgearman


-----------
DESCRIPTION
-----------

:c:func:`gearman_worker_error` and :c:func:`gearman_worker_errno` report on the last errors that the worker reported/stored in :c:type:`gearman_worker_st`. If you are interested in recording all errors please see :c:func:`gearman_worker_set_log_fn`.

------------
RETURN VALUE
------------

:c:func:`gearman_worker_errno` returns the last :manpage:`errno` that the worker recorded.

----
HOME
----

To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_


.. seealso::
  :manpage:`gearmand(8)` :manpage:`libgearman(3)`


