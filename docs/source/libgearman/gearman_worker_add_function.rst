=================================
Create a function with a callback
=================================

--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

.. c:type:: gearman_worker_fn

.. c:function:: gearman_return_t gearman_worker_add_function(gearman_worker_st *worker, const char *function_name, uint32_t timeout, gearman_worker_fn *function, void *context)

Compile and link with -lgearman

-----------
DESCRIPTION
-----------

:c:func:`gearman_worker_add_function` adds function with a callback to a :c:type:`gearman_worker_st` . 
A :c:type:`gearman_job_st` is passed to the callback, it can be used to send messages to the client about the state of the work. 

The context provided will only be shared with the function created.

To remove the function call :c:func:`gearman_worker_unregister`.


------------
RETURN VALUE
------------


:c:func:`gearman_worker_add_function` returns a :c:type:`gearman_return_t` with either :c:type:`GEARMAN_SUCCESS`, or an error. Additional information on the error can be found with :c:function::`gearman_worker_error` 


----
HOME
----


To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_


--------
SEE ALSO
--------

:manpage:`gearmand(8)` :manpage:`libgearman(3)` :manpage:`gearman_worker_st(3)`
