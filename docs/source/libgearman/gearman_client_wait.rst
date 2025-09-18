=================================
Sleeping until client is has I/O.
=================================


--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

.. c:function:: gearman_return_t gearman_client_wait(gearman_client_st *client)

Link with -lgearman

-----------
DESCRIPTION
-----------

Calling :c:func:`gearman_client_wait` causes the calling code to sleep until either the timeout in :c:type:`gearman_client_st` is reached or :program:`gearmand` responds to the client.

------------
RETURN VALUE
------------

:c:type:`gearman_return_t`

----
HOME
----

To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_


--------
SEE ALSO
--------

:manpage:`gearmand(8)` :manpage:`libgearman(3)`
