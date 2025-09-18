============== 
Executing Work
============== 


--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

.. c:function:: gearman_task_st *gearman_execute(gearman_client_st *client, const char *function_name, size_t function_name_length, const char *unique, size_t unique_length, gearman_work_t *workload, gearman_argument_t *arguments, void *context)

.. c:function:: gearman_task_st *gearman_execute_by_partition(gearman_client_st *client, const char *partition_function, const size_t partition_function_length, const char *function_name, const size_t function_name_length, const char *unique_str, const size_t unique_length, gearman_work_t *workload, gearman_argument_t *arguments, void *context)

Link with -lgearman

-----------
DESCRIPTION
-----------

:c:func:`gearman_execute` is used to create a new :c:type:`gearman_task_st` that is executed against the function that is found via the function_name argument. 

:c:type:`gearman_work_t` can be used to describe the work that will be
executed, it is built with :c:func:`gearman_argument_make`.  The argument
unique_str is optional, but if supplied it is used for coalescence by
:program:`gearmand`.

:c:type:`gearman_argument_t` is the work that the :term:`client` will send
the to the server

If :c:func:`gearman_execute` is given a :c:type:`gearman_work_t` that has been built with a reducer, it takes the :c:type:`gearman_argument_t` and executs it against a :term:`function` as it normally would, but it tells the function to then process the results through a :term:`reducer` function that the :c:type:`gearman_work_t` was created with.

What is happening is that the function is mappping/splitting work up into units, and then sending each of them to the reducer function. Once all work is completed, the :term:`mapper` function will aggregate the work via an aggregator function, :c:type:`gearman_aggregator_fn`, and return a result.

If any of the units of work error, the job will be aborted. The resulting value will be stored in the :c:type:`gearman_task_st`.

The result can be obtained from the task by calling
:c:func:`gearman_task_result` to gain the :c:type:`gearman_result_st`.

------------
RETURN VALUE
------------


:c:func:`gearman_execute` returns a c:type:`gearman_task_st`.  

------- 
Example 
-------

.. literalinclude:: examples/gearman_execute_example.c  
   :language: c

----
HOME
----


To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_


--------
SEE ALSO
--------

:manpage:`gearmand(8)` :manpage:`libgearman(3)`
