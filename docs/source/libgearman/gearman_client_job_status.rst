==============================
Retrieving the status of a job
==============================

--------
SYNOPSIS
--------

#include <libgearman/gearman.h>

.. c:function:: gearman_return_t gearman_client_job_status(gearman_client_st *client, gearman_job_handle_t job_handle, bool *is_known, bool *is_running, uint32_t *numerator, uint32_t *denominator)

Link with -lgearman

-----------
DESCRIPTION
-----------

:c:func:`gearman_client_job_status` is used to find the status of a job via its :c:type:`gearman_job_handle_t`. The arguments is_known, is_running, numerator, and denominator are all optional. Unless a Gearman worker has sent numerator/denominator via :c:func:`gearman_job_send_status`, those values will be zero.

If the arguments is_known and is_running are omitted then the function returns state data via :c:type:`gearman_return_t`. 

------------
RETURN VALUE
------------

:c:type:`gearman_return_t` will be returned. :c:type:`GEARMAN_SUCCESS` will be returned upon success of the status being updated. 
If the arguments is_known and is_running are omitted then :c:func:`gearman_client_job_status` will return :c:type:`GEARMAN_JOB_EXISTS` if the :c:type:`gearman_job_handle_t` is known on the server, and
:c:type:`GEARMAN_IN_PROGRESS` if it is running. Knowing whether the job is running or not has higher precedence. :c:func:`gearman_continue` can be used for loops where you want to exit from the loop once the job has been completed. 

.. warning:: 
  For loops you should always check :c:type:`gearman_return_t` with :c:func:`gearman_continue` even if you specifiy the argument is_known or is_running. A non-blocking IO call can return something other then :c:type:`GEARMAN_SUCCESS`, in some cases you will want to treat those values not as errors.


----
HOME
----


To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_

.. seealso::
  :manpage:`gearmand(8)` :manpage:`libgearman(3)` :manpage:`gearman_client_st(3)` :manpage:`gearman_job_handle_t(3)` :manpage:`gearman_continue(3`

