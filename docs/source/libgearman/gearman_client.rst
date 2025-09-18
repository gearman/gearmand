=====================================
Creating a Gearman Client Connections
=====================================

--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

.. c:function:: gearman_client_st *gearman_client_create(gearman_client_st *client)

.. c:function:: gearman_client_st *gearman_client_clone(gearman_client_st *client, const gearman_client_st *from)

.. c:function:: void gearman_client_free(gearman_client_st *client)

Link with -lgearman

-----------
DESCRIPTION
-----------


:c:func:`gearman_client_create` is used to create a c:type:`gearman_client_st`  structure that will then
be used by other libgearman client functions to communicate with the server. You
should either pass a statically declared :c:type:`gearman_client_st`  to :c:func:`gearman_client_create` or
a NULL. If a NULL passed in then a structure is allocated for you.

:c:func:`gearman_client_clone` is similar to :c:func:`gearman_client_create` but it copies the
defaults and list of servers from the source :c:type:`gearman_client_st` . If you pass a null as
the argument for the source to clone, it is the same as a call to gearman_client_create.
If the destination argument is NULL a :c:type:`gearman_client_st`  will be allocated for you.

To clean up memory associated with a :c:type:`gearman_client_st`  structure you should pass
it to gearman_client_free when you are finished using it. :c:func:`gearman_client_free` is
the only way to make sure all memory is deallocated when you finish using
the structure.

.. warning::
        You may wish to avoid using :c:func:`gearman_client_create` or :c:func:`gearman_client_clone` with a
        stack based allocation, ie the first parameter. The most common issues related to ABI safety involve
        stack allocated structures.

------------
RETURN VALUE
------------

:c:type:`gearman_client_create` returns a pointer to the gearman_client_st
that was created (or initialized). On an allocation failure, it returns
NULL.

:c:type:`gearman_client_clone` returns a pointer to the gearman_client_st that was created
(or initialized). On an allocation failure, it returns NULL.


----
HOME
----


To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_

.. seealso:: :manpage:`gearmand(8)` :manpage:`libgearman(3)` :manpage:`gearman_strerror(3)` :manpage:`gearman_client_st(3)`
