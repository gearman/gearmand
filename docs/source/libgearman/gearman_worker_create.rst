===================================
Creating Gearman Worker Connections
===================================

--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

.. c:function:: gearman_worker_st *gearman_worker_create(gearman_worker_st *client)

.. c:function:: gearman_worker_st *gearman_worker_clone(gearman_worker_st *client, const gearman_worker_st *from)

.. c:function:: void gearman_worker_free(gearman_worker_st *client)

Link with -lgearman

-----------
DESCRIPTION
-----------


gearman_worker_create() is used to create a :c:type:`gearman_worker_st`  structure that will then
be used by other libgearman(3) client functions to communicate with the server. You
should either pass a statically declared :c:type:`gearman_worker_st`  to gearman_worker_create) or
a NULL. If a NULL passed in then a structure is allocated for you.

:c:func:`gearman_worker_clone` is similar to :c:func:`gearman_worker_create` but it copies the
defaults and list of servers from the source :c:type:`gearman_worker_st`. If you pass a null as
the argument for the source to clone, it is the same as a call to gearman_worker_create().
If the destination argument is NULL a :c:type:`gearman_worker_st`  will be allocated for you.

To clean up memory associated with a :c:type:`gearman_worker_st`  structure you should pass
it to :c:func:`gearman_worker_free` when you are finished using it. :c:type:`gearman_worker_free` is
the only way to make sure all memory is deallocated when you finish using
the structure.

You may wish to avoid using :c:func:`gearman_worker_create` or :c:func:`gearman_worker_clone` with a
stack based allocation, ie the first parameter. The most common issues related to ABI safety involve
heap allocated structures.


------------
RETURN VALUE
------------


:c:func:`gearman_worker_create` returns a pointer to the :c:type:`gearman_worker_st` that was created
(or initialized). On an allocation failure, it returns NULL.

:c:func:`gearman_worker_clone` returns a pointer to the :c:type:`gearman_worker_st` that was created
(or initialized). On an allocation failure, it returns NULL.


----
HOME
----


To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_


.. seealso::
  :manpage:`gearmand(8)` :manpage:`libgearman(3)` :manpage:`gearman_strerror(3)`
