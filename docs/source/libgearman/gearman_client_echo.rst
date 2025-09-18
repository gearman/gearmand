
===========================
Testing Clients and Workers
===========================

--------
SYNOPSIS
--------

#include <libgearman/gearman.h>


.. c:function:: gearman_return_t gearman_client_echo(gearman_client_st *client, const void *workload, size_t workload_size)

.. c:function:: gearman_return_t gearman_worker_echo(gearman_worker_st *worker, const void *workload, size_t workload_size)

-----------
DESCRIPTION
-----------

:c:func:`gearman_client_echo` and :c:func:`gearman_worker_echo` send a message to a :program:`gearmand` server. The server will then respond with the message that it sent. These commands are just for testing the connection to the servers that were configure for the :c:type:`gearman_client_st` and the :c:type:`gearman_worker_st` that were used.

------------
RETURN VALUE
------------

:c:type:`gearman_return_t`

----
HOME
----

To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_

.. seealso::
  :manpage:`gearmand(8)` :manpage:`libgearman(3)`

