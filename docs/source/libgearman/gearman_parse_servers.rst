====================
Parsing server lists
====================

--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

.. c:type:: gearman_parse_server_fn

.. c:function:: gearman_return_t gearman_parse_servers(const char *servers, gearman_parse_server_fn *function, void *context)

Link with -lgearman

-----------
DESCRIPTION
-----------

:c:func:`gearman_parse_servers` parses a list of servers and call the :c:func:`gearman_parse_server_fn` for each server.

------------
RETURN VALUE
------------

A value of type :c:type:`gearman_return_t`  is returned.
On success that value will be :c:type:`GEARMAN_SUCCESS`.
Use gearman_strerror to translate this value to a printable string.

----
HOME
----


To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_


--------
SEE ALSO
--------

:manpage:`gearmand(8)` :manpage:`libgearman(3)`
