
=================================
Job handle (gearman_job_handle_t)
=================================

-------- 
SYNOPSIS 
--------


#include <libgearman/gearman.h>

.. c:type:: gearman_job_handle_t

-----------
DESCRIPTION
-----------

A :c:type:`gearman_job_handle_t` represents a "job handle". A job handle is
a text string that represents the name of a task (in truth
gearman_job_handle_t is a typedef'ed
char[:c:macro:`GEARMAN_JOB_HANDLE_SIZE`]).

:c:func:`gearman_client_job_status` use handles to find the status of tasks. When passed to :c:func:`gearman_client_do_background` it will be populated with the job handle.

------------
RETURN VALUE
------------

None.

----
HOME
----

To find out more information please check:
`https://gearman.org/gearmand/ <https://gearman.org/gearmand/>`_

--------
SEE ALSO
--------

:manpage:`gearmand(8)` :manpage:`libgearman(3)` :c:type:`gearman_job_st`
