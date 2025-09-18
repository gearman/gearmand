=============================
Gearman Client/Worker Library
=============================

.. index:: libgearman

--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

Link -lgearman

-----------
DESCRIPTION
-----------

:doc:`libgearman` is a small, thread-safe client library for the
gearman protocol. The code has all been written with an eye to allow
for both web and embedded usage. It handles the work behind routing
particular keys to specific servers that you specify (and values are
matched based on server order as supplied by you). It implements both
the :term:`client` and :term:`worker` interfaces.

All operations are performed against either a client, ie :c:type:`gearman_client_st`
or worker, ie :c:type:`gearman_worker_st`.

Client and Worker structures can either be dynamically allocated or statically
allocated. They must then b initialized by :c:func:`gearman_client_create()` or :c:func:`gearman_worker_create()`. 

Functions have been written in order to encapsulate all structures in the library. It is
recommended that you do not operate directly against the structure.

Nearly all functions return a :c:type:`gearman_return_t` value.
This value can be translated to a printable string with :c:func:`gearman_strerror()`.

:c:type:`gearman_client_st` and :c:type:`gearman_worker_st` structures are thread-safe, but each thread must
contain its own structure (that is, if you want to share these among
threads you must provide your own locking). No global variables are
used in this library.

If you are working with GNU autotools you will want to add the following to
your configure.ac to properly include libgearman in your application.

PKG_CHECK_MODULES(DEPS, libgearman >= 0.8.0)
AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)

Hope you enjoy it!

---------
CONSTANTS
---------


A number of constants have been provided for in the library.

.. c:macro:: GEARMAN_DEFAULT_TCP_PORT
 
The default port used by gearmand(3).

.. c:macro:: GEARMAN_DEFAULT_TCP_SERVICE
 
The default service used by gearmand(3).

.. c:macro:: LIBGEARMAN_VERSION_STRING
 
String value of the libgearman version such as "0.20.4"

.. c:macro:: LIBGEARMAN_VERSION_HEX
 
Hex value of the version number. "0x00048000" This can be used for comparing versions based on number.

.. c:macro:: GEARMAN_UNIQUE_SIZE

Largest number of characters that can be used for a unique value.

.. c:macro:: GEARMAN_JOB_HANDLE_SIZE

Largest number of characters that can will be used for a job handle. Please
see :c:type:`gearman_job_handle_t` for additional information.

---------------------
THREADS AND PROCESSES
---------------------


When using threads or forked processes it is important to keep an instance
of :c:type:`gearman_client_st` or :c:type:`gearman_worker_st`  per process
or thread.  Without creating your own locking structures you can not share
a single :c:type:`gearman_client_st` or :c:type:`gearman_worker_st`.


----
HOME
----


To find out more information please check:
`https://github.com/gearman/gearmand <https://github.com/gearman/gearmand>`_


--------
SEE ALSO
--------

:manpage:`gearmand(8)` :manpage:`libgearman_examples(3)`

