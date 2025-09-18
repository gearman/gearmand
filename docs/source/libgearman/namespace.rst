================ 
Using namespaces 
================

--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

.. c:function:: void gearman_client_set_namespace(gearman_client_st *self, const char *namespace_key, size_t namespace_key_size)

.. c:function:: void gearman_worker_set_namespace(gearman_worker_st *self, const char *namespace_key, size_t namespace_key_size) 

Compile and link with -lgearman

-----------
DESCRIPTION
-----------

gearman_client_set_namespace() and gearman_worker_set_namespace() set
a "namespace" for a given set of functions. Only clients and workers sharing
a namespace_key can see one anothers workloads and functions.

By setting namespace_key to NULL you can disable the namespace.

------
RETURN
------

None

----
HOME
----

To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_


--------
SEE ALSO
--------

:manpage:`gearmand(8)` :manpage:`libgearman(3)`
