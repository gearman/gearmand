===========================================
Verbose levels for logs (gearman_verbose_t)
===========================================

--------
SYNOPSIS 
--------

#include <libgearman/gearman.h>

.. c:type:: gearman_verbose_t

.. c:function:: const char *gearman_verbose_name(gearman_verbose_t verbose)

Link with -lgearman

-----------
DESCRIPTION 
-----------

:c:func:`gearman_verbose_name` takes a :c:type:`gearman_verbose_t` and returns a character representation of it.

Possible values of :c:type:`gearman_verbose_t`:

.. c:type:: GEARMAN_VERBOSE_FATAL

Fatal errors.

.. c:type:: GEARMAN_VERBOSE_ERROR

All errors.

.. c:type:: GEARMAN_VERBOSE_INFO

General information state about any events.

.. c:type:: GEARMAN_VERBOSE_DEBUG

Information calls left in the code for debugging events.


------------
RETURN VALUE
------------

A character string representing the verbose leval.

----
HOME
----

To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_


--------
SEE ALSO
--------

:manpage:`gearmand(8)` :manpage:`libgearman(3)`
