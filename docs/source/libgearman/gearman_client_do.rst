======================== 
Issuing a single request 
========================


--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

.. c:function:: void *gearman_client_do(gearman_client_st *client, const char *function_name, const char *unique, const void *workload, size_t workload_size, size_t *result_size, gearman_return_t *ret_ptr)

.. versionchanged:: 0.21
   :c:type:`GEARMAN_PAUSE` will no longer be returned. A do operation will now run till completion or error.

.. c:function:: void *gearman_client_do_high(gearman_client_st *client, const char *function_name, const char *unique, const void *workload, size_t workload_size, size_t *result_size, gearman_return_t *ret_ptr) 

.. c:function:: void *gearman_client_do_low(gearman_client_st *client, const char *function_name, const char *unique, const void *workload, size_t workload_size, size_t *result_size, gearman_return_t *ret_ptr)


-----------
DESCRIPTION
-----------


:c:func:`gearman_client_do` executes a single request to the gearmand
server and waits for a reply. 

:c:func:`gearman_client_do_high` and :c:func:`gearman_client_do_low` are
identical to :c:func:`gearman_client_do`, only they set the priority to
either high or low. 

All of the functions will block until either a response or an error is
returned.

------------
RETURN VALUE
------------


:c:func:`gearman_client_do` returns a pointer to a value that the caller must release. If ret_ptr is provided any errors that have occurred will be stored in it. Since a NULL/zero value is a valid value, you will always need to check ret_ptr if you are concerned with errors.

-------
Example
-------

.. literalinclude:: examples/gearman_client_do_example.c
  :language: c


----
HOME
----


To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_


.. seealso::
  :manpage:`gearmand(8)` :manpage:`libgearman(3)` :manpage:`gearman_strerror(3)`
