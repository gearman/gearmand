
==============
Adding Servers
==============


--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

.. c:function:: gearman_return_t gearman_worker_add_server(gearman_worker_st *worker, const char *host, in_port_t port)

.. c:function:: gearman_return_t gearman_worker_add_servers(gearman_worker_st *worker, const char *servers)

.. c:function:: void gearman_worker_remove_servers(gearman_worker_st *worker)

Link with -lgearman

-----------
DESCRIPTION
-----------

:c:func:`gearman_worker_add_server` will add an additional :program:`gearmand` server to the list of servers that the worker will take work from. 

:c:func:`gearman_worker_remove_servers` will remove all servers from the :c:type:`gearman_worker_st`.

:c:func:`gearman_worker_add_servers` takes a list of :program:`gearmand` servers that will be parsed to provide servers for the worker. The format for this is SERVER[:PORT][,SERVER[:PORT]]...

Examples of this are::
 
  10.0.0.1,10.0.0.2,10.0.0.3
  localhost234,jobserver2.domain.com:7003,10.0.0.3


------------
RETURN VALUE
------------

:c:func:`gearman_worker_add_server` and :c:func:`gearman_worker_remove_servers` return :c:type:`gearman_return_t`.

----
HOME
----

To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_

.. seealso:: :program:`gearmand` :doc:`../libgearman`  :c:type:`gearman_worker_st`
