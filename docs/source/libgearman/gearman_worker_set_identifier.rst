
=============================
Setting a worker's identifier
=============================


--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

.. c:function:: gearman_return_t gearman_worker_set_identifier(gearman_worker_st *worker, const char *id, size_t id_size)

Link with -lgearman

-----------
DESCRIPTION
-----------

:c:func:`gearman_worker_set_identifier` sets the identifier that the server uses to identify the worker.


------------
RETURN VALUE
------------

:c:func:`gearman_worker_set_identifier` return :c:type:`gearman_return_t`.

----
HOME
----

To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_

.. seealso:: :program:`gearmand` :doc:`../libgearman`  :c:type:`gearman_worker_st`

