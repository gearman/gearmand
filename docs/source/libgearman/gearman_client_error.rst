
=============================
Errors reported to the client
=============================

--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

.. c:function:: const char *gearman_client_error(const gearman_client_st *client)

.. c:function:: int gearman_client_errno(gearman_client_st *client)

Ling with -lgearman


-----------
DESCRIPTION
-----------

:c:func:`gearman_client_error` and :c:func:`gearman_client_errno` report on the last errors that the client reported/stored in :c:type:`gearman_client_st`. If you are interested in recording all errors please see :c:func:`gearman_client_set_log_fn`.

------------
RETURN VALUE
------------

:c:func:`gearman_client_errno` returns the last :manpage:`errno` that the client recorded.

----
HOME
----

To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_

.. seealso::
  :manpage:`gearmand(8)` :manpage:`libgearman(3)`



